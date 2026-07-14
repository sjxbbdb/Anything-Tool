# Anything Tool Linux Agent Tool Layer v0.1.1-security-hardening 开发报告

报告日期：2026-07-14

明确时间戳：2026-07-14 10:19:26 UTC+0800

项目名：Anything Tool

子项目名：Linux Agent Tool Layer

建议版本号：v0.1.1-security-hardening

报告类型：下一版本开发报告

报告依据：`审核报告/Anything-Tool-v0.1.0-2026714955-Linux-Agent-Tool-Layer-源码审核报告.md`

报告撰写者：开发报告撰写者

## 项目背景

Anything Tool 的 Linux Agent Tool Layer 是一个面向 agent 的 Linux 用户态 C 工具层。它通过 `anythingd` daemon 和 `anythingctl` CLI，在 agent 与操作系统能力之间建立受控边界。项目的长期目标是让 agent 能够在安全、可审批、可审计、可限制的范围内调用系统能力。

`v0.1.0` 已经完成 skeleton 和最小闭环雏形，包含 CMake、基础源码目录、daemon/CLI/common/tools 模块、`sys.info`、approval pending store、audit log、contract tests、Linux smoke test 脚本等资产。源码审核确认其已经达到“内部 skeleton 已落地”的阶段性目标。

但源码审核结论为“有条件不通过”。当前版本尚未达到安全可扩展基线，不能作为可信安全边界发布，也不能在此基础上扩展更多系统能力。下一版本必须先进行安全加固和真实 Linux 验证。

## 项目目标

`v0.1.1-security-hardening` 的目标是把 `v0.1.0` 的最小闭环从“雏形可见”推进到“安全边界可验证”。

核心目标：

- 修复源码审核报告中列出的高风险和中风险问题。
- 强化 admin approval 控制面，避免同 UID 非授权进程审批 agent 请求。
- 确保关键审计事件写入失败时 fail-closed。
- 确保所有 response 和 audit log 都是合法 JSON。
- 防止单个慢连接阻塞整个 daemon。
- 替换或严格化 JSON-RPC parser。
- 将测试从“契约存在性”推进到“行为安全验证”。
- 在真实 Linux 环境运行并记录 smoke test 输出。

本版本不追求新工具能力，不扩大系统操作面。

## 项目内容

本版本只允许修改安全核心、协议处理、审计可靠性、测试和验证资产。

允许开发内容：

- admin socket 授权模型加固。
- audit log 启动验证与 fail-closed。
- JSON escaping helper 或可靠 JSON writer/parser。
- JSON-RPC parser 严格化或替换。
- accepted socket 读取超时、非阻塞处理或连接状态机。
- 行为测试和安全回归测试。
- Linux smoke test 完整执行和结果记录。
- 开发报告、审核报告、开发日志同步更新。

明确禁止在本版本新增：

- `proc.spawn`
- `proc.kill`
- `net.http_request`
- `fs.write`
- `fs.delete`
- `fs.mkdir`
- root daemon
- 内核模块、驱动或硬件控制
- MCP server wrapper
- 任意 shell 执行能力

## v0.1.0 审核结论摘要

源码审核结论：有条件不通过。

阶段性正向结果：

- skeleton 和最小闭环雏形已存在。
- contract tests 在当前环境通过。
- CMake 在 Windows 主机上能配置并运行 contract tests。
- CodeGraph 已能索引源码，审核时统计为 18 个文件、170 nodes、374 edges。
- 设计文档中的部分安全边界已经进入源码雏形。

未通过原因：

- admin socket 不能真正代表人类审批边界。
- audit 写入失败被忽略。
- JSON 输出未转义，`sys.info` 和 audit log 可能生成非法 JSON。
- 单连接阻塞读可能导致 daemon 停止服务。
- RPC parser 不是严格 JSON-RPC parser。
- 测试偏契约存在性，缺少行为安全验证。
- 未在真实 Linux 环境验证 daemon、Unix socket、`SO_PEERCRED`、approval flow 和 audit log。

## 本次开发内容

下一版本开发应按以下顺序执行。

### 1. admin approval 控制面加固

当前问题：

- tool socket 和 admin socket 都使用 `0600` 权限。
- requester/approver 分离只比较 UID/GID/PID。
- 同一用户下另一个进程可能连接 admin socket 并审批请求。

开发要求：

- 增加 admin UID/GID allowlist 或 dedicated admin group。
- admin socket 放入独立目录，目录权限和 socket 权限必须能表达 admin 角色边界。
- approval 管理方法必须校验调用方是否属于 admin allowlist/group。
- requester 不能审批自己的请求仍然保留，但不能只依赖 PID 差异。
- 增加测试：同 UID 非授权进程连接 admin socket 必须被拒绝。

完成标准：

- 非授权调用方访问 `approval.approve`、`approval.reject`、`approval.execute` 返回 `control_plane_denied`。
- 授权 admin 调用方可以审批。
- requester 与 approver 身份分离测试通过。

### 2. audit fail-closed

当前问题：

- `anything_audit_write_event` 返回失败时 daemon 调用处未检查。
- audit log 路径不可写、目录不存在或权限错误时，请求可能继续执行。

开发要求：

- daemon 启动时创建并验证 audit log 文件。
- 检查 audit log 目录权限、文件权限和可写性。
- audit log 路径不得位于 agent writable allowlist 下。
- approval、execution、failure 等关键事件写入失败时必须 fail-closed。
- 增加 audit write failure 错误响应。

完成标准：

- audit 初始化失败时 daemon 拒绝启动。
- 关键事件写入失败时请求不得继续执行。
- 测试覆盖 audit path 不存在、不可写、位于 writable allowlist、运行期写入失败。

### 3. JSON 输出转义和 JSON 合法性

当前问题：

- `sys.info` 将 `/etc/os-release` 第一行直接拼接进 JSON。
- audit log、RPC result/error、approval list 等位置存在手写 JSON 拼接。
- 含双引号、反斜杠或控制字符的字段会导致非法 JSON 或日志注入。

开发要求：

- 增加统一 JSON string escaping helper。
- 所有 JSON 字符串字段必须经过 escaping。
- response、error、audit event、approval list 都必须输出合法 JSON。
- smoke test 使用 Python `json.loads` 或 `jq` 校验响应和 audit log。
- 不得继续扩大未转义手写 JSON 拼接范围。

完成标准：

- 常见 Ubuntu `/etc/os-release` 内容不会破坏 `sys.info` JSON。
- audit log 每一行均可被 JSON parser 解析。
- 恶意 summary、method、params 中的引号、反斜杠、换行不会造成日志注入。

### 4. socket 阻塞读防护

当前问题：

- daemon `poll` 到 socket 可读后，接受连接并阻塞式 `read`。
- 客户端发送少量数据但不发送 newline、不关闭连接时，daemon 可能被单连接拖住。

开发要求：

- accepted fd 设置非阻塞，或设置接收超时。
- 增加 per-request read deadline。
- 可采用简单 event loop 状态机，也可采用受限 worker 模型。
- admin socket 不能因 tool socket 慢连接而失去响应。

完成标准：

- 测试中一个 slow client 连接不发送 newline 时，daemon 仍能响应其他 tool/admin 请求。
- 超时连接被关闭并记录 audit 或 debug 事件。
- `max_request_bytes` 与 read deadline 同时生效。

### 5. JSON-RPC parser 严格化

当前问题：

- parser 使用 `strstr` 查找 `"jsonrpc"`、`"id"`、`"method"`、`"params"`。
- 字段不要求位于顶层。
- 不支持完整 JSON string escaping。
- 嵌套同名字段可能导致误解析。

开发要求：

- 优先引入真实 JSON parser。
- 若暂不引入依赖，必须实现严格顶层 scanner，并明确限制能力范围。
- request hash 应基于规范化后的 method、params、session，而不是不稳定原始片段。
- 增加 invalid shape、nested duplicate keys、escaped string、oversized params 测试。

完成标准：

- 嵌套 `params.method` 不会污染顶层 method。
- escaped string 被正确解析或明确拒绝。
- 非对象 params、重复关键字段、缺失 `jsonrpc`、错误版本都被拒绝。

### 6. 行为安全测试

当前问题：

- 当前测试主要验证文件和字符串存在。
- Windows 上 `ctest` 通过不能证明 Linux daemon 可运行。

开发要求：

- contract test 可以保留，但不能作为主要验收依据。
- 增加 C 单元测试或 Linux 集成测试。
- `scripts/linux_smoke_test.sh` 必须在真实 Linux 环境运行。
- 每次可审核版本必须记录真实运行输出。

最低测试集：

- JSON 合法性测试。
- audit failure fail-closed 测试。
- admin unauthorized 测试。
- approval same requester denial 测试。
- request hash mismatch 测试。
- unknown method 测试。
- socket slow client 测试。
- Linux smoke test。

## 建议技术实现

### admin 授权

建议配置中新增：

```toml
[admin]
allowed_uids = [1000]
allowed_gids = [1001]
require_admin_group = true
```

实现上优先按 GID/group 控制 admin socket。开发环境可以允许显式 UID allowlist，但必须在报告中标注这只是开发便利，不代表强人类认证。

### audit

建议将 audit 从“每次写入时 fopen append”调整为 daemon startup 阶段初始化：

- 启动时打开 audit log。
- 校验目录和文件安全。
- 保持 fd 或封装 audit writer。
- 关键事件写入失败时返回错误并阻止后续执行。

### JSON

建议至少先实现：

- `anything_json_escape_string`
- `anything_json_write_string_field`
- audit event builder
- RPC result/error builder

如果引入第三方 JSON 库，必须记录版本、license、来源、引入方式和输入大小限制。

### transport

短期可接受方案：

- accepted fd 设置 `SO_RCVTIMEO`。
- 超时后关闭连接。
- read buffer 严格受 `max_request_bytes` 限制。

更长期方案：

- nonblocking fd。
- poll 多 fd 状态机。
- 每连接有限状态 buffer。

### parser

建议 v0.1.1 直接定案 JSON parser，避免继续在手写 parser 上叠补丁。若为了快速修复选择严格 scanner，必须把替换为真实 parser 写入后续版本计划。

## 需要特别关注的点

- 不要用“不同 PID”当作人类审批证明。
- 不要在 audit 写失败时继续执行工具。
- 不要继续手写未转义 JSON。
- 不要让 slow client 阻塞 admin 审批入口。
- 不要用 contract test 代替行为测试。
- 不要在真实 Linux 验证前宣称 `SO_PEERCRED`、socket 和 daemon 行为可靠。
- 不要在本版本扩展 `proc.*`、`net.*` 或文件写能力。
- 不要把 `v0.1.0` 作为安全可信版本发布。

## 验证与测试建议

必须运行并记录：

- `cmake -S . -B build`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- Linux 环境下 `bash scripts/linux_smoke_test.sh`
- `codegraph sync .`
- `codegraph status .`

必须新增或强化测试：

- admin 非授权调用被拒绝。
- 同 UID 非授权进程不能审批。
- requester 不能审批自己。
- audit 初始化失败时 daemon 启动失败。
- audit 写入失败时关键流程 fail-closed。
- `sys.info` response 可被 JSON parser 解析。
- audit log 每一行可被 JSON parser 解析。
- response/error 中引号、反斜杠、换行被正确转义。
- slow client 不阻塞其他请求。
- nested duplicate JSON-RPC fields 不导致误解析。
- request hash mismatch 被拒绝。

## 下版本完成定义

`v0.1.1-security-hardening` 只有满足以下条件才能提交审核：

- 所有高风险项已修复。
- 所有中高风险项已修复。
- JSON 输出合法性有自动化测试。
- audit fail-closed 有自动化测试。
- admin 授权边界有自动化测试。
- socket slow client 有自动化测试。
- JSON-RPC parser 严格化有自动化测试。
- Linux smoke test 在真实 Linux 环境运行并记录完整输出。
- CodeGraph 已同步且状态为 `[OK] Index is up to date`。
- 开发报告已更新。
- 桌面开发日志已更新，署名为 `开发报告撰写者`。

## 后续工作建议

开发顺序建议：

1. 先补测试，确保现有问题可复现。
2. 实现 JSON escaping，修复 JSON 合法性。
3. 实现 audit startup validation 和 fail-closed。
4. 加固 admin 授权模型。
5. 加入 socket read timeout 或 nonblocking 处理。
6. 替换或严格化 JSON-RPC parser。
7. 在真实 Linux 环境运行 smoke test。
8. 更新开发报告、桌面日志和 CodeGraph。
9. 再进入源码审核。

本版本完成前，不得开发任何新的高风险系统能力。

署名：开发报告撰写者
