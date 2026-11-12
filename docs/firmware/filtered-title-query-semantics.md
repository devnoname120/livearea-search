# `sceLsdbGetFilteredTitleIdsFromDatabase`: Home-row query semantics

Contract ID: `filtered-title-query-semantics-v1`

## Scope

This report verifies the semantic meaning of the private LSDB query used by LiveArea Search to retain only catalog records that own ordinary Home/LiveArea icon rows. It complements `filtered-title-array-ownership.md`, which proves output allocation and cleanup.

The evidence is from the local 3.60 retail-reference IDA databases for `SceLsdb` and `SceShell`.

## LiveArea Search query

LiveArea Search calls NID `0xDD7D68C0` after `sceLsdbGetAllAppInfo` has completed. It supplies:

- filter key `0x21CA76B1`;
- an empty value array;
- operation `0`;
- mask `0`;
- no chained filter;
- flags `0`.

The exported entry is `0x811804B3`; the normalized implementation chain reaches `fun_QueryFilteredTitleIds` at `0x81189FC6`.

## `0x21CA76B1` is the category metadata key

The firmware getter `sceLsdbGetCategory` (NID `0x6469796F`, implementation `0x8118F8FC`) calls the generic string getter with key `0x21CA76B1`.

The filter therefore requires the title to own a normal app-info `category` field. It does not compare the category against a blacklist or a hard-coded category value.

## Empty values mean key existence

The predicate builder `fun_BuildAppInfoFilterPredicateSql` constructs one correlated `EXISTS` predicate per filter node.

For the current filter it emits the equivalent of:

```sql
EXISTS (
    SELECT 1
    FROM tbl_appinfo a0
    WHERE a0.titleId = i.titleId
      AND a0.key = ?
)
```

The value-condition branch executes only when `filter.values.count != 0`. With an empty value array, operation `0` is not applied and no `val = ?` clause is emitted. The binder correspondingly binds only key `0x21CA76B1`.

This is a metadata-existence test, not an equality test against an absent value.

## Flags `0` select the icon-backed query

`fun_QueryFilteredTitleIds` chooses between two SQL builders:

- flags bit `0x1` set: app-info-only query;
- flags bit `0x1` clear: icon-backed query `fun_BuildIconFilteredTitleQuery` at `0x8118924A`.

LiveArea Search uses flags `0`, so the icon-backed query is selected.

The resulting SQL is equivalent to:

```sql
SELECT i.titleId
FROM tbl_appinfo_icon i
WHERE i.pageNo != -100000000
  AND EXISTS (
      SELECT 1
      FROM tbl_appinfo a0
      WHERE a0.titleId = i.titleId
        AND a0.key = 0x21CA76B1
  )
  AND i.type = 0
ORDER BY i.title
```

The firmware binds `i.type` to integer `0` for this path. With flags bit `0x2` clear, it excludes the `pageNo == -100000000` sentinel. With flags bit `0x4` clear, it orders by the icon title.

## Icon type `0` is the ordinary application-icon class

SceShell consumes `SceLsdbIconInfo` objects when populating the Home pages.

Relevant firmware functions:

- `fun_PopulateTopMenuPagesFromIconLists` at `0x832B1994`;
- `fun_InitTopMenuIcon` at `0x832CDF0A`;
- special-icon predicate `sub_832CD9C8` at `0x832CD9C8`.

`SceLsdbIconInfo::type` is the 32-bit field at offset `0x24`.

The special-icon predicate returns the special path immediately when `type != 0`. Only type `0` continues through title-ID checks that distinguish ordinary application icons from a bounded set of system application IDs. The Home population code separately recognizes type `5` as a multi-icon/special page and type `6` as the power icon.

Consequently, `i.type = 0` excludes folders/special pages and the power icon while retaining application icon rows, including system applications that genuinely own ordinary LiveArea rows.

## The sole official filtered-query caller is unrelated

SceShell has one code caller of `sceLsdbGetFilteredTitleIdsFromDatabase`, at `0x834907EE` inside `NpCommIdSearchJob::Run` (`0x83490744`).

Its constructor is `NpCommIdSearchJob::NpCommIdSearchJob(paf::string const&, char const*, int)` at `0x834905C0`. `Run` filters key `0x10C1A2E0` against one string value stored at object offset `0x1C`, the constructor's NP communication ID. It is a generic related-title lookup, not the Home-page population query.

This official caller confirms the API is generic; its key/value pair must not be substituted for the icon-backed Home-row query.

## Resulting eligibility rule

A title is eligible for LiveArea Search only when all of the following are true:

1. `sceLsdbGetAllAppInfo` produced a valid, bootable catalog record.
2. The title has a category metadata field.
3. LSDB has an icon row for the title.
4. The icon row has type `0`, the ordinary application-icon class.
5. The icon row is not on the hidden `-100000000` page sentinel.

The query derives eligibility from LSDB's app-info and icon metadata. It does not use a title-ID or category blacklist.

## Bounded assumptions

This analysis proves the 3.60 query semantics and relies on the compatibility manifest's exact export signature checks across the local >=3.60 LSDB corpus. The release must continue to reject unknown LSDB export signatures rather than applying these assumptions to an unverified variant.
