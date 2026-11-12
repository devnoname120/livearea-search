# SceAppMgr system-UI overlay state

## Conclusion

`_sceAppMgrGetAppState` (SceAppMgr function NID `0x210C0046`) returns a fixed-size state object whose third 32-bit word is the normalized system-UI-overlay flag:

```text
+0x00  uint32_t system_event_num
+0x04  uint32_t app_event_num
+0x08  uint32_t is_system_ui_overlaid
+0x0C  reserved
...
+0x7F  reserved
```

The output is exactly `0x80 bytes`. The value at offset `0x08` is written as either zero or one by each provider examined. It is therefore suitable as the worker-thread quick-menu ownership signal. A nonzero value must suppress LiveArea Search before the main PAF callback resumes.

This field does not cover the notification panel: retail runtime probes showed the notification panel pauses the PAF callback without changing this word. Notification ownership remains a separate gate.

## Export resolution

The wrapper addresses were resolved from each ELF's SceAppMgr export table, not inferred from function names:

| Image | Export wrapper for `0x210C0046` | Provider |
|---|---:|---:|
| Retail 3.60 `SceAppMgr.enriched.elf` | `0x024F2EE0` | `0x024D14D4` |
| Internal 3.200 tool AppMgr | `0x810477A4` | `0x810250C8` |
| Internal 4.00 INA AppMgr | `0x81032EC8` | `0x810114D4` |

The 3.200 image is below the supported >=3.60 runtime range, but its debug-bearing implementation independently corroborates the ABI and field meaning.

## Retail 3.60

Image SHA-256:

```text
4a4c969d4093c0b6647b7009da2e420b882ad302bdb8e2bfb9a102f84d8958b3
```

The wrapper at `0x024F2EE0` zeroes a local 128-byte object, calls the provider, and copies exactly 128 bytes to the caller. Provider `0x024D14D4` rejects a size other than `0x80`, rejects versions above `0x03600011`, and writes the overlay word at offset `0x08` after normalizing it:

```text
BL      sub_24C8D3C
LDR     R3, [R0,#4]
CBZ     R3, loc_24D15BC
MOVS    R3, #1
STR     R3, [R6,#8]
```

The alternative branch loads the corresponding global state, converts any nonzero value to one, and reaches the same store.

## Internal 3.200 tool

Original source SHA-256:

```text
1fee57961a3d6517b161a7a0a96ab74a54767be85308d228f870d0f1a6a00fe1
```

IDA input SHA-256 after changing only ELF `e_type` bytes `0x10..0x11`:

```text
538564d011c126f8c0e7f5359c2fe4b51b85c0c8644d5a3d71e7c55eb7240877
```

The wrapper at `0x810477A4` allocates and zeroes `0x80` bytes, calls thunk `0x81046D50`, then copies `0x80` bytes to the caller. The thunk reaches provider `0x810250C8`. The provider accepts exactly `0x80` bytes and versions through `0x03200010`. Its normalized overlay write is:

```text
0x81025260  BL      sub_81012E30
0x81025264  LDR     R3, [R0,#4]
0x81025266  CBNZ    R3, loc_810252AE
...
0x81025272  ADDS    R3, #0
0x81025274  IT NE
0x81025276  MOVNE   R3, #1
0x81025278  STR     R3, [R6,#8]
```

The `CBNZ` branch writes literal one at `0x810252AE` before joining the same store.

## Internal 4.00 INA

Original source SHA-256:

```text
dda8d336b9986536f6aee9897ba57a81458e86bd8b79ee622f9bb6bd94a17337
```

IDA input SHA-256 after changing only ELF `e_type` bytes `0x10..0x11`:

```text
ded87541e0ff8098ee73263e2e79e4106144d6fef25ba40bcda97fbf491a70ea
```

The wrapper at `0x81032EC8` allocates and zeroes `0x80` bytes, calls thunk `0x8103234C`, then copies `0x80` bytes to the caller. The thunk reaches provider `0x810114D4`. The provider accepts exactly `0x80` bytes and ABI versions through `0x03650000`. Its normalized overlay write is:

```text
0x81011596  BL      sub_81008D3C
0x8101159A  LDR     R3, [R0,#4]
0x8101159C  CBZ     R3, loc_810115BC
0x8101159E  MOVS    R3, #1
0x810115A0  STR     R3, [R6,#8]
```

The zero branch loads the corresponding global state, normalizes nonzero to one at `0x810115C6..0x810115CA`, and joins the store at `0x810115A0`.

## Runtime policy

The plugin must fail closed when `_sceAppMgrGetAppState` fails. On success, Search may be exposed only when all of the following are true:

- no foreground application owns the shell surface;
- `is_system_ui_overlaid == 0`;
- no explicit post-launch suppression window is active.

PAF-wide visibility scans of `impose_plugin` and `impose_net_plugin` are not valid substitutes. Firmware/runtime probing showed those plugins can retain visible pages while their interactive quick-menu surface is absent. The AppMgr word is the bounded ownership signal that changes during the PS quick menu even while main-thread PAF callbacks are paused.
