# Native notification-bubble suppression design

## Status

Implemented and hardware-validated through goal revision 3 on 2026-11-15. The user rejected the opaque top-right occluder because it leaves a black rectangle, consumes header space, and shifts the `Recent` label left. The replacement temporarily hides Sony's native notification bubble and restores it when Search closes. The work is committed and pushed without publishing a release or tag.

## Problem

The current `NotificationCornerGuard` opens a LiveArea Search page containing two opaque `80 x 32` planes at draw priority 7. This masks only part of Sony's top-right indicator and forces `text_results_count` to `pos="-80,0"`. The supplied screenshot shows the resulting black strip, the remaining cyan edge of Sony's bubble, and the visibly displaced `Recent` label.

The occluder is therefore neither visually correct nor semantically correct: Sony's notification control remains alive underneath an unrelated plugin-owned rectangle.

## Firmware and runtime evidence

The indicator plugin's static page is already firmware-audited as `0xFFD7E3F0`. Runtime tree inventory on retail Vita 146 identified the actual Home notification bubble as widget `0x718CB7FF`, a visible `button` at the top-right. Its children are:

- bubble plane `0x6AE5A623`;
- count text `0x6E6CC672`;
- busy indicator `0x2D08677D`.

This is distinct from `0x8BF9D3C5`, the notification panel's close-corner widget used for panel ownership detection.

A self-disabling diagnostics-only prototype hid `0x718CB7FF` with native `Hide(Type_Reset)`, restored it with `Show(Type_Reset)`, removed the occluder, and set `Recent` to `pos="0,0"`. Native capture `build-vita-regression-diagnostic/notification-native-hide-prototype-capture-20261115T132745Z/native-hide-open.png` shows the intended result: no native bubble, no black rectangle, and no artificial right-side gap. The exact permanent release was restored and reboot-verified afterward.

## Selected architecture

`NotificationCornerGuard` will directly manage the audited native indicator widget.

- Resolve `indicator_plugin`, static page `0xFFD7E3F0`, and widget `0x718CB7FF` on the first suppression and again on restoration; never retain a PAF widget pointer across frames.
- On the first successful suppression, record whether the native widget was visible.
- Hide with `Widget::Hide(Type_Reset)`.
- Do not rewrite the native widget's event mask. PAF hidden widgets are removed from visible hit testing, and preserving Sony's event configuration avoids restoring touch state that this plugin did not own.
- While Search remains open, repeated suppression requests return success without invoking `Hide` again. Reissuing `Hide(Type_Reset)` every visibility tick restarts Sony's hide animation and leaves the bubble partially visible indefinitely; the one-shot call is the behavior proven by the successful prototype.
- On restoration, reacquire the widget and call `Show(Type_Reset)` only when the guard originally hid a visible widget.
- Clear guard ownership even when the indicator widget no longer exists, so plugin unload or shell transitions cannot leave stale state.

The guard will store only logical ownership state (`suppressed_` and `was_visible_`), never the native widget pointer.

## Resource changes

Delete the plugin-owned `page_notification_occluder`, its two planes, and both opaque styles. Move `text_results_count` from `pos="-80,0"` to `pos="0,0"`. No other Search layout, color, interaction, or animation changes are in scope.

## Failure handling

- If the indicator plugin, page, or bubble widget is unavailable during suppression, return failure and retry on the next visibility tick.
- If the widget disappears after successful suppression, restoration clears guard ownership without dereferencing stale memory.
- Restoration never shows a widget that was already hidden when suppression began.
- The existing notification-panel ownership gate remains unchanged; `0x8BF9D3C5` continues to detect panel opening/closure.
- The production build must contain no diagnostic tree traversal, automatic Search opening, startup logging, or self-disable behavior.

## Verification

Regression contracts must:

- require the audited page and native bubble hashes;
- require pointer reacquisition through `indicator_plugin`, `PageRoot`, and `FindChild`;
- require native `Hide(Type_Reset)` and conditional `Show(Type_Reset)`;
- forbid retaining `paf::ui::Widget*` in `NotificationCornerGuard`;
- forbid `page_notification_occluder`, its opaque styles, and the `-80` text offset;
- require `text_results_count` at `pos="0,0"`.

After normal and sanitizer host suites, firmware/resource/source audits, and warning-free Vita builds, the exact self-disabling candidate must pass on retail Vita 146:

- Search open: native notification bubble absent, no black strip, natural `Recent` alignment;
- Search close: native bubble restored;
- notification panel ownership unchanged;
- Start, Triangle/Circle, switcher, and PS/Home ownership unchanged;
- exact artifact/config readback and no new or overwritten core dump.

Only after the exact candidate passes may the matching diagnostics-off, non-self-disabling release be installed permanently. The work is committed and pushed to `main`; no GitHub release or tag is created.

## Hardware result

The final self-disabling candidate passed the complete retail Vita 146 matrix:

- Search open/close visual capture with no bubble remnant or opaque replacement;
- natural right-edge `Recent` alignment and native bubble restoration;
- two notification-panel ownership cycles;
- switcher suppression and lower-right input pass-through;
- PS/Home with active and dismissed IME;
- Start ownership during Home edit and an external folder IME;
- Triangle/Circle ownership after IME dismissal;
- exact artifact/configuration readback and unchanged core fingerprints.

The matching diagnostics-off release installed permanently with SUPRX
`745d93dab382c919cfaa46d716984f8916ea454dcd27b2487512ce1eaa0f1546`
and RCO
`ebab91aa4669d225b6b88568299b1b36269f9c766ee91a114b21ee1fdd271a1d`.
Two clean boots, Search/Back smoke, stable startup-log bytes, and the unchanged
443-core baseline passed. The final release-mode visual receipt is
`build-vita-regression-diagnostic/native-notification-final-release-visual-20261115T145229Z/report.json`;
the permanent deployment receipt is
`build-vita-regression-diagnostic/native-notification-final-permanent-deploy-20261115T145043Z/report.json`.
