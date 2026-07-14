# Anything Tool Linux Agent Tool Layer v0.1.1 实现开发报告

报告日期：2026-07-14

明确时间戳：2026-07-14 11:41:11 UTC+0800

项目名：Anything Tool

子项目名：Linux Agent Tool Layer

目标版本：v0.1.1

报告版本：v0.1.1-implementation

## 开发范围

本次开发依据：

- `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.1-security-hardening_2026-07-14_10-19-26_UTC+0800_dev-report.md`
- `审核报告/Anything-Tool-v0.1.0-2026714955-Linux-Agent-Tool-Layer-源码审核报告.md`
- `docs/superpowers/specs/2026-07-14-linux-agent-tool-layer-v0.1.1-security-hardening-design.md`
- `docs/superpowers/plans/2026-07-14-linux-agent-tool-layer-v0.1.1-security-hardening.md`

本版本只做安全加固和验证增强，不新增任何工具能力。

## 已完成内容

- 版本更新为 `v0.1.1`。
- 新增 `include/anything/json.h` 和 `src/common/json.c`，统一 JSON 字符串转义。
- `rpc`、`audit`、`approval list`、`sys.info` 输出路径接入 JSON escaping。
- `src/common/rpc.c` 从 `strstr` 字段查找改为严格顶层 JSON-RPC scanner。
- parser 拒绝 duplicate top-level key、non-object params、bad version、unsupported escapes、trailing data 等形态。
- 新增 audit startup validation：启动前验证 audit log 父目录和可写性。
- daemon 关键 audit 写入改为 checked/fail-closed，失败时返回 `audit_failed`。
- 新增 admin UID/GID allowlist 配置和 `anything_config_identity_is_admin`。
- admin socket 在处理 `approval.*` 前校验授权，非授权返回 `control_plane_denied`。
- accepted socket 增加 `SO_RCVTIMEO` read timeout。
- Linux smoke script 增加 `json.loads` 校验 response 和 audit JSON Lines。
- 新增 `tests/security/test_v0_1_1_security_contract.py`。
- CTest 新增 `v0_1_1_security_contract`。

## 明确未实现内容

本版本继续禁止：

- `proc.spawn`
- `proc.kill`
- `net.http_request`
- `fs.write`
- `fs.delete`
- `fs.mkdir`
- root daemon
- 内核模块、驱动、硬件控制
- MCP server wrapper
- 任意 shell 执行能力

## 变更文件

- `CMakeLists.txt`
- `include/anything/audit.h`
- `include/anything/config.h`
- `include/anything/json.h`
- `include/anything/transport.h`
- `src/common/approval.c`
- `src/common/audit.c`
- `src/common/config.c`
- `src/common/json.c`
- `src/common/rpc.c`
- `src/common/transport.c`
- `src/daemon/anythingd.c`
- `src/tools/sys_info.c`
- `config/anythingd.example.toml`
- `scripts/linux_smoke_test.sh`
- `tests/contract/test_v0_1_0_contract.py`
- `tests/security/test_v0_1_1_security_contract.py`
- `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.1-implementation_2026-07-14_11-28-04_UTC+0800_dev-report.md`

## 验证命令与结果

已在当前 Windows 开发主机运行：

- `python tests\contract\test_v0_1_0_contract.py`
  - 结果：通过，输出 `v0.1.1 contract tests passed`
- `python tests\security\test_v0_1_1_security_contract.py`
  - 结果：通过，输出 `v0.1.1 security contract tests passed`
- `cmake -S . -B build`
  - 结果：通过；当前 Windows 主机提示 Linux-only targets 只能运行 contract tests。
- `ctest --test-dir build --output-on-failure`
  - 结果：通过，2/2 tests passed。
- `cmake --build build`
  - 结果：通过，`ninja: no work to do.`
- `codegraph sync .`
  - 结果：成功，同步 14 个变更文件。
- `codegraph status .`
  - 结果：`[OK] Index is up to date`，21 files，214 nodes，538 edges。

未运行：

- `bash scripts/linux_smoke_test.sh`

原因：当前机器没有可用 Linux/WSL 环境。不能声称 Linux runtime、Unix socket、`SO_PEERCRED` 和真实 approval flow 已经通过运行验证。Linux 完整验证仍需在真实 Linux 环境执行并记录输出。

## CodeGraph

已运行：

- `codegraph sync .`
- `codegraph status .`

最终状态：`[OK] Index is up to date`

## Git 与 Tag

本报告所在提交计划作为 `v0.1.1` tag 目标提交。tag 创建命令：

- `git tag -a v0.1.1 -m "Linux agent tool layer v0.1.1 security hardening"`

已有 tag 不会移动、删除或复用。推送将在 final commit 和 tag 创建后执行：

- `git push origin master`
- `git push origin v0.1.1`

## 风险与后续事项

- 当前 JSON parser 仍是项目内严格子集 scanner，不是完整 JSON parser；后续扩展 tool params 前建议引入已审计 JSON parser。
- audit path 安全仍是路径字符串和启动可写性验证，未覆盖 symlink/canonical fd-based audit 打开模型。
- admin allowlist 是 UID/GID 级授权，不等同强人类认证；生产部署仍应使用 dedicated admin group 和目录权限隔离。
- 必须在 Linux 环境运行 `bash scripts/linux_smoke_test.sh` 后再进入下一轮源码审核。

## 署名

开发者
