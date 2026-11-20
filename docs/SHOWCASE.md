# Capture an effect showcase
The Linux-only development utility renders a short GIF or video through real
emulens filter instances in a separate libobs process. It does not open or edit
the normal OBS scene collection. Build using a configured native OBS SDK first.

    cmake -S . -B build
    cmake --build build --target emulens
    python3 tools/capture-showcase.py input.gif output-previews

The output directory must be new. Output includes a clean reference, one
animation and one matching still per mode, a labeled three-column contact sheet, capture logs, and a manifest
recording the exact runtime default or shipped preset, module and shader digests.
By default the filter is created with only its mode set; use `--named` to
render the alternate showcase presets instead. Input is
treated as opaque, scaled to a maximum side of 640 pixels and sampled at
30 fps; animated WebP previews are exported at 15 fps. Use `--loops 2` for
two repeats, or repeat `--effect` to select specific effects (see --help).
By default the process runs under Xvfb; `--display` uses the current X
display for GPU acceleration without opening a window. Only compatible
module/data pairs should be supplied through --module and --data.
Requirements: Python 3 with Pillow, FFmpeg/ffprobe with animated WebP support, CMake,
a C++17 compiler, Xvfb (unless using --display), and matching libobs SDK.
These captures show actual optical output, not a benchmark or proof of
support for other platforms.
