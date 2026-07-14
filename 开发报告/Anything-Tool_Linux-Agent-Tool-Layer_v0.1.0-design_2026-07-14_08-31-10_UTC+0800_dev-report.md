# Linux Agent Tool Layer 开发报告

报告日期：2026-07-14

版本号：v0.1.0-design

明确时间戳：2026-07-14 08:31:10 UTC+0800

交付状态：设计基线版本，无 C 源码、无构建系统、无测试资产、无可执行交付物。

面向读者：后续开发者、实现者、安全审核者

相关文档：

- `docs/superpowers/specs/2026-07-14-linux-agent-tool-layer-design.md`
- `docs/superpowers/specs/2026-07-14-linux-agent-tool-layer-design-review.md`
- `审核报告/Anything-Tool-v0.1.0-2026714742-Linux-Agent-Tool-Layer-开发报告审核报告.md`

## 版本与验收结论

`v0.1.0-design` 当前只能作为 Linux Agent Tool Layer 的设计基线版本验收。它证明项目方向、安全边界、审批模型、审计模型和第一版实现切片已经形成共识，但不能作为源码实现版本或可运行软件版本验收。

本版本没有可执行交付物。后续任何 release note、开发报告或审核报告引用本版本时，都应使用 `v0.1.0-design` 或明确写明“设计基线版本”。如果后续需要发布第一个可运行版本，应另起实现版本号，例如 `v0.1.0-alpha.1` 或 `v0.1.1-impl`，并附带源码、构建命令、测试命令和运行结果。

## 项目背景

本项目要建立一个 agent 可以调用的用户态工具层。工具层使用 C 开发，部署目标优先选择 Ubuntu/Linux。它的职责不是让 agent 直接获得系统控制权，而是在 agent 与操作系统能力之间放置一层可审计、可审批、可限制的安全边界。

用户的长期目标是：agent 可以在安全范围内调用系统工具，间接与内核、硬件、进程、文件、网络等资源交互。但第一版不进入内核模块、驱动、root daemon 或直接硬件控制范围，只先建立通用 OS 能力层。

当前仓库已经完成：

- Git 仓库初始化。
- CodeGraph 初始化，但当前没有 C 源码，所以索引统计仍为空。
- 第一版架构设计文档。
- 第三方审核报告。
- 根据审核意见补强后的安全设计。
- `开发报告` 与 `审核报告` 目录。
- 当前仓库尚未开始 C 源码、构建系统和测试代码实现。

## 项目目标

第一阶段目标是实现一个 Linux 用户态 C 工具层，允许 agent 通过受控接口请求有限系统能力。

核心目标：

- 提供稳定的 agent 调用协议。
- 使用 daemon 统一承载策略、审批、执行和审计。
- 默认拒绝所有能力，只有配置显式开启后才能使用。
- 所有 agent 请求先由系统预审，再按风险和配置进入人工审批。
- 人类审批通过后，工具层才允许执行相关能力。
- 审批、拒绝、执行、失败都必须进入审计日志。
- 为未来 MCP server 包装层保留清晰接口。

非目标：

- 第一版不实现内核模块。
- 第一版不实现直接硬件控制。
- 第一版不运行 root 主 daemon。
- 第一版不提供任意 shell 执行能力。
- 第一版不允许 agent 审批自己的请求。

## 项目内容

项目第一版由两个主要可执行程序组成：

- `anythingd`：常驻 daemon，负责 Unix domain socket 通信、JSON-RPC 解析、安全策略、审批状态、工具执行、审计日志。
- `anythingctl`：CLI 客户端，供人类、测试脚本和 agent 调用 daemon。

第一版工具命名采用接近 MCP tool 的风格，但底层协议使用 JSON-RPC 2.0。

计划中的工具域：

- `sys.*`：系统只读观测，例如内核版本、主机名、uptime、CPU、内存、磁盘、网卡。
- `fs.*`：受限文件操作，例如目录列举、文件读取，写入和删除默认关闭。
- `proc.*`：进程观测和受限命令执行，启动命令必须走结构化白名单。
- `net.*`：受限 DNS、TCP、HTTP 请求，默认关闭高风险网络能力。

第一版最小可用切片建议只做：

- `sys.info`
- `fs.list`
- `fs.read`
- per-call approval flow
- agent/admin 身份隔离
- JSON Lines audit log
- `anythingctl` 的 raw request、pending approvals、approve、reject 命令
- 启动时拒绝不安全的 audit log 配置

## 本次开发内容

本版开发工作的重点是设计固化，而不是源码实现。当前没有 `src/`、`include/`、`tests/`、`CMakeLists.txt` 或可运行二进制，因此不能做编译、单元测试、集成测试或代码级安全验证。

已经完成的开发内容：

- 明确项目以 Ubuntu/Linux 为第一目标平台。
- 明确技术栈以 C 为核心。
- 明确架构采用 `daemon + CLI`。
- 明确 daemon 通过 Unix domain socket 接收请求。
- 明确协议采用 JSON-RPC 2.0。
- 明确工具 schema 和命名向 MCP 兼容方向靠拢。
- 明确配置格式采用 TOML。
- 明确安全策略为默认拒绝。
- 明确 agent 请求必须先系统预审，再进入人工审批。
- 明确审批粒度支持 per-call、短时能力授权、session 授权，默认 per-call。
- 明确 agent 调用面和人类审批控制面必须隔离。
- 明确请求者不能审批自己的 pending request。
- 明确所有 approval 必须绑定 request hash、caller identity、session identity、scope、risk、expiry。
- 明确所有已批准请求执行前必须重新跑 policy。
- 明确文件路径必须使用 fd-based resolution，不能只靠字符串前缀判断。
- 明确命令执行必须使用结构化 command schema，不能通过 shell。
- 明确 audit log 不能位于 agent 可写 allowlist 下。
- 明确网络工具必须防 SSRF、DNS rebinding、redirect 绕过、私网地址绕过。
- 明确进程执行必须处理 process group、stdout/stderr drain、timeout、zombie reap。

已经提交的关键提交：

- `5cafe78 Add Linux agent tool layer design`
- `24a935e Incorporate security review into design`

## 建议技术实现

建议先做 project skeleton，再实现安全核心，最后增加工具能力。不要先堆工具数量。

### 构建与目录

建议使用 C11 + CMake，便于在 Ubuntu 服务器上构建和测试。

建议目录结构：

```text
src/
  daemon/
    main.c
    transport.c
    rpc.c
    policy.c
    approval.c
    audit.c
    config.c
    tool_registry.c
  cli/
    main.c
  tools/
    sys.c
    fs.c
    proc.c
    net.c
  common/
    json.c
    errors.c
    ids.c
    paths.c
include/
  anything/
tests/
  unit/
  integration/
config/
  anythingd.example.toml
```

### JSON 与 TOML

JSON-RPC 需要一个稳定 C JSON parser。实现时优先选择轻量、维护活跃、错误报告清晰的库。TOML parser 同理，不建议手写完整 TOML parser。

实现计划阶段必须明确 parser 选型、版本、license、来源和更新策略。初始候选可以评估 `yyjson` 作为 JSON parser、`tomlc99` 作为 TOML parser；正式采用前需要核验许可证、维护状态、输入错误处理能力、内存安全边界和 vendoring/CMake 集成方式。

要求：

- JSON 请求大小先按 `max_request_bytes` 限制再解析。
- JSON-RPC 字段类型必须严格校验。
- 参数校验必须在 tool 执行前完成。
- 错误响应统一使用 JSON-RPC error，`data.kind` 放机器可读错误类型。

### Unix Socket 与身份

daemon 需要使用 Unix domain socket。

必须实现：

- 通过 `SO_PEERCRED` 获取调用方 UID/GID/PID。
- tool request 与 approval/admin request 区分控制面。
- 可采用两个 socket：普通 tool socket 与 admin socket。
- 如果共用 socket，必须通过 method namespace 和 peer credential 做强校验。
- agent 客户端不能调用 approval 管理方法。

推荐第一版采用两个 socket，代码更直观，测试也更容易。

### Policy 与 Approval

policy 必须在任何 tool-specific syscall 前执行。

建议 policy 输出结构包括：

- decision：allow、deny、approval_required
- risk：low、medium、high、denied
- request_id
- request_hash
- summary
- denial_reason 或 approval_reason

approval 存储可以第一版先用 daemon 内存结构，后续再考虑持久化。即使使用内存 pending store，审计日志也必须记录 approval_required、approval_granted、approval_rejected。

批准执行前必须重新校验：

- request hash 未变化
- approval 未过期
- caller/session/scope 匹配
- requester 和 approver 不同
- 当前配置 policy 仍允许

### 文件系统实现

`fs.list` 和 `fs.read` 是第一版重点，路径安全不能做成字符串 starts-with。

建议：

- allowed root 启动时打开为 root fd。
- 请求 path 相对 allowed root 做 fd-based resolution。
- 优先使用 `openat2`。
- 使用 `RESOLVE_BENEATH`、`RESOLVE_NO_SYMLINKS` 等约束。
- 读文件使用大小限制。
- 打开后用 `fstat` 校验对象类型。

如果运行环境不支持 `openat2`，需要明确 fallback。对 mutating operation，如果 fallback 无法安全抵御 TOCTOU，就应该禁用。

### 命令执行

`proc.spawn` 第一版可以先不实现，但 schema 要先设计对。

原则：

- 不允许 shell。
- 使用绝对路径。
- 使用 `execve`、`posix_spawn` 或等价非 shell API。
- argv 必须完全匹配或按 schema 严格匹配。
- 默认清空环境变量。
- stdin 默认关闭。
- stdout/stderr 分别限流。
- 每个进程进入 daemon 管理的 process group。
- timeout 后终止整个 process group。
- daemon 必须负责 wait/reap。

### 网络实现

`net.*` 第一版可以延后，但设计必须保持安全。

必须考虑：

- DNS 结果 pinning。
- redirect 每一跳都重新校验。
- 默认拒绝 private、loopback、link-local、multicast 地址。
- 默认拒绝 IP literal。
- HTTPS 下 SNI、Host header、allowlist host 必须一致。
- 限制 header size、body size、redirect 次数、总耗时。

### 审计日志

第一版可以使用 JSON Lines。

必须记录：

- timestamp
- request_id
- session_id
- caller UID/GID/PID
- approval UID/GID/PID
- method
- risk
- decision
- error kind
- duration
- 参数摘要

注意不要记录完整 secret。敏感值只记录 hash 或摘要。

daemon 启动时必须拒绝 audit log 路径位于 agent 可写 allowlist 内的配置。

## 最小可运行里程碑

下一阶段应优先交付一个最小闭环，而不是扩大工具覆盖面。

该里程碑完成定义：

- 仓库出现 `CMakeLists.txt` 或等价构建入口。
- 仓库出现 `src/daemon`、`src/cli`、`src/common`、`src/tools` 初始源码目录。
- 仓库出现 `tests/`，至少包含 unit 和 integration 测试入口。
- `anythingd` 能启动并加载 TOML 配置。
- daemon 能创建 tool socket 和 admin socket。
- daemon 能通过 Unix socket 获取 `SO_PEERCRED` 调用方身份。
- daemon 能接收 JSON-RPC 请求并处理 unknown method、invalid params。
- `sys.info` 请求能触发 policy preflight。
- agent 发起 `sys.info` 时返回 `approval_required`。
- admin socket 能批准 pending request。
- daemon 能在执行前重新校验 request hash、approval expiry、scope 和 policy。
- daemon 能执行 `sys.info` 并返回 JSON result。
- daemon 能写入 JSON Lines audit log。
- 集成测试能验证 request -> approval -> execution -> audit 全链路。
- 不安全 audit log 路径配置会导致 daemon 拒绝启动。

该里程碑通过后，才能把版本从设计基线推进到实现预览版本。

## 需要特别关注的点

最需要关注的是安全边界，而不是工具数量。

特别关注项：

- agent 不能审批自己的请求。
- approval API 不能暴露给普通 agent 调用面。
- policy violation 必须在系统调用前拒绝。
- `fs.*` 不能只做字符串路径判断。
- audit log 不能被 agent 写能力覆盖或删除。
- `proc.spawn` 不能退化成任意命令执行。
- 网络请求不能被 redirect、DNS rebinding、私网地址或 Host header 绕过。
- 所有资源都要有限制：请求大小、输出大小、文件大小、运行时间、redirect 次数。
- 高风险能力默认关闭。
- 第一版主 daemon 不要以 root 运行。

## 验证与测试建议

建议先写测试，再实现安全核心。

第一批必须覆盖：

- JSON-RPC shape 校验。
- unknown method 返回错误。
- 默认拒绝策略。
- capability flag 开关。
- `sys.info` 成功返回。
- `fs.list` 和 `fs.read` 只能访问 allowlist 内路径。
- symlink escape。
- `..` escape。
- hard link 行为。
- rename race 或目录替换场景。
- per-call approval required。
- approval granted。
- approval rejected。
- approval expired。
- request hash mismatch。
- agent identity 与 approval identity 分离。
- agent 调 approval API 被拒绝。
- audit log 覆盖 request、preflight、approval、execution 事件。
- audit log 路径进入 agent writable allowlist 时 daemon 拒绝启动。

建议编译测试打开：

- `-Wall -Wextra -Werror`
- AddressSanitizer
- UndefinedBehaviorSanitizer
- 静态分析工具，例如 `clang-tidy` 或 `cppcheck`

## 后续工作建议

建议下一步先写实现计划，不直接开写全部功能。

推荐顺序：

1. 建立 C 项目 skeleton、CMake、基本测试框架。
2. 实现配置加载和 schema 校验。
3. 实现 Unix socket transport 和 `SO_PEERCRED`。
4. 实现 JSON-RPC parser、router、统一 error response。
5. 实现 policy preflight。
6. 实现 approval pending store 和 admin approval flow。
7. 实现 audit log 和不安全 audit path 拒绝。
8. 实现 `sys.info`。
9. 实现安全版 `fs.list`、`fs.read`。
10. 写集成测试：agent request -> approval required -> admin approve -> recheck -> execute -> audit。
11. 再考虑 `proc.*` 和 `net.*`。

这版最重要的交付不是“能调用很多系统能力”，而是把 agent 调用系统能力这件事关进一个可控、可审计、可验证的盒子里。后续所有能力都应该从这个盒子里长出来。
