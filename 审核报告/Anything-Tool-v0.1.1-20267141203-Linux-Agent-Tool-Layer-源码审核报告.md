# Anything Tool v0.1.1 Linux Agent Tool Layer 源码审核报告

项目名：Anything Tool

版本号：v0.1.1

时间戳：20267141203

审核时间：2026-07-14 12:03:37 UTC+0800

审核对象：v0.1.1 Linux Agent Tool Layer 源码、开发报告、开发日志与当前仓库状态

审核类型：源码审核、安全边界审核、开发报告核对、验证结果核对

审核结论：有条件不通过。v0.1.1 对 v0.1.0 的高风险问题做了明显加固，但 admin 授权默认关闭、Linux runtime 未验证、测试仍偏静态契约，安全闭环尚未完成；不建议进入 `proc.*`、`net.*`、文件写入删除或其他能力扩展。

## 审核范围

本次审核覆盖：

- `CMakeLists.txt`
- `include/anything/*.h`
- `src/common/*.c`
- `src/daemon/anythingd.c`
- `src/cli/anythingctl.c`
- `src/tools/sys_info.c`
- `tests/contract/test_v0_1_0_contract.py`
- `tests/security/test_v0_1_1_security_contract.py`
- `config/anythingd.example.toml`
- `scripts/linux_smoke_test.sh`
- `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.1-implementation_2026-07-14_11-28-04_UTC+0800_dev-report.md`
- `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.1-security-hardening_2026-07-14_10-19-26_UTC+0800_dev-report.md`
- 桌面开发日志 `C:\Users\admin\Desktop\anything tool开发日志.md`

## 仓库与版本状态

当前 Git 状态：

- 当前分支：`master`
- 当前提交：`0d59a96 Harden Linux agent tool layer v0.1.1`
- 当前 tag：`v0.1.1`
- 远程状态：`master...origin/master`，本地与远程同步。

CodeGraph 状态：

- 21 files
- 214 nodes
- 538 edges
- 状态为 `[OK] Index is up to date`

## 验证结果

已执行：

- `python tests\contract\test_v0_1_0_contract.py`
  - 结果：通过，输出 `v0.1.1 contract tests passed`。
- `python tests\security\test_v0_1_1_security_contract.py`
  - 结果：通过，输出 `v0.1.1 security contract tests passed`。
- `cmake -S . -B build`
  - 结果：通过，但 CMake 提示当前 Windows 主机只能运行 contract tests，Linux/Unix targets 不会真实构建。
- `ctest --test-dir build --output-on-failure`
  - 结果：通过，`2/2 tests passed`。
- `cmake --build build`
  - 结果：通过，输出 `ninja: no work to do.`。
- `codegraph status .`
  - 结果：通过，状态为 `[OK] Index is up to date`。

未完成：

- 未在真实 Linux 环境运行 `bash scripts/linux_smoke_test.sh`。
- 未完成 Linux 下 `anythingd`/`anythingctl` 编译验证。
- 未完成真实 Unix socket、`SO_PEERCRED`、admin allowlist、approval flow、audit fail-closed 和 socket timeout 行为验证。

## 正向确认

v0.1.1 相比 v0.1.0 有实质改进：

- 新增统一 JSON escaping helper，并接入 RPC、audit、approval list、`sys.info` 输出路径。
- `sys.info` 对 `/etc/os-release` 这类含引号内容的输出风险已有修复。
- audit 写入增加 `checked_audit`，关键事件失败时返回 `audit_failed`。
- daemon 启动前增加 audit log 可写性验证。
- RPC parser 从简单 `strstr` 改成顶层 scanner，能拒绝 duplicate top-level key、non-object params、trailing data 等形态。
- admin socket 增加 UID/GID allowlist 机制。
- accepted socket 增加 `SO_RCVTIMEO` 读取超时。
- Linux smoke script 增加 JSON response 和 audit JSON Lines 解析校验。

这些改动方向正确，可以作为安全加固基础继续推进。

## 主要问题

### 1. 高风险：admin allowlist 默认关闭，示例配置仍允许任意同用户进程走 admin 控制面

位置：

- `src/common/config.c:98`
- `src/common/config.c:240`
- `src/common/config.c:241`
- `config/anythingd.example.toml:21`
- `config/anythingd.example.toml:24`
- `src/daemon/anythingd.c:182`

`anything_config_init` 将 `require_admin_allowlist` 默认为 `0`。`anything_config_identity_is_admin` 在 allowlist 未启用时直接返回 true。示例配置也设置 `require_admin_allowlist = false`，并且 `allowed_uids`、`allowed_gids` 为空。

这意味着默认配置下，任何能连接 admin socket 的同 UID 进程仍可调用 `approval.approve`、`approval.reject`、`approval.execute`。这正是 v0.1.0 审核中要求修复的核心控制面问题。

影响：

- 默认部署仍不能证明 admin 操作来自授权人类。
- agent 只要能访问 admin socket 路径，就可能绕过审批边界。
- “新增 admin allowlist”只是一项可选能力，不是默认安全边界。

建议：

- 将 `require_admin_allowlist` 默认改为 true。
- daemon 启动时要求 admin allowlist 非空，除非显式使用 `--dev-insecure-admin` 之类开发模式。
- 示例配置必须启用 allowlist，或者明确标注为 insecure development only。
- 增加行为测试：默认空 allowlist 时 daemon 启动失败；非授权 UID/GID 调 admin API 被拒绝。

### 2. 中高风险：socket read timeout 只能限制单次阻塞，不能防持续慢速发送拖住 daemon

位置：

- `src/common/transport.c:56`
- `src/common/transport.c:67`
- `src/common/transport.c:69`
- `src/common/transport.c:70`
- `src/daemon/anythingd.c:252`
- `src/daemon/anythingd.c:265`

v0.1.1 为 accepted fd 设置了 `SO_RCVTIMEO`，这能处理完全不发送数据的连接。但 `anything_transport_read_request` 仍在单连接内循环读取，直到 newline、EOF 或 `max_request_bytes`。如果客户端每次在 timeout 前发送 1 字节，daemon 仍可能长时间卡在该连接上，期间无法处理其他 tool/admin 请求。

影响：

- slowloris 风险被缓解但没有消除。
- 一个慢速客户端仍可能拖住单线程 daemon。
- admin 审批入口可能被 tool socket 慢连接间接阻塞。

建议：

- 增加整体 request deadline，而不是只依赖每次 `read` 的 socket timeout。
- 或改为 nonblocking event loop/有限 worker 模型。
- 增加行为测试：慢客户端每隔小于 timeout 的时间发送字节时，daemon 仍能响应其他请求。

### 3. 中风险：audit path 安全仍是字符串和普通 `fopen` 模型，尚未防 symlink/canonical 绕过

位置：

- `src/common/audit.c:54`
- `src/common/audit.c:55`
- `src/common/audit.c:69`
- `src/common/audit.c:75`
- `src/common/audit.c:91`

v0.1.1 增加了 audit startup validation，能检查父目录存在、audit log 可写、audit path 不在 agent writable allowlist 下。但路径关系仍是字符串前缀判断，打开文件仍使用普通 `fopen("a")`，没有 canonicalize、`openat`、`O_NOFOLLOW`、`fstat` 或目录 fd 约束。

影响：

- symlink、目录替换、路径别名等情况仍可能绕过审计路径边界。
- audit log 防篡改模型仍不充分。
- 后续加入 fs 写能力后风险会放大。

建议：

- 使用 canonical path 或 fd-based open 模型。
- 打开 audit log 时使用 `open` + `O_NOFOLLOW`，并对 parent directory 和 final file 做 `fstat` 校验。
- 审计路径不得与任何可写 allowlist 通过 symlink/canonical 关系重叠。
- 增加 symlink audit path、目录替换、不可写运行期失败测试。

### 4. 中风险：测试仍偏静态契约，不能证明安全行为真实生效

位置：

- `tests/security/test_v0_1_1_security_contract.py:36`
- `tests/security/test_v0_1_1_security_contract.py:56`
- `tests/security/test_v0_1_1_security_contract.py:65`
- `tests/security/test_v0_1_1_security_contract.py:73`
- `tests/security/test_v0_1_1_security_contract.py:93`

新增 security contract tests 主要检查源码中是否存在关键字符串、helper、函数名和脚本片段。它们能防止误删结构，但不能证明行为正确。例如测试只检查配置样例包含 `require_admin_allowlist = false`，反而固化了默认不安全配置。

影响：

- admin 授权、audit fail-closed、JSON 合法性、socket timeout 等核心修复没有被执行级验证覆盖。
- Windows 上 `ctest` 通过仍不代表 daemon 在 Linux 可运行。
- 当前测试无法发现默认 admin allowlist 关闭这一安全缺口。

建议：

- 增加 Linux 集成测试，真实启动 daemon 并发请求。
- 增加 C 单元测试或小型 harness 测试 JSON parser、JSON escaping、config validation、audit validation。
- security tests 应校验“默认安全”，不要校验 insecure 示例配置存在。

### 5. 中风险：Linux smoke test 仍未运行，v0.1.1 不能按 runtime 安全版本验收

位置：

- `开发报告/Anything-Tool_Linux-Agent-Tool-Layer_v0.1.1-implementation_2026-07-14_11-28-04_UTC+0800_dev-report.md`
- `scripts/linux_smoke_test.sh`

开发报告明确未运行 `bash scripts/linux_smoke_test.sh`，当前机器没有 Linux/WSL 环境。本次审核也只能在 Windows 上运行契约测试和 CMake 配置。

影响：

- 不能确认 Linux 编译是否通过。
- 不能确认 `SO_PEERCRED`、Unix socket、admin allowlist、approval flow、audit fail-closed、JSON Lines 在真实运行中有效。
- 不能确认 `scripts/linux_smoke_test.sh` 本身可执行且覆盖预期。

建议：

- 在真实 Ubuntu/Linux 环境运行 `bash scripts/linux_smoke_test.sh`。
- 将完整输出写入开发报告或单独验证报告。
- 通过 Linux runtime 验证前，不进入新能力开发。

## 不足与边界问题

- v0.1.1 已经是比 v0.1.0 更好的安全加固版本，但默认配置仍没有 enforce admin allowlist。
- 当前 JSON parser 仍是项目内子集 scanner，不是完整 JSON parser；短期可接受，扩展复杂 params 前需要替换或显著增加测试。
- audit fail-closed 在代码路径中已有体现，但运行期失败场景没有真实测试验证。
- `fs.list`、`fs.read` 尚未实现，fd-based filesystem resolution 仍未进入实际源码审核范围。
- `approval.execute` 仍由 admin 端触发，agent 获取执行结果的产品协议边界仍需后续明确。

## 安全问题汇总

| 严重级别 | 问题 | 建议处理版本 |
| --- | --- | --- |
| 高 | admin allowlist 默认关闭，示例配置允许任意同用户进程使用 admin socket | v0.1.2 必修 |
| 中高 | read timeout 只能限制单次 read，不能防持续慢速发送阻塞 daemon | v0.1.2 必修 |
| 中 | audit path 仍未使用 canonical/fd-based 防 symlink 模型 | v0.1.2 必修 |
| 中 | security tests 偏静态契约，缺少执行级行为验证 | v0.1.2 必修 |
| 中 | Linux smoke test 未运行，runtime 安全边界未验收 | v0.1.2 必修 |

## 下一个版本开发建议

建议下一版本命名为 `v0.1.2-runtime-validation` 或 `v0.1.2-security-closure`，范围仍限定为安全闭环和真实验证，不新增工具能力。

优先级：

1. 默认启用 admin allowlist；空 allowlist 时拒绝启动，开发绕过必须显式标注 insecure。
2. 将示例配置改为安全默认，或拆分 `anythingd.example.toml` 与 `anythingd.dev-insecure.toml`。
3. 增加整体 request deadline 或 nonblocking/worker 模型，解决持续慢速发送阻塞。
4. audit log 使用 canonical/fd-based 打开模型，至少禁止 symlink final target。
5. 在 Linux 环境运行并记录 `bash scripts/linux_smoke_test.sh`。
6. 将 security contract tests 升级为行为测试，覆盖 admin unauthorized、audit fail-closed、JSON parse、slow client、default config safe failure。
7. 在以上闭环完成前，不开发 `proc.*`、`net.*`、`fs.write`、`fs.delete`、MCP wrapper。

## 最终结论

v0.1.1 解决了 v0.1.0 的一部分实质问题，尤其是 JSON escaping、audit checked write、parser 顶层扫描和 socket timeout 的基础实现。但由于 admin allowlist 默认关闭、Linux runtime 未验证、测试仍偏静态契约，本版本仍不能作为安全可扩展基线通过。

下一步应继续做安全闭环和真实 Linux 验证。通过前，不建议扩展任何新的系统能力。
