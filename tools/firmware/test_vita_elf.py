from __future__ import annotations

import struct
import unittest

from vita_elf import FormatError, parse_module_elf


ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIIIIIII")
MODULE_INFO = struct.Struct("<HH27sB15I")
EXPORT = struct.Struct("<BBHHHHHBBBBIIII")
IMPORT_SHORT = struct.Struct("<HHHHHH6I")
IMPORT_LONG = struct.Struct("<HHHHHH10I")


def _build_fixture(
    *,
    encoded_pointers: bool = False,
    duplicate_module: bool = False,
    shifted_program_offsets: bool = False,
    import_format: str | None = None,
) -> bytes:
    segment0 = bytearray(0x400)
    segment1 = bytearray(0x400)
    base0 = 0x81000000
    base1 = 0x81100000

    def pointer(segment: int, offset: int) -> int:
        if encoded_pointers:
            return (segment << 30) | offset
        return (base0 if segment == 0 else base1) + offset

    module_offset = 0x80
    export_offset = 0x100
    library_name_offset = 0x180
    nid_offset = 0x1C0
    entry_offset = 0x1D0
    export_end = export_offset + EXPORT.size
    import_offset = 0x220
    import_name_offset = 0x280
    import_function_nid_offset = 0x2A0
    import_function_entry_offset = 0x2B0
    import_variable_nid_offset = 0x2C0
    import_variable_entry_offset = 0x2D0
    import_tls_nid_offset = 0x2E0
    import_tls_entry_offset = 0x2F0

    segment1[library_name_offset:library_name_offset + 12] = b"TestLibrary\0"
    struct.pack_into("<2I", segment1, nid_offset, 0x11111111, 0x22222222)
    struct.pack_into("<2I", segment1, entry_offset, base0 + 0x20, base0 + 0x24)
    EXPORT.pack_into(
        segment1,
        export_offset,
        EXPORT.size,
        0,
        1,
        0,
        2,
        0,
        0,
        0,
        0,
        0,
        0,
        0x12345678,
        pointer(1, library_name_offset),
        pointer(1, nid_offset),
        pointer(1, entry_offset),
    )
    import_end = import_offset
    if import_format is not None:
        segment1[import_name_offset:import_name_offset + 12] = b"TestImport\0\0"
        struct.pack_into(
            "<2I",
            segment1,
            import_function_nid_offset,
            0x33333333,
            0x44444444,
        )
        struct.pack_into(
            "<2I",
            segment1,
            import_function_entry_offset,
            base0 + 0x28,
            base0 + 0x2C,
        )
        struct.pack_into("<I", segment1, import_variable_nid_offset, 0x55555555)
        struct.pack_into("<I", segment1, import_variable_entry_offset, base0 + 0x30)
        if import_format == "short":
            IMPORT_SHORT.pack_into(
                segment1,
                import_offset,
                IMPORT_SHORT.size,
                1,
                0,
                2,
                1,
                0,
                0x87654321,
                pointer(1, import_name_offset),
                pointer(1, import_function_nid_offset),
                pointer(1, import_function_entry_offset),
                pointer(1, import_variable_nid_offset),
                pointer(1, import_variable_entry_offset),
            )
            import_end += IMPORT_SHORT.size
        elif import_format == "long":
            struct.pack_into("<I", segment1, import_tls_nid_offset, 0x66666666)
            struct.pack_into("<I", segment1, import_tls_entry_offset, base0 + 0x34)
            IMPORT_LONG.pack_into(
                segment1,
                import_offset,
                IMPORT_LONG.size,
                1,
                0,
                2,
                1,
                1,
                0,
                0x87654321,
                pointer(1, import_name_offset),
                0,
                pointer(1, import_function_nid_offset),
                pointer(1, import_function_entry_offset),
                pointer(1, import_variable_nid_offset),
                pointer(1, import_variable_entry_offset),
                pointer(1, import_tls_nid_offset),
                pointer(1, import_tls_entry_offset),
            )
            import_end += IMPORT_LONG.size
        else:
            raise ValueError(f"unsupported import fixture format: {import_format}")
    module_values = (
        0,
        0x0101,
        b"TestModule".ljust(27, b"\0"),
        6,
        0,
        pointer(1, export_offset),
        pointer(1, export_end),
        pointer(1, import_offset),
        pointer(1, import_end),
        0xAABBCCDD,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    )
    MODULE_INFO.pack_into(segment0, module_offset, *module_values)
    if duplicate_module:
        MODULE_INFO.pack_into(segment0, module_offset + 0x100, *module_values)

    program_count = 3 if shifted_program_offsets else 2
    if shifted_program_offsets:
        data_offset0 = (ELF_HEADER.size + program_count * PROGRAM_HEADER.size + 7) & ~7
        data_offset1 = data_offset0 + len(segment0)
    else:
        data_offset0 = 0x1000
        data_offset1 = 0x2000
    image = bytearray(data_offset1 + len(segment1))
    ident = b"\x7fELF" + bytes((1, 1, 1, 0)) + bytes(8)
    image[:ELF_HEADER.size] = ELF_HEADER.pack(
        ident,
        0xFE04,
        40,
        1,
        module_offset,
        ELF_HEADER.size,
        0,
        0x05000400,
        ELF_HEADER.size,
        PROGRAM_HEADER.size,
        program_count,
        0,
        0,
        0,
    )
    if shifted_program_offsets:
        headers = (
            (1, 0, base0, base0, len(segment0), len(segment0), 5, 0x10),
            (1, data_offset0, base1, base1, len(segment1), len(segment1), 6, 0x10),
            (0x60000000, data_offset1, 0, 0, 0, 0, 0, 0x10),
        )
    else:
        headers = (
            (1, data_offset0, base0, base0, len(segment0), len(segment0), 5, 0x1000),
            (1, data_offset1, base1, base1, len(segment1), len(segment1), 6, 0x1000),
        )
    for index, values in enumerate(headers):
        PROGRAM_HEADER.pack_into(image, ELF_HEADER.size + index * PROGRAM_HEADER.size, *values)
    image[data_offset0:data_offset0 + len(segment0)] = segment0
    image[data_offset1:data_offset1 + len(segment1)] = segment1
    return bytes(image)


class VitaElfTests(unittest.TestCase):
    def test_parses_module_and_export_library(self) -> None:
        module = parse_module_elf(_build_fixture(), expected_name="TestModule")

        self.assertEqual(module.name, "TestModule")
        self.assertEqual(module.module_nid, 0xAABBCCDD)
        self.assertEqual(len(module.exports), 1)
        library = module.exports[0]
        self.assertEqual(library.name, "TestLibrary")
        self.assertEqual(library.nid, 0x12345678)
        self.assertEqual(library.function_nids, (0x11111111, 0x22222222))

    def test_parses_segment_encoded_pointers(self) -> None:
        module = parse_module_elf(
            _build_fixture(encoded_pointers=True),
            expected_name="TestModule",
        )

        self.assertEqual(module.exports[0].function_nids, (0x11111111, 0x22222222))

    def test_normalizes_shifted_program_offsets(self) -> None:
        module = parse_module_elf(
            _build_fixture(shifted_program_offsets=True),
            expected_name="TestModule",
        )

        self.assertEqual(module.info_offset, 0x80)
        self.assertEqual(module.exports[0].name, "TestLibrary")
        self.assertEqual(module.exports[0].function_nids, (0x11111111, 0x22222222))

    def test_parses_short_import_library(self) -> None:
        module = parse_module_elf(
            _build_fixture(import_format="short"),
            expected_name="TestModule",
        )

        self.assertEqual(len(module.imports), 1)
        library = module.imports[0]
        self.assertEqual(library.name, "TestImport")
        self.assertEqual(library.nid, 0x87654321)
        self.assertEqual(library.function_nids, (0x33333333, 0x44444444))
        self.assertEqual(library.variable_nids, (0x55555555,))
        self.assertEqual(library.tls_variable_nids, ())

    def test_parses_long_import_library(self) -> None:
        module = parse_module_elf(
            _build_fixture(import_format="long"),
            expected_name="TestModule",
        )

        library = module.imports[0]
        self.assertEqual(library.function_nids, (0x33333333, 0x44444444))
        self.assertEqual(library.variable_nids, (0x55555555,))
        self.assertEqual(library.tls_variable_nids, (0x66666666,))

    def test_rejects_ambiguous_module_info(self) -> None:
        with self.assertRaisesRegex(FormatError, "expected one module-info candidate"):
            parse_module_elf(
                _build_fixture(duplicate_module=True),
                expected_name="TestModule",
            )

    def test_rejects_truncated_elf(self) -> None:
        with self.assertRaisesRegex(FormatError, "program segment 1 is outside"):
            parse_module_elf(_build_fixture()[:-32], expected_name="TestModule")


if __name__ == "__main__":
    unittest.main()
