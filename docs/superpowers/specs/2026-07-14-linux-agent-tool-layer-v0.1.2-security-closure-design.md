# Linux Agent Tool Layer v0.1.2 Security Closure Design

## Overview

v0.1.2 closes the remaining safety gaps from the v0.1.1 review. It does not add tool capabilities. The release changes the default posture from development-convenient to secure-by-default, adds an explicit insecure development escape hatch, strengthens request read deadlines, and improves audit log path handling.

The code version and release tag are `v0.1.2`. Existing tags must not be moved.

## Goals

- Default `require_admin_allowlist` to enabled.
- Reject startup when admin allowlist is empty unless `--dev-insecure-admin` is passed.
- Split secure example config from insecure development config.
- Add total request read deadline in addition to `SO_RCVTIMEO`.
- Reject symlink final audit log paths and use canonical path comparison where available.
- Add v0.1.2 security behavior/source tests for the above.
- Keep Linux smoke validation explicit; do not claim Linux runtime success unless the script runs on Linux.

## Non-Goals

- No `proc.*`, `net.*`, `fs.*` expansion.
- No MCP wrapper.
- No root daemon, kernel module, driver, hardware control, or shell execution.

## Design

`config` gains secure defaults: `require_admin_allowlist = true`. Existing `anything_config_load` remains secure. A new `anything_config_load_with_options(path, config, allow_dev_insecure_admin, error, len)` lets `anythingd --dev-insecure-admin CONFIG` load insecure dev configs intentionally.

`anythingd` parses `--dev-insecure-admin`, logs a stderr warning, and only then permits `require_admin_allowlist = false`.

`transport` changes `anything_transport_read_request` to accept a `deadline_ms` and use monotonic elapsed time across the full request read loop. This prevents clients from extending the request forever by trickling bytes before each socket timeout.

`audit` validation and writes use `open(..., O_NOFOLLOW)` and `fstat` before append. Startup validation rejects symlink final targets. Writable allowlist comparisons use canonical parent paths when possible.

Tests remain host-runnable on Windows, but assert the security defaults and Linux validation hooks. The Linux smoke script uses the explicit dev-insecure config only for local smoke unless a caller supplies a secure config.

## Acceptance

- Host contract/security tests pass.
- CMake/CTest pass on the current host.
- CodeGraph status is `[OK] Index is up to date`.
- New tag `v0.1.2` points to the final implementation commit and is pushed.
- Linux smoke remains marked not-run unless actually executed on Linux.
