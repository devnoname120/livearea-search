# Magnifier geometry adjustment design

## Status

Approved by the user on 2026-11-15. The user requested a slightly larger lens circle and a slightly smaller handle, with no other visual or behavioral changes and no further design questions.

## Goal

Refine the existing stock-arrow-style Search magnifier so the circular lens carries more of the icon's visual mass and the handle is less dominant. Preserve the current white/translucent skeuomorphic treatment, transparent center, optical placement, and LiveArea corner-button integration.

## Considered approaches

1. **Scale the outer and inner lens radii together, then shorten and slightly thin the handle.** This preserves the current rim weight and changes only the requested proportions. This is the selected approach.
2. **Increase only the outer radius.** This would make the circle larger, but it would also produce a visibly heavier rim and change the selected stock-arrow-style weight.
3. **Scale and reposition the complete icon or its runtime plane.** This could rebalance the composition, but it would alter already validated layout and touch-state geometry for no demonstrated benefit.

## Geometry

The deterministic generator remains the only source of the runtime PNG. The canvas and lighting model do not change.

| Parameter | Current | Revised | Intent |
| --- | ---: | ---: | --- |
| Lens center | `(26.7, 25.8)` | unchanged | Preserve optical placement |
| Outer lens radius | `14.2` | `15.3` | Approximately 8% larger circle |
| Inner lens radius | `9.0` | `10.0` | Preserve nearly the same visible rim thickness |
| Handle start | `(35.8, 34.9)` | `(37.2, 36.3)` | Attach cleanly to the larger lower-right rim |
| Handle end | `(50.4, 49.5)` | `(49.0, 48.1)` | Shorten the handle by approximately 13% |
| Handle radius | `4.1` | `3.8` | Make the handle approximately 7% thinner |

The ring thickness changes only from `5.2` to `5.3` source pixels. This prevents the larger circle from becoming either hollow-looking or materially heavier.

## Preserved behavior and styling

The following remain byte-for-byte or structurally unchanged except for the regenerated PNG and its deterministic contract values:

- `64 × 64` RGBA PNG canvas;
- fully transparent lens center and canvas corners;
- grayscale body, directional alpha, highlight, bevel, and low-alpha shadow model;
- `48 × 48` runtime plane and `pos="14,-17,0"` placement;
- corner background tint, common highlight, common glow, sound, and touch behavior;
- resource ID and RCO pipeline.

No C++ runtime behavior changes are in scope.

## Verification

The asset contract will require:

- deterministic regeneration of the runtime PNG;
- byte identity between the runtime asset and review copy;
- exact PNG hash, alpha population, visible bounds, and transparent-center checks;
- unchanged XML geometry and resource references;
- visual inspection at native `64 × 64` and rendered `48 × 48` sizes on transparent, navy, and Vita-blue backgrounds;
- fresh normal and ASan/UBSan host suites;
- fresh warning-free release and self-disabling Vita builds;
- release and experimental binary audits plus resource/source audits.

Hardware deployment is not part of the geometry edit unless separately requested after offline verification.
