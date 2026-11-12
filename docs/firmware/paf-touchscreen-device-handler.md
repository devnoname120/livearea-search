# PAF touchscreen device-handler ABI

## Scope

This report records the private PAF touchscreen callback ABI used by
LiveArea Search to yield the Home notification corner before SceShell pauses
normal PAF callbacks. The permanent release uses the callback only for the
bounded pre-dispatch gate described below; coordinate logging remains confined
to self-disabling diagnostic builds.

The public reverse-engineered PAF header declares a callback receiving one
`touchscreen::Data` record. The actual weak stub and firmware export use a
callback receiving `touchscreen::DataList *` instead:

```text
_ZN3paf11inputdevice11touchscreen16SetDeviceHandlerEjPFvPNS1_8DataListEE
```

The export belongs to library `ScePafMisc` (`0x3D643CE8`) and has function
NID `0xCBAF777C`.

## Cross-family implementation

Representative local firmware images were inspected through IDA MCP.

| Family | Image SHA-256 | Implementation |
| --- | --- | --- |
| retail 3.60 | `1dee7e0c0239934ef00694c0a888d07b227799243f6145593df4d0caa2bc65aa` | Thumb entry `0x83246B61`, code `0x83246B60` |
| 3.200 CEX-for-tool | `9d0c66f4ba0658b864bca1bad811c1d344eeee0ab2bfc2da097a27321e08da68` | code `0x810447DC`; the raw, unrelocated export entry in the ELF is `0x81044705` |
| 4.00 INA | `927079011c79cf198891ff263872d97847ae9f2ef7641b2af5eb05a6298967ca` | Thumb entry `0x81045CD1`, code `0x81045CD0` |

All three implementations have the same bounded behavior:

1. Reject a port index greater than one by returning null.
2. Select one of two callback slots using `port * 4`.
3. Return the previous callback.
4. Store the replacement callback.

The two slots are therefore the front and rear touchscreen handlers. LiveArea
Search uses port zero only.

## Callback payload

The PAF touch-update routine was decompiled in the same firmware families.
It builds a fixed array of eight records and invokes the currently installed
callback once per touchscreen port after the array has been populated. The
default callback copies exactly `8 * 0x20 = 0x100` bytes.

Each record is 32 bytes:

| Offset | Size | Meaning |
| --- | ---: | --- |
| `0x00` | 4 | internal field |
| `0x04` | 4 | internal field |
| `0x08` | 4 | top-left X coordinate |
| `0x0C` | 4 | top-left Y coordinate |
| `0x10` | 4 | force |
| `0x14` | 4 | state (`0` none, `1` press, `2` release, `3` move) |
| `0x18` | 4 | centered X coordinate |
| `0x1C` | 4 | centered Y coordinate |

The coordinate offsets are independently confirmed by the exported PAF
accessors:

- `GetPointFromTopleft` NID `0x49BD531A` reads `0x08` and `0x0C`.
- `GetPointFromCenter` NID `0x4E922BF6` reads `0x18` and `0x1C`.

## Frame-order proof

Retail 3.60 PAF invokes the front callback from
`ScePafMisc_C0B69287` at `0x83245F58`. The callback slot is read at
`0x83246706` and invoked at `0x83246712`. The routine's sole caller is
`ScePafMisc_B51652DF` at `0x832432DE`.

After touch polling returns, that caller invokes `ScePafMisc_996A8C03`, which
iterates registered input consumers through virtual slot `+0x0C`, and then
`ScePafMisc_26F81894`, which invokes their `+0x10` phase. PAF's global
`MainThreadCallList` is one of those consumers: its listener vtable `+0x0C`
entry is `sub_83201E60`, which calls `CallList::Update` NID `0x3EF56DF3` at
`0x83201AE8`.

The ordering is therefore bounded and deterministic within one PAF frame:

1. build the complete touchscreen `DataList`;
2. invoke the installed touchscreen wrapper;
3. run `MainThreadCallList`, including LiveArea Search's visibility tick;
4. continue downstream input-consumer and widget processing.

This explains why a pre-dispatch opening latch is required. Hiding Search in
the touch callback without retaining state would be undone by a later
visibility tick before SceShell opens the panel. Hardware validation also
showed that more than one PAF frame may elapse between the press and the point
where SceShell suspends callbacks, so a single-poll latch is insufficient.

## Notification-panel state

The indicator plugin reports dynamically opened pages through
`paf::Plugin::GetOpenedPages`, exported by `ScePafToplevel`
(`0x4D9A9DD0`) as NID `0x3E9B4F77`. Normal Home has no opened indicator page.
The notification panel creates a visible dynamic page named
`page_popup_list_view0`.

### `GetOpenedPages` return semantics

The `int32_t` return is not an output-validity flag. Representative PAF
implementations use it as the last internal page-resolution status while
independently appending page identifiers to the caller-provided vector:

| Family | Entry and code | Status capture and return |
| --- | --- | --- |
| retail 3.60 | Thumb entry `0x8325F8B1`, code `0x8325F8B0` | page resolver result enters `R5` at `0x8325F952`; `R5` is returned at `0x8325FA9A` |
| 3.200 CEX-for-tool | raw export entry `0x8105DEF1`, relocated code `0x8105DFC8` | page resolver result enters `R11` at `0x8105E076`; `R11` is returned at `0x8105E1BA` |
| 4.00 INA | Thumb entry `0x8105F511`, code `0x8105F510` | page resolver result enters `R5` at `0x8105F5B2`; `R5` is returned at `0x8105F6FA` |

All three functions reject a null output vector with `-1`, but a non-null
vector is populated before the accumulated resolver status is returned. The
second internal opened-page registry also appends independently of that
status. A `-1` return can therefore accompany either a valid nonempty vector
or a valid empty vector.

Retail evidence in
`build-vita-regression-diagnostic/rev8-notification-fix-validation-20261112T134955Z`
confirms both cases: `-1` with `page_popup_list_view0` while the panel is open,
and `-1` with an empty vector after it closes. LiveArea Search consequently
classifies the populated vector as authoritative. A nonempty vector is
Visible regardless of status; an empty vector is Hidden for a nonnegative
status or the firmware-proven `-1` sentinel; any other negative empty result
remains Unknown and fails closed.

The compatibility audit requires both `0xCBAF777C` and `0x3E9B4F77`; all 213
locally available firmware images at 3.60 or later pass, spanning retail,
testkit, devkit, debug/internal, and reference families.

### Early ownership signal

Opened-page registration is delayed beyond the visible panel animation, so it
cannot by itself protect the notification corner. The earlier stable signal is
the panel's native corner widget. The static indicator page is `0xFFD7E3F0`
and its notification corner is `0x8BF9D3C5`. The corner is instantiated before
it becomes visible or active, remains present while the panel owns the corner,
and disappears only after closure.

Native evidence in
`build-vita-regression-diagnostic/rev8-notification-widget-probe-20261112T144228Z`
records the opening sequence as `0x00000001` (exists), then `0x03E80007`
(visible and active), and the closing sequence through `0x03E80003`,
`0x00000001`, then absent. The identifier occurs exactly once in all 42 audited indicator resources
at firmware 3.60 or later, alongside the static page identifier. This covers
retail, testkit, devkit, external debug, and internal families.

## Integration rule

The wrapper may inspect the list but must forward the same pointer, unchanged,
to the previous handler. Installation is valid only when a non-null, non-self
previous handler is returned. Teardown restores that handler and preserves any
handler that displaced LiveArea Search in the meantime.

On a front-touch press inside the stock top-right notification bubble, while
Search results are closed, the wrapper synchronously disables and hides the
Home Search page before forwarding. The press records a monotonic opening
deadline 500 ms in the future. Repeated valid empty-vector queries remain
suppressed until the native corner widget appears or the deadline expires.
Corner existence proves panel ownership before opened-page registration and
transitions the state to close tracking. Opened-page enumeration remains a
secondary ownership signal. Unreadable state fails closed. The first resumed
frame after the corner disappears and no opened page remains restores Search.
If the panel never opens, a known-hidden query at the deadline provides
bounded recovery. A close press while suppression is active does not rearm the
deadline.

Native evidence is stored in
`build-vita-regression-diagnostic/rev8-touchprobe-notification-20261112T125727Z`.
It records two complete top-right press/release sequences at PAF coordinates
`(920, 25)`, unchanged forwarding that opens Sony's panel, exact artifact and
configuration readback, and no new core dump. The unfixed probe also
reproduces the original failure: a lower-right tap opens Search and the IME
behind the still-visible notification panel.

Release binaries import both required PAF functions but contain no touch
coordinate logging, startup-log writes, TTY output, or diagnostic strings.
