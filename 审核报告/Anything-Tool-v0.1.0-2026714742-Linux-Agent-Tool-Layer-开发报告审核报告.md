# Anything Tool v0.1.0 Linux Agent Tool Layer 开发报告审核报告

项目名：Anything Tool

版本号：v0.1.0

时间戳：2026714742

审核对象：`开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.0_2026-07-14_07-39-15_UTC+0800_dev-report.md`

审核类型：开发报告审核、仓库状态核对、源码存在性核对

审核结论：设计版本可通过；源码实现未开始，不能按实现版本验收。

## 审核范围

本次审核覆盖以下内容：

- 新版本开发报告内容。
- 设计文档 `docs/superpowers/specs/2026-07-14-linux-agent-tool-layer-design.md`。
- 既有设计审核报告 `docs/superpowers/specs/2026-07-14-linux-agent-tool-layer-design-review.md`。
- Git 提交状态。
- CodeGraph 与仓库源码存在性。

本次审核未进行编译、单元测试、集成测试或安全测试，因为当前仓库尚无 C 源码、构建系统和测试代码。

## 核对结果

开发报告中声明“本版开发工作的重点是设计固化，而不是源码实现”，该描述与当前仓库状态一致。

CodeGraph 查询结果显示当前没有可审核的 C 实现源码。仓库文件核对也确认当前主要内容为开发报告、审核报告、设计文档、CodeGraph 数据和 `.gitignore`，尚未出现 `src/`、`include/`、`tests/`、`CMakeLists.txt` 等实现与构建文件。

Git 提交核对结果与开发报告一致，当前关键提交为：

- `5cafe78 Add Linux agent tool layer design`
- `24a935e Incorporate security review into design`

设计文档已经吸收上一轮安全审查中的核心要求，包括：

- agent/admin 控制面隔离。
- `SO_PEERCRED` 调用方身份记录。
- agent 不能调用 approval 管理方法。
- 请求者不能审批自己的 pending request。
- approval 绑定 request hash、caller/session identity、scope、risk、expiry。
- fd-based filesystem path resolution。
- audit log 不得位于 agent 可写 allowlist 下。
- 结构化 command schema。
- 网络请求防 DNS rebinding、redirect、私网地址和 Host/SNI 绕过。

## 主要发现

### 1. 中风险：v0.1.0 当前是设计版本，不应被误认为实现版本

开发报告文件名使用 `v0.1.0`，但报告正文明确说明本版重点是设计固化，不是源码实现。当前仓库也没有 C 源码、构建系统或测试代码。

该状态本身不是缺陷，但需要在版本管理和后续沟通中明确：`v0.1.0` 目前只能作为设计基线版本或文档版本验收，不能作为可运行软件版本验收。

建议：

- 在开发报告和后续 release note 中明确标注 `v0.1.0-design` 或 `v0.1.0 planning baseline`。
- 如果坚持使用 `v0.1.0`，应在报告结论处写明“无可执行交付物”。
- 下一版实现报告应补充源码目录、构建命令、测试命令和运行结果。

### 2. 中风险：尚无源码实现，无法进行代码级安全验证

本项目的核心风险集中在 Unix socket 身份、approval 控制面、fd-based path resolution、audit log 防篡改、command schema、network SSRF 防护等实现细节。当前报告已经把这些要求写入设计，但尚未有代码证明这些约束真正落地。

因此，本次只能确认“设计要求已纳入”，不能确认“安全机制已实现”。

建议下一版必须提供：

- `CMakeLists.txt` 或等价构建入口。
- `src/daemon`、`src/cli`、`src/common`、`src/tools` 初始源码。
- 配置 schema 校验代码。
- 最小 policy 与 approval 流程实现。
- 审计日志写入实现。
- 单元测试与集成测试入口。

### 3. 中风险：测试计划充分，但缺少可执行测试资产

开发报告列出了 JSON-RPC、默认拒绝、capability、approval、path escape、audit log 等测试建议，方向正确。但当前仓库没有测试框架、测试文件或验证脚本。

这意味着当前版本不能证明任何安全不变量可被自动回归验证。

建议下一版最少落地以下测试资产：

- policy 默认拒绝测试。
- approval requester/approver identity separation 测试。
- agent 调 approval API 被拒绝测试。
- audit log 路径进入 agent writable allowlist 时启动失败测试。
- `fs.read`/`fs.list` 的 symlink、`..`、rename race 测试。
- JSON-RPC unknown method 与 invalid params 测试。

### 4. 低风险：依赖选型仍停留在建议层

开发报告建议使用 C11 + CMake、稳定 JSON parser、TOML parser，但没有确定具体库、许可协议、引入方式或安全维护策略。

这对设计版本可以接受，但进入实现前需要明确，否则会影响构建可重复性、安全审计和后续许可证合规。

建议：

- 明确 JSON parser 和 TOML parser 的具体选择。
- 记录版本、license、来源和更新策略。
- 明确是否 vendoring、系统包安装，还是 CMake FetchContent。
- 对 parser 的错误处理和输入大小限制写入实现验收标准。

### 5. 低风险：实现计划还缺少最小可运行里程碑定义

开发报告给出了推荐顺序，但还没有把下一版的“完成定义”压缩成一个可执行验收切片。

建议下一版以一个最小闭环作为目标：

- daemon 启动并加载 TOML 配置。
- Unix socket 接收 JSON-RPC。
- `sys.info` 请求触发 policy preflight。
- agent 请求返回 `approval_required`。
- admin socket 批准 pending request。
- daemon 重新校验 policy。
- 执行 `sys.info`。
- 写入 JSON Lines audit log。
- 集成测试验证全链路。

## 正向确认

本版开发报告有以下值得保留的优点：

- 明确第一版不进入 root daemon、内核模块、驱动或直接硬件控制。
- 明确不提供任意 shell 执行能力。
- 明确 agent 不能审批自己的请求。
- 明确控制面隔离和 peer credential 要求。
- 明确 filesystem 不能只做字符串路径判断。
- 明确 audit log 不能被 agent 写能力覆盖。
- 明确 process 与 network 能力默认延后，并放在 disabled-by-default 能力后面。
- 把测试重点放在安全边界，而不是工具数量。

## 源码审核状态

当前源码审核状态：未开始。

原因：仓库当前没有 C 源码、构建系统、测试代码或可运行二进制。

本次源码相关结论仅限于“源码不存在性核对”。后续出现源码后，需要重新进行代码级审核，重点检查：

- Unix socket credential 获取和授权判断。
- JSON-RPC 解析边界。
- policy 是否在 tool syscall 前执行。
- approval request hash 与身份绑定。
- audit log 写入与路径拒绝逻辑。
- fd-based filesystem resolution。
- command schema 校验是否严格。
- network redirect、DNS、IP range、Host/SNI 校验。
- resource limits、timeout、错误路径和内存安全。

## 结论

`v0.1.0` 当前可以作为 Linux Agent Tool Layer 的设计基线版本通过审核。开发报告与仓库状态基本一致，且上一轮安全审核中的关键意见已经反映到设计文档中。

但该版本不能作为源码实现版本通过验收。下一阶段应优先交付最小可运行 skeleton、测试框架和一条完整的 request -> approval -> execution -> audit 闭环，再进入更多工具能力开发。
