#!/usr/bin/env python3
"""Render the real emulens OBS filter into labeled animated and still previews."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FPS = 30
# Named alternates remain available; default runs create fresh runtime settings.
EFFECTS = (
    ("original", "Original", "clean"),
    ("vidicon", "Vidicon", "vidicon_dream"),
    ("starburst", "Starburst", "starburst_cinema"),
    ("light-leak", "Light Leak", "leak_warm"),
    ("handheld", "Handheld", "handheld_run"),
    ("bokeh", "Bokeh", "bokeh_neon"),
    ("particles", "Particles", "part_magic"),
    ("orton", "Orton Glow", "orton_angelic"),
    ("film-grit", "Film Grit", "grit_grindhouse"),
    ("mirage", "Mirage", "mirage_split"),
    ("center-spot", "Center Spot", "spot_tunnel"),
    ("bloom", "Bloom", "bloom_dusty_lens"),
    ("pinhole", "Pinhole", "pinhole_antique"),
    ("polaroid", "Polaroid", "polaroid_vintage"),
    ("daguerreotype", "Daguerreotype", "daguerre_silver"),
    ("technicolor", "Technicolor 2-Strip", "technicolor_misaligned"),
    ("wet-lens", "Wet Lens", "wet_lens_night_window"),
    ("phone-wet-lens", "Phone Wet Lens", "phone_wet_streetlights"),
)


def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def make_contact_sheet(output, effects):
    from PIL import Image, ImageDraw, ImageFont
    import math
    width, height, header = 640, 360, 30
    cols = 3
    sheet = Image.new("RGB", (cols * width, math.ceil(len(effects) / cols) * (height + header)), (24,24,24))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("DejaVuSans.ttf", 19)
    except OSError:
        font = ImageFont.load_default()
    for i, effect in enumerate(effects):
        x = (i % cols) * width
        y = (i // cols) * (height + header)
        with Image.open(output / "screenshots" / (Path(effect["file"]).stem + ".png")) as im:
            picture = im.convert("RGB")
            picture.thumbnail((width,height))
            sheet.paste(picture, (x+(width-picture.width)//2,y+header+(height-picture.height)//2))
        draw.text((x + width//2, y + header//2),effect["name"],font=font,fill="white",anchor="mm")
    sheet.save(output / "contact-sheet.png")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("input", type=Path)
    p.add_argument("output", type=Path)
    p.add_argument("--build", type=Path, default=ROOT / "build")
    p.add_argument("--module", type=Path)
    p.add_argument("--data", type=Path, default=ROOT / "data")
    p.add_argument("--effect", action="append", choices=[e[0] for e in EFFECTS])
    p.add_argument("--named", action="store_true", help="use the named alternate presets instead of fresh runtime defaults")
    p.add_argument("--loops", type=int, choices=range(1, 5), default=1)
    p.add_argument("--display", action="store_true", help="use current X display for GPU acceleration instead of Xvfb")
    a = p.parse_args()
    source, output, build, data = (v.expanduser().resolve() for v in (a.input, a.output, a.build, a.data))
    module = a.module.expanduser().resolve() if a.module else build / "emulens.so"
    if not source.is_file() or not module.is_file() or not (data / "emulens.effect").is_file():
        p.error("input, module or shader not found")
    if output.exists():
        p.error("output directory already exists")
    selected = [e for e in EFFECTS if not a.effect or e[0] in a.effect]
    for slug, label, preset in selected:
        if preset != "clean" and not (data / "presets" / (preset + ".json")).is_file():
            p.error(f"missing preset: {preset}")
    info = json.loads(run("ffprobe", "-v", "error", "-select_streams", "v:0",
                          "-show_entries", "stream=width,height", "-of", "json",
                          source, capture_output=True, text=True).stdout)
    if not info.get("streams"):
        p.error("input has no video")
    w, h = (int(info["streams"][0][k]) for k in ("width", "height"))
    if min(w, h) < 16:
        p.error("input too small")
    scale = min(1, 640 / max(w, h))
    w, h = (int(n * scale) // 2 * 2 for n in (w, h))
    if min(w, h) < 16:
        p.error("input aspect ratio too extreme")
    size = w * h * 4
    limit = min(256 * 1024**2, 512 * 1024**2 // a.loops) // size
    with tempfile.TemporaryDirectory(prefix="emulens-capture-") as temp:
        work = Path(temp)
        run("cmake", "--build", build, "--target", "capture-showcase", "--parallel", 2)
        exe = build / "capture-showcase"
        raw = work / "input.rgba"
        run("ffmpeg", "-nostdin", "-v", "error", "-i", source,
            "-vf", f"fps={FPS},scale={w}:{h},format=rgb24,format=rgba", "-frames:v", limit + 1,
            "-pix_fmt", "rgba", "-f", "rawvideo", raw)
        count, rem = divmod(raw.stat().st_size, size)
        if rem or count < 2 or count > limit:
            p.error("invalid or excessively long clip")
        output.mkdir(parents=True)
        (output / "screenshots").mkdir()
        (output / "logs").mkdir()
        (work / "config").mkdir()
        env = dict(os.environ, XDG_CONFIG_HOME=str(work / "config"))
        launcher = [] if a.display else ["xvfb-run", "-a"]
        raw_args = ["-f", "rawvideo", "-pixel_format", "bgra", "-video_size", f"{w}x{h}", "-framerate", FPS]
        manifest = {"module_sha256": hashlib.sha256(module.read_bytes()).hexdigest(),
                    "shader_sha256": hashlib.sha256((data / "emulens.effect").read_bytes()).hexdigest(),
                    "size": [w,h], "fps": FPS, "frames": count*a.loops, "effects": []}
        for slug, label, preset in selected:
            mode = next(i-1 for i,e in enumerate(EFFECTS) if e[0]==slug)
            rendered = work / "capture.bgra"
            config = preset if preset == "clean" else (data / "presets" / (preset + ".json") if a.named else f"mode:{mode}")
            applied = preset if a.named or preset=="clean" else f"runtime-default:{mode}"
            with (output / "logs" / (slug + ".log")).open("w") as log:
                run(*launcher, exe, module, data, raw, w,h,FPS,a.loops,config,rendered,
                    env=env, stdout=log, stderr=subprocess.STDOUT)
            if rendered.stat().st_size != count*a.loops*size:
                raise RuntimeError(f"incomplete capture of {label}")
            run("ffmpeg", "-nostdin", "-v", "error", *raw_args, "-i", rendered,
                "-vf", "fps=15", "-c:v", "libwebp_anim", "-quality", 80, "-compression_level", 5,
                "-loop", 0, "-an", output / (slug + ".webp"))
            # Same point in each clip, where moving wand and background lights are visible.
            run("ffmpeg", "-nostdin", "-v", "error", *raw_args, "-i", rendered,
                "-ss", str((count//3)/FPS), "-frames:v", 1, output / "screenshots" / (slug + ".png"))
            rendered.unlink()
            manifest["effects"].append({"name": label, "file": slug+".webp", "preset": applied})
            (output / "manifest.json").write_text(json.dumps(manifest, indent=2)+"\n")
            print(f"Captured {label} ({applied})", flush=True)
        make_contact_sheet(output, manifest["effects"])
        print("Complete:", output)


if __name__ == "__main__":
    main()
