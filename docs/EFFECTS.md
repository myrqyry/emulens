# Optical behavior and controls

## Notable controls and optical behavior

### Vidicon Trail Decay

**Trail Decay** is a perceptual persistence control rather than a raw per-frame
feedback coefficient. Persistence is time-normalized so the trail duration is
stable across frame rates.

- `0.50` — short but clearly visible persistence
- `0.80` — natural/default tube persistence
- `0.90` — longer trail
- `0.95` — long trail
- `0.99` — very long but still decaying
- `1.00` — intentional indefinite hold

The feedback buffer stores trail energy rather than repeatedly re-exposing the
whole source frame. Per-channel temporal lag allows red to persist longer than
blue without faking channel lag through spatial offsets.

Scene collections carrying the recognizable dead legacy Vidicon combination
are migrated to the current tube defaults. Ordinary customized Vidicon
settings are not reset.

### Bloom / Dusty Lens

Bloom uses a bright-pass extraction, multilevel downsample/upsample pyramid,
and final combine stage. Its lens-contamination controls model light
interacting with the optical surface rather than a dirt texture painted over
the frame:

- **Dust Amount** — amount of fixed contamination revealed by captured light.
- **Dust Scale** — size of the grime/smudge structure.
- **Dust Contrast** — definition of the contamination pattern.
- **Veiling Glare** — broad light scatter through fine contamination.
- **Dust Specular Glint** — sharper glints from lit contamination.
- **Lens Surface Layers** — number of fixed dirt/smudge scales used to build
  the optical-surface pattern.
- **Refraction Amount / Refraction Scale** — chromatic/light displacement driven
  by the local gradient of the contamination pattern.
- **Ghost Amount / Scale / Color** — internal lens reflections.
- **Anamorphic** — stretched optical response for streak-like bloom.
- **Highlights Only** — restricts the combine toward extracted highlights.

With no extracted highlight energy, the contamination response stays dormant.
The pattern is fixed in lens space rather than drifting like particles.
Large-radius pyramid sampling treats samples outside the image as black instead
of replicating frame edges.

Bloom is a multipass strategy: mode-specific uniforms are rebound after common
parameter binding for each relevant pass so live UI changes reach extraction,
blur/pyramid, and combine techniques.

### Center Spot

The **Outer Optic** selector chooses how the ring surrounding the clear center
is constructed:

- **Soft Diffusion**
- **Diopter**
- **Radial Zoom**

The inner spot remains genuinely clear; blur/zoom behavior belongs to the outer
annulus.

### Starburst

Starburst ray reach is driven by highlight energy. Strong point highlights
produce longer/brighter rays while weaker highlights fall off instead of being
allowed to smear farther through a distance-lowered threshold. Point count,
rotation, ray length, thickness, smoothness, chromatic aberration, pulse,
anamorphic mode, colorization, samples, and blend mode remain independently
tunable.

### Mirage

Mirage uses mirrored boundary sampling at large **Radius** values. Out-of-frame
coordinates fold back into the image, so large displacements remain populated
instead of producing black wedges from zero valid samples.

### Particles

Large particles compensate their energy as size increases and use bounded
screen-style compositing. Dense overlaps therefore retain visible particle
structure instead of turning into opaque white blobs.

### Water-on-lens effects

**Wet Lens** uses stationary, seed-deterministic water beads to refract
the live source image. Captured highlights produce rim glints and subtle
chromatic separation. Coverage, bead size/stretch, refraction, light
response, and pattern seed are adjustable; **Clean Raindrops** and
**Night Window** provide starting points. Zero coverage leaves the frame
unchanged. It does not simulate flowing or merging droplets.

**Phone Wet Lens** uses broader, irregular wet-surface regions with
refracted imagery, illuminated contours, veiling glare, and directional
light ribbons derived from actual source highlights. Controls include
coverage, patch scale, contour strength, refraction, ribbon strength/reach,
glow, angle, and pattern. Presets include **Wet Phone Camera**,
**Rain on Streetlights**, and **Heavy Wet Optics**. The pattern remains
stationary; a dark source cannot generate bright ribbons on its own.

Both modes have isolated OBS/OpenGL pixel tests, including refraction,
light response, pattern stability, and alpha handling. These do not
establish real-time performance for every live scene or GPU.
