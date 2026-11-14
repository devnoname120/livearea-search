# SceShell application-switcher event ownership

Contract ID: `shell-application-switcher-events-v1`

## Scope

This report records the SceShell event pair used by LiveArea Search to yield
the Home corner while Sony's application switcher owns the screen. The
production implementation consumes exported event callbacks only. It does not
read the private LiveSpace controller object or any firmware-specific object
offset.

## Export identity

SceShell exports the registration pair from library `SceShell`
(`0x2404AE25`):

| Operation | NID |
|---|---:|
| register handler | `0x7FE4E732` |
| unregister handler | `0xD4C7D2B7` |

The effective prototypes are:

```c
typedef void (*SceShellEventHandler)(
    int event,
    int argument_2,
    int argument_3,
    void *user_data);

int sceShellRegisterEventHandler(
    int event,
    SceShellEventHandler handler,
    void *user_data);

int sceShellUnregisterEventHandler(
    int event,
    SceShellEventHandler handler,
    void *user_data);
```

Registration allocates a 16-byte callback object containing its vtable,
handler, user-data pointer, and deferred-removal byte. Dispatch invokes virtual
slot `+0x08`, which calls the stored handler as
`handler(event, argument_2, argument_3, user_data)`. Unregistration matches all
three registration arguments. Removal during dispatch is deferred until the
current handler iteration completes.

## Retail 3.60

Firmware image:
`/Volumes/data/Downloads/vitadump 3.60 ELF/SceShell.enriched.elf`

- register exported Thumb entry: `0x832057FF`
- register core: `sub_832054A0`
- unregister exported Thumb entry: `0x83205807`
- unregister core: `sub_832055F6`
- dispatcher core: `sub_83205700`
- dispatcher wrapper: `sub_8320580E`
- callback-object vtable: `off_83723218`

The switcher-entry routine `sub_8346EF6E` loads event `4` at `0x8346EFAA`
and calls the dispatcher at `0x8346EFB0`. It then changes the private
LiveSpace controller mode from Home to the entering-switcher state.

The switcher-exit routine `sub_8346BF04` loads event `5` at `0x8346C622`
and calls the dispatcher at `0x8346C626` before starting the return-to-Home
transition.

## Internal 4.00

Firmware image:
`/Volumes/data/Downloads/vita/decrypted/SCE/4.00_ina/inai400/fs_dec/vs0/vsh/shell/shell.elf`

- register exported Thumb entry: `0x81004A97`
- register core: `sub_81004738`
- unregister exported Thumb entry: `0x81004A9F`
- unregister core: `loc_8100488E`
- dispatcher core: `sub_81004998`
- dispatcher wrapper: `sub_81004AA6`
- callback invocation slot: `sub_8140B9AC` at vtable `off_8152EC84 + 0x08`

The `/livespace/indexview` entry routine `sub_81276316` dispatches event `4`
before the card-stack transition. The paired exit routine `sub_812732AC`
dispatches event `5` before the Home-return transition.

The private controller field at `+0x84` was used only to establish event
semantics during reverse engineering. Its observed values are `0` for Home,
`1` while entering the switcher, `2` while the switcher is active, and `3`
while returning Home. LiveArea Search does not access that field.

## Compatibility corpus

All 45 locally available SceShell images at firmware 3.60 or later export both
NIDs from the same library identity. The corpus spans retail-reference,
retail, testkit, devkit, external DEX, internal CEX, internal DEX, internal
devtool, versions 3.60 through 3.74, and internal 4.00.

The 3.200 tool image predates the supported minimum and does not publish this
export pair. It is not used as a compatibility fallback.

## Integration rule

1. Resolve both functions dynamically from module `SceShell`, library
   `0x2404AE25`; missing exports fail initialization closed.
2. Register the same callback for events `4` and `5` before exposing the Home
   Search page.
3. Event `4` establishes switcher ownership, disables Home Search input, and
   starts the native fast fade-out.
4. Event `5` releases switcher ownership. The next normal visibility update
   re-evaluates notification, quick-menu, title-LiveArea, external-IME, Home
   edit, and ordinary Home-surface gates before allowing the native fast
   fade-in.
5. Unregister both exact `(event, handler, user_data)` triples during teardown.
6. Never infer switcher ownership from persistent topmenu pages, PAF scene
   membership, widget transforms, or the private LiveSpace controller offset.
