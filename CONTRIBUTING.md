# Contributing

## Build Setup

```bash
# Prerequisites: Meson, Ninja, C++20 compiler
bash scripts/setup-wgpu.sh
bash scripts/setup-imgui.sh
meson setup builddir
meson compile -C builddir
meson test -C builddir
./builddir/app/game-gym
```

### Optional: V8 Scripting

```bash
bash scripts/setup-v8.sh
meson setup builddir --reconfigure -Denable_scripts=true
meson compile -C builddir
```

### Optional: Codegen

```bash
pip install -r scripts/requirements-codegen.txt
python scripts/codegen.py
```

### Optional: Coverage Gate

```bash
python3 -m pip install -r scripts/requirements-coverage.txt
bash scripts/install-git-hooks.sh
python3 scripts/check_coverage.py --builddir builddir-coverage --summary-out builddir-coverage/coverage-summary.json
```

### Release Tags

```bash
# build metadata comes from git tags
./builddir/app/game-gym --version

# create an annotated release tag
bash scripts/create_release_tag.sh v0.2.0

# publish it to GitHub
git push origin v0.2.0
```

## Code Style

- C++20 standard
- Formatting enforced by `.clang-format` (LLVM-based, 4-space indent, 100 column limit)
- Static analysis via `.clang-tidy` (bugprone, modernize, performance)
- Run locally: `clang-format -i <file>` before committing

## Commit Messages

```
<type>: <description>
```

Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`, `ci`, `style`

## Testing

- Framework: Google Test
- Every module has a corresponding `tests/test_<module>.cpp`
- Run specific test: `meson test -C builddir <test_name> -v`
- Default boot path: `./builddir/app/game-gym` loads `project.ggym` from the repo root
- Coverage gate threshold lives in `scripts/coverage-threshold.json`

## Adding Scriptable Types

1. Add `GG_SCRIPTABLE` to your struct in a header:
   ```cpp
   struct GG_SCRIPTABLE MyType {
       float value = 0.0f;
   };
   ```
2. Run codegen: `python scripts/codegen.py`
3. Add `assert_bound_MyType()` to `script_bindings.cpp`
4. Commit the generated files

## Branch Workflow

1. Create feature branch from `master`
2. Implement with tests
3. Push and create PR
4. CI must pass (format, build, tidy, codegen)
5. Code review required before merge
6. Create a `vMAJOR.MINOR.PATCH` tag only on the merge commit you want to release

## Project Patterns

- **Factory**: `static std::unique_ptr<T> create(...)` with private constructor
- **Pimpl**: `struct Impl; std::unique_ptr<Impl> impl_;` for hiding implementation details
- **Namespace**: `gg::` for all engine code
- **Non-copyable**: `= delete` on copy/move for resource-owning classes
