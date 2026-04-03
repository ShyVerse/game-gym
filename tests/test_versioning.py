import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "versioning.py"
SPEC = importlib.util.spec_from_file_location("versioning", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def test_exact_semver_tag_is_release() -> None:
    info = MODULE.resolve_version_info("v1.2.3", "0.1.0")

    assert info.release_tag == "v1.2.3"
    assert info.release_version == "1.2.3"
    assert info.display_version == "1.2.3"
    assert info.is_exact_tag is True
    assert info.is_dirty is False


def test_commit_after_tag_uses_dev_version() -> None:
    info = MODULE.resolve_version_info("v1.2.3-4-gabc1234", "0.1.0")

    assert info.release_tag == "v1.2.3"
    assert info.release_version == "1.2.3"
    assert info.display_version == "1.2.3-dev.4+gabc1234"
    assert info.is_exact_tag is False
    assert info.is_dirty is False


def test_dirty_commit_marks_display_version() -> None:
    info = MODULE.resolve_version_info("v1.2.3-4-gabc1234-dirty", "0.1.0")

    assert info.display_version == "1.2.3-dev.4+gabc1234.dirty"
    assert info.is_dirty is True


def test_no_tag_falls_back_to_base_version() -> None:
    info = MODULE.resolve_version_info("abc1234", "0.5.0")

    assert info.release_tag == "v0.5.0"
    assert info.release_version == "0.5.0"
    assert info.display_version == "0.5.0-dev+gabc1234"
    assert info.is_exact_tag is False
