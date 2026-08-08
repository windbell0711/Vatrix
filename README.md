# Vatrix

植物大战僵尸一代的非官方改版，类编程游戏，聚焦VBE砸罐无尽模式，所有特性均模仿年度版（1.2.0.1073 GOTY）。作者希望通过此游戏，展现AI对于人类的意义，并与玩家共同探讨命运与机遇这一宏大的话题。

玩家在开始游戏前，应当已经通关植物大战僵尸一代，了解游戏的基本规则和机制。同时，本游戏要求玩家具备入门级的编程基础。

## 免责声明

- 本项目为**非官方**作品，与 PopCap Games、Electronic Arts 及其关联公司**无任何关联**，未经其授权或认可。
- 项目仅供**学习交流**使用，**非商业用途**。
- 《植物大战僵尸》（Plants vs. Zombies）及相关名称、商标和游戏素材的版权归 PopCap/EA 所有。
- 运行本项目需要你自备一份**合法购买**的《植物大战僵尸》GOTY 版游戏数据，本项目不提供任何游戏资源。

## 版权与许可

- 本改版基于 [wszqkzqk/PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable)（fork 自上游 commit `7c11939ec729660ce9670c860cdeb14b2cffbd76`）修改，上游代码版权归作者 wszqkzqk 及所有贡献者。
- 代码部分沿用上游许可，为 **LGPL-3.0-or-later**（根目录 `LICENSE` 为 LGPL-3.0 文本，`COPYING` 为 GPL-3.0 文本，由 LGPL-3.0 引用）；本仓库同样以 LGPL-3.0 发布，修改记录见 [CHANGELOG.md](CHANGELOG.md)。
- 本仓库**不包含**任何 PopCap/EA 拥有版权的游戏素材（图片、音频、字体等）。
- 本改版基于公开资料与游戏测试编写，不涉及逆向工程。
- 仓库内嵌套的第三方代码（如 SexyAppFramework、SDL-Mixer-X 等）保留其原有许可证与版权声明，不做改动。









> 以下内容由codex生成。

## 快速开始

### 准备游戏数据

将合法购买的游戏数据放在可执行文件旁边（游戏默认从可执行文件所在目录读取资源）：

- `main.pak`
- `properties/` 目录（与 `main.pak` 同级）

也可用命令行参数自定义路径：

- `-resdir <path>`：指定资源目录（`main.pak` 和 `properties/` 所在位置）
- `-savedir <path>`：指定存档目录（覆盖系统默认的应用数据路径）

### 编译

依赖：CMake、Ninja、支持 C++20 的 C/C++ 编译器，以及 SDL2、libopenmpt、libogg、libvorbis、mpg123、libpng、libjpeg-turbo、OpenGL ES 2.0（或 OpenGL 2.1+）。在 `CMakeLists.txt` 所在目录执行：

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

常用选项：

| 选项 | 默认 | 说明 |
| :--- | :--- | :--- |
| `PVZ_DEBUG` | `OFF` | 启用作弊键、调试显示等调试功能 |
| `DO_FIX_BUGS` | `OFF` | 应用社区对官方 1.2.0.1073 GOTY 版"Bug"的修复（多数玩家视为特性） |
| `CONSOLE` | `OFF` | 显示控制台窗口（仅 Windows） |

各平台依赖安装命令（Arch/Debian/MSYS2/macOS）以及 Android、iOS、WASM 的构建与资源导入说明，详见上游 [README](https://github.com/wszqkzqk/PvZ-Portable)。
