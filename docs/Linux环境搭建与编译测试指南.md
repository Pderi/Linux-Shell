# Linux 环境搭建与编译测试指南

> **版本变更（2026-07-09，相较 exec 外部命令版）**
>
> - 环境说明：明确 ls/cat/grep 为**内建实现**，不 exec 外部程序
> - 自动化测试 A5：由 `grep | wc -l` 改为 `grep | grep`（纯内建管道）
> - 自动化测试 A14/A15：由 `sleep &` 改为 `grep &` / `ls &`
> - 新增 A16：验证非内建命令（如 `wc`）报 `command not found`
> - 手动测试 T6：`type ls` 预期改为 `ls is a shell builtin`

> 适用项目：myshell（操作系统课程设计 · 实验一）  
> 开发模式：Windows + Cursor 编辑代码，WSL2 / Linux 编译运行

---

## 目录

1. [环境说明](#1-环境说明)
2. [安装 WSL2](#2-安装-wsl2)
3. [安装编译依赖](#3-安装编译依赖)
4. [进入项目目录](#4-进入项目目录)
5. [编译 myshell](#5-编译-myshell)
6. [自动化测试（推荐）](#6-自动化测试推荐)
7. [交互运行 myshell](#7-交互运行-myshell)
8. [手动功能测试用例](#8-手动功能测试用例)
9. [修改代码后重新编译](#9-修改代码后重新编译)
10. [Cursor 中使用 WSL 终端](#10-cursor-中使用-wsl-终端)
11. [机房 / 其他 Linux 发行版](#11-机房--其他-linux-发行版)
12. [常见问题](#12-常见问题)
13. [日常开发流程](#13-日常开发流程)

---

## 1. 环境说明

| 环节 | 环境 | 作用 |
|------|------|------|
| 写代码 | Windows + Cursor | 编辑 C 源码 |
| 编译运行 | WSL2 + Ubuntu | `gcc` 编译、`./myshell` 运行 |
| 演示提交 | 任意 Linux | 机房或 WSL 均可 |

**为什么必须在 Linux 上编译？**

myshell 使用 `fork`、`pipe`、`open`、`chdir` 等 Linux 系统调用，只能在 Linux 环境下编译和运行，Windows 原生无法直接运行。实验命令（ls/cat/grep 等）均为**内建实现**，不 exec 外部程序。

**项目放在哪里都可以**：解压压缩包、Git 克隆、U 盘拷贝均可，只要进入**含 `Makefile` 的项目根目录**即可编译测试。

**Windows 路径与 WSL 路径换算（在 WSL 里访问 Windows 磁盘上的项目时使用）：**

| 规则 | 说明 |
|------|------|
| 盘符 | `C:` → `/mnt/c`，`D:` → `/mnt/d`（小写） |
| 分隔符 | `\` 改为 `/` |
| 示例 | `D:\study\myshell` → `/mnt/d/study/myshell` |
| 示例 | `C:\Users\zhang\Desktop\shell` → `/mnt/c/Users/zhang/Desktop/shell` |

若项目已在 Linux 主目录（如 `~/myshell`），在 WSL 中直接使用该路径，无需 `/mnt/...` 前缀。

---

## 2. 安装 WSL2

### 2.1 安装步骤

1. 以**管理员身份**打开 PowerShell  
   （开始菜单 → 搜索 PowerShell → 右键「以管理员身份运行」）

2. 执行：

```powershell
wsl --install
```

3. **重启电脑**

4. 重启后自动弹出 Ubuntu 初始化窗口，按提示：
   - 设置 Linux **用户名**（小写英文）
   - 设置 **密码**（输入时不显示，请记住）

### 2.2 验证安装

重启后在 PowerShell 中执行：

```powershell
wsl --version
```

应显示 WSL 版本信息。

进入 Linux：

```powershell
wsl
```

提示符类似：

```
yourname@DESKTOP-XXXX:~$
```

> 若已安装 WSL，跳过本节，直接从 [第 3 步](#3-安装编译依赖) 开始。

---

## 3. 安装编译依赖

在 **WSL / Ubuntu 终端**中执行：

```bash
sudo apt update
sudo apt install -y build-essential libreadline-dev
```

| 包名 | 用途 |
|------|------|
| `build-essential` | gcc、make 等编译工具 |
| `libreadline-dev` | Tab 补全（GNU readline 库） |

验证安装：

```bash
gcc --version
make --version
```

两条命令均有版本输出即表示成功。

---

## 4. 进入项目目录

无论项目从何处获得（压缩包、Git、U 盘），请先进入**项目根目录**——即与 `Makefile` 同级的目录。

### 4.1 确认你在正确目录

```bash
cd <你的项目路径>
ls Makefile
```

能列出 `Makefile` 即表示路径正确。若提示 `No such file`，说明还在上级或子目录，请调整 `cd` 目标。

完整目录应类似：

```
Makefile  README.md  docs  include  scripts  src
```

### 4.2 常见场景示例

**场景 A：在 WSL 中打开 Windows 磁盘上的项目**

```bash
# 将 <盘符>、<路径> 换成你的实际位置，例如 D:\study\myshell
cd /mnt/<盘符>/<路径>
```

**场景 B：解压到 Linux 主目录（推荐，编译较快）**

```bash
cd ~/myshell          # 解压后的文件夹名以你实际为准
```

**场景 C：机房 / 纯 Linux 环境**

```bash
cd ~/shell            # 或 cd /home/用户名/下载/myshell 等任意路径
```

### 4.3 查看源码结构

```bash
ls include/ src/ scripts/
```

| 目录 | 说明 |
|------|------|
| `include/` | 头文件 |
| `src/` | C 源代码（9 个模块） |
| `scripts/` | 自动化测试脚本 `run_tests.sh` |
| `docs/` | 设计文档、分工、讲解、本指南 |

---

## 5. 编译 myshell

### 5.1 编译命令

```bash
make
```

### 5.2 成功标志

- 终端无 `error:` 报错
- 当前目录生成可执行文件 `myshell`

```bash
ls -l myshell
```

应看到类似：

```
-rwxr-xr-x 1 user user xxxxx ... myshell
```

### 5.3 预期编译输出（示例）

```
gcc -Wall -Wextra -std=c11 -D_GNU_SOURCE -Iinclude -c -o src/main.o src/main.c
gcc -Wall -Wextra -std=c11 -D_GNU_SOURCE -Iinclude -c -o src/parser.o src/parser.c
...
gcc -Wall -Wextra -std=c11 -D_GNU_SOURCE -Iinclude -o myshell src/main.o ... -lreadline -ltinfo
```

### 5.4 常见编译错误

| 报错 | 原因 | 解决 |
|------|------|------|
| `readline/readline.h: No such file` | 未安装 readline 开发包 | `sudo apt install libreadline-dev` |
| `make: command not found` | 未安装编译工具 | `sudo apt install build-essential` |
| `undefined reference to 'readline'` | 链接缺少 readline | 确认 Makefile 含 `-lreadline -ltinfo` |
| `undefined reference to 'tigetstr'` 等 | 缺少 terminfo 库 | `sudo apt install libreadline-dev`（会拉取依赖） |

---

## 6. 自动化测试（推荐）

编译通过后，**先跑自动化测试**，确认核心功能正常，再进入交互模式手动体验。

### 6.1 一条命令完成编译 + 测试

```bash
make test
```

等价于：

```bash
make
bash scripts/run_tests.sh
```

### 6.2 成功标志

终端输出全部 `[PASS]`，末尾类似：

```
==============================
Results: 22 passed, 0 failed
==============================
```

> 脚本含 15 个测试场景、22 条断言（type 测 2 条、history 测 2 条、alias 测 4 条、T15 测 2 条等），**`0 failed` 即表示全部通过**。

### 6.3 自动化测试覆盖项

| 编号 | 测试内容 | 对应实验要求 |
|------|----------|--------------|
| A1 | `echo hello world` | echo |
| A2 | `type cd` / `type ls` | type |
| A3 | `cd /tmp` + `pwd` | cd |
| A4 | `>` / `>>` 重定向 | 重定向 |
| A5 | `grep ... \| grep root` | 管道（纯内建） |
| A6 | `echo hello \| grep hello` | 管道 + 内建 echo |
| A7 | `alias` 创建与展开（含 `alias lr='ls -l'`） | alias |
| A8 | `history 3` | history |
| A9 | `echo $HOME` | echo 环境变量 |
| A10 | `grep root /etc/passwd` | grep 内建 |
| A11 | 引号路径 `> "q out.txt"` | 重定向（含空格路径） |
| A12 | `cat /etc/passwd` | cat 内建 |
| A13 | `ls /bin/ls` | ls 内建 |
| A14 | `grep root /etc/passwd &` | 后台运行 |
| A15 | 引号内 `&` + 尾部空格 `&` | 后台标志解析 |
| A16 | `wc -l /etc/passwd` | 非内建命令拒绝（command not found） |

### 6.4 原理说明

myshell 在非交互模式（管道/脚本输入）下会自动使用 `getline` 读入，不显示 readline 提示符，因此 `scripts/run_tests.sh` 可通过 heredoc 批量喂命令：

```bash
./myshell < test_commands.txt
```

脚本中 `run_shell()` 会为每次用例创建**临时独立 HOME**，并将 myshell 的 **stdout 与 stderr 合并捕获**（`2>&1`），因此 `waitpid: No child processes` 等内部错误也会使对应用例失败，而不会「表面 PASS、实际有报错」。

Tab 补全、上下键历史等**交互功能**无法自动化，需在第 8 节手动测试。

---

## 7. 交互运行 myshell

### 7.1 启动

```bash
./myshell
```

### 7.2 成功标志

出现自定义提示符，格式为 `myshell:<当前工作目录>$`，例如：

```
myshell:/home/user/myshell$
myshell:/mnt/d/study/myshell$
```

- 普通用户以 `$` 结尾
- root 用户以 `#` 结尾
- 中间的目录会随你 `cd` 进入项目的位置而变化，**不必与示例完全一致**

### 7.3 退出方式

```bash
exit
```

或按 `Ctrl+D`。

---

## 8. 手动功能测试用例

在 `./myshell` 内逐项测试，重点覆盖**自动化测不到的交互功能**（Tab 补全、上下键历史等）。建议截图保存，用于课程报告和演示。

> 以下 T1–T21 与 `make test` 自动化项有重叠，主要用于**截图演示**；自动化以第 6 节为准。

### 8.1 命令执行（外部 / 内置）

| 编号 | 输入 | 预期结果 | 通过 |
|------|------|----------|------|
| T1 | `ls -l` | 列出当前目录文件详情 | ☐ |
| T2 | `cat /etc/passwd` | 输出 passwd 文件内容 | ☐ |
| T3 | `grep root /etc/passwd` | 显示含 root 的行 | ☐ |
| T4 | `echo hello world` | 输出 `hello world` | ☐ |
| T5 | `type cd` | 输出 `cd is a shell builtin` | ☐ |
| T6 | `type ls` | 输出 `ls is a shell builtin` | ☐ |

### 8.2 内置命令 cd

| 编号 | 输入 | 预期结果 | 通过 |
|------|------|----------|------|
| T7 | `cd /tmp`，再执行 `pwd` | 输出 `/tmp` | ☐ |
| T8 | `cd` | 回到 `$HOME` 目录 | ☐ |
| T9 | `cd ~` | 回到 `$HOME` 目录 | ☐ |
| T10 | `echo $HOME` | 输出当前用户主目录路径 | ☐ |

### 8.3 重定向

| 编号 | 输入 | 预期结果 | 通过 |
|------|------|----------|------|
| T11 | `echo test > /tmp/out.txt` | 创建文件，内容为 `test` | ☐ |
| T12 | `echo append >> /tmp/out.txt` | 文件追加一行 `append` | ☐ |
| T13 | `cat < /tmp/out.txt` | 输出文件全部内容 | ☐ |

验证文件内容（在 myshell 外或 myshell 内均可）：

```bash
cat /tmp/out.txt
```

### 8.4 管道

| 编号 | 输入 | 预期结果 | 通过 |
|------|------|----------|------|
| T14 | `ls \| grep myshell` | 只显示含 myshell 的行 | ☐ |
| T15 | `echo hello > /tmp/f.txt`，`cat /tmp/f.txt \| grep hello \| wc -l` | 输出 `1` | ☐ |
| T16 | `echo pipe \| wc -c` | 输出字符数（含换行） | ☐ |

### 8.5 后台运行

| 编号 | 输入 | 预期结果 | 通过 |
|------|------|----------|------|
| T17 | `sleep 3 &` | 立即显示 `[pid] running in background`，提示符立刻可用 | ☐ |

### 8.6 history / alias

| 编号 | 输入 | 预期结果 | 通过 |
|------|------|----------|------|
| T18 | 执行若干命令后输入 `history` | 按序号列出历史命令 | ☐ |
| T19 | `history 3` | 显示最近 3 条 | ☐ |
| T20 | `alias ll='ls -l'`，再输入 `ll` | 等价于 `ls -l` | ☐ |
| T21 | `unalias ll`，再输入 `ll` | 提示命令未找到 | ☐ |

### 8.7 Tab 补全（必须手动测）

| 编号 | 操作 | 预期结果 | 通过 |
|------|------|----------|------|
| T22 | 输入 `hi` 后按 **Tab** | 补全为 `history` | ☐ |
| T23 | 输入 `ls /et` 后按 **Tab** | 补全路径（如 `/etc/`） | ☐ |
| T24 | 输入前缀后按 **两次 Tab** | 列出所有匹配候选 | ☐ |

### 8.8 快速手动测试命令

`make test` 通过后，进入 `./myshell` 可粘贴以下命令做交互验证：

```bash
ls -l
echo hello world
echo $HOME
type cd
type ls
cd /tmp
pwd
cd ~
echo test > /tmp/out.txt
echo append >> /tmp/out.txt
cat < /tmp/out.txt
ls | grep myshell
echo hello | wc -l
sleep 1 &
history
history 3
alias ll='ls -l'
ll
unalias ll
exit
```

---

## 9. 修改代码后重新编译

每次改完代码：

```bash
make clean
make test
```

或一行：

```bash
make clean && make test
```

`make test` 通过后，再 `./myshell` 做交互体验。

仅修改单个 `.c` 文件时，直接 `make test` 即可（Makefile 会自动增量编译）。

---

## 10. Cursor 中使用 WSL 终端

1. 在 Cursor 中**打开项目根目录**（含 `Makefile` 的文件夹，路径因每人机器而异）
2. 打开终端：`` Ctrl+` ``
3. 点击终端面板右上角 **+** 旁的下拉箭头
4. 选择 **Ubuntu (WSL)**（或 Select Default Profile → Ubuntu）
5. 在新终端中进入项目并测试：

```bash
# 若 Cursor 已打开项目根目录，可先执行 pwd 确认当前路径
pwd
ls Makefile

# 若不在项目根目录，cd 到含 Makefile 的目录（WSL 路径见第 1 节换算规则）
cd <你的项目路径>
make test
./myshell
```

**小技巧**：在 Cursor 资源管理器中右键项目文件夹 →「在集成终端中打开」，通常会自动 `cd` 到正确目录；若终端是 WSL，请确认路径为 `/mnt/...` 形式。

此后工作流：

```
Cursor 编辑保存 → WSL 终端进入项目根目录 → make test → ./myshell 交互验证
```

---

## 11. 机房 / 其他 Linux 发行版

### 11.1 获取项目

将收到的压缩包解压，或通过 U 盘、Git 等方式拷贝到 Linux。**文件夹名称和存放位置不限**，记住解压后的路径即可。

### 11.2 安装依赖

**Ubuntu / Debian / 麒麟（apt 系）：**

```bash
sudo apt update
sudo apt install build-essential libreadline-dev
```

**RHEL / CentOS / Fedora（dnf 系）：**

```bash
sudo dnf install gcc make readline-devel
```

### 11.3 编译与测试

```bash
cd <解压或拷贝后的项目根目录>    # 该目录下应有 Makefile
ls Makefile                      # 确认无误后再编译
make test
./myshell
```

---

## 12. 常见问题

### Q1：`wsl` 不是内部或外部命令

WSL 未安装。以管理员 PowerShell 执行：

```powershell
wsl --install -d Ubuntu
```

然后重启。

### Q2：`Permission denied` 运行 `./myshell`

```bash
chmod +x myshell
./myshell
```

或重新 `make` 生成。

### Q3：提示符出现但输入命令无反应

检查是否在 myshell 内（提示符为 `myshell:...$`），而非普通 bash。

### Q4：中文路径 / 乱码

项目路径请尽量使用**英文目录名**（如 `~/myshell`、`D:\study\shell`），避免中文或特殊字符，以免 WSL / 终端编码异常。

### Q5：`make test` 有 `[FAIL]` 或终端出现 `waitpid: No child processes`

1. 查看失败项名称和实际输出
2. 确认在 Linux 环境运行（不是 Windows PowerShell）
3. 确认 `/etc/passwd` 存在（A5/A10/A12 依赖此文件）
4. 若 stderr 出现 `waitpid: No child processes`，说明 Shell 中 **SIGCHLD 处理与 waitpid 冲突**（例如误用 `signal(SIGCHLD, SIG_IGN)` 同时又 wait 前台子进程）；请使用最新版 `executor.c`（`executor_setup_signals()` + wait 期间屏蔽 SIGCHLD）
5. 将完整终端输出保存，便于排查

> 测试脚本会为每次用例创建**临时独立 HOME**，不会读写你真实的 `~/.myshell_history`，可放心反复运行。

### Q5b：`scripts/run_tests.sh: command not found`、`$'\r': command not found` 或 `syntax error near unexpected token '('`

**CRLF 换行（Windows 保存）：** 在 WSL 中执行：

```bash
sed -i 's/\r$//' scripts/run_tests.sh
chmod +x scripts/run_tests.sh
make test
```

**语法错误（含括号）：** 确认使用 `bash scripts/run_tests.sh`（不要用 `sh`）；并检查脚本是否为最新版（T5 引号闭合、T13 skip 分支不含未加引号的括号）。使用 Git 克隆的项目一般无此问题（仓库已配置 `.gitattributes` 保持 LF）。

### Q6：`make` 成功但功能异常

1. 确认运行的是当前目录的 `./myshell`，不是系统 `/bin/bash`
2. 用 `type ls` 确认进入的是 myshell
3. 将异常命令和完整输出截图，便于排查

### Q7：WSL 访问 Windows 文件很慢

属正常现象。若介意，可将项目复制到 Linux 原生目录（把源路径换成你的实际 WSL 路径）：

```bash
cp -r /mnt/d/你的路径/myshell ~/myshell
cd ~/myshell
make
./myshell
```

---

## 13. 日常开发流程

```
┌─────────────────────────────────────────┐
│  1. Cursor（Windows）编辑 src/*.c       │
└─────────────────┬───────────────────────┘
                  │ 保存
                  ▼
┌─────────────────────────────────────────┐
│  2. WSL 终端                             │
│     cd <项目根目录>（含 Makefile）        │
│     make test        ← 自动化 15 项      │
└─────────────────┬───────────────────────┘
                  │ 0 failed
                  ▼
┌─────────────────────────────────────────┐
│  3. ./myshell 交互测试                   │
│     Tab 补全、上下键历史等（第 8 节）      │
└─────────────────┬───────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────┐
│  4. 截图 → 课程报告 / 演示               │
└─────────────────────────────────────────┘
```

**推荐第一条命令（每次打开 WSL 终端）：**

```bash
cd <你的项目根目录> && make test
```

可将 `<你的项目根目录>` 换成固定路径后写入 shell 别名，例如：

```bash
# 写入 ~/.bashrc 后执行 source ~/.bashrc
alias mshell='cd ~/myshell && make test'
```

---

## 附录：测试结果记录表（报告用）

### 自动化测试

| 项目 | 结果 |
|------|------|
| `make test` 通过（0 failed） | ☐ |
| 测试环境（WSL / 机房 Linux） | ________ |
| 测试日期 | ________ |

### 手动测试（交互功能）

| 类别 | 通过数 | 总数 |
|------|--------|------|
| 命令执行 T1–T6 | /6 | |
| cd / echo T7–T10 | /4 | |
| 重定向 T11–T13 | /3 | |
| 管道 T14–T16 | /3 | |
| 后台 T17 | /1 | |
| history/alias T18–T21 | /4 | |
| Tab 补全 T22–T24 | /3 | |
| **手动合计** | **/24** | |

测试人：________
