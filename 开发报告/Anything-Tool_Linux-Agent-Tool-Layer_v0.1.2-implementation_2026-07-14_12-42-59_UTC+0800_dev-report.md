# Anything Tool Linux Agent Tool Layer v0.1.2 实现开发报告

报告日期：2026-07-14

明确时间戳：2026-07-14 12:42:59 UTC+0800

项目名：Anything Tool

子项目名：Linux Agent Tool Layer

目标版本：v0.1.2

报告版本：v0.1.2-implementation

## 开发范围

本次开发依据 `v0.1.2-security-closure` 开发报告和 `v0.1.1` 源码审核报告执行。范围限定为安全闭环收口、默认安全配置、request deadline、audit path 加固、测试增强和验证记录。

未新增任何工具能力。

## 已完成内容

- 版本更新为 `v0.1.2`。
- `require_admin_allowlist` 默认改为启用。
- 普通配置加载在 admin allowlist 为空时拒绝启动。
- 新增 `anything_config_load_with_options`，仅显式允许时可加载 insecure admin 配置。
- `anythingd` 新增 `--dev-insecure-admin` flag，并打印明确警告。
- `config/anythingd.example.toml` 改为安全默认配置。
- 新增 `config/anythingd.dev-insecure.toml`，文件头标明 `INSECURE DEVELOPMENT ONLY`。
- `scripts/linux_smoke_test.sh` 改为显式使用 dev-insecure 配置和 flag。
- `anything_transport_read_request` 增加 request-level `deadline_ms`，使用 `CLOCK_MONOTONIC` 限制整体读取耗时。
- audit startup validation 与 audit write 使用 `open(..., O_NOFOLLOW)`、`fstat` 和 fd 写入。
- audit path 与 writable allowlist 比较优先使用 `realpath` canonical 比较，失败时退回原字符串关系。
- 新增 `tests/security/test_v0_1_2_security_behavior.py`。
- CTest 新增 `v0_1_2_security_behavior`。

## 仍禁止内容

- `proc.spawn`
- `proc.kill`
- `net.http_request`
- `fs.write`
- `fs.delete`
- `fs.mkdir`
- MCP server wrapper
- root daemon
- 内核模块、驱动、硬件控制
- 任意 shell 执行能力

## 变更文件

- `CMakeLists.txt`
- `include/anything/config.h`
- `include/anything/transport.h`
- `src/common/audit.c`
- `src/common/config.c`
- `src/common/transport.c`
- `src/daemon/anythingd.c`
- `config/anythingd.example.toml`
- `config/anythingd.dev-insecure.toml`
- `scripts/linux_smoke_test.sh`
- `tests/contract/test_v0_1_0_contract.py`
- `tests/security/test_v0_1_1_security_contract.py`
- `tests/security/test_v0_1_2_security_behavior.py`
- `docs/superpowers/specs/2026-07-14-linux-agent-tool-layer-v0.1.2-security-closure-design.md`
- `docs/superpowers/plans/2026-07-14-linux-agent-tool-layer-v0.1.2-security-closure.md`

## 验证命令与结果

已在当前 Windows 开发主机运行：

- `python tests\contract\test_v0_1_0_contract.py`
  - 结果：通过，输出 `v0.1.2 contract tests passed`
- `python tests\security\test_v0_1_1_security_contract.py`
  - 结果：通过，输出 `v0.1.2 security contract tests passed`
- `python tests\security\test_v0_1_2_security_behavior.py`
  - 结果：通过，输出 `v0.1.2 security behavior tests passed`
- `cmake -S . -B build`
  - 结果：通过；当前 Windows 主机提示 Linux-only targets 只能运行 contract tests。
- `ctest --test-dir build --output-on-failure`
  - 结果：通过，3/3 tests passed。
- `cmake --build build`
  - 结果：通过，`ninja: no work to do.`
- `git diff --check`
  - 结果：无 whitespace error，仅 Windows 换行提示。
- `codegraph sync .`
  - 结果：成功，同步 9 个变更文件。
- `codegraph status .`
  - 结果：`[OK] Index is up to date`，22 files，233 nodes，587 edges。

未运行：

- `bash scripts/linux_smoke_test.sh`

原因：当前机器没有可用 Linux/WSL 环境。不能声称 Linux runtime、Unix socket、`SO_PEERCRED`、approval flow、audit fail-closed、request deadline 已经通过真实 Linux 运行验证。

## CodeGraph

已运行：

- `codegraph sync .`
- `codegraph status .`

最终状态：`[OK] Index is up to date`

## Git 与 Tag

计划在最终实现提交后创建新 tag：

- `v0.1.2`

已有 tag 不移动、不删除、不复用。

## 风险与后续事项

- 当前 Linux runtime smoke 仍未运行，必须在真实 Ubuntu/Linux 环境执行并记录完整输出后再申请安全可扩展基线审核。
- audit canonical/fd-based 模型已有 `O_NOFOLLOW` 和 canonical 比较增强，但尚未达到完整 openat/fd-root 模型。
- admin allowlist 是 UID/GID 授权，不等同强人类认证；生产仍建议 dedicated admin group 和受控 socket 目录。
- JSON parser 仍是项目内严格子集 scanner，复杂 params 扩展前应替换或继续增强测试。

## 署名

开发者
