# Native Home corner transition

Contract ID: `home-corner-native-transition-v1`

## Scope

LiveArea Search originally faded the complete `page_home_search` page with
`Type_FadeinFast`. Sony's Home Back control does not use a page fade. It keeps
full opacity and animates only the `CornerButton` scale around the corner style's
native pivot. This report records the resource identity, firmware entry points,
exact runtime calls, measured curve, and lifecycle required to reuse that native
mechanism for Search.

## Common-resource identity

Retail 3.60 `common_resource.rco` defines the stock lower-left Back corner as:

```text
style ID              0x35CA8EE7
style type            style_corner_button
layout_type           2
Back texture ID       0x352FFDE1
corner object size    64 x 64
corner object offset  (-13, -11, 0)
template ID           0x633EE8AB
CornerButton child    0xD7A27AEF
template position     (-416, -506, 0)
```

The symmetric lower-right common style/template are `0xD7220C5C` and
`0xB6BE778E`. Search's Home control is also a PAF `CornerButton`; its custom
style uses the lower-right `layout_type=3`, Sony's stock 64 x 64 plane geometry,
and the same common corner base/highlight/glow resources. The native transition
therefore belongs to the `CornerButton` class path, not to a separate RCO effect
table.

## Retail PAF entry points

The relevant retail 3.60 exports are:

| Operation | Library/NID | Retail address |
| --- | --- | --- |
| `transition::GetDefaultDelay` | `ScePafCommon/0xDE5322EB` | `0x832096C3` |
| `transition::GetDefaultDelayReverse` | `ScePafCommon/0xC85E4F3A` | `0x8320749D` |
| `transition::Do` | `ScePafCommon/0x6CF3A3C8` | `0x83208301` |
| `transition::DoReverse` | `ScePafCommon/0xB48C9BDC` | `0x8320977D` |
| `Widget::Show(Type)` | `ScePafWidget/0x44B22ACA` | `0x83353A4F` |
| `Widget::Hide(Type)` | `ScePafWidget/0xF1176ACC` | `0x83353B0F` |
| `Widget::SetScale` | `ScePafWidget/0xEE467CF8` | `0x83354205` |

For `Type_Reset` (`255`), `Widget::Show` and `Widget::Hide` dispatch through
virtual methods at vtable offsets `+144` and `+148`. On a `CornerButton`, those
virtual methods run the class-specific scale lifecycle below. The stock Back
control does not pass Popup3, Popup4, or Popup5 to `Widget::Show/Hide`.

## Exact show sequence

Six independent runtime cycles on retail Vita 146 produced the same calls for
the exact lower-left Back widget pointer:

1. The style/resource creates the corner at uniform scale `0.600`.
2. `CornerButton::Show(Type_Reset)` immediately calls `SetScale` with:

   ```text
   target          (0.300, 0.300, 1.000)
   timer           null
   animation ID    0x00010005 (Widget::ANIM_SCALE)
   option          0
   delay           0
   ```

3. The same Show call immediately schedules:

   ```text
   target          (1.000, 1.000, 1.000)
   source          (0.300, 0.300, 1.000)
   timer function  32 (Timer::FUNC_EXPO_INOUT in the shipped PAF header)
   timer duration  349 ms (nominal 350 ms)
   animation ID    0x00010005 (Widget::ANIM_SCALE)
   option          0
   delay           0
   ```

4. `Widget::Show(Type_Reset)` returns while the scale animation is active.

The measured composed scale reaches approximately `1.000` after 130 ms,
overshoots to `1.069` near 200 ms, and settles at `1.000` near 350 ms. The
numeric timer function is therefore part of a larger CornerButton animation path;
the observed curve, not the enum label alone, is the behavioral contract.

Throughout every visible show sample:

```text
show alpha   1.000
visible      true
touch events enabled
```

There is no opacity fade.

## Exact hide sequence

For every cycle, Sony closes the Back corner through
`CornerButton::Hide(Type_Reset)`. The call invokes:

```text
target          (0.600, 0.600, 1.000)
source          (1.000, 1.000, 1.000)
timer function  7 (Timer::FUNC_QUAD_IN)
timer duration  149 ms (nominal 150 ms)
animation ID    0x00010004 (Widget::ANIM_SIZE)
option          2
delay           0
```

The widget then leaves the active tree. No alpha animation is used. The stock
widget's event-enable bit remains set in the direct samples; effective input
ownership ends when Sony removes the object from the active page. A persistent
Search page must reproduce that effective boundary explicitly by disabling its
button events before starting the hide path, then inactivating or hiding the
otherwise-empty page when the transition completes.

## Reuse contract for Search

The narrow implementation is to reuse `CornerButton`'s native virtual
transition rather than recreate the timer curve:

- keep the Search page itself on `Type_Reset`; do not fade the page;
- show the Search button with `CornerButton::Show(Type_Reset)`;
- hide it with `CornerButton::Hide(Type_Reset)`;
- enable Search touch/gesture ownership before a show request;
- disable Search touch/gesture ownership before a hide request;
- keep the otherwise-empty Search page active for the native hide interval, then
  inactivate it after the firmware-derived 150 ms deadline;
- use an immediate page reset/inactivation for system-owned suppression,
  teardown, or result-page ownership;
- use one overwriteable transition deadline so a reversal invalidates the
  superseded completion;
- repeated visibility requests must not restart the active transition;
- show during hide and hide during show must issue the opposite native request,
  allowing the CornerButton class to replace its own animation IDs;
- teardown must leave the page hidden, inactive, and noninteractive.

This preserves Search artwork, bubble/background, position, sound, touch target,
notification ownership, switcher ownership, IME/results lifecycle, PS/Home
ownership, and Start shortcut behavior.

## Search implementation and frame comparison

The production state machine has four states: `Hidden`, `Showing`, `Visible`,
and `Hiding`. Ordinary visibility changes call the native
`CornerButton::Show/Hide(Type_Reset)` path. The state machine uses the exact
firmware durations as completion deadlines:

```text
show deadline  350,000 us
hide deadline  150,000 us
```

Repeated requests do not restart an active transition. A show during hide or a
hide during show issues the opposite native request and replaces the deadline.
System-owned surfaces and result-page ownership bypass the ordinary animation,
kill scale/size animation state, hide the page, and inactivate it immediately.

The self-disabling deadline candidate used for native validation was:

```text
SUPRX SHA-256  96892a9494f99acce53db0fcad9b6c59cd45ec19ae133a044a665923f189c66a
RCO SHA-256    e5472bd34a84f22ae0db302e55c0302174de72b5b650cd7c6abd83868d899d61
config         0 active, 1 disabled after startup
core files     442, exact baseline
```

The direct Search capture contains 1,400 native 960 x 544 frames and three
ordinary hide/show cycles:

```text
build-vita-regression-diagnostic/
  home-corner-deadline-validation-20261115T052457Z/
```

Each lower-right frame was compared against the hidden frame immediately before
its show request. The machine-readable comparison is:

```text
build-vita-regression-diagnostic/
  home-corner-deadline-validation-20261115T052457Z/
    stock-search-frame-comparison.json
```

Median frame measurements are:

| Measurement | Search capture | Stock Back contract |
| --- | ---: | ---: |
| Visible shrink interval | 95.863 ms | 149 ms timer |
| Visible expand/settle interval | 260.945 ms | 349 ms timer |
| Peak width scale | 1.053 | 1.069 |
| Peak height scale | 1.077 | 1.069 |
| Alpha values | no fade observed | 1.000 only |

The frame intervals exclude the low-delta beginning and ending tails of the
native scale animation; they are therefore expected to be shorter than the exact
timer durations. Synthetic controller delivery plus main-thread dispatch added a
median 172.393 ms before measurable hide motion and 157.892 ms before measurable
show appearance. Once motion is visible, all three cycles follow the stock
shape: the control shrinks into the lower-right anchor, show reaches nominal
size, overshoots, and settles. During hide, the measured lower-right edge stays
within 0-3 pixels horizontally and 0-2 pixels vertically while the upper-left
edge moves inward, confirming the corner pivot rather than a center fade.

Sony's stock Back widget leaves its event-enable bit set until page removal.
Search cannot copy that detail because its page deliberately persists across
ordinary Home visibility changes. Search therefore disables touch and gesture
events before issuing native hide. A lower-right touch during the captured hide
produced no `home search callback`, result page, or IME activation.

## Reversal and ownership validation

Rapid reversal was captured separately:

```text
build-vita-regression-diagnostic/
  home-corner-reversal-validation-20261115T052637Z/
```

Hide-to-show produced `hide native`, `show native`, then only `show complete`.
Show-to-hide produced `show native`, `hide native`, then only `hide complete`.
No superseded deadline completed, the artifact hashes and disabled configuration
remained exact, and no core file appeared.

The complete guarded retail matrix passed:

| Scenario | Evidence | Result |
| --- | --- | --- |
| Direct show/hide, touch boundary | `home-corner-deadline-validation-20261115T052457Z` | 1,400 frames; no invisible activation |
| Rapid hide/show reversal | `home-corner-reversal-validation-20261115T052637Z` | 900 frames; no stale completion |
| Application switcher | `home-corner-deadline-switcher-validation-20261115T052731Z` | event 4 hides; lower-right input passes through; event 5 restores |
| Notification ownership | `home-corner-notification-ownership-20261115T053344Z` | two cycles; immediate suppression; no Search behind popup |
| PS/Home with IME active and dismissed | `home-corner-ps-validation-20261115T053429Z` | 1,300 frames; no crossed circle, overview, or switcher leak |
| Start shortcut, Home edit, external IME | `home-corner-start-validation-20261115T053540Z` | 1,000 frames; visible Home opens Search; owned surfaces suppress Start |
| Triangle/Circle ownership | `home-corner-triangle-validation-20261115T055106Z` | 1,100 frames; Triangle suppressed only while Search owns Home |

Every run preserved the exact self-disabled artifact/configuration and the
442-core baseline. After the matrix, the permanent pre-change release was
restored byte-for-byte before diagnostics were removed from source.

## Final optimized artifacts

The probe-free production source initially exceeded Vita's SCE metadata gap in
an unoptimized release link: segment 0 ended at `0xF40B`, leaving 3,061 bytes
before segment 1 while `vita-elf-create` required 3,644 bytes. This was a build
layout limit, not a runtime or contract failure. The Vita build wrapper now uses
`MinSizeRel` by default and still permits an explicit Debug, Release,
RelWithDebInfo, or MinSizeRel override. The size-optimized segment 0 ends at
`0xA78E`; both final profiles convert successfully and pass their binary audits.

Fresh final verification produced:

```text
normal host suite       48 / 48
ASan + UBSan suite      48 / 48
firmware compatibility  213 / 213
resource compatibility   84 / 84
source contracts          26 / 26
```

The 213-image firmware result combines two independently version-checked roots:
203 images under `/Volumes/data/Downloads/vita` and 10 retail-reference images
under `/Volumes/data/Downloads/vitadump 3.60 ELF`.

Final artifact hashes are:

| Profile | VELF SHA-256 | SUPRX SHA-256 |
| --- | --- | --- |
| Self-disabling experimental | `c9e49ada67c99fc8892c24fb9696fd60a7d909c13adddc8e4e53a896b9151742` | `8dafad0f4d359534b1bcca39d1ad52a3ec1205ccb07a96a1f4a97f19ecc92ea7` |
| Permanent release | `4a3c19aff41ae2936fcf2f076ba7c751bb8e48183a32ae78439ab586691293d1` | `d5ff0ef8b52f2a8f74a1a593655d07da0b588552ee75c13b3bbbca3560dcc7e6` |

Both profiles contain byte-identical RCO
`e5472bd34a84f22ae0db302e55c0302174de72b5b650cd7c6abd83868d899d61`.
The permanent install ZIP is
`fab3e473230c95564c32fe9f7fa3e334de32423c41a6cdcb69031dfe6d7bb2d2`.

## Final optimized retail validation

The exact final experimental SUPRX was guarded-deployed and self-disabled before
risky initialization. It then passed the production interaction matrix without
probe-only controller hooks:

| Scenario | Evidence | Result |
| --- | --- | --- |
| Guarded deployment and natural edit cycles | `home-corner-final-experimental-20261115T060745Z` | exact hashes; 0 active/1 disabled; 442 cores |
| Application switcher | `home-corner-final-switcher-validation-20261115T060858Z` | 800 frames; suppression, pass-through, and restore passed |
| Notification ownership | `home-corner-final-notification-ownership-20261115T061220Z` | 800 frames; two immediate hide/native restore cycles; no activation behind popup |
| PS/Home with active and dismissed IME | `home-corner-final-ps-validation-20261115T061303Z` | 1,300 frames; no crossed circle, overview, or switcher leak |
| Start, Home edit, folder IME | `home-corner-final-start-validation-20261115T062013Z` | 1,000 frames; Start opens only on visible Home |
| Triangle/Circle ownership | `home-corner-final-triangle-validation-20261115T062057Z` | 1,100 frames; Search ownership suppresses Triangle only while required |

The notification test was run from a normalized stable Home state. A preliminary
attempt began after the switcher test had launched PS Store; Search was already
hidden and the harness correctly rejected that contaminated precondition. No
production change was made for that harness failure.

The exact permanent release was then installed from the disabled experimental
state and rebooted twice:

```text
evidence                 home-corner-final-permanent-deploy-20261115T062211Z
config                   1 active, 0 disabled
installed SUPRX          d5ff0ef8b52f2a8f74a1a593655d07da0b588552ee75c13b3bbbca3560dcc7e6
installed RCO            e5472bd34a84f22ae0db302e55c0302174de72b5b650cd7c6abd83868d899d61
stable startup.log       5279e6c4573db698390aae55ce797d93e4a6af5fa6ce47f4de616b58a4184728
startup.log size         21,685 bytes
core files               442, exact baseline
```

Search/Back smoke passed between the two clean boots. The release did not append
to `startup.log` after the first clean boot, and no persistent or transient core
file appeared.

## Firmware and runtime evidence

Static IDA MCP evidence:

```text
build-analysis/ida-corner-animation/
  retail-corner-cluster.json
  retail-transition-engine.json
  shell-transition-callers.json
  shell-back-identifiers.json
  shell-back-id-callers.json
```

Native camera baseline:

```text
build-vita-regression-diagnostic/
  native-back-transition-20261115T043938Z/
```

Direct state and call probes:

```text
build-vita-regression-diagnostic/
  back-animation-probe-20261115T044442Z/
  back-call-probe-20261115T045357Z/
  back-scale-probe-20261115T045709Z/
  back-scale-full-probe-20261115T045945Z/
```

The final machine-readable contract is:

```text
build-vita-regression-diagnostic/
  back-scale-full-probe-20261115T045945Z/
    stock-back-transition-contract.json
```

All probes used self-disabling experimental artifacts, preserved exact file
readback, and produced no new core. After each probe the permanent baseline was
restored. Final restored retail state before production work:

```text
config                  1 active, 0 disabled
SUPRX SHA-256           aa08c0d3eef615fd4c8c356dda399b8a29063b12d37c580a95ea29311fc18a64
RCO SHA-256             e5472bd34a84f22ae0db302e55c0302174de72b5b650cd7c6abd83868d899d61
startup.log SHA-256     eb2de228838a37243ec103392a1c69d896af66173bf9721c5827375bb0bfae59
core files              442, exact baseline
```
