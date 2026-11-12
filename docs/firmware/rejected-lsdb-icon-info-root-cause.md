# Rejected LSDB icon-query build: static root-cause analysis

## Scope

This report analyzes the retail boot failure introduced by the rejected LiveArea Search candidate built in `build-vita-release-polish2`. It is based on the exact rejected object code and the local 3.60 retail-reference firmware databases loaded through IDA MCP. No runtime crash dump was produced, so the conclusion rests on a deterministic caller/callee ABI violation visible in both binaries.

## Rejected artifact identity

- SUPRX: `build-vita-release-polish2/livearea_search.suprx`
- SUPRX SHA-256: `6ec4b939a2a710a3725e3d50baad65d28cae46bd294b7e148375098277974727`
- VELF: `build-vita-release-polish2/livearea_search_vita.velf`
- VELF SHA-256: `31d8c29384708b29cdaf9b775e70e5776b62b1e52690f30bf089ff0635f46d49`
- RCO SHA-256: `0e9711f8924f5c365c9589716df5bcb0804a2819b934d5941e11754fbf483274`
- Recovered object: `build-vita-release-polish2/CMakeFiles/livearea_search_vita.dir/src/vita/lsdb_index_source.cpp.obj`

The rejected SUPRX hash matches the artifact removed from retail Vita 146 during recovery.

## Firmware identity and private API

Firmware evidence comes from `/Volumes/data/Downloads/vitadump 3.60 ELF/SceLsdb.enriched.elf.i64`.

`SceLsdb` exports:

- Library NID: `0x6BC25E17`
- `sceLsdbGetIconInfoByTitleId` NID: `0xDE0DA538`
- Exported Thumb entry: `0x81180495`
- Normalized function address: `0x81180494`

IDA recovered the effective prototype:

```c
int sceLsdbGetIconInfoByTitleId(
    void *database,
    const SceLsdbString *titleId,
    SceLsdbIconInfoRef *iconInfo,
    int *pageNo,
    int useOriginalPathFallback);
```

Relevant ABI layouts from the firmware type database:

- `SceLsdbIconInfoRef`: 8 bytes, `{ object, control }`
- `SceLsdbIconInfo`: `0x58` bytes
- `SceLsdbIconInfo::type`: offset `0x24`
- `SceLsdbString`: `0x0C` bytes

## Rejected caller

IDA loaded the exact rejected object and recovered:

- `LsdbIndexSource::Resolve`: object offset `0x4`
- `LsdbIndexSource::Load`: object offset `0x250`
- `LsdbIndexSource::OnAppInfo`: object offset `0x43C`

`Resolve` binds NID `0xDE0DA538`. `OnAppInfo` then:

1. Builds a valid `SceLsdbString` for the current title ID.
2. Zero-initializes an 8-byte `SceLsdbIconInfoRef`.
3. Calls `sceLsdbGetIconInfoByTitleId(database, &title, &iconRef, &pageNo, 0)`.
4. On success, reads `iconRef.object->type` at offset `0x24`.
5. Releases the shared reference using the ordinary strong/weak count sequence.

The reference release sequence is structurally valid. The fatal error occurs before it can matter.

## Firmware call chain

The firmware call chain is:

```text
sceLsdbGetIconInfoByTitleId          0x81180494
  sub_811807D8                       0x811807D8
    sub_8118230A                     0x8118230A
      sub_8118A4CE                   0x8118A4CE
        sub_8118A25A                 0x8118A25A
          sub_811850E4               0x811850E4
```

The exported wrapper obtains the process-global AppDatabase state. `sub_8118230A` acquires an ActivityDb transaction and delegates the query. `sub_8118A25A` executes the SQL query for `tbl_appinfo_icon`.

The critical behavior is at `0x8118A3C2`:

```text
LDR R1, [R6]        ; load iconInfoRef->object
ADD R0, SP, ...     ; statement
MOV R2, R11         ; useOverridePath
BL  sub_811850E4
```

The function reads the caller-provided `iconInfoRef->object`. It does not allocate an `SceLsdbIconInfo`, construct a shared reference, or assign either word of the reference.

## First deterministic null dereference

`sub_811850E4` has the recovered signature:

```c
int sub_811850E4(
    void *statement,
    SceLsdbIconInfo *iconInfo,
    int useOverridePath);
```

It does not validate `iconInfo` before writing the decoded row. The first object-relative operations include:

- `0x8118512A`: compute `&iconInfo->iconPath` (`iconInfo + 0x18`)
- `0x81185148`: store `rowId` at `iconInfo + 0x00`
- subsequent stores update `position`, `iconPath`, `title`, `type`, `command`, `titleId`, `icon0Type`, parental-lock level, status, and `specialPage`

The rejected caller passed `iconInfoRef.object == NULL`. Once the SQL query returns a matching icon row, the row decoder writes through address zero. This is a deterministic ABI violation and explains the boot-time fault during startup catalog enumeration.

## Falsified alternatives

### Shared-reference cleanup

The rejected caller's strong/weak reference decrement sequence is compatible with the firmware's shared-reference representation. The failure occurs earlier because no reference was constructed by the callee.

### Non-recursive database deadlock

`sceLsdbGetAllAppInfo` invokes its callback while an ActivityDb transaction and statement remain open. The rejected callback performs a nested icon query. This is risky, but it is not the deterministic root cause observed here:

- ActivityDb acquisition NID `0x82B45E74` locks a `paf::thread::RMutex` and increments a nesting counter.
- Release NID `0x11D77056` decrements that counter and unlocks.
- ScePaf NIDs `0xF9FFA0BE` and `0x95F9C45F` are `RMutex::Lock` and `RMutex::Unlock`, backed by the recursive lightweight-mutex path.

The nested call therefore does not establish a simple self-deadlock.

### Invalid title-string layout

The rejected object constructs the 12-byte `SceLsdbString` in the same pointer/length/reserved shape expected by the firmware. The title argument is not the demonstrated fault.

## Correct API classification

`sceLsdbGetIconInfoByTitleId` is an in/out population API for a caller-owned, preconstructed `SceLsdbIconInfo` object referenced by `SceLsdbIconInfoRef`. It is not a factory that returns a newly allocated shared object through an initially empty reference.

A correct call would require an ABI-proven constructor and destructor for `SceLsdbIconInfo` and its nested strings/shared references. Fabricating the object from its size alone would be unsafe.

## Consequence for LiveArea Search

The rejected `0xDE0DA538` design must not return. LiveArea Search must not:

- resolve or call `sceLsdbGetIconInfoByTitleId`;
- pass a zero-initialized `SceLsdbIconInfoRef` as an output object;
- infer Home-row eligibility by constructing private `SceLsdbIconInfo` storage without a firmware-proven lifecycle.

The current filtered-title replacement remains unapproved for hardware deployment until its `SceLsdbStringArray` allocation and release ownership are independently proven from firmware.

## Evidence limitations

No new SceShell core appeared during the rejected boot, so there is no fault PC to compare against `0x811850E4`. The null write is nevertheless deterministic from the exact caller and callee instruction streams and requires no speculative state beyond the presence of a matching icon row.
