#!/usr/bin/env bash
#
# take-screenshots.sh — Build the watchface, launch each Pebble emulator
# platform, install via libpebble2, capture a protocol screenshot, and exit.
#
# Usage:
#   ./scripts/take-screenshots.sh [platform ...]
#
# If no platforms are specified, defaults to: basalt chalk emery
# Screenshots are saved to ./screenshots/<platform>.png
#
# Requirements (install into the Pebble SDK venv):
#   pip install libpebble2 pypng
#
set -euo pipefail

PEBBLE_SDK="${PEBBLE_SDK:-/opt/pebble-sdk}"
PYTHON="${PEBBLE_SDK}/.env/bin/python3"
SDK_ROOT="${HOME}/.pebble-sdk"
SDK_VERSION="4.3"

DEFAULT_PLATFORMS=(basalt chalk emery)
PLATFORMS=("${@:-${DEFAULT_PLATFORMS[@]}}")
OUTDIR="./screenshots"
RENDER_WAIT=5   # seconds to let the watchface render before capturing

mkdir -p "$OUTDIR"

# Start a virtual framebuffer if no display is set (headless / CI)
if [ -z "${DISPLAY:-}" ]; then
  echo "Starting Xvfb (headless mode)..."
  Xvfb :99 -screen 0 1024x768x24 &
  XVFB_PID=$!
  export DISPLAY=:99
  trap "kill $XVFB_PID 2>/dev/null" EXIT
fi

# Platform-specific QEMU machine flags
declare -A MACHINE_ARGS
MACHINE_ARGS[basalt]="-machine pebble-snowy-bb -cpu cortex-m4 -pflash"
MACHINE_ARGS[chalk]="-machine pebble-s4-bb -cpu cortex-m4 -pflash"
MACHINE_ARGS[emery]="-machine pebble-robert-bb -cpu cortex-m4 -pflash"
MACHINE_ARGS[diorite]="-machine pebble-silk-bb -cpu cortex-m4 -mtdblock"
MACHINE_ARGS[aplite]="-machine pebble-bb2 -cpu cortex-m3 -mtdblock"

for platform in "${PLATFORMS[@]}"; do
  echo "=== Capturing screenshot for: $platform ==="

  MICRO_FLASH="${SDK_ROOT}/SDKs/${SDK_VERSION}/sdk-core/pebble/${platform}/qemu/qemu_micro_flash.bin"
  SPI_FLASH="${SDK_ROOT}/${SDK_VERSION}/${platform}/qemu_spi_flash.bin"
  SPI_FLASH_BZ2="${SDK_ROOT}/SDKs/${SDK_VERSION}/sdk-core/pebble/${platform}/qemu/qemu_spi_flash.bin.bz2"

  if [ ! -f "$MICRO_FLASH" ]; then
    echo "  SKIP: no micro flash for $platform"
    continue
  fi

  # Decompress SPI flash if needed
  if [ ! -f "$SPI_FLASH" ]; then
    if [ -f "$SPI_FLASH_BZ2" ]; then
      mkdir -p "$(dirname "$SPI_FLASH")"
      bunzip2 -k -c "$SPI_FLASH_BZ2" > "$SPI_FLASH"
    else
      echo "  SKIP: no SPI flash for $platform"
      continue
    fi
  fi

  # Pick two free ports
  QEMU_PORT=$(python3 -c "import socket; s=socket.socket(); s.bind(('',0)); print(s.getsockname()[1]); s.close()")
  SERIAL_PORT=$(python3 -c "import socket; s=socket.socket(); s.bind(('',0)); print(s.getsockname()[1]); s.close()")

  # Read machine args
  IFS=' ' read -ra MARGS <<< "${MACHINE_ARGS[$platform]}"

  echo "  Starting QEMU (port=$QEMU_PORT, serial=$SERIAL_PORT)..."
  "${PEBBLE_SDK}/bin/qemu-pebble" \
    -rtc base=localtime \
    -serial null \
    -serial "tcp::${QEMU_PORT},server,nowait" \
    -serial "tcp::${SERIAL_PORT},server" \
    -pflash "$MICRO_FLASH" \
    -gdb "tcp::0,server,nowait" \
    "${MARGS[@]}" "$SPI_FLASH" &
  QEMU_PID=$!
  sleep 2

  if ! kill -0 "$QEMU_PID" 2>/dev/null; then
    echo "  ERROR: QEMU failed to start for $platform"
    continue
  fi

  # Wait for boot (connect to serial port, look for boot markers)
  echo "  Waiting for firmware boot..."
  "$PYTHON" -c "
import socket, time, sys
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(30)
s.connect(('localhost', ${SERIAL_PORT}))
data = b''
start = time.time()
while time.time() - start < 25:
    try:
        chunk = s.recv(4096)
        if chunk:
            data += chunk
            text = data.decode('latin-1')
            if '<SDK Home>' in text or '<Launcher>' in text or 'Ready for communication' in text:
                print('  Firmware booted.')
                break
    except socket.timeout:
        continue
else:
    print('  WARNING: Boot timeout, proceeding anyway.')
s.close()
"

  # Install watchface and take screenshot
  OUTFILE="${OUTDIR}/${platform}.png"
  echo "  Installing watchface and capturing screenshot..."
  "$PYTHON" -c "
import os, sys, time
from libpebble2.communication import PebbleConnection
from libpebble2.communication.transports.qemu import QemuTransport
from libpebble2.services.install import AppInstaller
from libpebble2.services.screenshot import Screenshot
import png

transport = QemuTransport('localhost', ${QEMU_PORT})
pebble = PebbleConnection(transport)
pebble.connect()
pebble.run_async()
print(f'  Connected: firmware {pebble.firmware_version}')

installer = AppInstaller(pebble, 'build/Percival.pbw')
installer.install()
print('  Watchface installed.')

time.sleep(${RENDER_WAIT})

screenshot = Screenshot(pebble)
image = screenshot.grab_image()
rgba = []
for row in image:
    rgba_row = []
    for x in range(0, len(row), 3):
        rgba_row.extend([row[x], row[x+1], row[x+2], 255])
    rgba.append(rgba_row)

os.makedirs(os.path.dirname('${OUTFILE}'), exist_ok=True)
png.from_array(rgba, mode='RGBA;8').save('${OUTFILE}')
print(f'  Saved: ${OUTFILE}')
"

  # Kill QEMU
  kill "$QEMU_PID" 2>/dev/null || true
  wait "$QEMU_PID" 2>/dev/null || true
  echo ""
done

echo "Done! Screenshots saved to $OUTDIR/"
ls -la "$OUTDIR"/*.png 2>/dev/null || echo "No screenshots generated."
