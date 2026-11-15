# Triangle ownership while Search is visible

Contract ID: `triangle-search-ownership-v1`

## Symptom

The Home pad device-handler chain previously removed only the owned PS and Start
bits from the forwarded PAF sample. After Circle dismissed the system IME, Search
remained open in its results layout, but Triangle (`PAD_MENU`) still reached the
previous SceShell Home handler. SceShell interpreted that press as the ordinary
LiveArea edit-mode command.

## PAF input contract

The verified PAF pad interface represents Triangle as `PAD_MENU = 0x40` in
`paf::inputdevice::pad::Data::Keycode`. `SearchOverlay::HomePadDeviceHandler`
operates before the previous Home handler and already forwards a copied
`paf::inputdevice::pad::Data` sample, so Search can remove only the button it
owns without mutating the original device sample or disturbing unrelated input.

## Ownership policy

Triangle is owned only when the Search results page is open and the IME editing
layout is no longer active:

```text
results_page_ != NULL && !results_editing_layout_
```

This distinction preserves any Triangle behavior owned by the system keyboard.
IME-active Search forwards Triangle unchanged.

When Search owns Triangle:

- the rising-edge sample is removed from the forwarded PAF sample;
- held and repeated samples remain removed;
- unrelated button bits are preserved;
- ownership remains until physical button-up, even if Search closes while the
  button is still held;
- release clears ownership without synthesizing an action;
- a fresh Triangle press is forwarded normally when Search is closed.

The button produces no Search action. The diagnostic build records
`triangle pad suppressed`; the results page remains open and SceShell does not
enter LiveArea edit mode.

## Verification

`tests/test_home_pad_filter.cpp` covers IME pass-through, the owned rising edge,
held/repeated samples, Search closure while held, physical release, unrelated
button preservation, and closed-Search pass-through. `tests/check_search_polish.cmake`
locks the PAF `PAD_MENU` path, the results/display-layout ownership gate, the
persistent ownership fields, forwarding through the copied sample, and the
diagnostic marker.

Native retail validation is recorded with the corresponding experimental and
permanent deployment evidence after the candidate passes hardware testing.
