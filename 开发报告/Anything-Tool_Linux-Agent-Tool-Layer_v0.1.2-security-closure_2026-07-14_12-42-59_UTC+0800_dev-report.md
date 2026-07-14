# Anything Tool Linux Agent Tool Layer v0.1.2-security-closure 开发报告

报告日期：2026-07-14

明确时间戳：2026-07-14 12:42:59 UTC+0800

项目名：Anything Tool

子项目名：Linux Agent Tool Layer

建议版本号：v0.1.2-security-closure

备选版本名：v0.1.2-runtime-validation

报告类型：下一版本开发报告

报告依据：`审核报告/Anything-Tool-v0.1.1-20267141203-Linux-Agent-Tool-Layer-源码审核报告.md`

报告撰写者：开发报告撰写者

## 项目背景

Anything Tool 的 Linux Agent Tool Layer 是一个面向 agent 的 Linux 用户态 C 工具层。它通过 `anythingd` daemon 和 `anythingctl` CLI，在 agent 与操作系统能力之间建立受控、可审批、可审计、可限制的安全边界。

`v0.1.1` 在 `v0.1.0` skeleton 基础上已经完成一轮安全加固，包括 JSON escaping、audit checked write、RPC 顶层 scanner、admin allowlist 机制雏形、socket read timeout 和 security contract tests。源码审核确认这些改动方向正确。

但 `v0.1.1` 审核结论仍为“有条件不通过”。主要原因是 admin allowlist 默认关闭、Linux runtime 未验证、测试仍偏静态契约、audit path 安全模型仍不充分、socket timeout 无法抵御持续慢速发送。因此下一版本不能进入工具能力扩展，必须继续做安全闭环收口和真实 Linux 验证。

## 项目目标

`v0.1.2-security-closure` 的目标是将 `v0.1.1` 的安全加固从“代码结构存在”推进到“默认安全、行为可测、Linux runtime 可验证”。

核心目标：

- 默认启用 admin allowlist，避免默认配置下任意同 UID 进程使用 admin 控制面。
- 让空 admin allowlist 成为启动失败条件，除非显式进入 insecure development mode。
- 将示例配置改为安全默认，或拆分安全示例与 insecure dev 示例。
- 为 socket 读取增加整体 request deadline，避免持续慢速发送拖住单线程 daemon。
- 加强 audit path 安全，至少禁止 symlink final target，并向 canonical/fd-based 模型推进。
- 将 security tests 从静态契约检查升级为行为测试。
- 在真实 Ubuntu/Linux 环境运行并记录 `bash scripts/linux_smoke_test.sh` 完整输出。
- 在以上闭环完成前，不新增任何高风险系统能力。

本版本完成后，项目才有资格再次申请“安全可扩展基线”审核。

## 项目内容

本版本只允许开发安全闭环、运行时验证和行为测试。

允许开发内容：

- config 默认安全策略调整。
- admin allowlist 强制启用。
- insecure development mode 显式化。
- 安全示例配置与开发不安全示例配置拆分。
- request-level socket deadline 或 nonblocking/worker 模型。
- audit path canonical/fd-based 安全增强。
- audit symlink、目录替换、运行期写失败测试。
- Linux runtime smoke test 执行记录。
- 行为级 security tests。
- 开发报告、开发日志、CodeGraph 状态记录。

禁止开发内容：

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

`fs.list` 和 `fs.read` 仍不应进入本版本，除非 admin 默认安全、socket deadline、audit path、Linux runtime 验证全部通过且另行审核批准。

## v0.1.1 审核结论摘要

源码审核结论：有条件不通过。

正向确认：

- 新增统一 JSON escaping helper。
- RPC、audit、approval list、`sys.info` 输出路径已接入 escaping。
- `sys.info` 对 `/etc/os-release` 含引号内容的输出风险已有修复。
- audit 写入增加 `checked_audit`，关键事件失败时返回 `audit_failed`。
- daemon 启动前增加 audit log 可写性验证。
- RPC parser 从简单 `strstr` 改为顶层 scanner。
- admin socket 增加 UID/GID allowlist 机制。
- accepted socket 增加 `SO_RCVTIMEO` 读取超时。
- Linux smoke script 增加 JSON response 和 audit JSON Lines 解析校验。

不通过原因：

- admin allowlist 默认关闭。
- 示例配置仍允许任意同用户进程使用 admin socket。
- socket read timeout 只能限制单次 read，不能防持续慢速发送。
- audit path 仍是字符串和普通 `fopen` 模型，未防 symlink/canonical 绕过。
- security tests 仍偏静态契约，不能证明行为真实生效。
- Linux smoke test 尚未在真实 Linux 环境运行。

## 不足与边界问题

### 不足

- `v0.1.1` 已有安全加固代码，但默认配置没有 enforce admin allowlist。
- admin allowlist 是可选能力，而不是默认安全边界。
- socket timeout 仍可能被“每次 timeout 前发送 1 字节”的慢客户端绕过。
- audit path 只做普通路径和文件写入验证，未使用 canonical/fd-based 防 symlink 模型。
- security tests 主要检查源码片段存在，缺少真实 daemon 行为验证。
- Linux runtime 验证缺失，无法确认 Unix socket、`SO_PEERCRED`、approval flow、audit fail-closed 在目标平台真实有效。

### 边界问题

- `v0.1.1` 不能作为安全可扩展基线通过。
- `v0.1.1` 不能作为可信 runtime 安全版本发布。
- 不能基于 `v0.1.1` 开发 `proc.*`、`net.*`、文件写入删除或 MCP wrapper。
- 当前 JSON parser 是项目内子集 scanner，短期可接受，但复杂 params 扩展前仍需替换或强化测试。
- `approval.execute` 仍由 admin 端触发，agent 获取最终执行结果的产品协议边界仍需后续明确。
- `fs.list`、`fs.read` 尚未实现，fd-based filesystem resolution 仍未进入实际源码审核范围。

## 安全问题

### 1. 高风险：admin allowlist 默认关闭

问题：

- `require_admin_allowlist` 默认值为 false。
- allowlist 未启用时 `anything_config_identity_is_admin` 直接返回 true。
- 示例配置中 `require_admin_allowlist = false` 且 UID/GID allowlist 为空。

影响：

- 默认部署仍不能证明 admin 操作来自授权人类。
- 同 UID agent 进程可能连接 admin socket 并执行 approval API。
- v0.1.0 的核心控制面问题仍以默认配置形式存在。

开发要求：

- 将 `require_admin_allowlist` 默认改为 true。
- daemon 启动时要求 allowlist 非空。
- 如果需要开发绕过，必须使用显式 insecure flag，例如 `--dev-insecure-admin`。
- 示例配置必须默认安全。
- 增加行为测试：默认空 allowlist 时 daemon 启动失败。

### 2. 中高风险：持续慢速发送仍可能阻塞 daemon

问题：

- `SO_RCVTIMEO` 只能限制单次 read。
- 如果客户端持续在 timeout 前发送少量字节，daemon 仍会卡在单连接读取循环。

影响：

- slowloris 风险没有完全消除。
- admin 审批入口可能被 tool socket 慢连接间接阻塞。

开发要求：

- 增加整体 request deadline。
- 或采用 nonblocking event loop。
- 或采用有限 worker 模型，避免单连接阻塞主循环。
- 增加行为测试：慢客户端持续发送字节时 daemon 仍能响应其他请求。

### 3. 中风险：audit path 未使用 canonical/fd-based 防护

问题：

- audit path 仍依赖字符串关系和普通 `fopen("a")`。
- 未使用 `open`、`O_NOFOLLOW`、`fstat`、parent directory fd 或 canonical path。

影响：

- symlink、目录替换、路径别名可能绕过审计路径边界。
- 后续加入 fs 写能力后风险会放大。

开发要求：

- 打开 audit log 时使用 `open` + `O_NOFOLLOW`。
- 对 parent directory 和 final file 做 `fstat` 校验。
- audit path 与 agent writable allowlist 比较应基于 canonical/fd-aware 结果。
- 增加 symlink audit path、目录替换、不可写运行期失败测试。

### 4. 中风险：测试仍偏静态契约

问题：

- security tests 仍主要检查源码中是否存在关键字符串、函数名和脚本片段。
- 这些测试不能证明 admin 授权、audit fail-closed、JSON 合法性、socket timeout 在运行中有效。

影响：

- 默认 admin allowlist 关闭这种安全缺口没有被测试拦住。
- Windows 上 `ctest` 通过不代表 Linux daemon 可运行。

开发要求：

- 新增 Linux 集成测试，真实启动 daemon 并发送请求。
- 新增 C 单元测试或小型 harness 测试 config、JSON escaping、RPC parser、audit validation。
- security tests 应校验默认安全，而不是校验 insecure 示例存在。

### 5. 中风险：Linux smoke test 未运行

问题：

- 尚未在真实 Linux 环境运行 `bash scripts/linux_smoke_test.sh`。
- 当前 Windows 环境只运行了 contract/security contract tests。

影响：

- 无法确认 Linux 编译通过。
- 无法确认 `SO_PEERCRED`、Unix socket、admin allowlist、approval flow、audit fail-closed、socket timeout 实际有效。

开发要求：

- 在 Ubuntu/Linux 环境运行 smoke test。
- 将完整输出写入开发报告或单独验证报告。
- Linux runtime 验证通过前，不进入新能力开发。

## 本次开发内容

下一版本开发应按以下顺序执行：

1. 将默认 admin allowlist 改为启用。
2. daemon 启动时拒绝空 admin allowlist。
3. 增加显式 insecure development mode。
4. 将示例配置拆分为安全默认配置和 insecure dev 配置。
5. 增加默认安全配置测试。
6. 增加 request-level deadline 或 nonblocking/worker 模型。
7. 增加 slow client 行为测试。
8. 加固 audit path，至少禁止 symlink final target。
9. 增加 audit path symlink、目录替换、运行期写失败测试。
10. 将 security contract tests 升级为行为测试。
11. 在真实 Linux 环境运行 `bash scripts/linux_smoke_test.sh`。
12. 记录 Linux smoke test 完整输出。
13. 运行并记录 `codegraph sync .` 与 `codegraph status .`。
14. 更新开发报告和桌面开发日志。

任何新增工具能力都不得早于第 14 项完成并通过审核。

## 建议技术实现

### admin allowlist 默认安全

建议配置默认值：

```toml
[admin]
require_admin_allowlist = true
allowed_uids = []
allowed_gids = []
```

语义：

- `require_admin_allowlist = true` 且 allowlist 为空时，daemon 拒绝启动。
- 只有显式 `--dev-insecure-admin` 才允许本地开发绕过。
- insecure mode 启动时必须打印警告并写 audit/debug 事件。

### 示例配置拆分

建议文件：

- `config/anythingd.example.toml`：安全默认配置。
- `config/anythingd.dev-insecure.toml`：仅开发使用，文件头必须明确警告。

安全示例不得包含默认关闭 allowlist 的 admin 配置。

### request deadline

建议短期实现：

- 在开始读取请求时记录 deadline。
- 每次 read 前后检查总耗时。
- 超过 deadline 立即关闭连接。
- 将 timeout/error 状态记录到 debug 或 audit 事件。

更长期实现：

- nonblocking event loop。
- 每连接状态 buffer。
- 或 worker pool，但必须限制 worker 数量，防止连接耗尽。

### audit path

建议实现：

- 使用 `open` 而不是 `fopen`。
- final target 使用 `O_NOFOLLOW`。
- 使用 `fstat` 校验文件类型、owner、mode。
- parent directory 使用 `stat`/`fstatat` 校验权限。
- audit path 与 writable allowlist 使用 canonical path 比较。

### 行为测试

建议新增：

- `tests/security/test_admin_default_safe.py`
- `tests/security/test_slow_client_behavior.py`
- `tests/security/test_audit_path_behavior.py`
- `tests/security/test_linux_runtime_smoke.py` 或脚本化验证记录

测试应尽量执行 daemon，而不是只读源码文本。

## 需要特别关注的点

- 不要让 `require_admin_allowlist = false` 成为默认示例。
- 不要把 insecure development mode 混进生产配置。
- 不要只靠 UID/PID 差异证明人类审批。
- 不要只依赖 `SO_RCVTIMEO` 证明 slowloris 防护完成。
- 不要继续用字符串前缀判断 audit path 安全。
- 不要把静态 contract test 当成行为安全测试。
- 不要在没有 Linux runtime 输出前宣称版本通过安全闭环。
- 不要在本版本开发任何新工具能力。

## 验证与测试建议

必须运行并记录：

- `python tests\contract\test_v0_1_0_contract.py`
- `python tests\security\test_v0_1_1_security_contract.py`
- 新增 v0.1.2 行为测试命令
- `cmake -S . -B build`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- Ubuntu/Linux 环境下 `bash scripts/linux_smoke_test.sh`
- `codegraph sync .`
- `codegraph status .`

必须新增测试覆盖：

- 默认 admin allowlist 启用。
- 空 admin allowlist 时 daemon 启动失败。
- insecure dev mode 必须显式开启。
- 非授权 admin API 调用被拒绝。
- 慢客户端持续发送字节时 daemon 仍响应其他请求。
- audit path symlink 被拒绝。
- audit path 与 writable allowlist canonical 重叠被拒绝。
- audit 运行期写失败 fail-closed。
- smoke test response 和 audit log 均可 JSON parse。

## 下版本完成定义

`v0.1.2-security-closure` 必须满足以下条件才能提交审核：

- admin allowlist 默认启用。
- 空 admin allowlist 默认启动失败。
- insecure dev mode 显式化并有警告。
- 安全示例配置与 insecure dev 示例配置分离。
- slow client 行为测试通过。
- audit path symlink/canonical 安全测试通过。
- security tests 包含执行级行为验证。
- Linux smoke test 在真实 Ubuntu/Linux 环境运行并记录完整输出。
- CodeGraph 状态为 `[OK] Index is up to date`。
- 不新增任何高风险工具能力。
- 开发报告和桌面日志更新完成。

## 后续工作建议

开发顺序建议：

1. 先调整 config 默认值和示例配置。
2. 补默认安全配置测试。
3. 实现 insecure dev mode。
4. 实现 request-level deadline。
5. 补 slow client 行为测试。
6. 加固 audit path 打开与校验。
7. 补 audit path 行为测试。
8. 在真实 Linux 环境跑 smoke test。
9. 更新开发报告、桌面日志和 CodeGraph。
10. 再进入下一轮源码审核。

本版本完成前，不得扩展任何新系统能力。

署名：开发报告撰写者
