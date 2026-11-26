#!/usr/bin/env python3
"""Guard the highlight-driven optical flare path and preserved user settings."""
from pathlib import Path
import json

root = Path(__file__).resolve().parent.parent
effect = (root / "data/emulens.effect").read_text()
bloom = (root / "src/strategies/strategy-bloom.c").read_text()
ui = (root / "src/lens-ui.c").read_text()
core = (root / "src/lens-core.c").read_text()
params = (root / "src/lens-params.h").read_text()
data = (root / "src/lens-data.h").read_text()

def check(ok, reason):
    if not ok:
        raise AssertionError(reason)

# Bloom ghosts must sample extracted highlight radiance, not the blurred glow
# or unthresholded original; switching them off remains a no-op.
check("uniform texture2d highlight_image;" in effect, "missing highlight texture")
check("gs_effect_get_param_by_name(filter->effect, \"highlight_image\")" in core,
      "highlight texture not cached by native plugin")
check("SET_PARAM_TEXTURE(filter->common.highlight_image, extracted);" in bloom,
      "extraction output not bound to Bloom combine")
check("bloom_ghost_elements" in params and "bloom_ghost_dispersion" in params,
      "missing persisted Bloom optics controls")
check("ghost_elements;" in data and "ghost_dispersion;" in data,
      "missing Bloom native uniform handles")
check("P_BLOOM_GHOST_ELEMENTS" in ui and "P_BLOOM_GHOST_DISPERSION" in ui,
      "Bloom ghost optics controls not visible in native OBS settings")
start = effect.index("float4 PSBloomCombine(")
end = effect.index("technique DrawBloomExtract")
combine = effect[start:end]
check("if (bloom_ghost_amount > 0.001)" in combine, "ghost amount must gate work")
check("highlight_image.Sample" in combine, "ghosts must use bright highlights")
check("opticalCenter" in combine and "ghostUV" in combine
      and "magnification" in combine and "reflectionSign" in combine,
      "ghosts must follow axis-centered signed optical geometry")
check("bloom_ghost_dispersion" in combine and "bloom_ghost_elements" in combine,
      "ghost size and dispersion must be adjustable")
check("float4 ret = float4(saturate(final_rgb), original.a);" in combine,
      "Bloom source alpha must be preserved")

# Curated Bloom presets must reset newly introduced optics controls; a
# preset lacking these keys could retain stale user values after switching.
for filename in ("default_bloom.json", "bloom_soft.json",
                 "bloom_dusty_lens.json", "bloom_cyberpunk.json",
                 "bloom_prism.json"):
    preset = json.loads((root / "data/presets" / filename).read_text())
    check(1 <= preset["bloom_ghost_elements"] <= 6, filename + ": lens count")
    check(0.0 <= preset["bloom_ghost_dispersion"] <= 1.0,
          filename + ": dispersion")

# Starburst's existing chroma parameter should color rays, not merely displace
# samples of the already-composited screen at the output pixel.
start = effect.index("float4 PSStarburst(")
end = effect.index("technique DrawStarburst")
star = effect[start:end]
check("spectralAccum" in star and "ray_accum" in star,
      "starburst lacks source-driven spectral ray accumulation")
check("sb_chroma" in star and "sb_colorize" in star, "existing controls lost")
check("image.Sample(textureSampler, v_in.uv - to_pixel * chroma_dist)" not in star,
      "starburst still samples final image for spectral rays")
check("original.a" in star, "starburst alpha lost")
check("StarburstSpectralColorTooltip" in ui,
      "Starburst chroma control no longer describes its actual behavior")
print("PASS: native highlight ghosts and spectral Starburst are wired")
