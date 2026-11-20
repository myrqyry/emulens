#!/usr/bin/env python3
"""Default modes must be legible starting points, without enabling extreme extras."""
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
def load(name):
    return json.loads((ROOT / "data/presets" / ("default_"+name+".json")).read_text())
def main():
    star = load("starburst")
    assert star["sb_chroma"] == 0 and star["sb_threshold"] <= .75
    assert star["sb_ray_length"] >= .20 and 12 <= star["sb_samples"] <= 16
    assert star["sb_points"] == 4 and star["sb_rotation"] == 45.0
    assert 1.0 <= star["sb_intensity"] <= 2.0
    leak = load("leak")
    assert leak["leak_blend_mode"] == 1 and .7 <= leak["leak_intensity"] <= 1.25
    hand = load("handheld")
    assert not hand["hand_blur_enable"]
    part = load("particles")
    assert .009 <= part["part_size"] <= .018 and 8 <= part["part_density"] <= 18
    phone = load("phone_wet")
    assert 1.0 <= phone["phone_ribbons"] <= 1.45 and phone["phone_glow"] <= .45
    assert .30 <= load("orton")["orton_opacity"] <= .45
    assert 0.5 <= load("mirage")["mir_rot_speed"] <= 3.0
    grit = load("grit")
    assert grit["grit_blend_mode"] == 6
    assert .2 <= grit["grit_noise"] <= .5
    assert .2 <= grit["grit_scratches"] <= .5
    print("PASS: visible default starter settings remain bounded")
if __name__ == "__main__":
    main()
