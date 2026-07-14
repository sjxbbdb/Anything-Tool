from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


REQUIRED_FILES = [
    "CMakeLists.txt",
    "include/anything/approval.h",
    "include/anything/audit.h",
    "include/anything/config.h",
    "include/anything/identity.h",
    "include/anything/policy.h",
    "include/anything/rpc.h",
    "include/anything/sys_info.h",
    "src/cli/anythingctl.c",
    "src/common/approval.c",
    "src/common/audit.c",
    "src/common/config.c",
    "src/common/policy.c",
    "src/common/rpc.c",
    "src/common/transport.c",
    "src/daemon/anythingd.c",
    "src/tools/sys_info.c",
    "config/anythingd.example.toml",
    "scripts/linux_smoke_test.sh",
]


REQUIRED_SNIPPETS = {
    "CMakeLists.txt": [
        "VERSION 0.1.1",
        "C_STANDARD 11",
        "anythingd",
        "anythingctl",
    ],
    "include/anything/config.h": [
        "ANYTHING_VERSION",
        "v0.1.1",
        "max_request_bytes",
        "approval_ttl_seconds",
    ],
    "src/common/transport.c": [
        "SO_PEERCRED",
        "tool socket",
        "admin socket",
    ],
    "src/common/policy.c": [
        "approval_required",
        "policy_denied",
        "request_hash",
    ],
    "src/common/approval.c": [
        "requester cannot approve",
        "request_changed",
        "approval_expired",
    ],
    "src/common/audit.c": [
        "request_received",
        "approval_granted",
        "execution_finished",
    ],
    "src/tools/sys_info.c": [
        "uname",
        "/proc/uptime",
        "/etc/os-release",
    ],
    "src/daemon/anythingd.c": [
        "approval.execute",
        "sys.info",
        "policy recheck",
    ],
    "scripts/linux_smoke_test.sh": [
        "cmake -S",
        "ctest",
        "anythingctl",
        "approval_required",
    ],
}


def read_text(path: str) -> str:
    file_path = ROOT / path
    assert file_path.exists(), f"Missing required file: {path}"
    return file_path.read_text(encoding="utf-8")


def test_required_files_exist() -> None:
    missing = [path for path in REQUIRED_FILES if not (ROOT / path).exists()]
    assert not missing, "Missing required files:\n" + "\n".join(missing)


def test_required_contract_snippets_exist() -> None:
    failures = []
    for path, snippets in REQUIRED_SNIPPETS.items():
        text = read_text(path)
        for snippet in snippets:
            if snippet not in text:
                failures.append(f"{path}: missing {snippet!r}")
    assert not failures, "Contract snippets missing:\n" + "\n".join(failures)


def test_no_deferred_tool_surface_in_v0_1_0() -> None:
    source_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore")
        for path in (ROOT / "src").rglob("*.[ch]")
    )
    forbidden_runtime_methods = [
        '"proc.spawn"',
        '"proc.kill"',
        '"net.http_request"',
        '"fs.write"',
        '"fs.delete"',
        '"fs.mkdir"',
    ]
    present = [needle for needle in forbidden_runtime_methods if needle in source_text]
    assert not present, "Deferred tool methods exposed in source: " + ", ".join(present)


if __name__ == "__main__":
    test_required_files_exist()
    test_required_contract_snippets_exist()
    test_no_deferred_tool_surface_in_v0_1_0()
    print("v0.1.1 contract tests passed")
