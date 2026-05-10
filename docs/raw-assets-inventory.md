# Raw Assets Inventory

Last updated: 2026-05-09

This file indexes `raw/` so source assets are easier to find before importing,
converting, or replacing them.

## Summary

Total raw payload:

- 157 files;
- about 98.84 MB;
- top-level groups: `characters`, `water`.

File types:

| Type | Count | Notes |
| --- | ---: | --- |
| `.jpg` | 77 | Water/detail textures. |
| `.png` | 64 | Selected 64x64 blue-noise textures. |
| `.md` | 5 | Source/license notes. |
| `.csv` | 5 | Source manifests. |
| `.hdr` | 3 | Poly Haven sky HDRIs. |
| `.glb` | 2 | Source character models. |
| `.zip` | 1 | Original blue-noise archive. |

## Character Sources

### `raw/characters/meshy_scout`

Files:

- `meshy_scout_running.glb` - 7.85 MB.

Current use:

- Imported/generated runtime assets live under `assets/characters/meshy_scout/`.
- Existing generated assets include skeleton, skinned mesh, material, and
  `Running_baselayer_clip0001.animation.json`.
- Good source for the current Meshy visual character.

Notes:

- Contains a running animation, but no separate idle animation was found in raw.
- Meshy currently uses the project animation set at
  `assets/characters/meshy_scout/meshy_scout.animation_set.json`.
- Idle currently falls back to the engine placeholder idle clip.

### `raw/characters/ual1`

Files:

- `ual1_standard.glb` - 7.74 MB.

Current use:

- Imported/generated runtime assets live under `assets/characters/ual1/`.
- Existing generated assets include skeleton, skinned mesh, materials, idle clip,
  and jog-forward clip.

Notes:

- Better candidate than Meshy for testing idle/move state switching, because it
  has at least `Idle_Loop` and `Jog_Fwd_Loop` imported already.
- Sandbox still has a separate `sandbox.ual_locomotion` graph path using direct
  clip paths; this is a later migration target.

## Water Sources

### `raw/water/cadhatch_seamless_water`

Files:

- 46 water `.jpg` textures;
- `README.source.md`;
- `source_manifest.csv`.

Image details:

- All texture images are 512x512.
- Many files come as color/normal pairs, for example:
  - `water_0175.jpg` / `water_0175normal.jpg`;
  - `water_0175b.jpg` / `water_0175bnormal.jpg`;
  - `water_0236.jpg` / `water_0236normal.jpg`;
  - `water_0325.jpg` / `water_0325normal.jpg`;
  - `water_0400b.jpg` / `water_0400bnormal.jpg`.

Source/license notes:

- Source: CADhatch seamless water textures.
- README says these are described as royalty-free seamless water textures with
  matching normal maps.
- Re-check source license before shipping.

Good for:

- quick water material variants;
- normal-map/detail-normal tests;
- lightweight 512x512 water surfaces.

### `raw/water/opengameart_keith333_water_batch`

Files:

- 30 water texture `.jpg` files;
- `README.source.md`;
- `source_manifest.csv`.

Pairs:

- `Foam_S.jpg` / `Foam_N.jpg`
- `FoamB_S.jpg` / `FoamB_N.jpg`
- `GreenCalm_S.jpg` / `GreenCalm_N.jpg`
- `GreenSea_S.jpg` / `GreenSea_N.jpg`
- `GreenSeaB_S.jpg` / `GreenSeaB_N.jpg`
- `Lake_S.jpg` / `Lake_N.jpg`
- `PondSediment_S.jpg` / `PondSediment_N.jpg`
- `Pool_S.jpg` / `Pool_N.jpg`
- `SeaDistant_S.jpg` / `SeaDistant_N.jpg`
- `SeaWaves_S.jpg` / `SeaWaves_N.jpg`
- `SeaWavesB_S.jpg` / `SeaWavesB_N.jpg`
- `SlimyWater_S.jpg` / `SlimyWater_N.jpg`
- `SlimyWaterB_S.jpg` / `SlimyWaterB_N.jpg`
- `StonesAndRipples_S.jpg` / `StonesAndRipples_N.jpg`
- `WaterFall_S.jpg` / `WaterFall_N.jpg`

Image details:

- Mostly 2048-wide images.
- Heights vary: 966, 1008, 1070, 1246, 1365, 1686, 1829.
- `_S` is the seamless/source texture.
- `_N` is the matching normal map.

Source/license notes:

- Source: OpenGameArt water batch by Keith333.
- License: CC-BY 3.0.
- Any shipped package using them needs attribution.

Good for:

- water foam/detail prototypes;
- normal-map testing;
- more varied water styles than the CADhatch set.

### `raw/water/momentsingraphics_blue_noise`

Files:

- `FreeBlueNoiseTextures.zip` - 40.33 MB;
- `README.source.md`;
- selected extracted set in `selected_64x64_ldr_rgba/`.

Selected extracted set:

- 64 PNG files: `ldr_rgba_0.png` through `ldr_rgba_63.png`;
- all are 64x64 RGBA;
- `source_manifest.csv` maps each selected file back to the archive entry.

Zip contents summary:

| Archive folder | Entry count |
| --- | ---: |
| `Data/16_16` | 512 |
| `Data/32_32` | 512 |
| `Data/64_64` | 512 |
| `Data/128_128` | 128 |
| `Data/256_256` | 64 |
| `Data/512_512` | 4 |
| `Data/1024_1024` | 1 |

Source/license notes:

- Source: Moments in Graphics blue noise textures by Christoph Peters.
- License: CC0 / public domain dedication.

Good for:

- water foam breakup;
- dithering;
- temporal jitter/noise;
- shader sampling experiments.

### `raw/water/polyhaven_sky_hdris`

Files:

- `aristea_wreck_puresky_2k.hdr`
- `autumn_field_puresky_2k.hdr`
- `belfast_sunset_puresky_2k.hdr`
- `README.source.md`
- `source_manifest.csv`

Source/license notes:

- Source: Poly Haven sky HDRIs.
- License: CC0 / public domain dedication.

Good for:

- water reflection/environment tests;
- sky lighting prototypes;
- comparing sunset/field/wreck sky moods.

### `raw/water/texturelabs_water_146`

Files:

- `Texturelabs_Water_146XL.jpg` - 4405x3112, about 2.85 MB;
- `README.source.md`;
- `source_manifest.csv`.

Source/license notes:

- Source: Texturelabs Water 146.
- README says Texturelabs marks it free for commercial use, but terms forbid raw
  redistribution, stock-pack resale, or bundling with an engine/tool/app.

Important:

- Treat this as local visual prototyping input only.
- Do not ship as a loose raw asset in public engine/plugin packages.

Good for:

- local look-dev;
- water surface visual reference;
- not ideal as a redistributable engine sample asset.

## Quick Picks

Use these first:

- For redistributable/no-attribution sky and noise tests:
  - `raw/water/polyhaven_sky_hdris/*.hdr`
  - `raw/water/momentsingraphics_blue_noise/selected_64x64_ldr_rgba/*.png`
- For water normals/detail while prototyping:
  - `raw/water/opengameart_keith333_water_batch/*_N.jpg`
  - `raw/water/cadhatch_seamless_water/*normal.jpg`
- For Meshy character work:
  - `raw/characters/meshy_scout/meshy_scout_running.glb`
- For locomotion state tests with idle and jog:
  - `raw/characters/ual1/ual1_standard.glb`

Avoid shipping without a license pass:

- `raw/water/cadhatch_seamless_water/*`
- `raw/water/texturelabs_water_146/*`

Attribution required if shipped:

- `raw/water/opengameart_keith333_water_batch/*` - Keith333, CC-BY 3.0.

## Follow-Up Import Ideas

- Create a small water material test set from CC0 assets first:
  Poly Haven HDRI + Moments in Graphics blue noise.
- Add a generated water-detail material using one OpenGameArt normal pair, with
  attribution tracked in project docs.
- Import or create a real Meshy idle animation; current Meshy idle is only a
  placeholder in the new character animation set flow.
- Migrate UAL to `CharacterTemplate + animation_set` because it already has idle
  and jog clips.
