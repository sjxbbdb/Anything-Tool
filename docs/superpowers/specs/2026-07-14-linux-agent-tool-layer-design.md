# Linux Agent Tool Layer Design

## Overview

This project builds a Linux user-space tool layer written in C. It gives agents a controlled way to inspect and operate on ordinary OS resources while keeping every capability inside explicit safety boundaries.

The first version targets Ubuntu/Linux and focuses on general OS abilities: system observation, bounded file operations, bounded process operations, and bounded network probes. It does not include root-only hardware control, kernel modules, drivers, or direct device control. Those can be added later through separate helpers with their own approval and isolation model.

## Goals

- Provide an agent-callable tool layer with a stable request/response protocol.
- Keep the core implementation in C and close to Linux system APIs.
- Run in user space with least privilege by default.
- Enforce a default-deny policy before any operation reaches a system call.
- Require system preflight and human approval before sensitive tools execute.
- Keep all approvals, denials, and executions auditable.
- Leave a clean path toward a future MCP server wrapper.

## Non-Goals

- No kernel module or privileged driver work in the first version.
- No direct hardware control in the first version.
- No root daemon in the first version.
- No automatic execution of high-risk agent requests.
- No broad shell execution surface.

## Architecture

The first version has two executables:

- `anythingd`: a daemon that owns transport, policy checks, approvals, execution, and audit logging.
- `anythingctl`: a CLI client used by humans, tests, and agents.

`anythingd` listens on a Unix domain socket. In development it runs as the current user and can use a path like `/run/user/<uid>/anythingd.sock`. In server deployments it should run as a dedicated `anythingd` system user and use a path like `/run/anythingd/anythingd.sock`.

`anythingctl` is a thin client. It converts subcommands or raw JSON into daemon requests, sends them over the socket, and prints JSON responses. Future MCP support should be implemented as a wrapper that translates MCP tool calls into the same JSON-RPC methods.

Internal daemon boundaries:

- `transport`: Unix domain socket accept/read/write.
- `rpc`: JSON-RPC 2.0 parsing, validation, dispatch, and response formatting.
- `policy`: default-deny capability and resource checks.
- `approval`: pending request storage, approval scopes, expiry, and request hash checks.
- `audit`: JSON Lines event logging.
- `tool_registry`: method lookup and tool metadata.
- `tools/sys`: read-only system information.
- `tools/fs`: bounded filesystem operations.
- `tools/proc`: process list and approved command execution.
- `tools/net`: approved DNS, TCP, and HTTP operations.

The daemon must treat tool execution and approval administration as separate control planes. Agent-originated tool requests and human/admin approval actions may share transport code, but they must be distinguishable in protocol routing and authorization checks. The daemon must use Unix peer credentials, such as `SO_PEERCRED`, to record caller UID, GID, and PID for every request.

## Protocol

The daemon accepts JSON-RPC 2.0 requests. Tool names and parameter shapes should stay close to MCP-style tool names and schemas so a future MCP server can be a translation layer rather than a redesign.

Example request:

```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "fs.read",
  "params": {
    "path": "/home/ubuntu/workspace/README.md",
    "max_bytes": 65536
  }
}
```

Successful responses use standard JSON-RPC result objects. Errors use standard JSON-RPC error objects with a structured `data` object.

## Configuration

Configuration uses TOML. All capabilities default to disabled unless the configuration explicitly enables them.

Example:

```toml
[daemon]
socket_path = "/run/user/%uid/anythingd.sock"
admin_socket_path = "/run/user/%uid/anythingd-admin.sock"
audit_log = "~/.local/state/anything/audit.log"
audit_hash_chain = true

[capabilities]
sys_read = true
fs_read = true
fs_write = false
fs_delete = false
proc_list = true
proc_spawn = false
proc_kill = false
net_probe = false
net_http = false

[[paths]]
path = "/home/ubuntu/workspace"
mode = "read-write"

[[commands]]
name = "git_status"
argv0 = "/usr/bin/git"
allowed_cwd = ["/home/ubuntu/workspace"]
stdin = "disabled"
env_allowlist = ["LANG", "LC_ALL"]
max_stdout_bytes = 1048576
max_stderr_bytes = 262144
timeout_ms = 10000

[[commands.argv]]
exact = ["status", "--short"]

[[commands.argv]]
exact = ["status", "--porcelain"]

[[hosts]]
host = "api.example.com"
ports = [443]
schemes = ["https"]
allow_private_ips = false
allow_ip_literals = false

[limits]
max_request_bytes = 1048576
max_output_bytes = 1048576
command_timeout_ms = 10000
max_file_read_bytes = 1048576
approval_ttl_seconds = 300
max_redirects = 3
http_timeout_ms = 10000
```

The policy layer checks capability flags, path allowlists, command allowlists, host allowlists, output limits, timeouts, and request size before any tool executes.

Command allowlists must be structured schemas, not substring checks. A command entry must define an absolute executable path, allowed working directories, environment handling, stdin handling, output limits, timeout, and exact or pattern-validated argv forms.

The daemon must reject startup if the audit log path is inside any path allowlist that grants agent write access.

## Permission Model

Development mode runs as the current user. Production deployment should use a dedicated `anythingd` user with only the filesystem, process, and network permissions needed by the configured tools.

The main daemon should not run as root in the first version. Future high-privilege operations should be implemented as separate, narrow helpers with their own policy and approval checks.

Approval administration requires a human/admin identity. The first version may implement this through a separate admin socket with stricter filesystem permissions, or through the same socket with a distinct admin method namespace and peer-credential authorization. In either case, agent clients must not be able to call approval management methods.

## Security Invariants

These rules are non-bypassable acceptance criteria for the first implementation:

- All capabilities default to disabled. A missing capability flag means deny.
- Policy violations are rejected before the first tool-specific system call.
- Every allowed, denied, pending, approved, rejected, started, and finished request writes an audit event.
- Agent-originated tool requests and human/admin approval actions are separate control planes.
- Agent clients cannot call approval management methods.
- The requester of a pending operation cannot approve that same operation.
- Approval binds to request hash, caller identity, session identity, method or capability scope, risk level, and expiry.
- Approved requests are rechecked against current policy before execution.
- Filesystem access must use fd-based path resolution under allowed roots. String prefix checks alone are invalid.
- Filesystem operations must prevent symlink, `..`, hard-link, and rename-race escapes from allowed roots.
- Audit log paths must never be inside an agent-writable allowlist.
- `proc.spawn` never invokes a shell. It executes an absolute binary path with structured argv validation.
- Spawned commands run with a minimized environment and disabled stdin unless explicitly configured.
- `proc.kill` can only affect processes created and tracked by this tool layer.
- Network tools must revalidate DNS results, redirects, final IPs, scheme, port, Host header, and TLS SNI against policy.
- Any sensitive value in audit logs is summarized or hashed rather than stored in full.

## Approval Flow

Every agent-originated request goes through preflight before execution:

1. Validate JSON-RPC shape and parameters.
2. Check static policy: capability, path, command, host, and limits.
3. Assign risk: `low`, `medium`, `high`, or `denied`.
4. Generate a human-readable approval summary.
5. If the request violates hard policy, deny it immediately.
6. If approval is required, store the pending request with a hash and return `approval_required`.
7. A human grants or rejects the request through `anythingctl`.
8. Before execution, reload the pending request, verify its hash, expiry, and scope, then run policy again.
9. Execute the tool and write audit events.

Approval scopes:

- Per-call approval is the default.
- Time-bound capability approval may allow a method group such as `sys.*` for a short window.
- Session-bound approval may allow a specific agent session to use selected read-only methods.

All approval scopes must include an expiry time, allowed methods or capability groups, caller/session identity, and audit records.

Approval actions must record both identities: the original requester's peer credentials and the approval actor's peer credentials. If those identities match, the daemon must deny the approval. Tests may use a dedicated test admin identity, but the same separation rule still applies.

Example approval-required response:

```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32010,
    "message": "approval required",
    "data": {
      "kind": "approval_required",
      "request_id": "req_...",
      "risk": "medium",
      "summary": "Read /home/ubuntu/workspace/README.md",
      "expires_at": "2026-07-14T12:30:00Z"
    }
  }
}
```

## Initial Tools

### `sys.*`

- `sys.info`: kernel version, hostname, uptime, OS release.
- `sys.cpu`: CPU count, load average, usage information.
- `sys.memory`: total and available memory.
- `sys.disk`: mounted filesystems and capacity.
- `sys.network_interfaces`: interface names and addresses.

These tools are read-only and can be configured for low-risk auto-approval.

### `fs.*`

- `fs.list`: list a directory under an allowed path.
- `fs.read`: read a file under an allowed path with byte limits.
- `fs.write`: write under an allowed path; disabled by default.
- `fs.mkdir`: create directories under an allowed path; disabled by default.
- `fs.delete`: delete files under an allowed path; disabled by default and high risk.

Filesystem paths must be resolved relative to an allowed root file descriptor. On Linux kernels that support it, implementation should use `openat2` with constraints such as `RESOLVE_BENEATH` and `RESOLVE_NO_SYMLINKS` where appropriate. File opens should use `O_NOFOLLOW` when symlinks are not allowed and must verify the opened object with `fstat`.

Fallback behavior for systems without `openat2` must be explicit. If the fallback cannot prevent escape races for a mutating operation, that operation must be disabled rather than silently downgraded. Tests must cover symlink escape, `..` escape, hard-link behavior, rename races, and directory replacement.

### `proc.*`

- `proc.list`: list visible processes.
- `proc.spawn`: start an allowlisted command; disabled by default and approval-gated.
- `proc.status`: inspect a process started by this tool layer.
- `proc.output`: read bounded output from a spawned process.
- `proc.kill`: terminate only processes started by this tool layer; approval-gated by default.

The daemon must never expose arbitrary shell execution. Commands are executed by absolute path with `execve`, `posix_spawn`, or an equivalent non-shell API. Argument validation must follow the structured command schema from configuration. Environment variables are cleared by default and restored only from an allowlist. Stdin is disabled by default.

Every spawned process belongs to a daemon-managed process group. The daemon must asynchronously drain stdout and stderr to avoid pipe deadlock. Output limits apply independently to stdout and stderr; when a limit is exceeded, the daemon records truncation and may terminate the process according to the command policy. Timeouts terminate the full process group, then reap children to avoid zombies. `proc.kill` can only target process IDs that the daemon created and still tracks.

### `net.*`

- `net.dns_lookup`: resolve an allowed name.
- `net.tcp_probe`: check TCP connectivity to an allowed host and port.
- `net.http_request`: make an HTTP request to an allowed host; disabled by default.

Network tools must enforce host, port, scheme, IP range, redirect, header, body, and timeout limits. DNS answers are pinned for the request lifetime. Redirects are followed only when every hop passes the same host, port, scheme, and IP range policy. IP literals are denied unless explicitly allowed. Loopback, link-local, private, multicast, and other special-use addresses are denied unless explicitly allowed. For HTTPS, TLS SNI and the HTTP Host header must match the allowlisted host.

## Error Handling

Errors use JSON-RPC error responses with structured `data.kind` values.

Initial error kinds:

- `invalid_request`: invalid JSON-RPC structure.
- `unknown_tool`: method is not registered.
- `invalid_params`: missing or invalid method parameters.
- `policy_denied`: hard policy denial.
- `approval_required`: human approval is required.
- `approval_expired`: approval was not granted in time.
- `request_changed`: pending request hash does not match.
- `control_plane_denied`: caller is not allowed to use approval/admin methods.
- `resource_limit`: timeout, output limit, file size limit, or request size limit.
- `execution_failed`: underlying system operation failed.

## Audit Logging

Audit logs use JSON Lines. Each line is one event that can be inspected with command-line tools or shipped to a log system.

Initial event types:

- `request_received`
- `preflight_allowed`
- `preflight_denied`
- `approval_required`
- `approval_granted`
- `approval_rejected`
- `execution_started`
- `execution_finished`
- `execution_failed`

Each event includes timestamp, request ID, session ID when available, caller UID, caller GID, caller PID, approval UID/GID/PID when relevant, method, risk, decision, parameter summary, error kind when present, and duration when relevant. Sensitive values are summarized or hashed rather than logged in full.

The audit log must have a tamper-resistance model. At minimum, the daemon rejects audit paths inside agent-writable allowlists and creates log directories/files with restrictive permissions. Production deployments should prefer journald or syslog forwarding under the dedicated `anythingd` user. File logging should use append-only behavior where available and may include a hash chain so deletion or modification is detectable.

## Testing Strategy

Testing focuses on safety boundaries before feature breadth.

- Policy unit tests cover default denial, capability flags, path allowlists, symlink escape attempts, `..` escape attempts, hard-link behavior, rename-race attempts, command schemas, host allowlists, private IP denial, redirect denial, and resource limits.
- Approval tests cover no-execution-before-approval, approval success, approval rejection, expiry failure, scope checks, request hash mismatch, requester/approver identity separation, and agent denial on approval APIs.
- RPC tests cover valid requests, invalid JSON-RPC, unknown methods, and invalid parameters.
- Tool tests cover read-only system observation, bounded filesystem operations, process group timeout, stdout/stderr drain, output truncation, zombie reaping, and network target denial.
- Integration tests start `anythingd`, send requests through `anythingctl`, approve a pending request through an admin path, execute it, and verify audit logs.
- Startup tests verify that unsafe audit log locations are rejected.

## First Implementation Boundary

The first build should create the project skeleton, configuration loading, JSON-RPC transport, policy preflight, approval storage, audit logging, and a small subset of low-risk tools. The safest useful slice is:

- `sys.info`
- `fs.list`
- `fs.read`
- per-call approval flow
- agent/admin identity separation
- JSON Lines audit log
- CLI commands for raw request, pending approvals, approve, and reject
- startup rejection for audit logs under agent-writable paths

After that slice works end to end, add process and network tools behind disabled-by-default capabilities.
