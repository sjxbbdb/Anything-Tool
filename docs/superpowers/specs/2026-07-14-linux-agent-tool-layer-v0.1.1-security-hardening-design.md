# Linux Agent Tool Layer v0.1.1 Security Hardening Design

## Overview

v0.1.1 hardens the v0.1.0 Linux Agent Tool Layer skeleton without adding new tool capabilities. The release keeps the minimum closed loop centered on `sys.info`, but makes the approval boundary, audit behavior, JSON handling, socket reads, parser behavior, and tests strong enough for another source-level security review.

The version string for this batch is `v0.1.1`. When implementation and verification are complete, the final commit will receive a new `v0.1.1` tag. Existing tags must not be moved, deleted, or reused.

## Goals

- Require explicit admin authorization before any approval management method runs.
- Fail closed when audit startup validation or key audit writes fail.
- Ensure RPC responses, approval lists, `sys.info`, and audit JSON Lines are valid JSON.
- Prevent one slow accepted connection from blocking the daemon.
- Replace the permissive `strstr` JSON-RPC parsing pattern with a strict top-level scanner for the supported v0.1.1 JSON subset.
- Add behavior-focused security tests alongside the existing contract test.
- Preserve the v0.1.0 boundary: no process, network, file mutation, root daemon, kernel, driver, hardware, MCP, or shell-execution surface.

## Non-Goals

- No `proc.spawn`, `proc.kill`, `net.http_request`, `fs.write`, `fs.delete`, or `fs.mkdir`.
- No third-party parser dependency in this batch unless the strict scanner proves insufficient during implementation.
- No change to old tags or release history.
- No claim that Linux runtime behavior is verified unless `scripts/linux_smoke_test.sh` runs on a real Linux host and its output is recorded.

## Architecture

The hardening stays within the existing C module boundaries:

- `config`: load and validate new `[admin]` settings, plus audit path safety.
- `json`: new shared escaping and string writer helpers used by RPC, audit, approval, and tools.
- `rpc`: strict top-level JSON-RPC scanner for the supported request shape.
- `audit`: startup validation plus checked writes for key security events.
- `transport`: accepted socket read deadline using `SO_RCVTIMEO`.
- `daemon`: fail-closed routing for audit failures and admin authorization.
- `tests`: Python behavior tests that inspect source contracts on Windows and run deeper Linux smoke checks when Linux is available.

This preserves a small implementation surface while replacing the most fragile hand-built JSON and routing behavior.

## Data Flow

Tool request flow:

1. Accepted tool connection receives a read timeout.
2. Peer credentials are captured with `SO_PEERCRED`.
3. Request bytes are bounded before parser entry.
4. Strict JSON-RPC parser accepts only a top-level object with supported fields.
5. `request_received` audit event is written and checked.
6. Policy preflight denies or creates a pending approval.
7. `approval_required` audit event is written and checked.
8. Response fields are escaped before JSON serialization.

Admin flow:

1. Accepted admin connection receives a read timeout.
2. Peer credentials are captured.
3. Caller must match configured admin UID/GID allowlist before `approval.*` routing.
4. Requester self-approval denial remains in place.
5. Approval/rejection/execution audit events are checked before proceeding.
6. Execute reconstructs the pending request, verifies request hash, reruns policy, writes checked execution audit events, then runs `sys.info`.

## Configuration

`config/anythingd.example.toml` gains:

```toml
[admin]
allowed_uids = []
allowed_gids = []
require_admin_allowlist = false
```

Development configs may set `require_admin_allowlist = false` for local smoke testing, but security tests must cover the required mode. In required mode, a caller is authorized if its UID is in `allowed_uids` or its GID is in `allowed_gids`; otherwise admin methods return `control_plane_denied`.

Audit startup validation must create/open the audit file before sockets are exposed. The daemon must reject audit paths inside configured writable allowlists, missing parent directories, and unwritable files.

## JSON Handling

All JSON string output goes through a shared escaping helper. The helper escapes:

- `"`
- `\`
- newline, carriage return, tab
- control characters below `0x20` as `\u00XX`

RPC result/error builders, approval list output, audit event output, and `sys.info` string fields must use this helper.

The v0.1.1 parser remains intentionally narrow: it accepts the project-supported JSON-RPC object subset and rejects ambiguous or unsupported shapes. It must reject nested keys masquerading as top-level keys, duplicate top-level keys, non-object params, bad `jsonrpc` versions, and strings containing unsupported escapes. Supported simple escapes in strings are decoded consistently for IDs, methods, session IDs, and request IDs.

## Error Handling

New or reinforced error kinds:

- `control_plane_denied`: non-admin caller on admin socket or agent caller on tool socket approval namespace.
- `audit_failed`: audit startup or critical event write failure.
- `invalid_request`: malformed JSON-RPC, duplicate top-level keys, non-object params, unsupported escaping, or oversized params.
- Existing `approval_required`, `approval_rejected`, `approval_expired`, `request_changed`, `policy_denied`, `resource_limit`, and `execution_failed` remain.

If a key audit event cannot be written, the daemon returns `audit_failed` and does not continue to approval or execution.

## Testing

Tests must be written before production changes where practical.

Minimum v0.1.1 tests:

- JSON escaping helper escapes quotes, backslashes, newlines, and control characters.
- `sys.info` and audit output are parseable as JSON when fields contain quotes.
- Admin unauthorized path returns `control_plane_denied`.
- Same requester approval denial remains covered.
- Audit startup failure and key event write failure fail closed.
- Slow accepted client has a read deadline and cannot block subsequent requests.
- Strict parser rejects nested duplicate `method`, non-object params, duplicate top-level keys, bad version, and oversized params.
- Deferred tool names remain absent from runtime source.
- Linux smoke script validates responses and audit lines with Python `json.loads`.

On the current Windows development host, CMake may only configure host contract/behavior checks. Linux runtime claims require a real Linux run of `bash scripts/linux_smoke_test.sh`.

## Release And Tagging

After implementation:

1. Run available host tests and CMake/CTest.
2. Run CodeGraph sync/status.
3. Update implementation report and desktop development log.
4. Commit all changes.
5. Create new tag `v0.1.1` on the verified final commit.
6. Push branch and tag to GitHub.

Existing tags must not be changed. If a release problem is found, rollback can checkout the previous commit or an older tag without rewriting `v0.1.1`.

## Acceptance Criteria

- v0.1.1 version references are updated where release identity is emitted.
- Security report high and medium findings are addressed or explicitly documented as Linux-runtime pending.
- Host tests pass.
- CodeGraph status is `[OK] Index is up to date`.
- New tag `v0.1.1` exists only after the final verified implementation commit.
- GitHub branch and tag are pushed.
