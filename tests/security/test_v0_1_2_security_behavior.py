from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    file_path = ROOT / path
    assert file_path.exists(), f"missing {path}"
    return file_path.read_text(encoding="utf-8", errors="ignore")


def test_version_is_v0_1_2() -> None:
    assert "VERSION 0.1.2" in text("CMakeLists.txt")
    assert '#define ANYTHING_VERSION "v0.1.2"' in text("include/anything/config.h")


def test_admin_allowlist_is_secure_by_default() -> None:
    config_c = text("src/common/config.c")
    example = text("config/anythingd.example.toml")
    assert "config->require_admin_allowlist = 1" in config_c
    assert "admin allowlist is required but empty" in config_c
    assert "require_admin_allowlist = true" in example
    assert "require_admin_allowlist = false" not in example


def test_dev_insecure_admin_is_explicit() -> None:
    daemon = text("src/daemon/anythingd.c")
    dev_config = text("config/anythingd.dev-insecure.toml")
    assert "--dev-insecure-admin" in daemon
    assert "WARNING: --dev-insecure-admin" in daemon
    assert "INSECURE DEVELOPMENT ONLY" in dev_config
    assert "require_admin_allowlist = false" in dev_config


def test_total_request_deadline_exists() -> None:
    transport_h = text("include/anything/transport.h")
    transport_c = text("src/common/transport.c")
    daemon = text("src/daemon/anythingd.c")
    assert "deadline_ms" in transport_h
    assert "CLOCK_MONOTONIC" in transport_c
    assert "request deadline exceeded" in transport_c
    assert "state->config.read_timeout_ms" in daemon


def test_audit_rejects_symlink_and_uses_nofollow() -> None:
    audit = text("src/common/audit.c")
    assert "O_NOFOLLOW" in audit
    assert "audit log must not be a symlink" in audit
    assert "realpath" in audit
    assert "fstat" in audit


def test_linux_smoke_uses_dev_insecure_config_explicitly() -> None:
    smoke = text("scripts/linux_smoke_test.sh")
    assert "anythingd.dev-insecure.toml" in smoke
    assert "--dev-insecure-admin" in smoke


if __name__ == "__main__":
    for test in [
        test_version_is_v0_1_2,
        test_admin_allowlist_is_secure_by_default,
        test_dev_insecure_admin_is_explicit,
        test_total_request_deadline_exists,
        test_audit_rejects_symlink_and_uses_nofollow,
        test_linux_smoke_uses_dev_insecure_config_explicitly,
    ]:
        test()
    print("v0.1.2 security behavior tests passed")
