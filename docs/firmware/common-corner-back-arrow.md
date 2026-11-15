# Sony common corner Back arrow provenance

## Scope

This report records the exact stock bottom-left Back-arrow artwork used by the
PS Vita common corner-button style and the derivation of LiveArea Search's
matching magnifier asset. The arrow PNG is a lossless pixel decode of Sony's
firmware resource; it is not traced, redrawn, or approximated.

No Vita was deployed to, rebooted, or tested while producing these assets.

## Source resource

The extraction uses the local 3.60 internal CEX-for-tool common resource:

```text
SCE/Internal/3.600/3_600_011.I.cex_for_tool.release.type3.qaf/
fs/vs0/data/external/common/common_resource.rco
```

Source RCO SHA-256:

```text
6ee58e58d1e78fecd99ea93e585c81efde3e940e3e0ca5cec4e64ba7cacdc806
```

The common-resource tree establishes the complete style chain:

| Element | Value |
| --- | --- |
| Bottom-left corner-button style | `0x35CA8EE7` |
| Style tree node | `0x00001730` |
| Corner-button layout type | `2` |
| Corner plane size | `64 × 64` |
| Corner plane position | `(-13, -11)` |
| Back-arrow texture ID | `0x352FFDE1` |
| Texture tree node | `0x00009ED8` |
| File-table offset | `0x00035270` |
| Compressed size | `1,514` bytes |
| Uncompressed GIM size | `5,328` bytes |
| GIM format | indexed 8-bit (`5`) |
| GIM pixel order | linear (`0`) |

The texture's compressed payload is zlib data. Its hashes are:

```text
compressed payload  c3dc3cd62a2f6145f0622c123c6cb9b340658a282238de00101f3ab49610981b
uncompressed GIM    a007d2290f159db87ccd49ac689053e3b57c927ff323de7275676c69a074c2ba
```

## Exact extracted artwork

The standalone lossless PNG is:

```text
docs/assets/sony-common-corner-back-arrow.png
```

SHA-256:

```text
6d18fe0829e3767f9ba972cb8774a908e6a0c01526295e11284df4fba6bbfa1d
```

It is a `64 × 64` grayscale RGBA image. Its nontransparent bounding box is
`(14, 17)–(56, 55)`. The artwork contains a bright white body, gray directional
beveling, a top-left highlight, and a low-alpha dark edge/shadow. The alpha
channel ranges from 6 to 255 over visible pixels, preserving the stock soft
compositing against differently colored corner-button backgrounds.

The machine-readable extraction receipt is:

```text
docs/assets/sony-common-corner-back-arrow.json
```

## Reproducible extraction

Run the bounded RCO/GIM extractor against the same source image:

```sh
python3 tools/firmware/extract_rco_texture.py \
  --rco "/path/to/common_resource.rco" \
  --texture-id 0x352FFDE1 \
  --png-out docs/assets/sony-common-corner-back-arrow.png \
  --metadata-out docs/assets/sony-common-corner-back-arrow.json \
  --source-label \
    "SCE/Internal/3.600/3_600_011.I.cex_for_tool.release.type3.qaf/fs/vs0/data/external/common/common_resource.rco"
```

The extractor validates RCO table bounds, resolves exactly one matching texture
ID, verifies the compressed resource type and original size, decodes the PSP GIM
image and palette blocks, and emits deterministic filter-0 RGBA PNG bytes.

## LiveArea Search magnifier

The replacement magnifier is generated deterministically by:

```text
tools/assets/generate_skeuomorphic_magnifier.py
```

The integrated asset and review copy are byte-identical:

```text
resources/texture/search.png
docs/assets/livearea-search-magnifier.png
```

SHA-256:

```text
36497dcd1c7d773433dfeea8750c51d3bebdf5144612afe5eb79ce1f7a2f42d2
```

The first generated revision (`e067e832...e86954e`) was rejected after a direct
side-by-side comparison with the exact arrow. Its glass fill, gray rim, and drop
shadow were visibly heavier than Sony's artwork, especially on white.

The corrected magnifier uses the stock arrow as a pixel-distribution reference,
not merely a thematic reference. It has a nearly white plastic body, transparent
negative space, top-left directional highlighting, a restrained pale-gray bevel,
and a sparse low-alpha dark edge. Its revised lens uses outer/inner radii
`15.3 / 10.0`; the handle centerline runs from `(37.2, 36.3)` to `(49.0, 48.1)`
with radius `3.8`. Relative to the prior revision, the lens is approximately 8%
larger while the handle is approximately 13% shorter and 7% thinner.

The visible artwork is deterministically pre-scaled to `60 × 60` with
premultiplied bilinear filtering and padded at `(2,4)` inside the `64 × 64`
source PNG. The final visible bounds are `(11, 12)–(54, 56)` with `991` visible
pixels, `79` pixels at alpha `240` or above, and a low-alpha dark edge capped at
alpha `30`. This preserves the user-selected apparent size while keeping the
runtime plane on Sony's firmware-proven geometry.

The Search control uses Sony's exact stock bottom-right icon-plane geometry:
`size="64,64,0"` and `pos="13,-11,0"`. The corner-button tint, highlight, glow,
sound, and interaction states are unchanged.

Regenerate it with:

```sh
python3 tools/assets/generate_skeuomorphic_magnifier.py \
  --output resources/texture/search.png
```

## Offline build verification

The final source asset was passed through the normal `psp2cxml-tool` RCO
pipeline. The generated RCO stores the PNG as one zlib-compressed
`texture/png` entry with ID `0xD743FC5B`; decompression returns all 2,242 source
bytes exactly, including SHA-256
`36497dcd1c7d773433dfeea8750c51d3bebdf5144612afe5eb79ce1f7a2f42d2`.
The RCO entry is at tree node `0x000015D8`; its zlib payload is 2,253 bytes.

Fresh pinned VitaSDK builds produced:

| Profile | VELF SHA-256 | SUPRX SHA-256 |
| --- | --- | --- |
| Permanent release | `914006eafcf4cb571fd792f559fc0ed6eaa87903e1c02af2b5b3bc0f9a674226` | `8de3fd0d220954af759d7f6e6f001e3467d580b149f029a09f13450ee25dd5f1` |
| Self-disabling experimental | `beda12a427c4dfe2d7e6e5461b99a848115cb2fcb10127702a229a6fb1a021e3` | `8555fb5df29cf04812733c338a678a51cedbb14a86eaf55e9ee578ff827c66e5` |

Both profiles contain the same generated RCO:

```text
e5472bd34a84f22ae0db302e55c0302174de72b5b650cd7c6abd83868d899d61
```

The permanent install ZIP SHA-256 is:

```text
9ac30527094c3c1af46eee1a9dc72c7b7fc3c9c1c36cfca922b2fc3b4ff35c8c
```

Both target builds completed without warnings and passed their respective
release-binary audit profiles.

## Hardware placement validation

The first `60 × 60` candidate used fractional position
`pos="13.25,-12.5,0"`. Its first permanent boot produced a transient
`psp2core-SceShell.spsp2dmp` before interaction testing. The exact dump was
captured as 543,504 bytes with SHA-256
`e6da835b16e46e7a454459353cc4c6e13241bf95904fee9030404661608fb664`,
then the verified `48 × 48` RCO was restored. Rollback, reboot, Search/Back
smoke, startup-log stability, and the 442-core baseline all passed.

Static comparison showed that the failed RCO differed from the working RCO only
in the two size floats and two position floats. Sony's audited 3.60
`common_resource.rco` contains no fractional `pos` or `size` array values. A
second candidate used integer `pos="13,-13,0"`; its self-disabling diagnostic
profile passed two boots, but the clean release produced another transient
SceShell core on a later reboot. Direct `60 × 60` plane geometry is therefore
rejected for permanent use.

The accepted candidate uses Sony's exact `64 × 64` plane at `pos="13,-11,0"`
and moves the selected `60 × 60` footprint into the texture itself. Its
self-disabling profile passed two independent retail boots, exact
config/hash readback, Search plus two-stage Back smoke, and a 180-frame native
Home capture with zero new cores. Evidence is stored under:

```text
build-vita-regression-diagnostic/
  magnifier-stock64-experimental-20261115T015321Z/
```

The native capture confirms the selected visual size and placement. Permanent
installation remains pending the final guarded release deployment. The currently
installed permanent build still uses the preceding `48 × 48` RCO
`652bd744c2674ca6898d26890bfad98b77f9352c40390c50cb46eccbb2867570`.
