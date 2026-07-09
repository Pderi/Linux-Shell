# myshell

> **版本变更（2026-07-09，相较 exec 外部命令版）**
>
> - 支持功能：`ls` / `cat` / `grep` / `pwd` 改为**内建命令**（系统调用实现）
> - 删除「外部命令 fork + exec」描述；不再调用 `/bin/ls` 等系统命令

操作系统课程设计实验一：简易 Linux Shell。

## 依赖

在 WSL2 / Ubuntu 中安装：

```bash
sudo apt install build-essential libreadline-dev
```

## 编译与运行

在项目根目录（含 `Makefile` 的目录）执行：

```bash
make          # 编译
make test     # 自动化功能测试（需 Linux/WSL，15 项场景 / 22 条断言）
./myshell     # 交互运行
```

首次在 WSL/Ubuntu 上请先安装依赖：

```bash
sudo apt install build-essential libreadline-dev
```

**一步到位验证**：`make test` 末尾显示 `0 failed` 即表示核心功能全部通过。

详细步骤见 **[docs/Linux环境搭建与编译测试指南.md](docs/Linux环境搭建与编译测试指南.md)**。

## 支持功能

- 内建命令：`cd`、`echo`、`pwd`、`ls`、`cat`、`grep`、`type`、`history`、`alias`、`unalias`、`exit`（系统调用实现，不 exec 外部程序）
- 管道：`|`
- 重定向：`>`、`>>`、`<`
- 后台运行：`&`
- Tab 命令/路径补全（GNU readline）
- 历史记录：`history` / `history n`

## 项目结构

```
include/   头文件
src/       源代码
scripts/   自动化测试（make test）
docs/      文档
```

详细设计见 [docs/设计方案.md](docs/设计方案.md)。  
项目原理与代码讲解见 [docs/项目讲解文档.md](docs/项目讲解文档.md)。
