# Repository Guidelines

## Project Structure & Module Organization
`engine/` contains the runtime subsystems: `core`, `renderer`, `ecs`, `physics`, `scene`, `script`, `editor`, and `mcp`. `app/` builds the executable entry point. `tests/` holds GoogleTest coverage for C++ modules plus a few Python helper tests. Runtime content lives in `assets/`, `scenes/`, and `shaders/`. Generated outputs belong in `generated/` and `typings/`. Helper automation such as dependency setup, codegen, coverage, and versioning lives in `scripts/`. Treat `builddir/` and `builddir-coverage/` as disposable build outputs.

## Build, Test, and Development Commands
Run `bash scripts/setup-wgpu.sh` and `bash scripts/setup-imgui.sh` once after cloning to fetch native dependencies. Use `meson setup builddir` to configure a default build, or `meson setup builddir --reconfigure -Denable_scripts=true` when working on the V8 scripting layer. Build with `meson compile -C builddir`. Run the C++ suite with `meson test -C builddir --print-errorlogs`. Enforce coverage with `python3 scripts/check_coverage.py --builddir builddir-coverage --summary-out builddir-coverage/coverage-summary.json`. Regenerate bindings after editing `GG_SCRIPTABLE` types with `python scripts/codegen.py`.

## Coding Style & Naming Conventions
This project uses C++20 with LLVM-based `clang-format`: 4-space indentation, 100-column limit, attached braces, and left-aligned pointers/references. Keep filenames in `snake_case` such as `test_scene_loader.cpp`, use `PascalCase` for types, and reserve `ALL_CAPS` for macros like `GG_SCRIPTABLE`. Run formatting on files under `engine/`, `app/`, and `tests/`. `clang-tidy` is enforced for `engine/` and treats bugprone, modernize, and performance findings as errors.

## Testing Guidelines
Add or extend `tests/test_*.cpp` files for engine changes, and keep the filename aligned with the subsystem you touched. Python checks live in `tests/test_*.py` and cover codegen, coverage, and versioning helpers. When you change those paths, run `pytest -q tests/test_codegen.py tests/test_coverage_gate.py tests/test_coverage_comment.py tests/test_versioning.py`. Keep the threshold in `scripts/coverage-threshold.json` green before pushing.

## Commit & Pull Request Guidelines
Follow the commit style already used in history: `feat: ...`, `fix: ...`, `style: ...`, with concise subsystem context when useful. Keep each commit focused on one change. Before opening a PR, run formatting, `meson test`, and any relevant Python checks. PRs should target `master`, summarize user-visible behavior changes, list the commands you ran, and note regenerated files or coverage-impacting changes. Include screenshots or short recordings for editor, renderer, or scene-authoring changes.
