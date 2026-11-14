# Home edit and external IME ownership

Contract ID: `home-edit-and-external-ime-ownership-v1`

## Scope

This report records the runtime ownership signals used to remove LiveArea Search from the lower-right corner when Sony owns that region for either the system IME or Home edit/add mode. It also records the narrower behavior required for ordinary Home transitions: use Sony-style fade-out rather than immediate reset hiding.

The fix intentionally does not make the Search bubble reachable through D-pad or analog focus. LiveArea Search remains an independent PAF plugin and preserves the existing touch-only composition.

## External IME ownership

SceShell exposes the system keyboard through `ime_plugin`. The identifier is present in all 45 audited SceShell images at firmware 3.60 or later across retail-reference, retail, testkit, devkit, external debug, internal, and 4.00 INA families.

On retail Vita 146, folder-name editing changed the runtime gate from `ime=0` to `ime=1`. Native capture showed the full green Enter key with no Search bubble above it. The production gate therefore treats a visible `ime_plugin` as immediate system ownership and disables, hides, and inactivates the Search page before it can intercept the lower-right keyboard region.

## Home edit/add mode

The first hypothesis searched the Home tree for resource-derived `edit_*` descendants. Hardware rejected it: the visible edit/add surface did not expose those descendants through `FindChild`, and Search remained present.

A bounded direct-child probe then compared ordinary Home with Home edit mode. Ordinary Home had no direct full-screen overlay or direct lower-left Back corner. Entering edit mode added both of the following visible direct children to the topmenu Home page root:

- a `widget_base` at `(0, 0)` with size `1000 × 600`;
- a `corner_button` at `(-480, -272)` with size `90 × 90`.

The second child is Sony's native lower-left Back control. The full-screen `widget_base` owns the opening edit/add surface that contains the per-bubble ellipsis controls and add-page button. It is transitional, so it is not sufficient as the only persistent ownership signal.

When edit mode settles, the known Home `scroll_view` expands to `(0, 0)` with size `960 × 544`, taking over the information-bar region. The lower-left Back corner remains present. The production predicate therefore accepts either the opening full-screen overlay or the settled full-screen scroll, but only while the native Back corner also exists.

Ordinary folder view provides the critical negative control. It also exposes a direct lower-left Back `corner_button`, but it has no direct full-screen `widget_base`; its Home scroll remains `960 × 512` at Y `-16`. Search therefore remains available in a folder while disappearing for both the opening and settled phases of Home edit mode.

The production predicate uses widget type, visibility, position, and bounded size ranges. It uses no magic PAF identifier and does not depend on the dynamic hashes observed during diagnostics.

## Transition policy

System-owned conflicts use immediate reset hiding:

- visible external `ime_plugin`;
- Home edit/add ownership;
- notification-panel ownership;
- AppMgr system-overlay ownership;
- opening LiveArea Search results.

Ordinary departure from the Home bubble surface uses PAF's fast fade transition. This covers folder/application navigation and matches the visual behavior of Sony's lower-left Back control. Returning to ordinary Home retains the existing fast fade-in.

## Evidence

Retail Vita 146 evidence was captured with self-disabling diagnostic artifacts whose taiHEN entries were disabled before risky initialization. Relevant captures and logs include:

- `build-vita-regression-diagnostic/issue2-folder-ime-20261114T151013Z`;
- `build-vita-regression-diagnostic/issue2-geometryprobe-edit-20261114T152656Z`;
- `build-vita-regression-diagnostic/issue2-geometryprobe-exit-20261114T152709Z`;
- `build-vita-regression-diagnostic/issue2-folder-check-open-20261114T153124Z`;
- `build-vita-regression-diagnostic/issue2-folder-launch-check-20261114T153201Z`.

The diagnostic probes preserved exact SUPRX/RCO readback, self-disabled their configuration entries for the next boot, and produced no new core dump.
