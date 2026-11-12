# `sceLsdbGetFilteredTitleIdsFromDatabase`: output ownership analysis

Contract ID: `filtered-title-array-ownership-v1`

## Scope

This report verifies the private LSDB ABI used by the current LiveArea Search Home-row eligibility filter. The implementation resolves NID `0xDD7D68C0`, calls the query only after `sceLsdbGetAllAppInfo` has returned, copies every returned title ID into project-owned strings, and then releases the firmware-owned `SceLsdbStringArray`.

The analysis uses the local 3.60 retail-reference IDA databases for `SceLsdb` and `ScePaf`. No hardware experiment is needed to establish the output layout or deallocation contract.

## Export and effective prototype

Firmware image: `/Volumes/data/Downloads/vitadump 3.60 ELF/SceLsdb.enriched.elf.i64`

- Library NID: `0x6BC25E17`
- Export NID: `0xDD7D68C0`
- Exported Thumb entry: `0x811804B3`
- Normalized export address: `0x811804B2`
- Process-global wrapper: `sub_811808FC`
- Database-scoped wrapper: `sub_811832EE`
- Query implementation: `fun_QueryFilteredTitleIds` at `0x81189FC6`

The effective ABI is:

```c
int sceLsdbGetFilteredTitleIdsFromDatabase(
    SceLsdbAppDatabase *database,
    const SceLsdbAppInfoFilter *filter,
    SceLsdbStringArray *output,
    uint32_t flags);
```

Verified layouts:

- `SceLsdbAppInfoFilter`: `0x24` bytes
- `SceLsdbString`: `0x0C` bytes
- `SceLsdbStringArray`: `0x0C` bytes: `{ data, count, capacity }`

## Transaction behavior

`sub_811832EE` validates the database token, acquires the ActivityDb transaction, calls `fun_QueryFilteredTitleIds`, and releases the transaction. The current LiveArea Search implementation invokes this query after `sceLsdbGetAllAppInfo` returns, so it does not create the rejected callback-nested query pattern.

## Existing-output cleanup performed by firmware

`fun_QueryFilteredTitleIds` treats `output` as an in/out vector. Before building the new result, it destroys any existing contents:

1. Iterate `output->count` elements at a 12-byte stride.
2. For each element, compare `string.data` against the firmware empty-string sentinel at `0x811B1CD8`.
3. Free every non-sentinel string allocation through `ScePafStdc_1B77082E`.
4. Set the destroyed element's first two words to `-1`.
5. Free `output->data` through `ScePafStdc_1B77082E`.
6. Reset `output->data`, `output->count`, and `output->capacity` to zero.

ScePaf identifies NID `0x1B77082E` as `sce_paf_private_free`. IDA proves it is a null-tolerant wrapper around public `sce_paf_free`:

```text
ScePafStdc_1B77082E  0x83254608
  CBZ ptr, return
  BL  sce_paf_free   0x83250ADA
```

Therefore storage allocated by the LSDB query can be released with `sce_paf_free`.

## Allocation and element construction

Returned strings are not borrowed SQL pointers. Each title ID is copied into independent PAF storage:

- `fun_AssignPafString` at `0x811AB18C` allocates `length + 1` bytes through `sce_paf_private_malloc` NID `0xFC5CD359`, copies the SQL text, stores the length, and appends NUL.
- Empty strings use the shared empty-string sentinel and length zero.
- `sub_811AB2A0` allocates or grows the element array as `capacity * 12` bytes using the same private PAF allocator.
- Existing strings are deep-copied when the vector grows, then the old string buffers and old array are released through NID `0x1B77082E`.

The output therefore owns both:

- every nonempty `SceLsdbString::data` buffer; and
- the `SceLsdbStringArray::data` element array.

## Compatibility with current LiveArea Search cleanup

Current `ReleaseStringArray`:

```c++
for (std::uint32_t index = 0; index < strings.count; ++index) {
    if (strings.data[index].data != NULL && strings.data[index].length != 0U) {
        sce_paf_free(strings.data[index].data);
    }
}
if (strings.data != NULL) {
    sce_paf_free(strings.data);
}
strings.data = NULL;
strings.count = 0U;
strings.capacity = 0U;
```

This matches the firmware ownership contract:

- A nonempty result has a separately allocated buffer and nonzero length, so it is freed.
- An empty result has the non-null sentinel but zero length, so it is deliberately not freed.
- The element array is always freed once after its children.
- Resetting the vector to null/zero is equivalent to the firmware's post-destruction state.
- Per-element `-1` poisoning is unnecessary because the entire array is immediately freed.

## Copy-before-release ordering

LiveArea Search copies every returned title ID into its own `paf::string` before calling `ReleaseStringArray`. No project record retains pointers into the LSDB output. Sorting and deduplication occur only after all data is project-owned.

## Result

The current `sceLsdbGetFilteredTitleIdsFromDatabase` output ABI and cleanup path are firmware-proven for the 3.60 implementation:

- the 12-byte array/string layouts are correct;
- result strings and the backing array are caller-owned PAF allocations;
- public `sce_paf_free` is the terminal deallocator used by the firmware's private-free wrapper;
- empty sentinel strings must not be freed, and the current zero-length guard satisfies that rule;
- the query occurs after enumeration, not inside the callback.

This resolves the output-ownership uncertainty that blocked a self-disarming hardware candidate. Functional semantics of the selected filter key/operation/flags remain a separate hardware acceptance item.
