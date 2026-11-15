# Results Back-corner close transition design

## Status

Approved in chat on 2026-11-15. Ordinary Search closes must run Sony's native bottom-left Back-corner disappearance animation before the results page is destroyed. Safety teardown paths remain immediate.

## Problem

The results page currently treats every close request the same:

```text
request close
-> wait one PAF frame
-> close page_search_results
```

The bottom-left control is a native PAF `CornerButton`, but the page is destroyed before its `Hide(Type_Reset)` transition can run. The Back control therefore vanishes instantly while the Home Search corner uses Sony's native 150 ms shrink animation.

The fix must preserve the existing input-ownership rules. In particular, PS/Home is intercepted and locked until the physical button is released so the held press cannot leak into SceShell and open the application switcher.

## Close classifications

### Ordinary animated closes

The following paths use the native Back-corner hide transition:

- Circle while the keyboard is already hidden and the results page is in display layout;
- tapping the bottom-left Back corner;
- PS/Home while the keyboard is hidden, after the physical PS button has been released.

For PS/Home, the `SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2` lock remains held throughout the 150 ms animation and is released only by final page teardown.

Circle while the keyboard is active remains a keyboard-dismiss action, not a page close. PS/Home while the keyboard is active has no visible Back corner to animate; after ending the IME and observing physical button release, that path closes safely without manufacturing a hidden corner animation.

### Immediate safety closes

A safety close bypasses the Back animation when Search cannot safely retain screen ownership. These paths include:

- successful app launch from a result;
- SceShell or another system surface taking ownership;
- initialization or lock-acquisition failure;
- plugin unload or teardown;
- escalation from an in-progress animated close to a safety close.

Immediate means "close on the next main-thread tick after the initiating callback unwinds", not destruction from inside the input callback.

## Selected state machine

Introduce a results-close policy with four states:

- `Idle`;
- `PendingAnimated`;
- `AnimatingBackCorner`;
- `PendingImmediate`.

An ordinary request immediately disables results-page, search-box, list, and
Back-corner input, releases widget focus, and moves `Idle` to
`PendingAnimated`. On the next visibility tick, after the initiating callback
has unwound, Search:

1. calls `close_button_->Hide(paf::common::transition::Type_Reset)` exactly once;
2. records a deadline `150000 us` in the future;
3. keeps `page_search_results` alive and keeps the native notification bubble suppressed.

At or after the deadline, the existing teardown closes the page, clears result state, restores the notification bubble, releases the Home-edit lock, and releases the PS lock.

An immediate request also disables input immediately and moves any
non-immediate state to `PendingImmediate`. The next main-thread tick performs
teardown without waiting for the corner deadline. Repeated ordinary requests
do not restart the animation or deadline. Repeated immediate requests are
idempotent.

The 150 ms duration reuses the firmware-derived `kHomeCornerHideDurationUs` constant so Home Search and results Back transitions cannot drift apart.

## Input and ownership invariants

Once any close is queued:

- list touch, gesture, key, analog, and focus input is disabled;
- search-box touch, gesture, key, analog, and focus input is disabled;
- Back-corner touch, gesture, key, analog, and focus input is disabled;
- the results page stops accepting key, touch, gesture, analog, and focus input;
- focused widgets release focus;
- no second launch or close callback can run.

During an animated PS/Home close:

- the physical PS button must already be released before animation starts;
- the PS lock remains held while the Back corner shrinks;
- final teardown releases the lock only after the animation deadline;
- no application-switcher event or crossed-circle lock indicator may appear.

## Alternatives rejected

1. **Fade the whole results page.** This does not match Sony's CornerButton animation and changes the entire screen rather than the bottom-left control.
2. **Close the page immediately and animate a copied Back button on a temporary overlay.** This duplicates Sony-owned state and introduces draw-order and input-ownership risk.
3. **Animate every teardown path.** App launch, shell ownership loss, initialization failure, and unload must not keep stale Search UI alive merely for visual continuity.
4. **Release the PS lock before animation.** The still-recent PS action could leak into SceShell during the 150 ms delay and open the application switcher.

## Verification

Policy tests must prove:

- ordinary requests begin one native hide transition after callback unwind;
- repeated ordinary requests do not restart the deadline;
- teardown occurs only at or after `kHomeCornerHideDurationUs`;
- immediate requests close on the next tick;
- an immediate request preempts an active animation;
- state and deadline return to idle after completion.

Source contracts must require:

- `close_button_->Hide(Type_Reset)` before `PageClose` for ordinary closes;
- complete input disabling before animation begins;
- PS lock release only in final teardown;
- explicit animated requests from Circle/tap-Back and released PS/Home paths;
- explicit immediate requests from app launch, shell ownership loss, failure, and unload paths.

Retail Vita 146 acceptance requires native capture of:

- Circle with keyboard hidden: Back corner shrinks for approximately 150 ms, then Search closes;
- tap-Back: identical visible sequence;
- PS/Home with keyboard hidden: identical visible sequence, no crossed-circle indicator, no Home overview/application switcher, and lock release after the hide interval;
- app launch and system-ownership safety paths remain immediate;
- exact experimental/release hashes and config readback;
- no new or overwritten core dump.

The matching diagnostics-off, non-self-disabling build is installed only after the self-disabling candidate passes. The change is committed and pushed to `main`; no GitHub release or tag is created.
