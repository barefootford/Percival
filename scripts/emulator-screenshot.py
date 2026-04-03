#!/usr/bin/env python3
"""
Connect directly to the Pebble emulator QEMU instance,
install the watchface PBW, and capture a screenshot.
"""
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

from libpebble2.communication import PebbleConnection
from libpebble2.communication.transports.qemu import QemuTransport
from libpebble2.services.install import AppInstaller
from libpebble2.services.screenshot import Screenshot

PLATFORM = sys.argv[1] if len(sys.argv) > 1 else "basalt"
PBW_PATH = sys.argv[2] if len(sys.argv) > 2 else "build/Percival.pbw"
OUTPUT = sys.argv[3] if len(sys.argv) > 3 else f"screenshots/{PLATFORM}-cloud.png"

# Read emulator info
info_path = os.path.join(tempfile.gettempdir(), 'pb-emulator.json')
with open(info_path) as f:
    info = json.load(f)

qemu_serial = info[PLATFORM][list(info[PLATFORM].keys())[0]]['qemu']['serial']

print(f"Connecting to QEMU serial port {qemu_serial}...")
transport = QemuTransport("localhost", qemu_serial)
pebble = PebbleConnection(transport)
pebble.connect()
pebble.run_async()

print(f"Connected! Firmware version: {pebble.firmware_version}")

print(f"Installing {PBW_PATH}...")
installer = AppInstaller(pebble, PBW_PATH)
installer.install()
print("Install complete!")

print("Waiting for watchface to render...")
time.sleep(5)

print("Taking screenshot...")
try:
    import png
    screenshot = Screenshot(pebble)
    image = screenshot.grab_image()
    # Convert to RGBA
    rgba = []
    for row in image:
        rgba_row = []
        for x in range(0, len(row), 3):
            rgba_row.extend([row[x], row[x+1], row[x+2], 255])
        rgba.append(rgba_row)

    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    png.from_array(rgba, mode='RGBA;8').save(OUTPUT)
    print(f"Screenshot saved to {OUTPUT}")
except Exception as e:
    print(f"Screenshot via protocol failed: {e}")
    print("Falling back to X display capture...")
    subprocess.run(["import", "-window", "root", OUTPUT], env={**os.environ, "DISPLAY": ":99"})
    print(f"Display capture saved to {OUTPUT}")
