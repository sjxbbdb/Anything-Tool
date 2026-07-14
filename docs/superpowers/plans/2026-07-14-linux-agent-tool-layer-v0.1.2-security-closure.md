# Linux Agent Tool Layer v0.1.2 Security Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship v0.1.2 secure-by-default closure for admin allowlists, request deadlines, audit path safety, behavior tests, reports, push, and tag.

**Architecture:** Keep current modules. Extend `config` with secure load options, `transport` with total request deadline, `audit` with `O_NOFOLLOW` file opening, and tests with v0.1.2 behavior contracts.

**Tech Stack:** C11, CMake, Linux APIs, Python security tests, CodeGraph, Git tag `v0.1.2`.

## Global Constraints

- Version string is `v0.1.2`.
- Create new tag `v0.1.2`; do not move older tags.
- Do not add `proc.*`, `net.*`, `fs.*`, MCP, root daemon, kernel/driver/hardware, or shell execution.
- Do not claim Linux runtime verification unless `bash scripts/linux_smoke_test.sh` runs on Linux.

---

### Task 1: Red Tests

**Files:**
- Create: `tests/security/test_v0_1_2_security_behavior.py`
- Modify: `CMakeLists.txt`
- Modify: `tests/security/test_v0_1_1_security_contract.py`

**Interfaces:**
- Produces CTest `v0_1_2_security_behavior`.

- [ ] Write tests that fail until secure defaults, dev-insecure config, deadline, and audit `O_NOFOLLOW` exist.
- [ ] Run `python tests\security\test_v0_1_2_security_behavior.py` and confirm RED.

### Task 2: Secure Admin Defaults

**Files:**
- Modify: `include/anything/config.h`
- Modify: `src/common/config.c`
- Modify: `src/daemon/anythingd.c`
- Modify: `config/anythingd.example.toml`
- Create: `config/anythingd.dev-insecure.toml`
- Modify: `scripts/linux_smoke_test.sh`

**Interfaces:**
- Produces `anything_config_load_with_options(..., int allow_dev_insecure_admin, ...)`.
- Produces daemon flag `--dev-insecure-admin`.

- [ ] Default `require_admin_allowlist` to true.
- [ ] Reject empty allowlist unless dev-insecure option is explicitly enabled.
- [ ] Split secure and dev-insecure configs.
- [ ] Make smoke script use dev-insecure flag/config for local smoke.

### Task 3: Request Deadline

**Files:**
- Modify: `include/anything/transport.h`
- Modify: `src/common/transport.c`
- Modify: `src/daemon/anythingd.c`

**Interfaces:**
- Updates `anything_transport_read_request` with `deadline_ms`.

- [ ] Add monotonic total deadline across the read loop.
- [ ] Pass `read_timeout_ms` from daemon.

### Task 4: Audit Path Closure

**Files:**
- Modify: `src/common/audit.c`

**Interfaces:**
- Uses `open` with `O_NOFOLLOW`, `fstat`, and fd-backed append.

- [ ] Reject symlink final audit path at startup.
- [ ] Use canonical parent/allowlist comparison where possible.
- [ ] Use `open`/`write` path for audit events.

### Task 5: Verify, Report, Commit, Push, Tag

**Files:**
- Create: `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.2-implementation_2026-07-14_12-42-59_UTC+0800_dev-report.md`
- Append: `C:\Users\admin\Desktop\anything tool开发日志.md`

- [ ] Run host tests, CMake, CTest, build.
- [ ] Run CodeGraph sync/status.
- [ ] Write reports/logs.
- [ ] Commit.
- [ ] Create and push tag `v0.1.2`.

## Self-Review

- Covers every v0.1.2 report item except real Linux smoke execution, which is explicitly blocked by current Windows environment unless a Linux runtime appears.
- No placeholders.
- Interfaces are named before use.
