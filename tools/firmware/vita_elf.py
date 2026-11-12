from __future__ import annotations

from dataclasses import dataclass
import struct
from typing import Sequence


ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIIIIIII")
MODULE_INFO = struct.Struct("<HH27sB15I")
EXPORT_COMMON = struct.Struct("<BBHHHHHBBBB")
EXPORT_1C = struct.Struct("<BBHHHHHBBBBIII")
EXPORT_20 = struct.Struct("<BBHHHHHBBBBIIII")
IMPORT_SHORT = struct.Struct("<HHHHHH6I")
IMPORT_LONG = struct.Struct("<HHHHHH10I")

PT_LOAD = 1
EM_ARM = 40
POINTER_OFFSET_MASK = 0x3FFFFFFF
SUPPORTED_MODULE_VERSIONS = frozenset((0x0100, 0x0200))


class FormatError(ValueError):
    pass


@dataclass(frozen=True)
class Segment:
    index: int
    data: bytes
    vaddr: int
    memory_size: int


@dataclass(frozen=True)
class ExportLibrary:
    name: str | None
    nid: int
    function_nids: tuple[int, ...]
    variable_nids: tuple[int, ...]
    tls_variable_nids: tuple[int, ...]


@dataclass(frozen=True)
class ImportLibrary:
    name: str
    nid: int
    function_nids: tuple[int, ...]
    variable_nids: tuple[int, ...]
    tls_variable_nids: tuple[int, ...]


@dataclass(frozen=True)
class ParsedModule:
    name: str
    module_nid: int
    version: int
    info_segment: int
    info_offset: int
    exports: tuple[ExportLibrary, ...]
    imports: tuple[ImportLibrary, ...] = ()


@dataclass(frozen=True)
class _Location:
    segment: Segment
    offset: int


class _AddressSpace:
    def __init__(self, segments: Sequence[Segment]) -> None:
        if not segments:
            raise FormatError("ELF has no load segments")
        self._segments = {segment.index: segment for segment in segments}
        if len(self._segments) != len(segments):
            raise FormatError("ELF has duplicate load-segment indices")

    @property
    def segments(self) -> tuple[Segment, ...]:
        return tuple(self._segments[index] for index in sorted(self._segments))

    def _validate(self, segment: Segment, offset: int, size: int) -> None:
        if size < 0:
            raise FormatError("negative read size")
        if offset < 0 or offset > len(segment.data) or size > len(segment.data) - offset:
            raise FormatError(
                f"pointer range outside segment {segment.index}: "
                f"offset={offset:#x} size={size:#x} limit={len(segment.data):#x}"
            )

    def resolve(self, pointer: int, *, default_segment: int, size: int = 1) -> _Location:
        if pointer < 0 or pointer > 0xFFFFFFFF:
            raise FormatError(f"pointer outside 32-bit range: {pointer}")
        for segment in self.segments:
            limit = len(segment.data)
            if segment.vaddr <= pointer <= segment.vaddr + limit:
                offset = pointer - segment.vaddr
                self._validate(segment, offset, size)
                return _Location(segment, offset)
        encoded_segment = pointer >> 30
        encoded_offset = pointer & POINTER_OFFSET_MASK
        if encoded_segment != 0 and encoded_segment in self._segments:
            segment = self._segments[encoded_segment]
            self._validate(segment, encoded_offset, size)
            return _Location(segment, encoded_offset)
        try:
            segment = self._segments[default_segment]
        except KeyError as error:
            raise FormatError(f"pointer references absent segment {default_segment}") from error
        self._validate(segment, pointer, size)
        return _Location(segment, pointer)

    def read(self, pointer: int, size: int, *, default_segment: int) -> bytes:
        location = self.resolve(pointer, default_segment=default_segment, size=size)
        return location.segment.data[location.offset:location.offset + size]

    def read_u32_array(
        self,
        pointer: int,
        count: int,
        *,
        default_segment: int,
    ) -> tuple[int, ...]:
        if count < 0 or count > 0x10000:
            raise FormatError(f"unreasonable array count: {count}")
        if count == 0:
            return ()
        payload = self.read(pointer, count * 4, default_segment=default_segment)
        return struct.unpack(f"<{count}I", payload)

    def read_c_string(
        self,
        pointer: int,
        *,
        default_segment: int,
        max_length: int = 0x1000,
    ) -> str:
        location = self.resolve(pointer, default_segment=default_segment)
        payload = location.segment.data[location.offset:location.offset + max_length]
        terminator = payload.find(b"\0")
        if terminator < 0:
            raise FormatError(
                f"unterminated string at segment {location.segment.index}:{location.offset:#x}"
            )
        try:
            return payload[:terminator].decode("ascii")
        except UnicodeDecodeError as error:
            raise FormatError("module string is not ASCII") from error


def _decode_name(raw: bytes) -> str:
    payload = raw.split(b"\0", 1)[0]
    try:
        name = payload.decode("ascii")
    except UnicodeDecodeError as error:
        raise FormatError("module name is not ASCII") from error
    if not name or any(not 0x20 <= ord(character) <= 0x7E for character in name):
        raise FormatError("module name is not printable ASCII")
    return name


def _parse_segments(image: bytes) -> tuple[Segment, ...]:
    if len(image) < ELF_HEADER.size:
        raise FormatError("ELF image is shorter than its header")
    values = ELF_HEADER.unpack_from(image)
    ident = values[0]
    if ident[:4] != b"\x7fELF":
        raise FormatError("invalid ELF magic")
    if ident[4:7] != bytes((1, 1, 1)):
        raise FormatError("unsupported ELF class, byte order, or version")
    if values[2] != EM_ARM:
        raise FormatError(f"unsupported ELF machine: {values[2]}")
    program_offset = values[5]
    header_size = values[8]
    program_entry_size = values[9]
    program_count = values[10]
    if header_size != ELF_HEADER.size:
        raise FormatError(f"unsupported ELF header size: {header_size}")
    if program_count and program_entry_size != PROGRAM_HEADER.size:
        raise FormatError(f"unsupported program-header size: {program_entry_size}")
    table_size = program_count * program_entry_size
    if program_offset > len(image) or table_size > len(image) - program_offset:
        raise FormatError("program-header table is outside the ELF image")

    programs: list[tuple[int, ...]] = []
    for program_index in range(program_count):
        offset = program_offset + program_index * program_entry_size
        programs.append(PROGRAM_HEADER.unpack_from(image, offset))

    shifted_offsets = (
        len(programs) > 1
        and programs[0][0] == PT_LOAD
        and programs[0][1] == 0
        and programs[1][1] >= program_offset + table_size
        and programs[1][1] + programs[0][4] <= len(image)
    )

    segments: list[Segment] = []
    for program_index, item in enumerate(programs):
        segment_type, declared_offset, vaddr, _paddr, file_size, memory_size, _flags, _align = item
        if shifted_offsets:
            if program_index + 1 >= len(programs):
                if file_size != 0:
                    raise FormatError(
                        "shifted program-offset layout ends with a nonempty segment"
                    )
                file_offset = declared_offset
            else:
                file_offset = programs[program_index + 1][1]
        else:
            file_offset = declared_offset
        if file_offset > len(image) or file_size > len(image) - file_offset:
            raise FormatError(f"program segment {program_index} is outside the ELF image")
        if segment_type != PT_LOAD:
            continue
        if memory_size < file_size:
            raise FormatError(
                f"load segment {program_index} memory size is smaller than file size"
            )
        index = len(segments)
        if index > 3:
            raise FormatError("ELF has more than four encoded load segments")
        segments.append(
            Segment(
                index=index,
                data=image[file_offset:file_offset + file_size],
                vaddr=vaddr,
                memory_size=memory_size,
            )
        )
    return tuple(segments)


def _candidate_valid(
    space: _AddressSpace,
    segment: Segment,
    values: tuple[object, ...],
) -> bool:
    try:
        _decode_name(values[2])
        version = int(values[1])
        module_type = int(values[3])
        if version & 0xFF00 not in SUPPORTED_MODULE_VERSIONS or module_type not in (0, 6):
            return False
        export_top, export_end, import_top, import_end = (int(value) for value in values[5:9])
        export_start = space.resolve(export_top, default_segment=segment.index, size=0)
        export_finish = space.resolve(export_end, default_segment=segment.index, size=0)
        import_start = space.resolve(import_top, default_segment=segment.index, size=0)
        import_finish = space.resolve(import_end, default_segment=segment.index, size=0)
        return (
            export_start.segment.index == export_finish.segment.index
            and import_start.segment.index == import_finish.segment.index
            and export_start.offset <= export_finish.offset
            and import_start.offset <= import_finish.offset
        )
    except (FormatError, TypeError, ValueError):
        return False


def _find_module_info(
    space: _AddressSpace,
    expected_name: str | None,
) -> tuple[Segment, int, tuple[object, ...]]:
    candidates: list[tuple[Segment, int, tuple[object, ...]]] = []
    for segment in space.segments:
        for offset in range(0, len(segment.data) - MODULE_INFO.size + 1, 4):
            values = MODULE_INFO.unpack_from(segment.data, offset)
            if not _candidate_valid(space, segment, values):
                continue
            name = _decode_name(values[2])
            if expected_name is not None and name != expected_name:
                continue
            candidates.append((segment, offset, values))
    if len(candidates) != 1:
        rendered = [
            f"{segment.index}:{offset:#x}:{_decode_name(values[2])}"
            for segment, offset, values in candidates
        ]
        raise FormatError(f"expected one module-info candidate, found {rendered}")
    return candidates[0]


def _split_nids(
    values: tuple[int, ...],
    function_count: int,
    variable_count: int,
    tls_count: int,
) -> tuple[tuple[int, ...], tuple[int, ...], tuple[int, ...]]:
    function_end = function_count
    variable_end = function_end + variable_count
    tls_end = variable_end + tls_count
    if tls_end != len(values):
        raise FormatError("library NID array length does not match declared counts")
    return values[:function_end], values[function_end:variable_end], values[variable_end:]


def _parse_exports(
    space: _AddressSpace,
    *,
    info_segment: int,
    top: int,
    end: int,
) -> tuple[ExportLibrary, ...]:
    start = space.resolve(top, default_segment=info_segment, size=0)
    finish = space.resolve(end, default_segment=info_segment, size=0)
    if start.segment.index != finish.segment.index:
        raise FormatError("export table spans multiple segments")
    segment = start.segment
    offset = start.offset
    libraries: list[ExportLibrary] = []
    while offset < finish.offset:
        if finish.offset - offset < EXPORT_COMMON.size:
            raise FormatError("truncated export table")
        size = segment.data[offset]
        if size == EXPORT_20.size:
            values = EXPORT_20.unpack_from(segment.data, offset)
            function_count, variable_count, tls_count = values[4:7]
            library_nid, name_pointer, nid_pointer = values[11:14]
        elif size == EXPORT_1C.size:
            values = EXPORT_1C.unpack_from(segment.data, offset)
            function_count, variable_count, tls_count = values[4:7]
            library_nid = 0
            name_pointer, nid_pointer = values[11:13]
        else:
            raise FormatError(
                f"unsupported export size {size:#x} at segment {segment.index}:{offset:#x}"
            )
        if size > finish.offset - offset:
            raise FormatError("truncated export record")
        total = function_count + variable_count + tls_count
        nids = space.read_u32_array(
            nid_pointer,
            total,
            default_segment=segment.index,
        )
        function_nids, variable_nids, tls_nids = _split_nids(
            nids,
            function_count,
            variable_count,
            tls_count,
        )
        name = None
        if name_pointer != 0:
            name = space.read_c_string(name_pointer, default_segment=segment.index)
        libraries.append(
            ExportLibrary(
                name=name,
                nid=library_nid,
                function_nids=function_nids,
                variable_nids=variable_nids,
                tls_variable_nids=tls_nids,
            )
        )
        offset += size
    if offset != finish.offset:
        raise FormatError("export table end is misaligned")
    return tuple(libraries)


def _parse_imports(
    space: _AddressSpace,
    *,
    info_segment: int,
    top: int,
    end: int,
) -> tuple[ImportLibrary, ...]:
    start = space.resolve(top, default_segment=info_segment, size=0)
    finish = space.resolve(end, default_segment=info_segment, size=0)
    if start.segment.index != finish.segment.index:
        raise FormatError("import table spans multiple segments")
    segment = start.segment
    offset = start.offset
    libraries: list[ImportLibrary] = []
    while offset < finish.offset:
        if finish.offset - offset < 2:
            raise FormatError("truncated import table")
        size = struct.unpack_from("<H", segment.data, offset)[0]
        if size == IMPORT_SHORT.size:
            if size > finish.offset - offset:
                raise FormatError("truncated short import record")
            values = IMPORT_SHORT.unpack_from(segment.data, offset)
            function_count, variable_count, tls_count = values[3:6]
            if tls_count != 0:
                raise FormatError("short import record declares TLS variables")
            library_nid = values[6]
            name_pointer = values[7]
            function_nid_pointer = values[8]
            function_entry_pointer = values[9]
            variable_nid_pointer = values[10]
            variable_entry_pointer = values[11]
            tls_nid_pointer = 0
            tls_entry_pointer = 0
        elif size == IMPORT_LONG.size:
            if size > finish.offset - offset:
                raise FormatError("truncated long import record")
            values = IMPORT_LONG.unpack_from(segment.data, offset)
            function_count, variable_count, tls_count = values[3:6]
            library_nid = values[7]
            name_pointer = values[8]
            function_nid_pointer = values[10]
            function_entry_pointer = values[11]
            variable_nid_pointer = values[12]
            variable_entry_pointer = values[13]
            tls_nid_pointer = values[14]
            tls_entry_pointer = values[15]
        else:
            raise FormatError(
                f"unsupported import size {size:#x} at segment {segment.index}:{offset:#x}"
            )

        function_nids = space.read_u32_array(
            function_nid_pointer,
            function_count,
            default_segment=segment.index,
        )
        variable_nids = space.read_u32_array(
            variable_nid_pointer,
            variable_count,
            default_segment=segment.index,
        )
        tls_nids = space.read_u32_array(
            tls_nid_pointer,
            tls_count,
            default_segment=segment.index,
        )
        space.read_u32_array(
            function_entry_pointer,
            function_count,
            default_segment=segment.index,
        )
        space.read_u32_array(
            variable_entry_pointer,
            variable_count,
            default_segment=segment.index,
        )
        space.read_u32_array(
            tls_entry_pointer,
            tls_count,
            default_segment=segment.index,
        )
        name = space.read_c_string(name_pointer, default_segment=segment.index)
        libraries.append(
            ImportLibrary(
                name=name,
                nid=library_nid,
                function_nids=function_nids,
                variable_nids=variable_nids,
                tls_variable_nids=tls_nids,
            )
        )
        offset += size
    if offset != finish.offset:
        raise FormatError("import table end is misaligned")
    return tuple(libraries)


def parse_module_elf(
    image: bytes,
    *,
    expected_name: str | None = None,
) -> ParsedModule:
    if not isinstance(image, bytes):
        raise FormatError("ELF image must be bytes")
    space = _AddressSpace(_parse_segments(image))
    info_segment, info_offset, values = _find_module_info(space, expected_name)
    name = _decode_name(values[2])
    version = int(values[1])
    export_top = int(values[5])
    export_end = int(values[6])
    import_top = int(values[7])
    import_end = int(values[8])
    module_nid = int(values[9])
    exports = _parse_exports(
        space,
        info_segment=info_segment.index,
        top=export_top,
        end=export_end,
    )
    imports = _parse_imports(
        space,
        info_segment=info_segment.index,
        top=import_top,
        end=import_end,
    )
    return ParsedModule(
        name=name,
        module_nid=module_nid,
        version=version,
        info_segment=info_segment.index,
        info_offset=info_offset,
        exports=exports,
        imports=imports,
    )
