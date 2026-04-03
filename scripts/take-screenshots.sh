#!/usr/bin/env bash
#
# take-screenshots.sh — Launch each Pebble emulator platform, wait for the
# watchface to render, capture a screenshot, and exit.
#
# Usage:
#   ./scripts/take-screenshots.sh [platform ...]
#
# If no platforms are specified, defaults to: basalt chalk emery
# Screenshots are saved to ./screenshots/<platform>.png
#
set -euo pipefail

DEFAULT_PLATFORMS=(basalt chalk emery)
PLATFORMS=("${@:-${DEFAULT_PLATFORMS[@]}}")
OUTDIR="./screenshots"
RENDER_WAIT=8   # seconds to let the watchface render before capturing

mkdir -p "$OUTDIR"

# Start a virtual framebuffer if no display is set (headless / CI)
if [ -z "${DISPLAY:-}" ]; then
  echo "Starting Xvfb (headless mode)..."
  Xvfb :99 -screen 0 1024x768x24 &
  XVFB_PID=$!
  export DISPLAY=:99
  trap "kill $XVFB_PID 2>/dev/null" EXIT
fi

for platform in "${PLATFORMS[@]}"; do
  echo "=== Capturing screenshot for: $platform ==="

  # Kill any leftover emulator from a previous run
  pebble kill 2>/dev/null || true

  # Launch the emulator and install the watchface
  pebble install --emulator "$platform" &
  INSTALL_PID=$!

  # Wait for the watchface to fully render
  echo "  Waiting ${RENDER_WAIT}s for watchface to render..."
  sleep "$RENDER_WAIT"

  # Capture screenshot
  OUTFILE="${OUTDIR}/${platform}.png"
  pebble screenshot --emulator "$platform" "$OUTFILE"
  echo "  Saved: $OUTFILE"

  # Clean up emulator
  wait "$INSTALL_PID" 2>/dev/null || true
  pebble kill 2>/dev/null || true

  echo ""
done

echo "Done! Screenshots saved to $OUTDIR/"
ls -la "$OUTDIR"/*.png
