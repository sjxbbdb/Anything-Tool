# Anything Tool v0.1.0 Linux Agent Tool Layer 源码审核报告

项目名：Anything Tool

版本号：v0.1.0

时间戳：2026714955

审核时间：2026-07-14 09:58:39 UTC+0800

审核对象：v0.1.0 Linux Agent Tool Layer 源码、开发报告、开发日志与当前仓库状态

审核类型：源码审核、安全边界审核、开发报告核对、验证结果核对

审核结论：有条件不通过。v0.1.0 已完成 skeleton 和最小闭环雏形，但安全边界、审计可靠性、JSON 处理和真实 Linux 验证仍不足；不建议进入 `proc.*`、`net.*`、文件写入删除或下一阶段能力扩展。

## 审核范围

本次审核覆盖：

- `CMakeLists.txt`
- `include/anything/*.h`
- `src/common/*.c`
- `src/daemon/anythingd.c`
- `src/cli/anythingctl.c`
- `src/tools/sys_info.c`
- `tests/contract/test_v0_1_0_contract.py`
- `config/anythingd.example.toml`
- `scripts/linux_smoke_test.sh`
- `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.0-implementation_2026-07-14_09-30-47_UTC+0800_dev-report.md`
- 桌面开发日志 `C:\Users\admin\Desktop\anything tool开发日志.md`

## 验证结果

已执行：

- `python tests\contract\test_v0_1_0_contract.py`
  - 结果：通过，输出 `v0.1.0 contract tests passed`。
- `cmake -S . -B build`
  - 结果：通过，但 CMake 提示当前 Windows 主机只能运行 contract tests，Linux/Unix targets 不会真实构建。
- `ctest --test-dir build --output-on-failure`
  - 结果：通过，`1/1 Test #1: v0_1_0_contract Passed`。
- `cmake --build build`
  - 结果：通过，输出 `ninja: no work to do.`。
- `codegraph status .`
  - 结果：通过，CodeGraph 索引 18 个文件、170 nodes、374 edges，状态为 `[OK] Index is up to date`。

未完成：

- 未在真实 Linux 环境运行 `bash scripts/linux_smoke_test.sh`。
- 未完成 Linux 下 `anythingd`/`anythingctl` 编译验证。
- 未完成真实 Unix socket、`SO_PEERCRED`、approval flow、audit log 行为验证。

## 主要问题

### 1. 高风险：admin socket 不能真正代表人类审批边界

位置：

- `src/daemon/anythingd.c:241`
- `src/daemon/anythingd.c:246`
- `include/anything/identity.h:12`

当前 daemon 创建 tool socket 和 admin socket 时都使用 `0600` 权限。审批者与请求者是否相同只通过 UID/GID/PID 三元组判断。这样只能阻止“同一个进程”审批自己的请求，不能阻止同一用户下的另一个进程连接 admin socket 并审批。

由于 agent 通常也运行在同一用户下，任何同 UID 进程只要知道 admin socket 路径，就可能调用 `approval.approve` 或 `approval.execute`。这削弱了“agent 不能审批自己请求”的核心安全目标。

影响：

- agent 可通过另起进程绕过 requester/approver 检查。
- admin socket 权限不足以证明人类审批。
- 当前控制面隔离仍是传输路径隔离，不是强身份隔离。

建议：

- 增加 admin UID/GID allowlist 或 dedicated admin group。
- admin socket 使用独立目录和组权限，例如 root/dedicated group 管理。
- approval 身份判断应至少基于授权角色，而不是仅比较 PID。
- 增加测试：同 UID 非授权进程连接 admin socket 必须被拒绝。

### 2. 高风险：审计失败被忽略，无法保证“所有操作可审计”

位置：

- `src/common/audit.c:37`
- `src/common/audit.c:38`
- `src/daemon/anythingd.c:44`
- `src/daemon/anythingd.c:77`
- `src/daemon/anythingd.c:122`
- `src/daemon/anythingd.c:133`

`anything_audit_write_event` 在 `fopen` 失败时返回 `-1`，但 daemon 调用处没有检查返回值。也就是说 audit log 路径不可写、目录不存在、磁盘满或权限错误时，请求仍可能继续执行。

影响：

- 审批、执行、失败等关键事件可能没有审计记录。
- 与设计目标“所有 approvals、denials、executions auditable”不一致。
- 安全事件可能在无日志情况下发生。

建议：

- daemon 启动时创建并验证 audit log 文件可写。
- 对关键事件写入失败执行 fail-closed，至少对 approval 与 execution 阶段必须拒绝继续。
- 为 audit write failure 增加错误响应和测试。
- 将 audit log 打开、权限、目录安全检查前置到 config validation 或 daemon startup。

### 3. 高风险：手写 JSON 输出没有转义，`sys.info` 结果在常见 Linux 上可能不是合法 JSON

位置：

- `src/tools/sys_info.c:37`
- `src/tools/sys_info.c:39`
- `src/tools/sys_info.c:41`
- `src/common/audit.c:44`
- `src/common/audit.c:52`
- `src/common/rpc.c:129`
- `src/common/rpc.c:133`

`sys.info` 将 `/etc/os-release` 第一行直接插入 JSON 字符串。常见 Linux 的 `/etc/os-release` 第一行形如 `PRETTY_NAME="Ubuntu ..."` 或 `PRETTY_NAME="Debian ..."`，其中包含双引号。当前拼接会生成非法 JSON。

同类问题也存在于 audit log、RPC result/error、approval list 等 JSON 拼接位置。任何包含引号、反斜杠或控制字符的字段都可能破坏 JSON Lines 格式，甚至造成日志注入。

影响：

- `sys.info` 成功执行后返回的 JSON 可能不可解析。
- audit log 可能被用户输入污染，导致后续审计工具误判。
- contract test 和 smoke test 只 grep 字符串，不能发现 JSON 合法性问题。

建议：

- 增加统一 JSON string escaping helper。
- 所有输出 JSON 字符串字段必须经过 escaping。
- smoke test 增加 `jq` 或 Python `json.loads` 校验响应和 audit log。
- 优先替换为可靠 JSON writer/parser，至少不要继续扩展手写拼接范围。

### 4. 中高风险：单连接阻塞读可导致 daemon 停止服务

位置：

- `src/daemon/anythingd.c:191`
- `src/daemon/anythingd.c:212`
- `src/common/transport.c:55`
- `src/common/transport.c:58`
- `src/common/transport.c:70`

主循环使用 `poll` 等待 socket 可读，但 `accept_one` 接受连接后调用阻塞式 `read`，直到读到换行、EOF 或达到 `max_request_bytes`。如果客户端连接后只发送少量数据并保持连接不关闭，daemon 会阻塞在该连接上，无法处理 tool/admin 其他请求。

影响：

- 单个恶意或异常客户端即可阻塞整个 daemon。
- admin socket 也会受影响，人工审批可能无法进入。
- `max_request_bytes` 不能防 slowloris 类连接占用。

建议：

- 对 accepted fd 设置非阻塞或接收超时。
- 增加 per-request read deadline。
- 采用 event loop 状态机或每连接 worker。
- 增加测试：客户端连接后不发送 newline，daemon 仍能响应其他请求。

### 5. 中风险：RPC parser 不是严格 JSON-RPC parser，字段查找存在误匹配

位置：

- `src/common/rpc.c:7`
- `src/common/rpc.c:10`
- `src/common/rpc.c:38`
- `src/common/rpc.c:41`
- `src/common/rpc.c:97`

当前 parser 使用 `strstr` 在整段 JSON 中查找 `"jsonrpc"`、`"id"`、`"method"`、`"params"`，没有限制字段必须位于顶层，也不支持完整 JSON string escaping。嵌套对象中出现同名字段时，解析结果可能被请求字段顺序影响。

影响：

- 非法 JSON-RPC shape 可能被接受。
- 顶层字段和 params 内字段可能混淆。
- 后续增加更多 tool params 时风险会快速扩大。

建议：

- v0.1.1 前替换为真实 JSON parser，或实现严格顶层 scanner。
- 增加 invalid shape、nested duplicate keys、escaped string、oversized params 测试。
- request hash 应基于规范化后的 method/params/session，而不是不稳定原始片段。

### 6. 中风险：测试仍偏契约存在性，缺少行为和安全回归测试

位置：

- `tests/contract/test_v0_1_0_contract.py:88`
- `tests/contract/test_v0_1_0_contract.py:93`
- `tests/contract/test_v0_1_0_contract.py:103`

当前测试主要验证文件存在、代码片段存在、延期方法字符串未出现。它不能证明 approval 身份隔离、audit fail-closed、JSON 合法性、socket timeout、policy recheck 等核心行为。

影响：

- 代码里存在的高风险问题不会被当前测试发现。
- Windows 上 `ctest` 通过不代表 Linux daemon 可运行。
- 后续重构容易破坏安全边界而测试不报警。

建议：

- 增加 C 单元测试或 Linux 集成测试。
- Linux smoke test 必须进入 CI 或至少在每次可审核版本中记录真实运行输出。
- contract test 保留，但不能作为主要验收依据。

## 不足与边界问题

- v0.1.0 已有 skeleton，但仍不是完整安全实现。
- Windows 主机未真实构建 Linux targets，当前构建通过只说明 contract test 可运行。
- `fs.list`、`fs.read` 未实现，因此 fd-based path resolution 尚未进入代码审核范围。
- approval pending store 是内存态，daemon 重启后 pending 丢失，符合 skeleton 范围但需在报告中持续标注。
- `approval.execute` 当前由 admin socket 触发，agent 侧如何拿到最终执行结果还需要在产品协议上明确。
- audit path 只检查字符串前缀，未做 canonicalize、symlink、目录权限和文件打开前置验证。

## 安全问题汇总

| 严重级别 | 问题 | 建议处理版本 |
| --- | --- | --- |
| 高 | admin socket 仅靠同用户 `0600` 和 PID 差异，不能证明人类审批 | v0.1.1 必修 |
| 高 | audit 写入失败不阻断执行 | v0.1.1 必修 |
| 高 | JSON 输出未转义，`sys.info` 和 audit log 可能非法或可注入 | v0.1.1 必修 |
| 中高 | 阻塞 read 可被单连接拖住 daemon | v0.1.1 必修 |
| 中 | RPC parser 非严格 JSON-RPC parser | v0.1.1 必修 |
| 中 | 测试主要是契约存在性，缺少行为安全验证 | v0.1.1 必修 |

## 下一个版本开发建议

建议下一版本命名为 `v0.1.1-security-hardening` 或 `v0.1.0-alpha.2`，范围限定为安全修复和验证增强，不新增高风险工具。

优先级：

1. 修复 JSON escaping，确保所有 response 和 audit log 都是合法 JSON。
2. 让 audit log 在 daemon startup 阶段完成可写性和路径安全验证；关键审计事件写入失败时 fail-closed。
3. 强化 admin socket 授权，增加 admin allowlist 或独立 admin group，禁止同 UID 非授权进程审批。
4. 为 accepted socket 增加读取超时或非阻塞处理，防止单连接阻塞整个 daemon。
5. 替换或严格化 JSON-RPC parser。
6. 在 Linux 环境运行并记录 `bash scripts/linux_smoke_test.sh` 完整输出。
7. 增加行为测试：JSON 合法性、audit failure、admin unauthorized、approval same requester denial、request hash mismatch、unknown method、socket slow client。

在以上问题修复前，不建议开发：

- `proc.spawn`
- `proc.kill`
- `net.http_request`
- `fs.write`
- `fs.delete`
- MCP server wrapper

## 最终结论

v0.1.0 达到了“最小闭环 skeleton 已落地”的阶段性目标，但尚未达到安全可扩展基线。当前版本可作为内部 skeleton 保留，不建议作为安全边界可信版本发布，也不建议在此基础上扩展更多系统能力。

下一步应进入安全加固版本，先修复 admin 身份、audit fail-closed、JSON escaping、socket 阻塞和真实 Linux 验证问题。
