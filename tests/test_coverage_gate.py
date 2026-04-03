import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "check_coverage.py"


def write_summary(path: Path, line_percent: float) -> None:
    path.write_text(
        json.dumps(
            {
                "line_total": 100,
                "line_covered": int(line_percent),
                "line_percent": line_percent,
                "function_total": 0,
                "function_covered": 0,
                "function_percent": 0.0,
                "branch_total": 0,
                "branch_covered": 0,
                "branch_percent": 0.0,
            }
        )
    )


def run_gate(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def test_fails_when_summary_is_below_threshold(tmp_path: Path) -> None:
    summary_path = tmp_path / "summary.json"
    write_summary(summary_path, 55.9)

    result = run_gate(
        "--summary-json",
        str(summary_path),
        "--fail-under",
        "56.0",
    )

    assert result.returncode == 1
    assert "Coverage check failed" in result.stderr


def test_passes_when_summary_meets_threshold(tmp_path: Path) -> None:
    summary_path = tmp_path / "summary.json"
    write_summary(summary_path, 56.7)

    result = run_gate(
        "--summary-json",
        str(summary_path),
        "--fail-under",
        "56.0",
    )

    assert result.returncode == 0
    assert "Line coverage: 56.7%" in result.stdout


def test_reads_threshold_from_file(tmp_path: Path) -> None:
    summary_path = tmp_path / "summary.json"
    threshold_path = tmp_path / "threshold.json"
    write_summary(summary_path, 60.0)
    threshold_path.write_text(json.dumps({"line": 61.0}))

    result = run_gate(
        "--summary-json",
        str(summary_path),
        "--fail-under-file",
        str(threshold_path),
    )

    assert result.returncode == 1
    assert "61.0%" in result.stderr
