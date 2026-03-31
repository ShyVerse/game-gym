"""Unit tests for the codegen module (scripts/codegen.py).

Tests the pure-logic functions used to generate C++ JSON converters,
TypeScript type definitions, and link-time binding guards from parsed
struct / enum metadata.
"""

import sys
import os
import textwrap

import pytest

# Add scripts/ to the import path so we can import codegen as a module.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))

from codegen import (
    snake_to_camel,
    cpp_type_to_ts,
    FieldInfo,
    StructInfo,
    EnumInfo,
    generate_to_json,
    generate_from_json,
    generate_ts_interface,
    generate_ts_enum,
    generate_bindings_check,
    generate_enum_to_string,
    generate_enum_from_string,
)


# ---------------------------------------------------------------------------
# snake_to_camel
# ---------------------------------------------------------------------------

class TestSnakeToCamel:
    def test_single_word(self):
        assert snake_to_camel("name") == "name"

    def test_two_words(self):
        assert snake_to_camel("body_id") == "bodyId"

    def test_three_words(self):
        assert snake_to_camel("body_id_a") == "bodyIdA"

    def test_already_camel(self):
        assert snake_to_camel("bodyId") == "bodyId"

    def test_leading_underscore_preserved(self):
        # edge case: leading underscore should not cause empty first segment
        assert snake_to_camel("half_x") == "halfX"

    def test_single_char_segments(self):
        assert snake_to_camel("x") == "x"

    def test_motion_type(self):
        assert snake_to_camel("motion_type") == "motionType"

    def test_is_sensor(self):
        assert snake_to_camel("is_sensor") == "isSensor"

    def test_half_height(self):
        assert snake_to_camel("half_height") == "halfHeight"


# ---------------------------------------------------------------------------
# cpp_type_to_ts
# ---------------------------------------------------------------------------

class TestCppTypeToTs:
    def test_float(self):
        assert cpp_type_to_ts("float") == "number"

    def test_double(self):
        assert cpp_type_to_ts("double") == "number"

    def test_int(self):
        assert cpp_type_to_ts("int") == "number"

    def test_uint32(self):
        assert cpp_type_to_ts("uint32_t") == "number"

    def test_uint8(self):
        assert cpp_type_to_ts("uint8_t") == "number"

    def test_int32(self):
        assert cpp_type_to_ts("int32_t") == "number"

    def test_bool(self):
        assert cpp_type_to_ts("bool") == "boolean"

    def test_string(self):
        assert cpp_type_to_ts("std::string") == "string"

    def test_unsupported_raises(self):
        with pytest.raises(ValueError, match="Unsupported"):
            cpp_type_to_ts("std::vector<int>")


# ---------------------------------------------------------------------------
# generate_to_json
# ---------------------------------------------------------------------------

class TestGenerateToJson:
    def test_vec3(self):
        vec3 = StructInfo(
            name="Vec3",
            fields=[
                FieldInfo(name="x", cpp_type="float", is_struct=False, is_enum=False),
                FieldInfo(name="y", cpp_type="float", is_struct=False, is_enum=False),
                FieldInfo(name="z", cpp_type="float", is_struct=False, is_enum=False),
            ],
        )
        result = generate_to_json(vec3)
        assert "inline nlohmann::json to_json(const Vec3& v)" in result
        assert '{"x", v.x}' in result
        assert '{"y", v.y}' in result
        assert '{"z", v.z}' in result

    def test_transform_nested(self):
        transform = StructInfo(
            name="Transform",
            fields=[
                FieldInfo(name="position", cpp_type="Vec3", is_struct=True, is_enum=False),
                FieldInfo(name="rotation", cpp_type="Quat", is_struct=True, is_enum=False),
                FieldInfo(name="scale", cpp_type="Vec3", is_struct=True, is_enum=False),
            ],
        )
        result = generate_to_json(transform)
        assert "to_json(v.position)" in result
        assert "to_json(v.rotation)" in result
        assert "to_json(v.scale)" in result

    def test_contact_event_with_enum(self):
        ce = StructInfo(
            name="ContactEvent",
            fields=[
                FieldInfo(name="body_id_a", cpp_type="uint32_t", is_struct=False, is_enum=False),
                FieldInfo(name="body_id_b", cpp_type="uint32_t", is_struct=False, is_enum=False),
                FieldInfo(name="type", cpp_type="ContactType", is_struct=False, is_enum=True),
                FieldInfo(name="point", cpp_type="Vec3", is_struct=True, is_enum=False),
                FieldInfo(name="normal", cpp_type="Vec3", is_struct=True, is_enum=False),
            ],
        )
        result = generate_to_json(ce)
        assert "contact_type_to_string(v.type)" in result
        assert "to_json(v.point)" in result


# ---------------------------------------------------------------------------
# generate_from_json
# ---------------------------------------------------------------------------

class TestGenerateFromJson:
    def test_vec3_has_guard(self):
        vec3 = StructInfo(
            name="Vec3",
            fields=[
                FieldInfo(name="x", cpp_type="float", is_struct=False, is_enum=False),
                FieldInfo(name="y", cpp_type="float", is_struct=False, is_enum=False),
                FieldInfo(name="z", cpp_type="float", is_struct=False, is_enum=False),
            ],
        )
        result = generate_from_json(vec3)
        assert "vec3_from_json" in result
        assert "!j.is_object()" in result
        assert 'j.value("x"' in result
        assert 'j.value("y"' in result
        assert 'j.value("z"' in result

    def test_transform_nested_from_json(self):
        transform = StructInfo(
            name="Transform",
            fields=[
                FieldInfo(name="position", cpp_type="Vec3", is_struct=True, is_enum=False),
                FieldInfo(name="rotation", cpp_type="Quat", is_struct=True, is_enum=False),
                FieldInfo(name="scale", cpp_type="Vec3", is_struct=True, is_enum=False),
            ],
        )
        result = generate_from_json(transform)
        assert "transform_from_json" in result
        assert "vec3_from_json" in result
        assert "quat_from_json" in result

    def test_from_json_with_enum_field(self):
        s = StructInfo(
            name="ContactEvent",
            fields=[
                FieldInfo(name="body_id_a", cpp_type="uint32_t", is_struct=False, is_enum=False),
                FieldInfo(name="type", cpp_type="ContactType", is_struct=False, is_enum=True),
            ],
        )
        result = generate_from_json(s)
        assert "contact_type_from_string" in result


# ---------------------------------------------------------------------------
# generate_ts_interface
# ---------------------------------------------------------------------------

class TestGenerateTsInterface:
    def test_vec3(self):
        vec3 = StructInfo(
            name="Vec3",
            fields=[
                FieldInfo(name="x", cpp_type="float", is_struct=False, is_enum=False),
                FieldInfo(name="y", cpp_type="float", is_struct=False, is_enum=False),
                FieldInfo(name="z", cpp_type="float", is_struct=False, is_enum=False),
            ],
        )
        result = generate_ts_interface(vec3)
        assert "interface Vec3" in result
        assert "x: number;" in result
        assert "y: number;" in result
        assert "z: number;" in result

    def test_transform_nested_types(self):
        transform = StructInfo(
            name="Transform",
            fields=[
                FieldInfo(name="position", cpp_type="Vec3", is_struct=True, is_enum=False),
                FieldInfo(name="rotation", cpp_type="Quat", is_struct=True, is_enum=False),
                FieldInfo(name="scale", cpp_type="Vec3", is_struct=True, is_enum=False),
            ],
        )
        result = generate_ts_interface(transform)
        assert "position: Vec3;" in result
        assert "rotation: Quat;" in result
        assert "scale: Vec3;" in result

    def test_snake_case_to_camel(self):
        s = StructInfo(
            name="ContactEvent",
            fields=[
                FieldInfo(name="body_id_a", cpp_type="uint32_t", is_struct=False, is_enum=False),
                FieldInfo(name="body_id_b", cpp_type="uint32_t", is_struct=False, is_enum=False),
                FieldInfo(name="type", cpp_type="ContactType", is_struct=False, is_enum=True),
                FieldInfo(name="point", cpp_type="Vec3", is_struct=True, is_enum=False),
                FieldInfo(name="normal", cpp_type="Vec3", is_struct=True, is_enum=False),
            ],
        )
        result = generate_ts_interface(s)
        assert "bodyIdA: number;" in result
        assert "bodyIdB: number;" in result
        assert "type: ContactType;" in result

    def test_bool_field(self):
        s = StructInfo(
            name="Foo",
            fields=[
                FieldInfo(name="is_sensor", cpp_type="bool", is_struct=False, is_enum=False),
            ],
        )
        result = generate_ts_interface(s)
        assert "isSensor: boolean;" in result


# ---------------------------------------------------------------------------
# generate_ts_enum
# ---------------------------------------------------------------------------

class TestGenerateTsEnum:
    def test_motion_type(self):
        e = EnumInfo(name="MotionType", values=["Static", "Dynamic", "Kinematic"])
        result = generate_ts_enum(e)
        assert 'type MotionType = "static" | "dynamic" | "kinematic";' in result

    def test_contact_type(self):
        e = EnumInfo(name="ContactType", values=["Begin", "Persist", "End"])
        result = generate_ts_enum(e)
        assert 'type ContactType = "begin" | "persist" | "end";' in result


# ---------------------------------------------------------------------------
# generate_enum_to_string / generate_enum_from_string
# ---------------------------------------------------------------------------

class TestEnumHelpers:
    def test_to_string(self):
        e = EnumInfo(name="MotionType", values=["Static", "Dynamic", "Kinematic"])
        result = generate_enum_to_string(e)
        assert "motion_type_to_string" in result
        assert 'return "static"' in result
        assert 'return "dynamic"' in result
        assert 'return "kinematic"' in result
        assert "default:" in result

    def test_from_string(self):
        e = EnumInfo(name="MotionType", values=["Static", "Dynamic", "Kinematic"])
        result = generate_enum_from_string(e)
        assert "motion_type_from_string" in result
        assert 'if (s == "static")' in result
        assert "MotionType::Static" in result
        assert "MotionType::Dynamic" in result

    def test_contact_type_to_string(self):
        e = EnumInfo(name="ContactType", values=["Begin", "Persist", "End"])
        result = generate_enum_to_string(e)
        assert "contact_type_to_string" in result
        assert 'return "begin"' in result

    def test_contact_type_from_string(self):
        e = EnumInfo(name="ContactType", values=["Begin", "Persist", "End"])
        result = generate_enum_from_string(e)
        assert "contact_type_from_string" in result


# ---------------------------------------------------------------------------
# generate_bindings_check
# ---------------------------------------------------------------------------

class TestGenerateBindingsCheck:
    def test_bindings_check_output(self):
        structs = [
            StructInfo(name="Vec3", fields=[]),
            StructInfo(name="Quat", fields=[]),
            StructInfo(name="Transform", fields=[]),
        ]
        result = generate_bindings_check(structs)
        assert "namespace gg::codegen" in result
        assert "void assert_bound_Vec3();" in result
        assert "void assert_bound_Quat();" in result
        assert "void assert_bound_Transform();" in result
        assert "inline void check_all_bindings()" in result
        assert "assert_bound_Vec3();" in result
        assert "assert_bound_Quat();" in result
        assert "assert_bound_Transform();" in result

    def test_auto_generated_header(self):
        result = generate_bindings_check([])
        assert "AUTO-GENERATED" in result
        assert "#pragma once" in result

    def test_empty_structs(self):
        result = generate_bindings_check([])
        assert "inline void check_all_bindings()" in result
