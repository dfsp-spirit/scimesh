#!/bin/sh
set -e

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ERROR: ffmpeg not found. Install it with your package manager." >&2
    exit 1
fi

ffmpeg -framerate 24 -i build/frame_%04d.png -c:v libx264 -pix_fmt yuv420p -y brain.mp4 && echo "Wrote 'brain.mp4'"
