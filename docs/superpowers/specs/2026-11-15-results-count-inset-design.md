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
