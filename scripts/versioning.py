#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import subprocess
from dataclasses import dataclass, asdict
from pathlib import Path


DESCRIBE_RE = re.compile(
    r"^(?P<tag>v\d+\.\d+\.\d+)"
    r"(?:-(?P<distance>\d+)-g(?P<sha>[0-9a-f]+))?"
    r"(?P<dirty>-dirty)?$"
)


@dataclass
class VersionInfo:
    project_version: str
    release_tag: str
    release_version: str
    describe: str
    display_version: str
    is_exact_tag: bool
    is_dirty: bool


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Resolve build version metadata from git tags.")
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--base-version", required=True)
    parser.add_argument("--describe")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--header-output")
    parser.add_argument("--require-exact-tag", action="store_true")
    return parser.parse_args()


def git_describe(repo_root: str) -> str:
    result = subprocess.run(
        [
            "git",
            "describe",
            "--tags",
            "--dirty",
            "--always",
            "--match",
            "v[0-9]*",
        ],
        cwd=repo_root,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "git describe failed")
    return result.stdout.strip()


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists() and path.read_text() == content:
        return False
    path.write_text(content)
    return True


def resolve_version_info(describe: str, base_version: str) -> VersionInfo:
    describe = describe.strip()
    match = DESCRIBE_RE.match(describe)

    if match:
        release_tag = match.group("tag")
        release_version = release_tag[1:]
        distance = match.group("distance")
        sha = match.group("sha")
        dirty = match.group("dirty") is not None
        exact = distance is None and not dirty

        if exact:
            display_version = release_version
        elif distance is None:
            display_version = f"{release_version}+dirty"
        else:
            display_version = f"{release_version}-dev.{distance}+g{sha}"
            if dirty:
                display_version += ".dirty"

        return VersionInfo(
            project_version=base_version,
            release_tag=release_tag,
            release_version=release_version,
            describe=describe,
            display_version=display_version,
            is_exact_tag=exact,
            is_dirty=dirty,
        )

    clean = describe.removesuffix("-dirty")
    dirty = describe.endswith("-dirty")
    display_version = f"{base_version}-dev+g{clean}"
    if dirty:
        display_version += ".dirty"

    return VersionInfo(
        project_version=base_version,
        release_tag=f"v{base_version}",
        release_version=base_version,
        describe=describe,
        display_version=display_version,
        is_exact_tag=False,
        is_dirty=dirty,
    )


def resolve_current_version_info(repo_root: str, base_version: str) -> VersionInfo:
    try:
        describe = git_describe(repo_root)
    except RuntimeError:
        return VersionInfo(
            project_version=base_version,
            release_tag=f"v{base_version}",
            release_version=base_version,
            describe=f"v{base_version}",
            display_version=base_version,
            is_exact_tag=False,
            is_dirty=False,
        )

    return resolve_version_info(describe, base_version)


def render_header(info: VersionInfo) -> str:
    return "\n".join(
        [
            "#pragma once",
            "",
            "namespace gg::build_version {",
            f'inline constexpr char kProjectVersion[] = "{info.project_version}";',
            f'inline constexpr char kReleaseTag[] = "{info.release_tag}";',
            f'inline constexpr char kReleaseVersion[] = "{info.release_version}";',
            f'inline constexpr char kGitDescribe[] = "{info.describe}";',
            f'inline constexpr char kDisplayVersion[] = "{info.display_version}";',
            f"inline constexpr bool kIsExactTag = {'true' if info.is_exact_tag else 'false'};",
            f"inline constexpr bool kIsDirty = {'true' if info.is_dirty else 'false'};",
            "} // namespace gg::build_version",
            "",
        ]
    )


def main() -> int:
    args = parse_args()
    if args.describe:
        info = resolve_version_info(args.describe, args.base_version)
    else:
        info = resolve_current_version_info(args.repo_root, args.base_version)

    if args.require_exact_tag and not info.is_exact_tag:
        raise SystemExit(f"Tag release requires an exact semantic tag, got: {info.describe}")

    if args.header_output:
        write_if_changed(Path(args.header_output), render_header(info))

    if args.json:
        print(json.dumps(asdict(info), indent=2))
    elif not args.header_output:
        print(info.display_version)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
