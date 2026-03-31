#!/usr/bin/env python3
"""IDL codegen tool for game-gym engine.

Parses C++ headers with libclang, finds structs annotated with
``GG_SCRIPTABLE`` and enums annotated with ``GG_SCRIPTABLE_ENUM``,
and generates:

  - generated/script_types_gen.h   -- C++ <-> JSON conversion helpers
  - generated/engine_gen.d.ts      -- TypeScript type definitions
  - generated/codegen_bindings_check.h -- link-time guard declarations

Usage:
    python scripts/codegen.py [--check]
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import subprocess
import sys
import textwrap
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class FieldInfo:
    """Metadata for a single struct field."""
    name: str
    cpp_type: str
    is_struct: bool
    is_enum: bool


@dataclass(frozen=True)
class StructInfo:
    """Metadata for a GG_SCRIPTABLE struct."""
    name: str
    fields: Sequence[FieldInfo]


@dataclass(frozen=True)
class EnumInfo:
    """Metadata for a GG_SCRIPTABLE_ENUM enum class."""
    name: str
    values: Sequence[str]


# ---------------------------------------------------------------------------
# Pure helper: naming conversion
# ---------------------------------------------------------------------------

def snake_to_camel(name: str) -> str:
    """Convert a ``snake_case`` identifier to ``camelCase``.

    >>> snake_to_camel("body_id_a")
    'bodyIdA'
    """
    parts = name.split("_")
    return parts[0] + "".join(p.capitalize() for p in parts[1:])


def struct_name_to_snake(name: str) -> str:
    """Convert ``CamelCase`` struct name to ``snake_case``.

    >>> struct_name_to_snake("ContactEvent")
    'contact_event'
    """
    s1 = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s1).lower()


# ---------------------------------------------------------------------------
# Pure helper: type mapping
# ---------------------------------------------------------------------------

_CPP_TO_TS: dict[str, str] = {
    "float": "number",
    "double": "number",
    "int": "number",
    "int32_t": "number",
    "uint8_t": "number",
    "uint32_t": "number",
    "bool": "boolean",
    "std::string": "string",
}

_CPP_DEFAULT: dict[str, str] = {
    "float": "0.0f",
    "double": "0.0",
    "int": "0",
    "int32_t": "0",
    "uint8_t": "0u",
    "uint32_t": "0u",
    "bool": "false",
    "std::string": '""',
}

# Per-struct field default overrides: (struct_name, field_name) -> default_value_str
_FIELD_DEFAULT_OVERRIDES: dict[tuple[str, str], str] = {
    ("Quat", "w"): "1.0f",
}


def cpp_type_to_ts(cpp_type: str) -> str:
    """Map a C++ primitive type to its TypeScript equivalent.

    Raises ``ValueError`` for unsupported types.
    """
    ts = _CPP_TO_TS.get(cpp_type)
    if ts is None:
        raise ValueError(f"Unsupported C++ type for TS mapping: {cpp_type}")
    return ts


# ---------------------------------------------------------------------------
# Code generators: C++ JSON helpers
# ---------------------------------------------------------------------------

def _json_field_expr(f: FieldInfo, var: str = "v") -> str:
    """Return the JSON initialiser expression for one field."""
    camel = snake_to_camel(f.name)
    if f.is_struct:
        return f'{{"{camel}", to_json({var}.{f.name})}}'
    if f.is_enum:
        enum_snake = struct_name_to_snake(f.cpp_type)
        return f'{{"{camel}", {enum_snake}_to_string({var}.{f.name})}}'
    return f'{{"{camel}", {var}.{f.name}}}'


def generate_to_json(s: StructInfo) -> str:
    """Generate an ``inline nlohmann::json to_json(...)`` function."""
    pairs = ",\n        ".join(_json_field_expr(f) for f in s.fields)
    lines = [
        f"inline nlohmann::json to_json(const {s.name}& v) {{",
        f"    return {{",
        f"        {pairs},",
        f"    }};",
        f"}}",
        "",
    ]
    return "\n".join(lines)


def _from_json_field_line(f: FieldInfo, struct_name: str = "") -> str:
    """Return the `.value(...)` or nested-from-json expression for *from_json*."""
    camel = snake_to_camel(f.name)
    if f.is_struct:
        snake = struct_name_to_snake(f.cpp_type)
        return (
            f'.{f.name} = {snake}_from_json('
            f'j.value("{camel}", nlohmann::json::object())),'
        )
    if f.is_enum:
        enum_snake = struct_name_to_snake(f.cpp_type)
        return (
            f'.{f.name} = {enum_snake}_from_string('
            f'j.value("{camel}", "")),'
        )
    default = _FIELD_DEFAULT_OVERRIDES.get(
        (struct_name, f.name),
        _CPP_DEFAULT.get(f.cpp_type, "0"),
    )
    return f'.{f.name} = j.value("{camel}", {default}),'


def generate_from_json(s: StructInfo) -> str:
    """Generate an ``inline <Type> <type>_from_json(...)`` function."""
    fn_name = struct_name_to_snake(s.name) + "_from_json"
    field_lines = "\n".join(
        f"        {_from_json_field_line(f, s.name)}" for f in s.fields
    )
    lines = [
        f"inline {s.name} {fn_name}(const nlohmann::json& j) {{",
        f"    if (!j.is_object()) {{",
        f"        return {{}};",
        f"    }}",
        f"    return {{",
        field_lines,
        f"    }};",
        f"}}",
        "",
    ]
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Code generators: C++ enum string helpers
# ---------------------------------------------------------------------------

def generate_enum_to_string(e: EnumInfo) -> str:
    """Generate ``<enum_snake>_to_string(EnumType)`` function."""
    fn = struct_name_to_snake(e.name) + "_to_string"
    case_lines = []
    for v in e.values:
        case_lines.append(f"    case {e.name}::{v}:")
        case_lines.append(f'        return "{v[0].lower() + v[1:]}";')
    default_val = e.values[0][0].lower() + e.values[0][1:] if e.values else ""
    case_lines.append(f"    default:")
    case_lines.append(f'        return "{default_val}";')
    cases_str = "\n".join(case_lines)
    lines = [
        f"inline std::string {fn}({e.name} mt) {{",
        f"    switch (mt) {{",
        cases_str,
        f"    }}",
        f"}}",
        "",
    ]
    return "\n".join(lines)


def generate_enum_from_string(e: EnumInfo) -> str:
    """Generate ``<enum_snake>_from_string(string)`` function."""
    fn = struct_name_to_snake(e.name) + "_from_string"
    check_lines = []
    for v in e.values:
        check_lines.append(f'    if (s == "{v[0].lower() + v[1:]}")')
        check_lines.append(f"        return {e.name}::{v};")
    default_val = f"{e.name}::{e.values[0]}" if e.values else ""
    checks_str = "\n".join(check_lines)
    lines = [
        f"inline {e.name} {fn}(const std::string& s) {{",
        checks_str,
        f"    return {default_val};",
        f"}}",
        "",
    ]
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Code generators: TypeScript
# ---------------------------------------------------------------------------

def _ts_field_type(f: FieldInfo) -> str:
    """Return the TypeScript type string for a field."""
    if f.is_struct or f.is_enum:
        return f.cpp_type
    return cpp_type_to_ts(f.cpp_type)


def generate_ts_interface(s: StructInfo) -> str:
    """Generate a TypeScript ``interface`` definition."""
    lines = []
    for f in s.fields:
        camel = snake_to_camel(f.name)
        ts_type = _ts_field_type(f)
        lines.append(f"    {camel}: {ts_type};")
    body = "\n".join(lines)
    return f"interface {s.name} {{\n{body}\n}}\n"


def generate_ts_enum(e: EnumInfo) -> str:
    """Generate a TypeScript string literal union type."""
    literals = " | ".join(
        f'"{v[0].lower() + v[1:]}"' for v in e.values
    )
    return f"type {e.name} = {literals};\n"


# ---------------------------------------------------------------------------
# Code generator: bindings check
# ---------------------------------------------------------------------------

_AUTO_HEADER = (
    "// AUTO-GENERATED by scripts/codegen.py -- DO NOT EDIT\n"
)


def generate_bindings_check(structs: Sequence[StructInfo]) -> str:
    """Generate ``codegen_bindings_check.h`` content."""
    decls = "\n".join(
        f"void assert_bound_{s.name}();" for s in structs
    )
    calls = "\n".join(
        f"    assert_bound_{s.name}();" for s in structs
    )
    lines = [
        _AUTO_HEADER.rstrip(),
        "#pragma once",
        "",
        "namespace gg::codegen {",
        "",
        decls,
        "",
        "inline void check_all_bindings() {",
        calls,
        "}",
        "",
        "} // namespace gg::codegen",
        "",
    ]
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Full file generators
# ---------------------------------------------------------------------------

def generate_script_types_gen_h(
    structs: Sequence[StructInfo],
    enums: Sequence[EnumInfo],
) -> str:
    """Generate the full ``script_types_gen.h`` file content."""
    parts: list[str] = [
        _AUTO_HEADER,
        "#pragma once\n",
        '#include "ecs/components.h"',
        '#include "physics/physics_components.h"',
        "",
        "#include <nlohmann/json.hpp>",
        "#include <string>",
        "",
        "namespace gg::script_types {",
        "",
    ]

    # Enum helpers first (structs may reference them).
    for e in enums:
        parts.append(f"// ---- {e.name} helpers " + "-" * (50 - len(e.name)))
        parts.append("")
        parts.append(generate_enum_to_string(e))
        parts.append(generate_enum_from_string(e))

    for s in structs:
        parts.append(f"// ---- {s.name} " + "-" * (60 - len(s.name)))
        parts.append("")
        parts.append(generate_to_json(s))
        parts.append(generate_from_json(s))

    parts.append("} // namespace gg::script_types")
    parts.append("")
    return "\n".join(parts)


def generate_engine_gen_dts(
    structs: Sequence[StructInfo],
    enums: Sequence[EnumInfo],
) -> str:
    """Generate the full ``engine_gen.d.ts`` file content."""
    parts: list[str] = [
        _AUTO_HEADER,
        "",
    ]
    for e in enums:
        parts.append(generate_ts_enum(e))
    for s in structs:
        parts.append(generate_ts_interface(s))
    return "\n".join(parts)


# ---------------------------------------------------------------------------
# libclang parsing
# ---------------------------------------------------------------------------

def _detect_system_includes() -> list[str]:
    """Detect C++ system include paths by querying the host compiler.

    Returns a list of ``-isystem`` / ``-isysroot`` flags suitable for
    passing to libclang so it can resolve ``<string>``, ``<cstdint>``, etc.
    """
    flags: list[str] = []
    try:
        result = subprocess.run(
            ["clang++", "-E", "-x", "c++", "-std=c++17", "-v", "/dev/null"],
            capture_output=True,
            text=True,
        )
        output = result.stderr
        in_search = False
        for line in output.splitlines():
            if "#include <...> search starts here:" in line:
                in_search = True
                continue
            if "End of search list." in line:
                break
            if in_search:
                path = line.strip()
                # Skip framework directories
                if "(framework directory)" in path:
                    continue
                if os.path.isdir(path):
                    flags.extend(["-isystem", path])
    except FileNotFoundError:
        # clang++ not available; try xcrun on macOS
        try:
            sdk = subprocess.check_output(
                ["xcrun", "--show-sdk-path"],
                text=True,
            ).strip()
            if sdk:
                flags.extend(["-isysroot", sdk])
        except (FileNotFoundError, subprocess.CalledProcessError):
            pass
    return flags


def _parse_headers(
    header_paths: Sequence[str | Path],
    project_root: Path,
) -> tuple[list[StructInfo], list[EnumInfo]]:
    """Parse *header_paths* with libclang and return scriptable metadata.

    Redefines ``GG_SCRIPTABLE`` / ``GG_SCRIPTABLE_ENUM`` as annotate
    attributes so libclang records them in the AST.
    """
    from clang.cindex import Index, CursorKind, Config as ClangConfig  # type: ignore[import-untyped]

    index = Index.create()
    sys_flags = _detect_system_includes()

    # Build a set of all scriptable struct / enum names across all headers
    # first pass: identify annotated types.
    scriptable_struct_names: set[str] = set()
    scriptable_enum_names: set[str] = set()
    structs: list[StructInfo] = []
    enums: list[EnumInfo] = []

    # Shadow scriptable.h: the real header defines GG_SCRIPTABLE as empty.
    # We place a shadow version earlier on the include path that defines
    # the macros as __attribute__((annotate("..."))) so libclang can
    # detect annotated types in the AST.
    override_dir = project_root / "generated"
    override_dir.mkdir(parents=True, exist_ok=True)

    for header in header_paths:
        shadow_dir = override_dir / "_codegen_shadow" / "script"
        shadow_dir.mkdir(parents=True, exist_ok=True)
        shadow_path = shadow_dir / "scriptable.h"
        shadow_content = textwrap.dedent("""\
            #pragma once
            #define GG_SCRIPTABLE __attribute__((annotate("scriptable")))
            #define GG_SCRIPTABLE_ENUM __attribute__((annotate("scriptable_enum")))
        """)
        shadow_path.write_text(shadow_content)

        tu = index.parse(
            str(header),
            args=[
                "-x", "c++",
                "-std=c++17",
                f"-I{override_dir / '_codegen_shadow'}",
                f"-I{project_root / 'engine'}",
                *sys_flags,
            ],
            options=0x01,  # CXTranslationUnit_DetailedPreprocessingRecord
        )

        # Report parse diagnostics as warnings.
        for diag in tu.diagnostics:
            severity = diag.severity
            if severity >= 3:  # Error
                print(f"[codegen] parse warning: {diag}", file=sys.stderr)

        _walk_tu(tu.cursor, scriptable_struct_names, scriptable_enum_names, structs, enums)

    # Deduplicate: a struct from components.h may appear when parsing both
    # components.h directly and physics_components.h (which includes it).
    seen_struct_names: set[str] = set()
    deduped_structs: list[StructInfo] = []
    for s in structs:
        if s.name not in seen_struct_names:
            seen_struct_names.add(s.name)
            deduped_structs.append(s)
    structs = deduped_structs

    seen_enum_names: set[str] = set()
    deduped_enums: list[EnumInfo] = []
    for e in enums:
        if e.name not in seen_enum_names:
            seen_enum_names.add(e.name)
            deduped_enums.append(e)
    enums = deduped_enums

    # Second pass: resolve field types (mark is_struct / is_enum).
    resolved_structs: list[StructInfo] = []
    for s in structs:
        resolved_fields: list[FieldInfo] = []
        for f in s.fields:
            is_struct = f.cpp_type in scriptable_struct_names
            is_enum = f.cpp_type in scriptable_enum_names

            # Validate: if not primitive, must be scriptable struct or enum.
            if not is_struct and not is_enum and f.cpp_type not in _CPP_TO_TS:
                raise ValueError(
                    f"Unsupported field type '{f.cpp_type}' in struct "
                    f"'{s.name}' field '{f.name}'. "
                    f"Type must be a primitive, GG_SCRIPTABLE struct, or "
                    f"GG_SCRIPTABLE_ENUM."
                )

            resolved_fields.append(
                FieldInfo(
                    name=f.name,
                    cpp_type=f.cpp_type,
                    is_struct=is_struct,
                    is_enum=is_enum,
                )
            )
        resolved_structs.append(StructInfo(name=s.name, fields=resolved_fields))

    return resolved_structs, enums


def _walk_tu(
    cursor,
    struct_names: set[str],
    enum_names: set[str],
    structs: list[StructInfo],
    enums: list[EnumInfo],
):
    """Recursively walk the AST looking for annotated types."""
    from clang.cindex import CursorKind  # type: ignore[import-untyped]

    for child in cursor.get_children():
        if child.kind == CursorKind.NAMESPACE:
            _walk_tu(child, struct_names, enum_names, structs, enums)
            continue

        # --- Struct / class with scriptable annotation ---
        if child.kind in (CursorKind.STRUCT_DECL, CursorKind.CLASS_DECL):
            if _has_annotation(child, "scriptable"):
                name = child.spelling
                struct_names.add(name)
                fields = _extract_fields(child)
                structs.append(StructInfo(name=name, fields=fields))
            continue

        # --- Enum class with scriptable_enum annotation ---
        if child.kind == CursorKind.ENUM_DECL:
            if _has_annotation(child, "scriptable_enum"):
                name = child.spelling
                enum_names.add(name)
                values = _extract_enum_values(child)
                enums.append(EnumInfo(name=name, values=values))
            continue


def _has_annotation(cursor, annotation_text: str) -> bool:
    """Check whether *cursor* carries an ``annotate("...")`` attribute."""
    from clang.cindex import CursorKind  # type: ignore[import-untyped]

    for child in cursor.get_children():
        if child.kind == CursorKind.ANNOTATE_ATTR and child.spelling == annotation_text:
            return True
    return False


def _extract_fields(struct_cursor) -> list[FieldInfo]:
    """Extract field declarations from a struct cursor."""
    from clang.cindex import CursorKind  # type: ignore[import-untyped]

    fields: list[FieldInfo] = []
    for child in struct_cursor.get_children():
        if child.kind == CursorKind.FIELD_DECL:
            cpp_type = _resolve_type(child)
            fields.append(
                FieldInfo(
                    name=child.spelling,
                    cpp_type=cpp_type,
                    is_struct=False,  # resolved later
                    is_enum=False,    # resolved later
                )
            )
    return fields


def _resolve_type(field_cursor) -> str:
    """Return a clean C++ type name for a field cursor.

    Strips qualifiers, collapses elaborated type names, and maps
    ``gg::Vec3`` back to ``Vec3``.
    """
    t = field_cursor.type
    type_str = t.spelling

    # Strip ``gg::`` namespace prefix.
    type_str = re.sub(r"\bgg::", "", type_str)

    # Strip ``const`` / ``volatile``.
    type_str = re.sub(r"\b(const|volatile)\s+", "", type_str)

    # Strip ``enum `` / ``struct `` elaborated prefixes.
    type_str = re.sub(r"\b(enum|struct|class)\s+", "", type_str)

    return type_str.strip()


def _extract_enum_values(enum_cursor) -> list[str]:
    """Extract enumerator names from an enum cursor."""
    from clang.cindex import CursorKind  # type: ignore[import-untyped]

    values: list[str] = []
    for child in enum_cursor.get_children():
        if child.kind == CursorKind.ENUM_CONSTANT_DECL:
            values.append(child.spelling)
    return values


# ---------------------------------------------------------------------------
# File writing / --check
# ---------------------------------------------------------------------------

def _file_hash(path: Path) -> str:
    """SHA-256 hex digest of a file, or empty string if missing."""
    if not path.exists():
        return ""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _write_if_changed(path: Path, content: str) -> bool:
    """Write *content* to *path* only when content differs.

    Returns ``True`` if the file was written (i.e. content changed).
    """
    new_hash = hashlib.sha256(content.encode()).hexdigest()
    if path.exists() and _file_hash(path) == new_hash:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    return True


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="game-gym IDL codegen")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify generated files are up to date (CI mode).",
    )
    args = parser.parse_args(argv)

    project_root = Path(__file__).resolve().parent.parent
    headers = [
        project_root / "engine" / "ecs" / "components.h",
        project_root / "engine" / "physics" / "physics_components.h",
    ]
    gen_dir = project_root / "generated"

    structs, enums = _parse_headers(headers, project_root)

    cpp_content = generate_script_types_gen_h(structs, enums)
    dts_content = generate_engine_gen_dts(structs, enums)
    bindings_content = generate_bindings_check(structs)

    files = {
        gen_dir / "script_types_gen.h": cpp_content,
        gen_dir / "engine_gen.d.ts": dts_content,
        gen_dir / "codegen_bindings_check.h": bindings_content,
    }

    if args.check:
        stale: list[str] = []
        for path, content in files.items():
            new_hash = hashlib.sha256(content.encode()).hexdigest()
            if not path.exists() or _file_hash(path) != new_hash:
                stale.append(str(path.relative_to(project_root)))
        if stale:
            print(
                "[codegen] ERROR: generated files are out of date:\n  "
                + "\n  ".join(stale),
                file=sys.stderr,
            )
            print("[codegen] Run: python scripts/codegen.py", file=sys.stderr)
            return 1
        print("[codegen] All generated files are up to date.")
        return 0

    changed = False
    for path, content in files.items():
        if _write_if_changed(path, content):
            rel = path.relative_to(project_root)
            print(f"[codegen] wrote {rel}")
            changed = True

    if not changed:
        print("[codegen] All generated files already up to date.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
