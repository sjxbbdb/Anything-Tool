# Anything Tool Linux Agent Tool Layer v0.1.0 开发基准报告

报告日期：2026-07-14

明确时间戳：2026-07-14 08:58:40 UTC+0800

项目名：Anything Tool

子项目名：Linux Agent Tool Layer

目标版本：v0.1.0

报告版本：v0.1.0-development-baseline

基准状态：开发准入有条件通过。本报告作为后续 v0.1.0 实现开发的基准。

面向读者：后续开发者、实现者、安全审核者、测试与验收人员

## 基准依据

本报告依据以下已确认文档生成：

- `docs/superpowers/specs/2026-07-14-linux-agent-tool-layer-design.md`
- `docs/superpowers/specs/2026-07-14-linux-agent-tool-layer-design-review.md`
- `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.0-design_2026-07-14_08-31-10_UTC+0800_dev-report.md`
- `审核报告/Anything-Tool-v0.1.0-design-2026714854-Linux-Agent-Tool-Layer-开发准入审核报告.md`

开发准入审核结论为：有条件通过。可以开始开发，但开发范围必须限定为最小可运行闭环和安全核心 skeleton。不得直接进入扩展工具能力开发。

## 项目背景

Anything Tool 的 Linux Agent Tool Layer 是一个面向 agent 的用户态系统工具层。它使用 C 开发，优先部署在 Ubuntu/Linux 云服务器上。它的职责是在 agent 和操作系统能力之间建立一层受控边界，使 agent 能够在安全范围内请求系统信息、有限文件读取和后续受限系统能力。

长期方向是允许 agent 在可审计、可审批、可限制的框架下间接调用系统能力。第一版不做 root daemon、内核模块、驱动、直接硬件控制，也不提供任意 shell 执行能力。

当前仓库状态仍是开发前状态：已有设计文档、审核报告和开发报告；尚无 C 源码、构建系统、测试资产和可执行交付物。

## 项目目标

v0.1.0 的目标不是实现完整工具平台，而是交付一条安全核心闭环：

`agent request -> policy preflight -> approval required -> admin approval -> policy recheck -> sys.info execution -> audit log`

v0.1.0 必须证明以下事情已经落地：

- C 项目 skeleton 可构建。
- daemon 可以启动并加载 TOML 配置。
- Unix domain socket transport 可用。
- daemon 能通过 `SO_PEERCRED` 获取调用方 UID/GID/PID。
- tool request 与 admin approval 控制面隔离。
- JSON-RPC 请求可以被解析、路由和统一错误响应。
- policy 在任何 tool-specific syscall 前执行。
- agent 请求默认进入审批流程。
- requester 不能审批自己的 pending request。
- approval 绑定 request hash、caller identity、session identity、scope、risk、expiry。
- 批准后执行前必须重新校验 policy 和 request hash。
- `sys.info` 可以通过完整闭环返回 JSON result。
- audit log 从第一条闭环开始写入 JSON Lines。
- 自动化测试能验证该闭环。

## 项目内容

v0.1.0 只包含安全核心和最小工具能力。

必须包含：

- `anythingd` daemon。
- `anythingctl` CLI。
- CMake 或等价构建入口。
- 基础测试框架。
- 配置加载和 schema 校验。
- Unix domain socket transport。
- `SO_PEERCRED` 调用方身份获取。
- JSON-RPC 2.0 基础解析、路由和错误响应。
- policy preflight。
- approval pending store。
- admin approval flow。
- JSON Lines audit log。
- `sys.info` 工具。
- 最小集成测试。

必须暂缓：

- `proc.spawn`
- `proc.kill`
- `net.http_request`
- 文件写入、删除、目录创建
- root daemon
- 内核模块、驱动、硬件控制
- MCP server 包装层
- 任何任意 shell 执行能力

`fs.list` 和 `fs.read` 可以作为 v0.1.0 后半段候选项，但只有在 `sys.info` 最小闭环完成并通过测试后才能进入实现。

## 本次开发内容

后续 v0.1.0 开发应从以下内容开始：

1. 建立仓库工程结构。
2. 建立 CMake 构建入口。
3. 建立测试入口。
4. 明确 JSON parser、TOML parser 和 C 测试框架。
5. 实现 `anythingd` 基础启动流程。
6. 实现配置文件加载和基础 schema 校验。
7. 实现 tool socket 与 admin socket。
8. 实现 peer credential 获取。
9. 实现 JSON-RPC 请求解析和统一错误响应。
10. 实现 method registry。
11. 实现 policy preflight。
12. 实现 approval pending store。
13. 实现 admin approve/reject。
14. 实现 audit log。
15. 实现 `sys.info`。
16. 实现最小闭环集成测试。

任何新增工具能力都不得早于第 16 项完成。

## 建议技术实现

### 构建系统

建议使用 C11 + CMake。

最低要求：

- 顶层 `CMakeLists.txt`。
- 默认开启严格编译警告。
- Debug 构建支持 AddressSanitizer 和 UndefinedBehaviorSanitizer。
- 测试可通过 `ctest` 或明确脚本运行。

建议目录：

```text
src/
  daemon/
  cli/
  common/
  tools/
include/
  anything/
tests/
  unit/
  integration/
config/
  anythingd.example.toml
```

### 依赖选型

开发 parser/config 代码前必须定案：

- JSON parser 具体库名、版本、license、来源、引入方式。
- TOML parser 具体库名、版本、license、来源、引入方式。
- 单元测试框架。
- 集成测试脚本语言。
- sanitizer 和静态分析命令。

初始候选可以评估：

- JSON parser：`yyjson`
- TOML parser：`tomlc99`
- 测试框架：轻量 C 测试框架或自研极小 test harness

正式采用前必须记录许可证和维护策略。不得在未限制输入大小的情况下直接把 agent 请求送进 parser。

### Socket 与控制面

推荐 v0.1.0 使用两个 Unix socket：

- tool socket：接收 agent tool request。
- admin socket：接收 human/admin approve/reject/list pending。

daemon 必须对每个连接读取 `SO_PEERCRED`。agent 调用面不得访问 approval 管理方法。即使测试环境中 tool socket 和 admin socket 由同一用户访问，也必须在协议和身份模型里保留 requester/approver 分离检查。

### JSON-RPC

v0.1.0 支持最小 JSON-RPC 2.0：

- `jsonrpc`
- `id`
- `method`
- `params`

必须处理：

- invalid JSON
- invalid request shape
- unknown method
- invalid params
- policy denied
- approval required
- approval rejected
- approval expired
- request hash mismatch
- control plane denied
- execution failed

错误响应必须使用 JSON-RPC error，并在 `data.kind` 中提供机器可读错误类型。

### Policy

policy 是 v0.1.0 的核心，不是后补模块。

policy 输入：

- method
- params
- caller UID/GID/PID
- session identity
- config capabilities
- resource limits

policy 输出：

- decision：`deny`、`approval_required`、`allow`
- risk：`low`、`medium`、`high`、`denied`
- request hash
- human-readable summary
- machine-readable reason

`sys.info` 也必须经过 policy 和 approval 闭环，不能因为它是低风险工具而绕过核心流程。

### Approval

v0.1.0 approval pending store 可以先放在 daemon 内存中，但必须具备：

- request ID
- request hash
- original caller UID/GID/PID
- session identity
- method
- params summary
- risk
- expiry
- approval status

admin approve 前必须校验：

- pending request 存在。
- approval 未过期。
- approver 不是 requester。
- request hash 未变化。
- approval scope 覆盖该 method。
- 当前 policy 仍允许进入执行。

### Audit

audit log 使用 JSON Lines。

v0.1.0 必须记录：

- `request_received`
- `preflight_denied`
- `approval_required`
- `approval_granted`
- `approval_rejected`
- `execution_started`
- `execution_finished`
- `execution_failed`

每条事件至少包含：

- timestamp
- request_id
- session_id
- caller UID/GID/PID
- approver UID/GID/PID when relevant
- method
- risk
- decision
- error kind
- duration when relevant
- params summary

daemon 启动时必须检查 audit log 路径。如果 audit log 位于 agent 可写 allowlist 下，daemon 必须拒绝启动。

### `sys.info`

`sys.info` 是 v0.1.0 唯一必须完成的 tool。

建议返回：

- hostname
- kernel version
- OS release
- uptime
- architecture

实现时只读取只读系统信息，不做任何写操作，不启动子进程。

## 开发边界

v0.1.0 开发必须遵守以下边界：

- 先 skeleton，后工具扩展。
- 先测试入口，后功能堆叠。
- 先安全核心，后系统能力。
- 默认拒绝策略必须先于任何工具执行。
- approval/admin 接口不得暴露给普通 agent 调用面。
- requester 不能审批自己的 pending request。
- `sys.info` 必须走完整 policy/approval/audit 流程。
- audit log 必须从第一条闭环开始存在。
- 在最小闭环测试通过前，不实现 `proc.*`、`net.*`、文件写入、文件删除和 MCP server。

## 需要特别关注的点

开发者需要特别注意：

- 不要为了快速跑通而绕过 policy。
- 不要让 `sys.info` 成为特殊通道。
- 不要把 approval 做成普通 JSON-RPC method 后暴露给 agent。
- 不要把 request hash 只当日志字段；它必须参与执行前校验。
- 不要把 audit log 放在 agent 可写路径内。
- 不要在 parser 前忽略请求大小限制。
- 不要在 v0.1.0 引入 shell 执行。
- 不要提前实现网络 HTTP 或进程启动。
- 不要把设计基线版本 `v0.1.0-design` 当作可运行版本。

## 验证与测试建议

v0.1.0 的测试必须服务于最小闭环。

最低测试集：

- daemon 可加载合法配置。
- daemon 拒绝非法配置。
- audit log 在 agent writable allowlist 下时 daemon 拒绝启动。
- JSON-RPC invalid JSON 返回错误。
- JSON-RPC unknown method 返回错误。
- agent 请求 `sys.info` 返回 `approval_required`。
- pending request 记录 request hash。
- requester 自己 approve 被拒绝。
- admin approve 成功。
- approval expired 后执行被拒绝。
- request hash mismatch 后执行被拒绝。
- policy recheck 失败后执行被拒绝。
- approval 后 `sys.info` 执行成功。
- audit log 包含 request、approval、execution 事件。
- 集成测试覆盖完整 request -> approval -> execution -> audit 链路。

建议验证命令在实现计划中固定下来，例如：

- 配置检查命令
- 构建命令
- 单元测试命令
- 集成测试命令
- sanitizer 测试命令

## v0.1.0 完成定义

v0.1.0 只有满足以下条件才能进入实现版本审核：

- 存在 C 源码目录。
- 存在构建系统。
- 存在测试资产。
- `anythingd` 可运行。
- `anythingctl` 可调用 daemon。
- tool/admin 控制面隔离可测试。
- `SO_PEERCRED` 身份获取可测试。
- policy preflight 可测试。
- approval pending store 可测试。
- requester/approver 分离可测试。
- `sys.info` 完整闭环可测试。
- audit log 可测试。
- 所有最低测试集通过。
- 开发报告更新为实现报告，包含源码目录、构建命令、测试命令和运行结果。

## 后续工作建议

下一步不应直接写业务工具，而应先产出 v0.1.0 实现计划。

实现计划应按以下顺序展开：

1. 依赖和测试框架选型。
2. 项目 skeleton 和 CMake。
3. 配置 schema。
4. transport 和 peer credentials。
5. JSON-RPC。
6. method registry。
7. policy。
8. approval。
9. audit。
10. `sys.info`。
11. CLI。
12. 集成测试。
13. 开发报告更新。
14. 开发审核。

本报告是后续 v0.1.0 开发的基准。任何扩大范围的开发项都应先更新本报告或新增变更说明，并经过审核确认。
