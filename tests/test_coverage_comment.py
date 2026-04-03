import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "render_coverage_comment.py"


def write_summary(path: Path) -> None:
    path.write_text(
        json.dumps(
            {
                "line_total": 100,
                "line_covered": 57,
                "line_percent": 57.0,
                "function_total": 20,
                "function_covered": 15,
                "function_percent": 75.0,
                "branch_total": 40,
                "branch_covered": 12,
                "branch_percent": 30.0,
                "files": [
                    {
                        "filename": "engine/core/engine.cpp",
                        "line_total": 20,
                        "line_covered": 12,
                        "line_percent": 60.0,
                        "function_total": 4,
                        "function_covered": 3,
                        "function_percent": 75.0,
                        "branch_total": 8,
                        "branch_covered": 2,
                        "branch_percent": 25.0,
                    },
                    {
                        "filename": "engine/script/script_manager.cpp",
                        "line_total": 10,
                        "line_covered": 8,
                        "line_percent": 80.0,
                        "function_total": 2,
                        "function_covered": 2,
                        "function_percent": 100.0,
                        "branch_total": 6,
                        "branch_covered": 4,
                        "branch_percent": 66.7,
                    },
                ],
            }
        )
    )


def run_renderer(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def test_renders_changed_file_table(tmp_path: Path) -> None:
    summary_path = tmp_path / "summary.json"
    changed_path = tmp_path / "changed.txt"
    output_path = tmp_path / "comment.md"
    threshold_path = tmp_path / "threshold.json"

    write_summary(summary_path)
    changed_path.write_text("engine/core/engine.cpp\nREADME.md\n")
    threshold_path.write_text(json.dumps({"line": 56.0}))

    result = run_renderer(
        "--summary-json",
        str(summary_path),
        "--changed-files",
        str(changed_path),
        "--threshold-file",
        str(threshold_path),
        "--output",
        str(output_path),
    )

    assert result.returncode == 0
    content = output_path.read_text()
    assert "## Coverage Report" in content
    assert "57.0%" in content
    assert "engine/core/engine.cpp" in content
    assert "README.md" not in content


def test_marks_failure_when_below_threshold(tmp_path: Path) -> None:
    summary_path = tmp_path / "summary.json"
    changed_path = tmp_path / "changed.txt"
    output_path = tmp_path / "comment.md"
    threshold_path = tmp_path / "threshold.json"

    write_summary(summary_path)
    changed_path.write_text("")
    threshold_path.write_text(json.dumps({"line": 60.0}))

    result = run_renderer(
        "--summary-json",
        str(summary_path),
        "--changed-files",
        str(changed_path),
        "--threshold-file",
        str(threshold_path),
        "--output",
        str(output_path),
    )

    assert result.returncode == 0
    content = output_path.read_text()
    assert "Status: fail" in content
    assert "Threshold: 60.0%" in content
