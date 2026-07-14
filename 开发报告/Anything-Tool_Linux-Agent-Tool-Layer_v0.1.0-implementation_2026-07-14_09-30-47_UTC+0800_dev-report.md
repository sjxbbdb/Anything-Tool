# Anything Tool Linux Agent Tool Layer v0.1.0 实现开发报告

报告日期：2026-07-14

明确时间戳：2026-07-14 09:30:47 UTC+0800

项目名：Anything Tool

子项目名：Linux Agent Tool Layer

目标版本：v0.1.0

报告版本：v0.1.0-implementation

## 开发范围

本次开发按 `v0.1.0-development-baseline` 进入最小可运行闭环和安全核心 skeleton，不实现延期工具能力。

已新增：

- C11 + CMake 项目入口。
- `anythingd` Linux daemon skeleton。
- `anythingctl` Linux CLI skeleton。
- 配置加载与 schema 校验。
- tool/admin 双 Unix socket transport。
- Linux `SO_PEERCRED` peer identity 获取。
- 最小 JSON-RPC 2.0 子集解析、响应与错误格式。
- policy preflight。
- approval pending store、approve、reject、execute。
- requester 不能审批自己的 pending request 检查。
- request hash 绑定与 execute 前校验。
- execute 前 policy recheck。
- JSON Lines audit log。
- `sys.info` 只读工具。
- Windows 开发主机可运行的 contract test。
- Linux smoke test 脚本。

未实现且仍明确排除：

- `proc.spawn`
- `proc.kill`
- `net.http_request`
- 文件写入、删除、目录创建
- root daemon
- 内核模块、驱动、硬件控制
- MCP server 包装层
- 任意 shell 执行能力

## 关键文件

- `CMakeLists.txt`
- `include/anything/*.h`
- `src/common/*.c`
- `src/daemon/anythingd.c`
- `src/cli/anythingctl.c`
- `src/tools/sys_info.c`
- `config/anythingd.example.toml`
- `tests/contract/test_v0_1_0_contract.py`
- `scripts/linux_smoke_test.sh`
- `docs/superpowers/plans/2026-07-14-linux-agent-tool-layer-v0.1.0.md`

## 验证命令与结果

当前开发主机是 Windows，不能真实编译 Linux `SO_PEERCRED`/Unix socket 目标；本次已执行可在当前主机验证的命令：

- `python tests\contract\test_v0_1_0_contract.py`
  - 结果：通过，输出 `v0.1.0 contract tests passed`
- `cmake -S . -B build`
  - 结果：通过，生成 build files；CMake 提示 Linux-only target 在当前主机只能运行 contract tests。
- `ctest --test-dir build --output-on-failure`
  - 结果：通过，`1/1 Test #1: v0_1_0_contract Passed`
- `cmake --build build`
  - 结果：通过，`ninja: no work to do.`
- `codegraph sync .`
  - 结果：同步 18 个变更文件。
- `codegraph index .`
  - 结果：索引 18 个文件，170 nodes，374 edges。
- `codegraph status .`
  - 结果：`[OK] Index is up to date`

Linux 完整验证入口：

- `bash scripts/linux_smoke_test.sh`

该脚本会执行 CMake Debug 构建、CTest、启动 `anythingd`、用 `anythingctl` 发起 `sys.info`、检查 `approval_required`、执行 admin approve/execute，并验证 audit log 包含 request、approval、execution 事件。

## 风险与限制

- 当前 Windows 开发主机没有可用 WSL，未能执行真实 Linux 编译与运行 smoke test。
- v0.1.0 使用内部有界 JSON/TOML 子集解析器，仅服务最小闭环；后续若扩展输入形态，应引入已审计 parser 或增强测试。
- requester/approver 分离当前以 `SO_PEERCRED` 的 UID/GID/PID 作为身份模型；同一 UID 的不同进程在测试环境中可代表不同调用身份。
- approval pending store 当前为 daemon 内存态，符合 v0.1.0 skeleton 范围；daemon 重启会清空 pending。
- 仓库未配置 GitHub `origin`，因此当前无法推送；需要确认远程仓库 URL 后再配置 remote 并推送。

## CodeGraph

已运行：

- `codegraph sync .`
- `codegraph index .`
- `codegraph status .`

最终状态：`[OK] Index is up to date`

## GitHub 推送状态

当前 `git remote -v` 无输出，仓库没有 `origin`。未使用 GitHub API key，未执行推送。待确认 GitHub 仓库 URL 后可配置 remote 并推送。

## 署名

开发者
