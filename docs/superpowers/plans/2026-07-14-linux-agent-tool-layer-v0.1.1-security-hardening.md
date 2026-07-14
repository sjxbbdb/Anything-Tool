# Linux Agent Tool Layer v0.1.1 Security Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Harden the v0.1.0 Linux Agent Tool Layer skeleton into v0.1.1 by fixing admin authorization, audit fail-closed behavior, JSON validity, socket read deadlines, strict JSON-RPC parsing, and security behavior tests.

**Architecture:** Keep the existing C11/CMake daemon and CLI structure. Add a focused `json` common module, strengthen `config`, `rpc`, `audit`, `transport`, and `daemon`, then prove the behavior through Python host tests plus the Linux smoke script.

**Tech Stack:** C11, CMake, Linux system APIs, Python 3 contract/security tests, CodeGraph, Git tag `v0.1.1`.

## Global Constraints

- Version string for this batch is exactly `v0.1.1`.
- Final implementation commit receives a new `v0.1.1` tag.
- Existing tags must not be moved, deleted, or reused.
- No `proc.spawn`, `proc.kill`, `net.http_request`, `fs.write`, `fs.delete`, or `fs.mkdir`.
- No root daemon, kernel module, driver, direct hardware, MCP wrapper, or shell-execution surface.
- Current Windows host can run host tests and CMake/CTest only; Linux runtime claims require `bash scripts/linux_smoke_test.sh` on a real Linux host.
- All JSON string output must pass through the shared escaping helper.
- Key audit event write failure must return `audit_failed` and stop approval or execution.
- Admin approval methods must check configured admin UID/GID allowlists before routing.

---

## File Structure

- Create `include/anything/json.h`: shared JSON escaping and writer declarations.
- Create `src/common/json.c`: JSON string escaping and quoted-field writer.
- Create `tests/security/test_v0_1_1_security_contract.py`: host-runnable security behavior/source contract tests.
- Modify `CMakeLists.txt`: version `0.1.1`, include `json.c`, add v0.1.1 security test to CTest.
- Modify `include/anything/config.h` and `src/common/config.c`: admin allowlist config fields and parsing.
- Modify `include/anything/audit.h` and `src/common/audit.c`: audit startup validation and escaped JSON Lines.
- Modify `include/anything/rpc.h` and `src/common/rpc.c`: strict top-level scanner and escaped responses.
- Modify `include/anything/transport.h` and `src/common/transport.c`: read timeout support.
- Modify `src/daemon/anythingd.c`: checked audit writes, admin authorization, v0.1.1 routing behavior.
- Modify `src/common/approval.c`: escaped approval list JSON.
- Modify `src/tools/sys_info.c`: escaped `sys.info` fields and version `v0.1.1`.
- Modify `config/anythingd.example.toml`: add `[admin]`.
- Modify `scripts/linux_smoke_test.sh`: validate responses and audit lines with Python `json.loads`.
- Create `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.1-implementation_2026-07-14_11-28-04_UTC+0800_dev-report.md` after implementation.
- Append `C:\Users\admin\Desktop\anything tool开发日志.md` after implementation.

## Task 1: v0.1.1 Security Contract Tests

**Files:**
- Create: `tests/security/test_v0_1_1_security_contract.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `python tests/security/test_v0_1_1_security_contract.py`
- Produces: CTest test name `v0_1_1_security_contract`

- [ ] **Step 1: Write the failing security contract test**

Create `tests/security/test_v0_1_1_security_contract.py` with checks for v0.1.1 source requirements:

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    file_path = ROOT / path
    assert file_path.exists(), f"missing {path}"
    return file_path.read_text(encoding="utf-8", errors="ignore")


def test_version_is_v0_1_1() -> None:
    assert 'VERSION 0.1.1' in text('CMakeLists.txt')
    assert '#define ANYTHING_VERSION "v0.1.1"' in text('include/anything/config.h')


def test_json_helper_is_used_by_output_modules() -> None:
    assert 'anything_json_escape_string' in text('include/anything/json.h')
    for path in ['src/common/rpc.c', 'src/common/audit.c', 'src/common/approval.c', 'src/tools/sys_info.c']:
        assert 'anything_json_' in text(path), f'{path} must use JSON helper'


def test_audit_fail_closed_contract_exists() -> None:
    daemon = text('src/daemon/anythingd.c')
    audit_h = text('include/anything/audit.h')
    assert 'anything_audit_validate_startup' in audit_h
    assert 'audit_failed' in daemon
    assert 'checked_audit' in daemon


def test_admin_authorization_contract_exists() -> None:
    config_h = text('include/anything/config.h')
    daemon = text('src/daemon/anythingd.c')
    assert 'admin_allowed_uids' in config_h
    assert 'admin_allowed_gids' in config_h
    assert 'anything_config_identity_is_admin' in config_h
    assert 'control_plane_denied' in daemon


def test_transport_read_timeout_contract_exists() -> None:
    transport = text('src/common/transport.c')
    assert 'SO_RCVTIMEO' in transport
    assert 'read_timeout_ms' in text('include/anything/config.h')


def test_rpc_parser_rejects_ambiguous_shapes_contract_exists() -> None:
    rpc = text('src/common/rpc.c')
    assert 'duplicate top-level key' in rpc
    assert 'non-object params' in rpc
    assert 'nested top-level key confusion' in rpc


def test_linux_smoke_validates_json() -> None:
    smoke = text('scripts/linux_smoke_test.sh')
    assert 'json.loads' in smoke
    assert 'audit json lines valid' in smoke
```

- [ ] **Step 2: Run the test and verify RED**

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: FAIL because `include/anything/json.h` does not exist and version is still `v0.1.0`.

- [ ] **Step 3: Register the test in CMake**

Modify `CMakeLists.txt` to add:

```cmake
add_test(
  NAME v0_1_1_security_contract
  COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR} python ${CMAKE_SOURCE_DIR}/tests/security/test_v0_1_1_security_contract.py
)
```

- [ ] **Step 4: Run host tests**

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: still FAIL until the following tasks implement the required source contracts.

## Task 2: JSON Escaping And Strict RPC Scanner

**Files:**
- Create: `include/anything/json.h`
- Create: `src/common/json.c`
- Modify: `CMakeLists.txt`
- Modify: `include/anything/rpc.h`
- Modify: `src/common/rpc.c`
- Modify: `src/common/approval.c`
- Modify: `src/common/audit.c`
- Modify: `src/tools/sys_info.c`

**Interfaces:**
- Produces: `int anything_json_escape_string(const char *input, char *out, size_t out_len);`
- Produces: `int anything_json_write_string_field(char *out, size_t out_len, const char *name, const char *value);`
- Produces: `anything_rpc_parse` that rejects duplicate top-level keys, nested key confusion, non-object params, bad version, unsupported escapes, and oversized params.

- [ ] **Step 1: Write focused JSON behavior tests**

Extend `tests/security/test_v0_1_1_security_contract.py` with exact source checks:

```python
def test_json_escape_handles_required_characters() -> None:
    json_c = text('src/common/json.c')
    assert r'case \'"\':' in json_c
    assert r'case \'\\\\\':' in json_c
    assert r'case \'\\n\':' in json_c
    assert '\\\\u%04x' in json_c
```

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: FAIL because `src/common/json.c` is missing.

- [ ] **Step 2: Implement JSON helper**

Create `include/anything/json.h`:

```c
#ifndef ANYTHING_JSON_H
#define ANYTHING_JSON_H

#include <stddef.h>

int anything_json_escape_string(const char *input, char *out, size_t out_len);
int anything_json_write_string_field(char *out, size_t out_len, const char *name, const char *value);

#endif
```

Create `src/common/json.c` with `anything_json_escape_string` that writes escaped content without surrounding quotes and returns `-1` on truncation. `anything_json_write_string_field` writes `"name":"escaped-value"`.

- [ ] **Step 3: Wire JSON helper into CMake**

Add `src/common/json.c` to `anything_common` in `CMakeLists.txt`.

- [ ] **Step 4: Replace output string interpolation**

Update:

- `anything_rpc_result` and `anything_rpc_error` to escape `id`, `kind`, and `message`.
- `anything_approval_list_json` to escape request ID, method, risk, and summary.
- `anything_audit_write_event` to escape all string fields.
- `anything_sys_info_json` to escape hostname, kernel version, OS release, uptime, and architecture.

- [ ] **Step 5: Stricten parser**

Replace the `strstr` parser in `src/common/rpc.c` with a top-level scanner that:

- reads only one top-level object,
- records top-level keys,
- rejects duplicate `jsonrpc`, `id`, `method`, and `params`,
- rejects non-object `params`,
- copies params object as a bounded raw object string,
- rejects unsupported string escapes,
- decodes `\"`, `\\`, `\n`, `\r`, and `\t` for string scalar fields,
- never searches nested objects for top-level keys.

Keep the literal comment strings `duplicate top-level key`, `non-object params`, and `nested top-level key confusion` in rejection paths for source-contract tests.

- [ ] **Step 6: Verify JSON/RPC contract**

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: JSON helper and RPC source-contract tests pass; later task checks may still fail.

## Task 3: Audit Startup Validation And Fail-Closed Routing

**Files:**
- Modify: `include/anything/audit.h`
- Modify: `src/common/audit.c`
- Modify: `src/daemon/anythingd.c`
- Modify: `src/common/config.c`

**Interfaces:**
- Produces: `int anything_audit_validate_startup(const anything_config *config, char *error, size_t error_len);`
- Produces: `checked_audit(...)` helper in daemon that returns `audit_failed` on write failure.

- [ ] **Step 1: Add failing source-contract test for audit checks**

Extend `tests/security/test_v0_1_1_security_contract.py`:

```python
def test_daemon_checks_key_audit_writes() -> None:
    daemon = text('src/daemon/anythingd.c')
    for event in ['request_received', 'approval_required', 'approval_granted', 'execution_started', 'execution_finished']:
        assert f'checked_audit(&state->config, "{event}"' in daemon
```

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: FAIL because daemon still calls `anything_audit_write_event` directly.

- [ ] **Step 2: Implement audit startup validation**

Add `anything_audit_validate_startup` to `src/common/audit.c`:

- reject empty audit paths,
- reject paths already caught by writable allowlist validation,
- open with append mode,
- write no event during validation,
- fail if the parent directory is absent or file cannot be opened.

- [ ] **Step 3: Add daemon checked audit helper**

In `src/daemon/anythingd.c`, add:

```c
static int checked_audit(const anything_config *config, const char *event, const char *request_id, const char *session_id, anything_identity caller, const anything_identity *approver, const char *method, const char *risk, const char *decision, const char *error_kind, const char *params_summary, long duration_ms, int fd, const char *rpc_id) {
  if (anything_audit_write_event(config, event, request_id, session_id, caller, approver, method, risk, decision, error_kind, params_summary, duration_ms) == 0) {
    return 0;
  }
  char response[ANYTHING_RPC_MAX_RESPONSE];
  anything_rpc_error(rpc_id, -32050, "audit_failed", "audit write failed", "", response, sizeof(response));
  anything_transport_write_response(fd, response);
  return -1;
}
```

- [ ] **Step 4: Replace key audit calls**

Use `checked_audit` before continuing through request, approval, and execution stages. For `execution_finished`, write the audit event before returning the `sys.info` result; if the audit write fails, return `audit_failed` instead of the result.

- [ ] **Step 5: Validate startup before sockets**

Call `anything_audit_validate_startup(&state.config, error, sizeof(error))` after config load and before `anything_transport_listen`.

- [ ] **Step 6: Verify audit contract**

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: audit source-contract tests pass; later task checks may still fail.

## Task 4: Admin Authorization

**Files:**
- Modify: `include/anything/config.h`
- Modify: `src/common/config.c`
- Modify: `src/daemon/anythingd.c`
- Modify: `config/anythingd.example.toml`

**Interfaces:**
- Produces: `int anything_config_identity_is_admin(const anything_config *config, anything_identity identity);`
- Produces config fields `admin_allowed_uids`, `admin_allowed_gids`, `admin_allowed_uid_count`, `admin_allowed_gid_count`, `require_admin_allowlist`.

- [ ] **Step 1: Add failing admin authorization source-contract test**

Extend `tests/security/test_v0_1_1_security_contract.py`:

```python
def test_config_example_contains_admin_allowlist() -> None:
    config = text('config/anythingd.example.toml')
    assert '[admin]' in config
    assert 'allowed_uids = []' in config
    assert 'allowed_gids = []' in config
    assert 'require_admin_allowlist = false' in config
```

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: FAIL because the example config has no `[admin]` table.

- [ ] **Step 2: Extend config types**

Add these fields to `anything_config`:

```c
long admin_allowed_uids[16];
long admin_allowed_gids[16];
size_t admin_allowed_uid_count;
size_t admin_allowed_gid_count;
int require_admin_allowlist;
int read_timeout_ms;
```

Declare:

```c
int anything_config_identity_is_admin(const anything_config *config, anything_identity identity);
```

- [ ] **Step 3: Parse `[admin]` arrays**

In `src/common/config.c`, parse simple arrays such as `allowed_uids = [1000, 1001]` and `allowed_gids = [1001]`. Parse `require_admin_allowlist = true` and default it to `0` for development smoke config.

- [ ] **Step 4: Enforce admin authorization**

At the top of `handle_admin_request`, after parsing the request and before any approval method handling, call `anything_config_identity_is_admin`. If false, return JSON-RPC `control_plane_denied` and write checked audit `preflight_denied`.

- [ ] **Step 5: Update example config**

Add:

```toml
[admin]
allowed_uids = []
allowed_gids = []
require_admin_allowlist = false
```

- [ ] **Step 6: Verify admin contract**

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: admin authorization source-contract tests pass; later task checks may still fail.

## Task 5: Socket Read Timeout And Linux Smoke JSON Validation

**Files:**
- Modify: `include/anything/transport.h`
- Modify: `src/common/transport.c`
- Modify: `src/daemon/anythingd.c`
- Modify: `scripts/linux_smoke_test.sh`

**Interfaces:**
- Produces: `int anything_transport_set_read_timeout(int fd, int timeout_ms, char *error, size_t error_len);`
- `accept_one` applies timeout before reading request bytes.

- [ ] **Step 1: Add failing slow-client source-contract test**

Extend `tests/security/test_v0_1_1_security_contract.py`:

```python
def test_accept_one_sets_read_timeout_before_reading() -> None:
    daemon = text('src/daemon/anythingd.c')
    set_pos = daemon.index('anything_transport_set_read_timeout')
    read_pos = daemon.index('anything_transport_read_request')
    assert set_pos < read_pos
```

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: FAIL because `accept_one` does not set a read timeout.

- [ ] **Step 2: Implement timeout function**

In `src/common/transport.c`, implement `anything_transport_set_read_timeout` with `SO_RCVTIMEO` and `struct timeval`. Return `-1` with an error string if `setsockopt` fails.

- [ ] **Step 3: Apply timeout in daemon**

Call `anything_transport_set_read_timeout(fd, state->config.read_timeout_ms, error, sizeof(error))` in `accept_one` immediately after peer identity succeeds and before allocating/reading the request.

- [ ] **Step 4: Update smoke script JSON validation**

In `scripts/linux_smoke_test.sh`, after each CLI response capture, run Python `json.loads`. Add audit validation:

```bash
python - "${AUDIT_LOG}" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as fh:
    for line in fh:
        json.loads(line)
print("audit json lines valid")
PY
```

- [ ] **Step 5: Verify timeout and smoke contracts**

Run: `python tests\security\test_v0_1_1_security_contract.py`

Expected: security contract tests pass.

## Task 6: Version, Verification, Reports, Commit, Push, And Tag

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `include/anything/config.h`
- Modify: `tests/contract/test_v0_1_0_contract.py`
- Create: `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.1-implementation_2026-07-14_11-28-04_UTC+0800_dev-report.md`
- Append: `C:\Users\admin\Desktop\anything tool开发日志.md`

**Interfaces:**
- Produces final Git commit.
- Produces new tag `v0.1.1`.
- Pushes branch and tag to `origin`.

- [ ] **Step 1: Update version references**

Change:

- `project(AnythingTool VERSION 0.1.0 LANGUAGES C)` to `VERSION 0.1.1`
- `#define ANYTHING_VERSION "v0.1.0"` to `"v0.1.1"`
- contract test expected version to v0.1.1 or rename it to avoid stale v0.1.0 expectations.

- [ ] **Step 2: Run host verification**

Run:

```powershell
python tests\contract\test_v0_1_0_contract.py
python tests\security\test_v0_1_1_security_contract.py
cmake -S . -B build
ctest --test-dir build --output-on-failure
cmake --build build
```

Expected: all commands exit 0 on Windows host. Linux-only targets remain placeholders on Windows.

- [ ] **Step 3: Run CodeGraph**

Run:

```powershell
codegraph sync .
codegraph status .
```

Expected: status reports `[OK] Index is up to date`.

- [ ] **Step 4: Record implementation report and desktop log**

Create the v0.1.1 implementation report with:

- timestamp,
- files changed,
- tests run,
- CodeGraph result,
- Linux smoke status,
- GitHub push status,
- tag plan,
- `开发者` signature.

Append matching details to `C:\Users\admin\Desktop\anything tool开发日志.md` with `开发者` signature.

- [ ] **Step 5: Commit**

Run:

```powershell
git status --short
git add . "C:\Users\admin\Desktop\anything tool开发日志.md"
git commit -m "Harden Linux agent tool layer v0.1.1"
```

Do not add the desktop log if it is outside the repository; append it locally and commit only repository files.

- [ ] **Step 6: Tag without touching prior tags**

Run:

```powershell
git tag --list
git tag -a v0.1.1 -m "Linux agent tool layer v0.1.1 security hardening"
```

Expected: `v0.1.1` is a new tag on the final implementation commit.

- [ ] **Step 7: Push branch and tag**

Use the local GitHub token only as an environment variable:

```powershell
$token = (Get-Content -Raw "C:\Users\admin\Desktop\GitHub apikey.txt").Trim()
$env:GH_TOKEN = $token
gh auth setup-git | Out-Null
git push origin master
git push origin v0.1.1
Remove-Item Env:GH_TOKEN
```

Expected: branch and `v0.1.1` tag are pushed to GitHub.

## Self-Review

- Spec coverage: plan covers admin authorization, audit fail-closed, JSON validity, socket deadline, parser strictness, behavior tests, Linux smoke script, CodeGraph, reports, commit, push, and tag.
- Placeholder scan: no placeholder markers are used.
- Type consistency: `anything_json_escape_string`, `anything_json_write_string_field`, `anything_audit_validate_startup`, `anything_config_identity_is_admin`, and `anything_transport_set_read_timeout` are defined before later tasks consume them.
