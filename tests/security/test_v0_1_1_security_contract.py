from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    file_path = ROOT / path
    assert file_path.exists(), f"missing {path}"
    return file_path.read_text(encoding="utf-8", errors="ignore")


def test_version_is_v0_1_1() -> None:
    assert "VERSION 0.1.1" in text("CMakeLists.txt")
    assert '#define ANYTHING_VERSION "v0.1.1"' in text("include/anything/config.h")


def test_json_helper_is_used_by_output_modules() -> None:
    assert "anything_json_escape_string" in text("include/anything/json.h")
    for path in [
        "src/common/rpc.c",
        "src/common/audit.c",
        "src/common/approval.c",
        "src/tools/sys_info.c",
    ]:
        assert "anything_json_" in text(path), f"{path} must use JSON helper"


def test_json_escape_handles_required_characters() -> None:
    json_c = text("src/common/json.c")
    assert "case '\"':" in json_c
    assert "case '\\\\':" in json_c
    assert "case '\\n':" in json_c
    assert "\\\\u%04x" in json_c


def test_audit_fail_closed_contract_exists() -> None:
    daemon = text("src/daemon/anythingd.c")
    audit_h = text("include/anything/audit.h")
    assert "anything_audit_validate_startup" in audit_h
    assert "audit_failed" in daemon
    assert "checked_audit" in daemon


def test_daemon_checks_key_audit_writes() -> None:
    daemon = text("src/daemon/anythingd.c")
    for event in [
        "request_received",
        "approval_required",
        "approval_granted",
        "execution_started",
        "execution_finished",
    ]:
        assert f'checked_audit(&state->config, "{event}"' in daemon


def test_admin_authorization_contract_exists() -> None:
    config_h = text("include/anything/config.h")
    daemon = text("src/daemon/anythingd.c")
    assert "admin_allowed_uids" in config_h
    assert "admin_allowed_gids" in config_h
    assert "anything_config_identity_is_admin" in config_h
    assert "control_plane_denied" in daemon


def test_config_example_contains_admin_allowlist() -> None:
    config = text("config/anythingd.example.toml")
    assert "[admin]" in config
    assert "allowed_uids = []" in config
    assert "allowed_gids = []" in config
    assert "require_admin_allowlist = false" in config


def test_transport_read_timeout_contract_exists() -> None:
    transport = text("src/common/transport.c")
    assert "SO_RCVTIMEO" in transport
    assert "read_timeout_ms" in text("include/anything/config.h")


def test_accept_one_sets_read_timeout_before_reading() -> None:
    daemon = text("src/daemon/anythingd.c")
    set_pos = daemon.index("anything_transport_set_read_timeout")
    read_pos = daemon.index("anything_transport_read_request")
    assert set_pos < read_pos


def test_rpc_parser_rejects_ambiguous_shapes_contract_exists() -> None:
    rpc = text("src/common/rpc.c")
    assert "duplicate top-level key" in rpc
    assert "non-object params" in rpc
    assert "nested top-level key confusion" in rpc


def test_linux_smoke_validates_json() -> None:
    smoke = text("scripts/linux_smoke_test.sh")
    assert "json.loads" in smoke
    assert "audit json lines valid" in smoke


if __name__ == "__main__":
    for test in [
        test_version_is_v0_1_1,
        test_json_helper_is_used_by_output_modules,
        test_json_escape_handles_required_characters,
        test_audit_fail_closed_contract_exists,
        test_daemon_checks_key_audit_writes,
        test_admin_authorization_contract_exists,
        test_config_example_contains_admin_allowlist,
        test_transport_read_timeout_contract_exists,
        test_accept_one_sets_read_timeout_before_reading,
        test_rpc_parser_rejects_ambiguous_shapes_contract_exists,
        test_linux_smoke_validates_json,
    ]:
        test()
    print("v0.1.1 security contract tests passed")
