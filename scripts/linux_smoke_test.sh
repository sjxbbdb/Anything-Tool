#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build-linux-smoke"
CONFIG="${ROOT}/config/anythingd.example.toml"
TOOL_SOCKET="/tmp/anythingd-tool.sock"
ADMIN_SOCKET="/tmp/anythingd-admin.sock"
AUDIT_LOG="/tmp/anything-audit.jsonl"

cmake -S "${ROOT}" -B "${BUILD}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "${BUILD}" --target anythingd anythingctl
ctest --test-dir "${BUILD}" --output-on-failure

rm -f "${TOOL_SOCKET}" "${ADMIN_SOCKET}" "${AUDIT_LOG}"
"${BUILD}/anythingd" "${CONFIG}" &
DAEMON_PID=$!
trap 'kill "${DAEMON_PID}" 2>/dev/null || true; wait "${DAEMON_PID}" 2>/dev/null || true' EXIT

for _ in $(seq 1 20); do
  [[ -S "${TOOL_SOCKET}" && -S "${ADMIN_SOCKET}" ]] && break
  sleep 0.1
done

json_check() {
  python - "$1" <<'PY'
import json, sys
json.loads(sys.argv[1])
PY
}

REQUEST_OUTPUT="$("${BUILD}/anythingctl" --socket "${TOOL_SOCKET}" sys-info smoke-session)"
echo "${REQUEST_OUTPUT}"
json_check "${REQUEST_OUTPUT}"
grep -q "approval_required" <<<"${REQUEST_OUTPUT}"
REQUEST_ID="$(sed -n 's/.*"request_id":"\([^"]*\)".*/\1/p' <<<"${REQUEST_OUTPUT}")"
test -n "${REQUEST_ID}"

PENDING_OUTPUT="$("${BUILD}/anythingctl" --admin-socket "${ADMIN_SOCKET}" pending)"
json_check "${PENDING_OUTPUT}"
APPROVE_OUTPUT="$("${BUILD}/anythingctl" --admin-socket "${ADMIN_SOCKET}" approve "${REQUEST_ID}")"
json_check "${APPROVE_OUTPUT}"
EXECUTE_OUTPUT="$("${BUILD}/anythingctl" --admin-socket "${ADMIN_SOCKET}" execute "${REQUEST_ID}")"
json_check "${EXECUTE_OUTPUT}"
grep -q '"hostname"' <<<"${EXECUTE_OUTPUT}"

grep -q "request_received" "${AUDIT_LOG}"
grep -q "approval_granted" "${AUDIT_LOG}"
grep -q "execution_finished" "${AUDIT_LOG}"

python - "${AUDIT_LOG}" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as fh:
    for line in fh:
        json.loads(line)
print("audit json lines valid")
PY

echo "anything v0.1.1 smoke test passed"
