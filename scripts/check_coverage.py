#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path


DEFAULT_BUILD_DIR = "builddir-coverage"
DEFAULT_THRESHOLD_FILE = "scripts/coverage-threshold.json"
DEFAULT_SUMMARY_OUT = "builddir-coverage/coverage-summary.json"
DEFAULT_FILTER = "^(engine|app)/"
DEFAULT_EXCLUDE = "^(tests|subprojects|third_party|generated)/"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run and enforce coverage checks.")
    parser.add_argument("--builddir", default=DEFAULT_BUILD_DIR)
    parser.add_argument("--summary-json", help="Read an existing gcovr summary JSON file.")
    parser.add_argument("--summary-out", default=DEFAULT_SUMMARY_OUT)
    parser.add_argument("--fail-under", type=float)
    parser.add_argument("--fail-under-file", default=DEFAULT_THRESHOLD_FILE)
    parser.add_argument("--root", default=".")
    parser.add_argument("--filter", default=DEFAULT_FILTER)
    parser.add_argument("--exclude", default=DEFAULT_EXCLUDE)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-tests", action="store_true")
    parser.add_argument("--enable-scripts", action="store_true", default=True)
    return parser.parse_args()


def run(cmd: list[str], cwd: Path) -> None:
    subprocess.run(cmd, cwd=cwd, check=True)


def resolve_threshold(args: argparse.Namespace) -> float:
    if args.fail_under is not None:
        return args.fail_under

    threshold_path = Path(args.fail_under_file)
    data = json.loads(threshold_path.read_text())
    return float(data["line"])


def detect_llvm_cov() -> str:
    xcrun = shutil.which("xcrun")
    if xcrun:
        result = subprocess.run(
            [xcrun, "--find", "llvm-cov"],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0:
            return result.stdout.strip()

    llvm_cov = shutil.which("llvm-cov")
    if llvm_cov:
        return llvm_cov

    raise RuntimeError("llvm-cov is required for clang coverage collection")


def ensure_gcovr_available() -> None:
    try:
        import gcovr  # noqa: F401
    except ImportError as exc:
        raise RuntimeError(
            "gcovr is not installed. Run `python3 -m pip install -r scripts/requirements-coverage.txt`."
        ) from exc


def ensure_builddir(repo_root: Path, builddir: str, enable_scripts: bool) -> None:
    builddir_path = repo_root / builddir
    meson_cmd = ["meson"]
    if builddir_path.exists():
        meson_cmd.extend(["configure", builddir, "-Db_coverage=true"])
        meson_cmd.append(f"-Denable_scripts={'true' if enable_scripts else 'false'}")
    else:
        meson_cmd.extend(["setup", builddir, "-Db_coverage=true"])
        meson_cmd.append(f"-Denable_scripts={'true' if enable_scripts else 'false'}")
    run(meson_cmd, repo_root)


def collect_summary(args: argparse.Namespace, repo_root: Path) -> Path:
    ensure_gcovr_available()
    ensure_builddir(repo_root, args.builddir, args.enable_scripts)

    if not args.skip_build:
        run(["meson", "compile", "-C", args.builddir], repo_root)
    if not args.skip_tests:
        run(["meson", "test", "-C", args.builddir, "--print-errorlogs"], repo_root)

    summary_path = repo_root / args.summary_out
    summary_path.parent.mkdir(parents=True, exist_ok=True)

    llvm_cov = detect_llvm_cov()
    gcovr_cmd = [
        sys.executable,
        "-m",
        "gcovr",
        "--root",
        args.root,
        args.builddir,
        "--gcov-executable",
        f"{llvm_cov} gcov",
        "--filter",
        args.filter,
        "--exclude",
        args.exclude,
        "--json-summary",
        str(summary_path),
        "--json-summary-pretty",
    ]
    run(gcovr_cmd, repo_root)
    return summary_path


def load_summary(path: Path) -> dict:
    return json.loads(path.read_text())


def check_threshold(summary: dict, fail_under: float) -> tuple[bool, str]:
    line_percent = float(summary["line_percent"])
    message = (
        f"Line coverage: {line_percent:.1f}% "
        f"(threshold: {fail_under:.1f}%, lines: {summary['line_covered']}/{summary['line_total']})"
    )
    return line_percent >= fail_under, message


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[1]

    try:
        fail_under = resolve_threshold(args)
        if args.summary_json:
            summary_path = Path(args.summary_json)
        else:
            summary_path = collect_summary(args, repo_root)

        summary = load_summary(summary_path)
        ok, message = check_threshold(summary, fail_under)
        if ok:
            print(message)
            return 0

        print(f"Coverage check failed: {message}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as exc:
        print(f"Coverage command failed: {' '.join(exc.cmd)}", file=sys.stderr)
        return exc.returncode or 1
    except Exception as exc:  # noqa: BLE001
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
