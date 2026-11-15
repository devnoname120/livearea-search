# Triangle ownership while Search is visible

Contract ID: `triangle-search-ownership-v2`

## Symptom

After Circle dismissed the system IME, Search remained open in its results
layout. Pressing Triangle then entered SceShell's ordinary LiveArea edit mode,
which removed Search from the visible Home surface.

The first candidate removed Triangle from the copied PAF sample before invoking
the previous Home pad handler. Native validation proved that filter executed—the
diagnostic log contained `triangle pad suppressed`—but SceShell still entered
edit mode. The edit transition therefore has an independent TopMenu consumer and
cannot be fixed by forwarding policy alone.

## PAF ownership contract

The verified PAF pad interface represents Triangle as `PAD_MENU = 0x40` in
`paf::inputdevice::pad::Data::Keycode`. `SearchOverlay::HomePadDeviceHandler`
forwards a copied `paf::inputdevice::pad::Data` sample, so Search removes only
the button it owns without mutating the original device sample or disturbing
unrelated input.

Triangle is filtered only when the Search results page is open and the IME
editing layout is no longer active:

```text
results_page_ != NULL && !results_editing_layout_
```

IME-active Search forwards Triangle unchanged to the system keyboard. A fresh
Triangle press is also forwarded normally when Search is closed.

For an owned Triangle press:

- the rising-edge sample is removed from the forwarded PAF sample;
- held and repeated samples remain removed;
- unrelated button bits are preserved;
- ownership remains until physical button-up, even if Search closes while the
  button remains held;
- release clears ownership without synthesizing an action.

## Firmware-backed TopMenu lock

Retail 3.60 firmware shows that `sceShellUtilLock(mask)` reaches SceShell through
SceShellSvc IPC method `0xC0000`; unlock uses `0xC0001`. SceShell dispatches the
lock request at `sub_8344663C`, builds it at `sub_83442BC0`, and applies it through
the refcounted lock manager at `sub_83441E04`.

The manager maps `SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN = 0x1` to TopMenu callbacks
`sub_8346C700` and `sub_8346892E`. Those increment and decrement TopMenu field 48
and invoke the paired input-state helpers `sub_83468AB6` and `sub_834688C4`.
SceShell checks field 48 in `sub_834732EC` before executing the Home key-event
path that can enter edit mode. By contrast, the existing
`SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2 = 0x800` lock routes through PowerManage and
does not gate this TopMenu path.

The source therefore keeps the existing `0x800` lock for Search's no-stop-symbol
PS-button lifecycle and adds a distinct `0x1` TopMenu edit lock only while the
keyboard-dismissed Search layout owns Home.

## Lock lifecycle

The TopMenu edit lock is acquired before Search exposes its display layout. It
is released when Search returns to IME editing, begins closing, yields to the
application switcher, or is destroyed. If Triangle is physically held when
release becomes necessary, unlock is deferred until button-up so a held or
repeat sample cannot leak into SceShell after Search yields ownership. Teardown
may force-release the lock to prevent a process-lifetime leak.

The lifecycle policy is deterministic:

- inactive or IME-active Search does not acquire the edit lock;
- keyboard-dismissed Search acquires it once;
- an already-held lock is stable;
- a no-longer-needed lock is released after Triangle button-up;
- a held Triangle defers ordinary unlock;
- teardown force-releases even during a held press.

The diagnostic build records `home edit lock`, `home edit unlock`, and
`home edit unlock deferred`. `triangle pad suppressed` remains as defense in
depth and as evidence that the copied PAF sample was filtered.

## Verification

`tests/test_home_pad_filter.cpp` covers IME pass-through, the owned Triangle
rising edge, held/repeated samples, Search closure while held, physical release,
unrelated button preservation, closed-Search pass-through, and every edit-lock
lifecycle transition. `tests/check_search_polish.cmake` locks the exact
`PAD_MENU` gate, both ShellUtil lock types, persistent ownership fields, copied
sample forwarding, and diagnostic markers.

The first rejected hardware candidate is recorded in:

```text
build-vita-regression-diagnostic/
  triangle-ownership-validation-20261115T025016Z/
```

Native validation of the firmware-backed candidate is recorded with its
experimental and permanent deployment evidence after all gates pass.
