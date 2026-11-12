# SceShell PS-button ownership

LiveArea Search must own two independent input paths while its results page is open:

1. the PAF pad sample delivered to widget input consumers; and
2. SceShell's system-level PS-button route.

Filtering only the copied PAF sample is insufficient. Retail hardware showed that Search closed exactly once while the same physical PS press still entered Sony's Home overview. The first restored shell-lock candidate used lock type `0x1`; it prevented Home overview, but native capture showed Sony's crossed-circle stop symbol after Search teardown. The shell ABI provides a distinct PS-button ownership type, `0x800`, specifically for suppressing the system route without that stop symbol. The required lifetime is still bounded by the physical press: acquire before Search becomes interactive, retain ownership while PS is down, then unlock before Search teardown after button-up.

## Export identity

The implementation resolves the private functions dynamically from module `SceShellSvc`, library `SceShellUtil` (`0xD2B1C8AE`):

| Function | NID |
|---|---:|
| event-client initialization | `0xE8AD11EC` |
| lock | `0xA9537233` |
| unlock | `0x21A6CF54` |

All 45 audited SceShellSvc images at firmware 3.60 or later export all three functions under that library. The corpus spans retail, retail-reference, testkit, devkit, external DEX, internal CEX/DEX/devtool, versions 3.60 through 3.74, and 4.00 INA.

Representative export entries are:

| Firmware family | InitEvents | Lock | Unlock |
|---|---:|---:|---:|
| retail 3.60 | `0x8116D404` | `0x8116D732` | `0x8116D848` |
| 3.200 tool | `0x8100C760` | `0x8100CA8E` | `0x8100CBA4` |
| 4.00 INA | `0x8100C794` | `0x8100CAC2` | `0x8100CBD8` |

The 3.200 and 4.00 raw ELFs use Vita image type `0xFE04`, which stock IDA does not load noninteractively. Diagnostic analysis copies changed only the ELF `e_type` field to `ET_EXEC`; export identities and entry addresses above were parsed from the original images.

## Retail 3.60 implementation

IDA decompilation of retail 3.60 establishes the effective contracts:

- `sceShellUtilInitEvents` creates the client mutex, condition variable, and worker thread. It returns `0x80101802` when the client is already initialized; callers must accept that value as ready rather than treating it as fatal.
- `sceShellUtilLock` queues a lock operation through the SceShellSvc client. When the lock mask contains bit `0x800`, it also applies ownership to shell service IDs 53 through 57 and increments a process-local reference count.
- `sceShellUtilUnlock` queues the inverse operation. For bit `0x800`, it decrements that count and releases IDs 53 through 57 when the final owner exits.

The shell-util ABI distinguishes two PS-button lock types:

| Enum | Value | Observed user interface |
|---|---:|---|
| `SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN` | `0x1` | blocks Home overview but permits Sony's crossed-circle stop symbol |
| `SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2` | `0x800` | firmware-backed alternate PS ownership; the public header describes it as “without the stop symbol” |

The dedicated `0x800` branch in firmware independently corroborates that this is not an alias for `0x1`. LiveArea Search must use `SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2` for both lock and unlock.

## Required runtime ordering

The accepted sequence is:

1. Resolve `SceShellUtil` by module, library NID, and function NID; reject missing or unknown combinations.
2. Initialize the event client, accepting success or the already-initialized result.
3. Acquire `SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2` before opening the Search results page.
4. When PS is pressed, copy the PAF pad record, remove only the PS bit from the forwarded copy, and pass all unrelated bits to Sony's previous handler unchanged.
5. End IME editing immediately, but keep the Search page visible while the physical PS press remains owned. This prevents the shell lock indicator from becoming exposed over Home.
6. On the first button-up sample, unlock before Search teardown, then request the existing deferred page close.
7. Release the lock on every failed-open and destruction path as a fail-safe.

The critical invariants are **use the no-stop-symbol `PS_BTN_2` type** and **unlock before Search teardown**. Together they suppress the independent SceShell Home route without deliberately requesting Sony's crossed-circle UI.

## Evidence boundary

The static evidence proves export presence, function identity, the distinct `0x1` and `0x800` semantics, the `0x800` PS-lock branch, and the required call ordering. The rejected `0x1` candidate is captured under `build-vita-regression-diagnostic/rev8-pslock-validation-20261112T160519Z`: it produced no Home overview but exposed the crossed-circle indicator for 34–40 frames after each close.

Native retail acceptance of the `PS_BTN_2` implementation is captured under `build-vita-regression-diagnostic/rev8-pslock-validation-20261112T162042Z`. Both the IME-active and IME-dismissed sequences retained the Search surface while PS was physically held, then closed after button-up with zero Home-overview frames and zero crossed-circle frames. The same negative conditions remained green in the permanently installed release smoke at `build-vita-regression-diagnostic/rev8-final-release-smoke-20261112T173000Z`.
