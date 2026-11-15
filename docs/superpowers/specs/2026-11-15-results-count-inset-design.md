# Results-count right-inset design

## Status

Approved in chat on 2026-11-15. The search box remains `750 x 58`; only the right-side result label is repositioned.

## Problem

After removing the notification occluder and its artificial 80-pixel reservation, `Recent` and `<n> results` use the screen's exact right edge. The text now looks visually cramped. Moving it inward must not create an overlap with the search box when the count has four digits.

## Selected layout

Keep the existing header regions and move only the result-label region:

- search box: `size="750,58" pos="18,0"`, unchanged;
- result label: `size="150,64" pos="-16,0"`;
- text remains right-aligned with ellipsis enabled.

The current geometry leaves 42 pixels between the search box and result-label region. Moving the label region 16 pixels left reduces that separation to 26 pixels while preserving non-overlapping fixed regions.

A 150-pixel right-aligned region accommodates `9999 results`, the longest count form in scope, without changing the search box or introducing runtime layout shifts. The singular `1 result` and empty-query `Recent` labels use the same region.

## Alternatives rejected

1. **Shrink the search box to preserve the original 42-pixel gap.** Rejected because the user explicitly requested that the text box remain unchanged; the remaining 26-pixel gap is sufficient.
2. **Measure the label and resize the search box dynamically.** Rejected because it would make the header shift while typing and adds runtime complexity for a bounded four-digit label.
3. **Add text padding through a new style object.** Rejected because the existing fixed layout region already provides a simpler, deterministic inset.

## Verification

Regression contracts must require:

- the unchanged `750 x 58` search-box geometry;
- the `150 x 64` result-label region at `pos="-16,0"`;
- right-aligned result text with ellipsis enabled;
- formatting for `Recent`, singular `1 result`, plural results, and a four-digit count;
- no legacy `pos="0,0"` or `pos="-80,0"` result-label placement.

After normal and ASan/UBSan host suites and resource/source checks, a retail Vita 146 native capture must confirm:

- `Recent` has visible right-side breathing room;
- a four-digit count remains fully within the reserved label region;
- the count does not touch or overlap the search box;
- Search, IME, notification-bubble suppression, and close/restoration behavior remain unchanged.

The change is committed and pushed to `main`; no release or tag is created.

## Implementation and final verification

The production resource now uses the approved geometry:

```xml
<layout_hint align="2,1" anchor="2,1" size="150,64" pos="-16,0" />
```

The search box remains byte-for-byte unchanged at `size="750,58"`
and `pos="18,0"`. CMake contracts calculate the fixed regions directly:
the search box ends at x=768, the count region begins at x=794, and the
resulting non-overlap gap is 26 pixels. The count style remains right-aligned
with ellipsis enabled. The 32-byte formatter buffer safely contains the
four-digit boundary string `9999 results`; runtime layout does not resize or
shift while typing.

Fresh offline verification completed successfully:

- normal host graph: 48/48;
- ASan+UBSan host graph: 48/48;
- resource compatibility: 84/84;
- source-to-firmware contracts: 26/26;
- diagnostics-off and self-disabling MinSizeRel Vita builds: warning-free,
  with both binary audits passing.

Exact artifacts:

| Profile | VELF SHA-256 | SUPRX SHA-256 |
| --- | --- | --- |
| Permanent release | `ecd9a64ce9454f1baef0a8907ebcc0c13fc310a73d6525efc30bcd5e44cacfcd` | `745d93dab382c919cfaa46d716984f8916ea454dcd27b2487512ce1eaa0f1546` |
| Self-disabling experimental | `13e0e2e0646ba63e48559c5203528971899c465eb942efae0d80b71e5f9b0b6f` | `f76d79a984879cb65d646e7bbd7b9c361f094c7b6b55629ea0d158772a627f94` |

Both profiles contain the same RCO:

```text
9ed52828b725bc5d7dd16b4d8dfd78bf67dd452d95922a2c7292c1e09f7fbbee
```

The permanent install ZIP SHA-256 is:

```text
adc0a13ecbed3411c25ef4526038b363c565aca214e6dcaf3411ea4768c23c98
```

Retail Vita 146 validation used the exact self-disabling candidate, followed
by the matching permanent release. The native capture displays `Recent` and
`95 results` with the new right inset and a clear gap from the unchanged search
box. Search close restores Sony's native notification bubble; no black
occluder or header shift reappears. The four-digit requirement is guaranteed by
the fixed-region and formatter contracts rather than by altering production to
synthesize a catalog count.

Experimental visual evidence:

```text
build-vita-regression-diagnostic/
  results-count-inset-visual-20261115T164533Z/
```

Permanent deployment and release-mode visual evidence:

```text
build-vita-regression-diagnostic/
  results-count-inset-permanent-deploy-20261115T165150Z/
  results-count-inset-release-visual-20261115T165549Z/
```

The permanent release passed two clean boots, Search/Back smoke, one active and
zero disabled config entries, exact artifact readback, byte-stable logless
startup bytes, and unchanged 443-core/fixed-core state. The stable startup-log
SHA-256 is:

```text
12fa8c27639c4ca8a2963d50a19ced7ddd70366e0566860c28561b11ba63d673
```
