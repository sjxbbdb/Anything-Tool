# Anything Tool v0.1.0-design Linux Agent Tool Layer 开发准入审核报告

项目名：Anything Tool

版本号：v0.1.0-design

时间戳：2026714854

审核对象：`开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.0-design_2026-07-14_08-31-10_UTC+0800_dev-report.md`

审核类型：开发报告审核、开发准入判断、仓库状态核对

审核结论：有条件通过。可以开始开发，但仅限进入最小可运行闭环与安全核心 skeleton 开发；不建议直接开发扩展工具能力。

## 审核范围

本次审核覆盖：

- 新版 `v0.1.0-design` 开发报告。
- 当前仓库源码存在性。
- CodeGraph 索引状态。
- 是否具备进入下一阶段开发的条件。

本次未进行代码级安全审核、编译验证、单元测试或集成测试，因为当前仓库仍无 C 源码、构建系统、测试资产和可执行交付物。

## 核对结果

新版开发报告已经明确标注：

- 版本号为 `v0.1.0-design`。
- 交付状态为设计基线版本。
- 当前无 C 源码、无构建系统、无测试资产、无可执行交付物。
- 不能作为源码实现版本或可运行软件版本验收。

该修订解决了上一份审核报告中指出的版本语义问题。`v0.1.0-design` 现在不会被误认为可运行实现版本。

CodeGraph 查询结果仍显示当前没有相关实现源码。仓库状态与开发报告描述一致。

## 开发准入判断

可以开始开发。

但这里的“开始开发”应限定为以下范围：

- 建立 C 项目 skeleton。
- 建立 CMake 或等价构建入口。
- 建立基础测试框架。
- 实现配置加载与 schema 校验。
- 实现 Unix socket transport 与 `SO_PEERCRED` 身份获取。
- 实现 JSON-RPC 基础解析、路由和统一错误响应。
- 实现 policy preflight。
- 实现 approval pending store 与 admin approval flow。
- 实现 JSON Lines audit log。
- 实现 `sys.info`。
- 实现 request -> approval -> execution -> audit 的最小集成闭环。

不建议在下一阶段直接开发：

- `proc.spawn`。
- `proc.kill`。
- `net.http_request`。
- 文件写入、删除、目录创建。
- root daemon。
- 内核模块、驱动或硬件控制。
- MCP server 包装层。

## 通过理由

### 1. 版本定位已经清楚

报告明确写明 `v0.1.0-design` 是设计基线版本，不再把设计交付物包装成实现版本。这为后续实现版本号、审核报告和 release note 留出了清晰边界。

### 2. 安全边界已经足够进入 skeleton 阶段

报告已经明确下一阶段必须围绕以下安全边界展开：

- agent/admin 控制面隔离。
- `SO_PEERCRED` 调用方身份识别。
- 请求者不能审批自己的 pending request。
- approval 绑定 request hash、caller identity、session identity、scope、risk、expiry。
- policy 必须在 tool-specific syscall 前执行。
- audit log 不得位于 agent 可写 allowlist 下。
- filesystem 后续必须使用 fd-based resolution。

这些约束足够指导第一阶段 skeleton 和最小闭环实现。

### 3. 最小可运行里程碑定义明确

报告新增了“最小可运行里程碑”，并把完成定义压缩到可验证的闭环：

- daemon 启动并加载配置。
- 创建 tool socket 和 admin socket。
- 通过 Unix socket 获取调用方身份。
- 接收 JSON-RPC 请求。
- `sys.info` 触发 policy preflight。
- agent 请求返回 `approval_required`。
- admin 批准 pending request。
- 执行前重新校验 approval 和 policy。
- 返回 `sys.info` JSON result。
- 写入 audit log。
- 集成测试验证全链路。

这是合理的下一阶段开发目标。

## 仍需在开发前明确的事项

以下事项不阻塞启动 skeleton 开发，但应在真正写 parser/config 相关代码前定案：

### 1. JSON/TOML parser 选型

报告提出可评估 `yyjson` 和 `tomlc99`，但尚未最终确定依赖。

开发前需要明确：

- 具体库名与版本。
- license。
- 引入方式：vendoring、系统包、submodule、CMake FetchContent。
- 解析失败时的错误返回策略。
- 输入大小限制是否在 parser 前执行。

### 2. 测试框架选型

报告要求先写测试，但没有确定 C 测试框架。

建议开发前确定：

- 单元测试框架。
- 集成测试脚本语言。
- CI 或本地验证命令。
- sanitizer 开关。

### 3. 开发版本号

实现阶段不应继续使用 `v0.1.0-design` 作为可运行版本号。

建议第一个实现版本使用：

- `v0.1.0-alpha.1`，或
- `v0.1.1-impl`

并在开发报告中附带源码、构建命令、测试命令和运行结果。

## 开发边界要求

下一阶段开发必须遵守：

- 先测试与 skeleton，后工具扩展。
- 先安全核心，后功能广度。
- 默认拒绝策略必须先于任何 tool 执行。
- approval/admin 接口不得暴露给普通 agent 调用面。
- `sys.info` 也应经过 policy/approval 闭环，而不是绕过核心流程。
- audit log 必须从第一条闭环开始存在。
- 集成测试必须覆盖 request -> approval -> execution -> audit。

## 不通过项

无阻塞性不通过项。

但以下内容仍处于未验收状态：

- 源码实现：未开始。
- 构建系统：未开始。
- 单元测试：未开始。
- 集成测试：未开始。
- 代码级安全：未开始。
- 可运行二进制：未交付。

## 最终结论

`v0.1.0-design` 可以作为开发前设计基线通过审核。

可以开始下一阶段开发，但开发范围应严格限定为最小可运行闭环：project skeleton、构建系统、测试框架、配置加载、Unix socket、peer credentials、JSON-RPC、policy、approval、audit 和 `sys.info`。在该闭环通过测试前，不建议开发 `proc.*`、`net.*`、文件写入删除或其他高风险能力。
