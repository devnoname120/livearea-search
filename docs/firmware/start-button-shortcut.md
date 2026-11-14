# Start-button Search shortcut

Contract ID: `start-button-shortcut-v1`

## Scope

This report records the input ownership rule for opening LiveArea Search with the Vita Start button. The shortcut does not add D-pad or analog focus to the Home magnifier.

## PAF input identity

The verified PAF pad interface represents Start as `PAD_START = 0x200` in `paf::inputdevice::pad::Data::Keycode`. LiveArea Search already owns the PAF pad device-handler chain for bounded PS-button handling, so the shortcut reuses that pre-dispatch path rather than polling SceCtrl or adding a second input listener.

## Ownership rule

Start is owned only when the Search Home page is visibly exposed and no Search results page exists. On the rising edge:

1. copy the PAF pad record;
2. remove only `PAD_START` from the forwarded copy;
3. preserve every unrelated button and analog field;
4. retain ownership and continue filtering Start until physical button-up;
5. queue the open request for the PAF main-thread call list;
6. re-evaluate the complete live Home ownership gate before opening Search.

The second gate is required because Home ownership can change between raw pad sampling and main-thread processing. It covers foreground LiveArea ownership, AppMgr system UI, the notification panel, external IME ownership, Home edit/add mode, and the topmenu Home surface.

When the magnifier is hidden, Search is already open, or Start is otherwise unowned, the original pad record is forwarded unchanged. This preserves Sony's native behavior outside the single visible-Home shortcut.

## Regression coverage

`tests/test_start_button_filter.cpp` verifies the rising edge, hold, release, unrelated-button preservation, and the hidden-state pass-through rule. The Vita source contract additionally requires the live visibility recheck before `OpenResultsPage`.

## Native acceptance

Retail Vita 146 acceptance is recorded in
`build-vita-regression-diagnostic/start-button-validation-20261114T165432Z`.
The harness captured 1,000 native `960 × 544` frames and correlated them with
diagnostic lifecycle markers and exact device snapshots:

- a held Start press on visible Home opened Search exactly once;
- keeping Start held did not queue or open a second Search page;
- Start produced no Search-open markers after Home edit mode had taken immediate
  ownership;
- Start produced no Search-open markers while the external folder-name IME owned
  the lower screen, and the complete green Enter key remained unobstructed;
- the experimental SUPRX remained
  `90e922cb88970edf1f1422edbc671cf800d9a0f50c5407b0e59688806fc83ce0`,
  the config retained exactly one disabled entry, and no new core appeared.

The exact permanent release was then installed and reboot-verified under
`build-vita-regression-diagnostic/start-button-final-release-deploy-20261114T165548Z`.
Its SUPRX is
`f71f4cac34d69bf9736c67f1a8314f548193f7fb38770a3a90b906e223ceb102`;
the RCO is
`7527df4b4df2c36b9e3bdeb276c3631c8cb0a51995b085b5902e2dc5635f63c8`.
The release retained one active configuration entry, wrote nothing to the
diagnostic startup log during Start interaction or a subsequent reboot, and
produced no new core.
