#pragma once

// Marker macros for the codegen tool (scripts/codegen.py).
// These expand to nothing at compile time. libclang uses them
// to identify types that should be exposed to the scripting layer.
//
// Usage:
//   struct GG_SCRIPTABLE Vec3 { float x, y, z; };
//   enum class GG_SCRIPTABLE_ENUM MotionType { Static, Dynamic };

#define GG_SCRIPTABLE
#define GG_SCRIPTABLE_ENUM
