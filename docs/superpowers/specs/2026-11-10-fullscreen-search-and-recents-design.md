# Full-Screen Search and Recent Apps Design

## Status

Approved on 2026-11-10 and corrected after Vita 146 hardware validation. This document supersedes the compact IME surface and empty-query-close behavior in `2026-11-09-livearea-search-design.md`. The existing Home-only magnifier control, progressive search, icon-cache policy, deferred kinetic-scroll teardown, startup self-disable fail-safe, and title launch route remain in force unless explicitly changed below.

## Goal

Make LiveArea Search read as a native full-screen shell surface rather than an inset dialog. The Vita information bar remains visible. Everything below it is owned by the search page, with no LiveArea blue bleed-through, no partially hidden search field, no gap above the first result, and no result drawing outside the search viewport.

When the query is empty, show up to 20 applications most recently selected through LiveArea Search. The selection is recorded even when opening the title's LiveArea subsequently fails. This history persists across reboot and is independent of ordinary LiveArea icon selections.

## Screen composition

The Vita logical display is `960 × 544`. The upper 32 px are reserved for the existing SceShell information bar. Search owns the remaining `960 × 512` area.

### Results mode without IME

- An opaque `960 × 512` background begins immediately below the information bar and extends to the bottom edge.
- A `960 × 64` header occupies the top of the search-owned area.
- The TextBox and status label retain native ScePaf styling, with horizontal padding contained inside the header rather than created by narrowing the whole surface.
- A 1 px divider forms the exact boundary between header and list.
- The ListView occupies the remaining `960 × 448` rectangle. Both viewport edges are aligned to PAF's 32-pixel ListView raster.
- The first 70 px row begins directly below the divider. There is no spacer, top inset, or black margin.
- The ListView and every recycled row are clipped to that `960 × 448` viewport. Overscroll, animation, focus glow, and recycled children must not draw above the divider, below the screen, or outside the horizontal bounds.
- The result background is fully opaque. The underlying Home wallpaper, bubbles, and top-right notification corner cannot show through it.

### Results mode with IME

PAF sorts pages by the owning plugin/view priority and then by page priority. Vita 146 runtime evidence measured Search at `6`, the IME at `35`, and Home at `50`, with lower numbers rendered in front. Search therefore moves to page priority `36` while editing: immediately behind the IME so key-preview popovers remain visible, but still in front of Home. The page remains logically full-screen, while its ListView camera is clipped exactly to the region above the keyboard:

- The same full-width header is aligned immediately below the information bar.
- A `960 × 208` opaque results surface begins below the header.
- The ListView keeps a 32-pixel-aligned internal size of `960 × 224` at y=`64`. When Search enters the editing layout, `SyncViewSize()` is followed by one camera view box assignment of `[-480, 112, 480, -96]`, exposing exactly `960 × 208` and ending at logical screen row `304`, the measured keyboard boundary.
- The frame update never reapplies the compact view box. PAF uses the same camera state for touch tracking and kinetic motion, so an unconditional per-frame assignment pins the list to its initial camera and suppresses IME-open scrolling.
- Progressive results remain visible in the available area above the keyboard.
- No `920` px inset surface is used; there are no blue side margins.
- Rows, focus glows, recycled cells, and kinetic overscroll are clipped by the ListView camera at the keyboard boundary; they never rely on a sibling mask plane or an ordinary parent Widget clip.
- When editing ends, the ListView expands to the full `960 × 448` viewport without changing its model or query.

### Information bar and notification corner

The information bar, including clock, connection state, and battery, remains visible and unchanged.

The blue top-right notification corner must not be visible while the search page is open. Vita 146 hardware evidence showed that it is a higher-priority Home/topmenu draw path rather than the indicator widget originally inferred from the resource tree. The validated implementation therefore uses composition only: a separate non-interactive PAF page contains an `80 × 32` black plane over the unused extreme top-right information-bar corner and an `80 × 32` header-colored plane immediately below it. Only this narrow page uses draw priority 7; the main result page remains below the system IME. It opens and closes with the result page, restores the Home bubble on close, and never mutates or hides SceShell widgets, the information bar, or the indicator plugin.

## Search and navigation behavior

- Opening Search creates the page, shows the recent-app list immediately, and begins TextBox editing.
- Text changes update search results progressively.
- Clearing the TextBox switches immediately from search results back to Recent.
- Pressing the IME Return key ends editing and keeps the current list visible.
- Cancelling or dismissing the IME also keeps the page visible; it does not close the search page merely because the query is empty.
- Tapping the TextBox after editing ends reopens the IME and preserves the current list.
- Circle while the page is visible and the IME is closed requests the existing one-frame deferred close.
- All close paths retain the verified ordering: stop input, let the callback unwind, clear rows while model data is valid, suppress icon notifications, reset pins, close without a page fade, then clear the model.

## Recent-app model

### Semantics

Recent history records row activations in LiveArea Search, including selections made from the Recent list itself. Recency is updated before attempting to open the title, so a failed `psgm:open` request does not discard the user's selection.

On row activation:

1. Remove any existing occurrence of the title ID.
2. Insert the title ID at the front.
3. Truncate the sequence to 20 entries.
4. Persist the new sequence atomically.

Ordinary bubble selections and launches performed by other applications do not affect this list.

### Display

When the normalized query is empty:

- Resolve stored title IDs against the current `AppIndex`.
- Omit entries that are uninstalled, malformed, duplicate, or no longer launchable.
- Preserve MRU order rather than applying search ranking.
- Display the same icon and single-line title row used by search results.
- Show `Recent` in the header status area.
- If no valid recent entries remain, show `No recent apps` in the existing empty-state widget.

When the query is non-empty, the existing ranked search behavior and numeric result count apply.

### Persistence format

Use `ur0:data/livearea-search/recent_apps.txt` with this bounded text format:

```text
LIVEAREA_SEARCH_RECENTS_V1
<TITLE_ID_1>
<TITLE_ID_2>
...
```

Rules:

- Maximum 20 title-ID lines.
- Accept only title IDs that pass the existing title-ID validation policy.
- Ignore duplicate lines after the first occurrence.
- A missing file means an empty history.
- An invalid header, unreadable file, embedded NUL, oversized file, or malformed content means an empty history; search itself must still initialize.
- Save by writing a temporary file in the same directory, synchronizing it, and verifying the complete bytes. Vita returns `EEXIST` rather than replacing an existing destination, so replacement uses a recoverable transaction: move the current destination to `.bak`, move the verified temporary file into place, verify the installed bytes, then remove `.bak`. Any failed install restores the backup, and startup/load recovers a stranded `.bak` when the destination is absent. A failed save leaves the prior valid file recoverable and does not change whether the subsequent title-open request is attempted or how its result is reported.

The portable core owns ordering, deduplication, truncation, filtering, and serialization rules. A Vita adapter owns bounded file I/O and atomic replacement.

## Component boundaries

### `RecentApps`

A portable core type that:

- loads validated title IDs from a bounded byte sequence;
- records a row selection with move-to-front semantics;
- returns valid installed entries in MRU order;
- serializes the versioned bounded representation.

It has no ScePaf, SceShell, or Vita file dependency and is covered by host tests.

### `RecentAppsFile`

A Vita-specific adapter that:

- reads the persistent file during overlay initialization;
- treats missing or invalid data as an empty history;
- writes and verifies a same-directory temporary file;
- installs only verified bytes through a same-directory backup transaction and recovers an interrupted replacement before loading.

### `SearchOverlay`

The overlay:

- switches between `Recent` and ranked-search models based on normalized TextBox content;
- records recency immediately before calling `OpenTitleLiveArea`, independent of that call's result;
- changes between IME and full-list geometry without rebuilding unrelated shell state;
- owns notification-corner occlusion/restoration for exactly the page lifetime;
- preserves the deferred safe close state machine.

## Failure handling

- Recent file missing, malformed, or unreadable: continue with an empty Recent list.
- Recent save failure: retain the in-memory MRU for the current session, log the error, and still attempt to open the selected title.
- No valid recent apps: show `No recent apps`.
- App removed after being recorded: omit it from the displayed list; compact it out on the next successful save.
- IME composition constraint: restore Search page priority `6` after editing, use priority `36` only while the IME page at `35` is active, and never render rows or plugin chrome below the exact camera boundary.
- Notification-corner occluder open failure: log the failure and do not fall back to private shell mutation; the information bar and unrelated shell widgets must remain untouched.
- Page close during active scrolling: use the already verified deferred/quiescent teardown path.

## Verification

### Host and resource tests

- MRU insertion, deduplication, move-to-front ordering, and 20-entry truncation.
- Filtering missing and non-launchable apps while preserving MRU order.
- Versioned parse/serialize round trip.
- Missing, malformed, oversized, duplicate, and embedded-NUL persistence input.
- Resource contract for a `960 × 512` search-owned area below the information bar.
- Header width and position, zero gap before the first row, `960 × 448` expanded viewport, and fully opaque untextured backgrounds.
- Editing layout uses a `960 × 208` full-width backing surface, a 32-pixel-aligned `960 × 224` ListView, and an exact `960 × 208` camera view box established once on layout entry. Contracts require priority `36` during editing, priority `6` afterward, prohibit camera resets from the per-frame IME transition, and prohibit the rejected sibling-mask, separate-mask-page, and private Widget-clip approaches.
- Existing deferred-close lifecycle contract remains passing.

### Vita 146 validation

Capture native screenshots and interaction evidence for:

1. Opening Search with no history: information bar visible, notification corner absent, `No recent apps`, no blue border.
2. IME open with an empty query: full-width header and dark area to the keyboard boundary, no hidden header, no side bleed, and no black strip above the keyboard.
3. Hold an IME key whose preview extends upward: the preview renders above every Search row and surface.
4. Progressive typed results and aggressive kinetic flicks: the first row remains flush below the divider, while rows, glows, recycled cells, and overscroll stop exactly at the keyboard boundary in immediate and settled frames.
5. Return key: keyboard closes and the full-height list expands correctly.
6. Select a row whose title-open request succeeds, then reopen Search: the selected title appears first under Recent.
7. Select a row while forcing or observing a title-open failure, then reopen Search: the selected title still appears first under Recent.
8. Selecting more than 20 unique titles: only the newest 20 remain.
9. Reselecting an older title: it moves to the front without duplication.
10. Reboot and reopen: Recent order persists.
11. Clear a query: Recent reappears immediately.
12. Kinetic flick plus immediate Circle: the page closes safely with no service loss or new core dump.

### Build and publication

Run clean normal and sanitizer host suites, a fresh warning-free pinned VitaSDK target build, XML/source/static/binary checks, safe deployment with artifact readback and the startup self-disable fail-safe, then commit and push the validated revision to `devnoname120/livearea-search`.
