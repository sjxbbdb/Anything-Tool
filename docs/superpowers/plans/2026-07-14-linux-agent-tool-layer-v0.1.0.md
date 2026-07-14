# Linux Agent Tool Layer v0.1.0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the v0.1.0 Linux-only minimum closed loop: `agent request -> policy preflight -> approval required -> admin approval -> policy recheck -> sys.info execution -> audit log`.

**Architecture:** The project uses C11 and CMake. `anythingd` owns config, Unix sockets, peer credentials, JSON-RPC routing, policy, approvals, audit, and tool execution; `anythingctl` is a thin JSON client for tool/admin sockets. v0.1.0 intentionally implements only `sys.info` as the executable tool and keeps process, network, write/delete filesystem, root daemon, kernel, driver, and MCP work out of scope.

**Tech Stack:** C11, CMake, Linux system APIs, internal bounded JSON/TOML subset parsers for v0.1.0, Python contract tests for this Windows development host, CTest entrypoints for Linux builds.

## Global Constraints

- Version string is exactly `v0.1.0`.
- Target platform is Ubuntu/Linux; Windows host verification is limited to contract tests and CMake file inspection.
- Request bytes are checked before JSON parsing.
- Agent tool socket and admin socket are separate Unix domain sockets.
- Every connection records `SO_PEERCRED` UID/GID/PID on Linux.
- `sys.info` must go through policy, approval, policy recheck, execution, and audit.
- Requester cannot approve their own pending request.
- Approval binds request hash, caller identity, session identity, method scope, risk, and expiry.
- Audit log path under an agent writable allowlist is rejected at startup.
- No `proc.*`, `net.*`, file write/delete/mkdir, root daemon, kernel module, driver, direct hardware, shell execution, or MCP server in v0.1.0.

---

### Task 1: Project Skeleton And Contract Tests

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/anything/*.h`
- Create: `src/common/*.c`
- Create: `src/daemon/*.c`
- Create: `src/cli/*.c`
- Create: `src/tools/*.c`
- Create: `tests/contract/test_v0_1_0_contract.py`
- Create: `config/anythingd.example.toml`

**Interfaces:**
- Produces: Linux-only `anythingd` and `anythingctl` CMake targets.
- Produces: `python tests/contract/test_v0_1_0_contract.py` host verification command.

- [x] **Step 1: Write failing contract tests**

The contract test asserts the required skeleton, version, security invariants, and commands exist before implementation.

- [ ] **Step 2: Run test to verify it fails**

Run: `python tests/contract/test_v0_1_0_contract.py`
Expected: FAIL because `CMakeLists.txt` and source files are absent.

- [ ] **Step 3: Implement skeleton**

Add CMake targets, headers, C modules, sample config, and scripts.

- [ ] **Step 4: Run contract test to verify it passes**

Run: `python tests/contract/test_v0_1_0_contract.py`
Expected: PASS.

### Task 2: Minimum Closed Loop Core

**Files:**
- Modify: `src/daemon/anythingd.c`
- Modify: `src/cli/anythingctl.c`
- Modify: `src/common/config.c`
- Modify: `src/common/rpc.c`
- Modify: `src/common/policy.c`
- Modify: `src/common/approval.c`
- Modify: `src/common/audit.c`
- Modify: `src/tools/sys_info.c`

**Interfaces:**
- Consumes: `anything_config_load(const char *, anything_config *)`.
- Produces: JSON-RPC methods `sys.info`, `approval.list`, `approval.approve`, `approval.reject`, `approval.execute`.
- Produces: JSON Lines audit events `request_received`, `approval_required`, `approval_granted`, `approval_rejected`, `execution_started`, `execution_finished`, `execution_failed`.

- [ ] **Step 1: Implement bounded config and RPC parsing**

Use small internal parsers with explicit max request bytes and max scalar lengths.

- [ ] **Step 2: Implement policy and approval**

`sys.info` returns `approval_required` on the tool socket, stores a pending request, and requires admin approval before execution.

- [ ] **Step 3: Implement audit and sys.info**

Audit writes JSON Lines from first request; `sys.info` reads hostname, kernel release, OS release, uptime, and architecture without subprocesses.

- [ ] **Step 4: Add Linux integration entrypoint**

Provide a script documenting `cmake`, `ctest`, daemon launch, CLI request, approval, execute, and audit checks.

### Task 3: Verification, CodeGraph, And Logs

**Files:**
- Create: `scripts/linux_smoke_test.sh`
- Create: desktop development log under `C:\Users\admin\Desktop\anything tool开发日志`

**Interfaces:**
- Produces: verification command list for development report and audit.

- [ ] **Step 1: Run host contract tests**

Run: `python tests/contract/test_v0_1_0_contract.py`

- [ ] **Step 2: Run CMake configure where possible**

Run: `cmake -S . -B build`

- [ ] **Step 3: Run CodeGraph sync**

Run: `codegraph sync .`

- [ ] **Step 4: Record development log**

Write timestamped log signed `开发者`.
