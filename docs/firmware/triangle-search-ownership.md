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

That candidate proved the copied PAF sample was filtered, but SceShell still
entered edit mode through the independent TopMenu path. It was never installed
permanently.

## Offline verification

The firmware-backed implementation passed fresh normal and ASan+UBSan host
graphs, each with `46/46` tests. The compatibility and contract audits passed:

```text
firmware images       213/213
resource variants       84/84
source contracts         26/26
```

Both Vita profiles built without warnings and passed their binary audits:

| Profile | VELF SHA-256 | SUPRX SHA-256 |
| --- | --- | --- |
| Permanent release | `88990d61a36cdf075d79c99b939208e202f9957e04fa3b939dbfe7113986c735` | `aa08c0d3eef615fd4c8c356dda399b8a29063b12d37c580a95ea29311fc18a64` |
| Self-disabling experimental | `e3b5ac17bfd810e93c09892312175982bb37d168a08b866cb7520e407de10151` | `7c0c7f0fa6417e433a873ec2c197609e430650669f5445869c4b5ddb7eec9f6d` |

The change is code-only. Both profiles contain the existing v1.0.3 RCO:

```text
e5472bd34a84f22ae0db302e55c0302174de72b5b650cd7c6abd83868d899d61
```

## Experimental hardware validation

The exact self-disabling candidate was guarded-deployed to retail Vita 146 with
the config entry disabled during staging and exact file readback before each
armed boot. The candidate self-disabled for the following boot, reached
`overlay active`, registered shell events, retained exact artifact hashes, and
produced no new core.

The user-reported path passed across `1,100` contiguous native frames:

1. touch the Search magnifier;
2. dismiss the system IME with Circle;
3. hold and release Triangle;
4. close Search with Circle;
5. press Triangle again on ordinary Home.

The diagnostic log contains one successful `home edit lock` and one
`triangle pad suppressed`. Search remains visible before, during, and after the
Triangle press. There is no `edit=1`, Search close, ShellUtil unlock,
notification restore, crossed-circle frame, or Home-overview frame during that
press. The later Circle closes Search and releases both ShellUtil locks. The
fresh Home Triangle press then reaches Sony and enters LiveArea edit mode.

The complete regression matrix also passed:

- PS/Home with the IME active and dismissed: `1,300` frames, Search retained
  through physical button-up and closed afterward, with no crossed circle,
  Home overview, or application-switcher event;
- Start shortcut: `1,000` frames, visible Home opens Search once and Sony edit
  mode suppresses Start;
- notification ownership: `800` frames over two cycles, with no Search or IME
  activation behind the popup;
- application switcher: `800` frames, event 4 suppresses Search, lower-right
  input passes through to Sony, and event 5 clears ownership.

Evidence:

```text
build-vita-regression-diagnostic/
  triangle-topmenu-lock-validation-20261115T034110Z/
  triangle-topmenu-lock-ps-validation-20261115T034221Z/
  triangle-topmenu-lock-start-validation-20261115T034601Z/
  triangle-topmenu-lock-notification-validation-20261115T034837Z/
  triangle-topmenu-lock-switcher-validation-20261115T034934Z/
```

## Permanent installation

The exact permanent release from source commit
`ac58bbdb955c475e655afda4b89cb1be91e9accb` was installed on retail Vita 146.
The guarded installation staged files while the taiHEN entry was disabled,
activated the config last, and passed two clean boots plus Start/Back smoke.

Final device state:

```text
config                  1 active, 0 disabled
SUPRX SHA-256           aa08c0d3eef615fd4c8c356dda399b8a29063b12d37c580a95ea29311fc18a64
RCO SHA-256             e5472bd34a84f22ae0db302e55c0302174de72b5b650cd7c6abd83868d899d61
startup.log SHA-256     eb2de228838a37243ec103392a1c69d896af66173bf9721c5827375bb0bfae59
startup.log size        16091 bytes
core files              442, exact baseline
fixed-name core         absent
```

The clean-release startup log remained byte-stable through smoke testing and a
second reboot. A final `900`-frame permanent capture repeated the exact
Circle-then-Triangle path: Search remained visible through Triangle hold and
release, Circle closed it, ordinary Home Triangle entered edit mode, and Circle
returned to normal Home. Exact hashes, config, startup log, and the core set
remained unchanged.

Permanent evidence and aggregate report:

```text
build-vita-regression-diagnostic/
  triangle-topmenu-lock-permanent-deploy-20261115T035058Z/
  triangle-topmenu-lock-permanent-smoke-20261115T035431Z/
  triangle-topmenu-lock-final-20261115T035607Z/report.json
```
