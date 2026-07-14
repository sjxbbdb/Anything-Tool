# Anything Tool v1 Linux Agent Tool Layer Design 审核报告

项目名：Anything Tool

版本号：v1

时间戳：2026714740

审核对象：`2026-07-14-linux-agent-tool-layer-design.md`

审核日期：2026-07-14

## 审核结论

该设计方向正确，安全边界意识较强，适合作为第一版架构草案。文档已经明确了默认拒绝、最小权限、审批、审计、能力开关、路径 allowlist、命令 allowlist、网络 allowlist 等核心原则。

但当前版本仍有若干关键安全点停留在原则描述层，尚未沉淀为可实现、可测试、不可绕过的机制。建议在进入正式实现前，补充一节 `Security Invariants` 或 `Non-bypassable Rules`，把审批身份隔离、路径安全、命令执行、审计防篡改、网络 SSRF 防护等约束写成明确验收标准。

总体建议：架构草案可以继续推进，但不建议直接进入功能实现。应先补齐关键安全约束，再实现第一版 skeleton。

## 风险概览

| 严重级别 | 问题 | 影响 |
| --- | --- | --- |
| 高 | 审批控制面和 agent 调用面未明确隔离 | agent 可能理论上审批自己的请求 |
| 高 | 文件路径安全只描述 canonicalize | 可能出现 symlink 或 TOCTOU 绕过 |
| 中高 | 命令 allowlist 语义过弱 | `proc.spawn` 容易演变成变相 shell 能力 |
| 中 | 审计日志缺少防篡改模型 | 关键安全事件可能被同 UID 进程修改或删除 |
| 中 | 网络工具缺少 SSRF 与 DNS rebinding 防护细节 | allowlist 可能被 redirect、DNS 或私网地址绕过 |
| 中 | 进程生命周期约束不足 | 可能出现输出阻塞、孤儿进程、超时清理不完整 |

## 详细发现

### 1. 审批控制面和 agent 调用面未明确隔离

文档定义了 approval required、pending request、approve/reject 等流程，但没有明确 `anythingctl approve/reject` 与 agent 发起的 tool request 是否共享同一 Unix socket/API，也没有说明 daemon 如何区分人类审批者、agent 调用者和测试客户端。

如果审批管理接口与普通工具调用接口共享同一权限面，后续实现中可能出现 agent 发起请求后再调用 approval API 自行批准的风险。

建议：

- 使用 Unix socket `SO_PEERCRED` 获取调用方 UID、GID、PID。
- 在协议层区分 agent request API 和 human/admin approval API。
- approval 必须绑定 request hash、caller identity、session identity 和 expiry。
- 明确禁止请求发起者审批自己的 pending request。
- 审批动作必须进入 audit log，并记录审批者身份和原始请求发起者身份。

### 2. 文件路径安全只描述 canonicalize，不足以防 TOCTOU

文档要求所有路径在 policy check 前 canonicalize，并要求 symlink traversal 不得逃出 allowed roots。该原则正确，但对 `fs.write`、`fs.mkdir`、`fs.delete` 等会改变文件系统状态的工具来说，仅靠字符串 canonicalization 容易产生 race condition。

例如，policy check 通过后，目标路径或中间目录可能被替换为 symlink，导致实际 syscall 操作到 allowed root 之外的路径。

建议：

- 路径解析应基于 dirfd/root fd，而不是纯字符串前缀判断。
- 优先使用 `openat2`，并配置 `RESOLVE_BENEATH`、`RESOLVE_NO_SYMLINKS` 等约束。
- 对不支持 `openat2` 的环境，设计明确 fallback 策略，并说明安全降级边界。
- 对文件打开使用 `O_NOFOLLOW`，打开后通过 `fstat` 校验实际对象。
- path escape 测试必须覆盖 symlink、`..`、rename race、hard link、目录替换等场景。

### 3. 命令 allowlist 语义过弱

配置示例中的 `allow_args = ["status", "log", "diff"]` 容易被实现成“首参数允许”或“参数包含允许词”。对 `proc.spawn` 这类高风险能力来说，这种表达不够精确。

如果参数校验不严格，允许的命令可能通过额外参数、配置覆盖、环境变量、工作目录或 stdin 行为扩大权限边界。

建议：

- 将 `allow_args` 替换或扩展为结构化 command schema。
- 每个命令配置应明确允许的子命令、参数数量、选项白名单、参数值模式、工作目录 allowlist。
- 执行时必须使用绝对路径和 `execve`/`posix_spawn` 一类接口，不经过 shell。
- 默认清理环境变量，仅允许显式声明的 env。
- 默认禁用 stdin，或对 stdin 大小和内容做限制。
- stdout/stderr 必须有独立大小限制和截断策略。

### 4. 审计日志缺少防篡改模型

文档定义了 JSON Lines audit log 和事件类型，但没有说明日志文件如何避免被删除、覆盖或篡改。示例配置中 audit log 位于用户 state 目录；如果 daemon 与 agent 以同一用户运行，且 filesystem write 能力开启，审计日志存在被同 UID 进程修改的风险。

建议：

- audit log 路径不得位于任何 agent 可写 allowlist 下。
- daemon 启动时应检测并拒绝不安全的 audit log 配置。
- 日志文件权限和目录权限应有明确要求。
- 生产部署可考虑 journald/syslog 转发。
- 如继续使用文件日志，可考虑 hash chain 或 append-only 策略，至少保证篡改可检测。
- 审计事件应包含 request id、caller identity、approval identity、method、decision、risk、duration、error kind 和参数摘要。

### 5. 网络工具缺少 SSRF 与 DNS rebinding 防护细节

文档要求 network tools enforce host and port allowlists，但对 HTTP 请求而言，host allowlist 本身不足以覆盖常见 SSRF 绕过方式。

风险包括 DNS rebinding、HTTP redirect 到非 allowlist host、Host header 与 SNI 不一致、IP literal 绕过、解析到 loopback/link-local/private address 等。

建议：

- DNS 解析结果应在请求生命周期内 pinning。
- 每次 redirect 后重新执行 host、port、scheme、IP range 校验。
- 明确是否允许 private、loopback、link-local、multicast 地址。
- 明确是否允许 IP literal。
- TLS SNI、HTTP Host header 和 allowlist host 应保持一致。
- 限制 response body、header size、redirect 次数和总耗时。

### 6. 进程生命周期约束不足

文档定义了 `proc.spawn`、`proc.status`、`proc.output`、`proc.kill`，但没有说明 stdout/stderr drain、防输出死锁、进程组、超时清理、僵尸回收等行为。

这些细节会直接影响工具层稳定性和安全性。尤其是长期运行命令、输出大量数据、子进程派生后脱离父进程等场景，如果没有统一规则，后续实现容易出现资源泄漏或 kill 不完整。

建议：

- 每个 spawned process 应归属 daemon 管理的 process group。
- timeout 后应终止整个 process group，而不是只 kill 父进程。
- stdout/stderr 应异步 drain，避免 pipe 阻塞。
- 输出超过限制后的行为应明确：截断、终止进程，或两者结合。
- daemon 必须负责 wait/reap，避免 zombie process。
- `proc.kill` 只能作用于 tool layer 创建并记录的进程。

## 建议补充的安全不变量

建议在原设计文档中新增以下不可绕过规则：

- 所有 capability 默认关闭，未显式启用即拒绝。
- agent 不能调用 approval 管理接口。
- agent 不能审批自己发起的 pending request。
- approval 必须绑定 request hash、caller identity、session identity、method scope 和 expiry。
- 所有文件路径操作必须基于 fd/root 约束，禁止只做字符串前缀检查。
- audit log 路径不得位于任何 agent 可写路径 allowlist 下。
- `proc.spawn` 不允许 shell，参数必须按结构化 schema 校验。
- `proc.kill` 只能终止本工具层启动并追踪的进程。
- network request 的 DNS、redirect、最终 IP、Host header 和 TLS SNI 都必须满足 allowlist。
- 任何 policy violation 都必须在系统调用前拒绝，并写入 audit log。

## 建议的第一版验收标准

第一版实现建议优先验收安全边界，而不是追求工具数量。

必须覆盖：

- JSON-RPC shape 校验和 unknown method 处理。
- 默认拒绝策略。
- capability flag 开关。
- `sys.info` 正常返回。
- `fs.list` 和 `fs.read` 仅能访问 allowlist 内路径。
- symlink escape、`..` escape、rename race 测试。
- per-call approval required、approval granted、approval rejected、approval expired。
- request hash mismatch 必须拒绝执行。
- agent identity 与 approval identity 分离。
- audit log 覆盖 request received、preflight denied、approval required、approval granted/rejected、execution started、execution finished/failed。
- audit log 路径进入 fs writable allowlist 时，daemon 应拒绝启动或拒绝配置。

## 推荐处理顺序

1. 在设计文档中补充 `Security Invariants`。
2. 收紧 configuration schema，特别是 command allowlist 和 audit log 相关配置。
3. 明确 Unix socket peer credential 与 approval 权限模型。
4. 明确 filesystem 实现必须使用 fd-based path resolution。
5. 为 network 和 process 工具补充安全细节，但可以暂不进入第一版实现。
6. 再开始第一版 skeleton：config、transport、rpc、policy、approval、audit、`sys.info`、`fs.list`、`fs.read`。

## 最终建议

该设计可以作为项目的基础架构方向继续推进，但应在正式实现前完成安全机制补充。当前最值得优先修订的不是工具覆盖面，而是把“默认拒绝、审批、人类控制、路径边界、审计可信度”这些安全原则转化为明确的实现约束和测试验收项。
