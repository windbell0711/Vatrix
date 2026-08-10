# Vatrix

植物大战僵尸一代的非官方改版，类编程游戏。冒险模式在年度版（1.2.0.1073 GOTY）基础上扩展出世界 6（6-1~6-10），以砸罐关卡为主线，玩家可用内嵌的 Python 3.12 编写脚本读取/修改部分游戏状态（打碎罐子、种植、铲除、查询场上单位等）。作者希望通过此游戏，展现 AI 对于人类的意义，并与玩家共同探讨命运与机遇这一宏大的话题。

玩家在开始游戏前，应当已经通关植物大战僵尸一代，了解游戏的基本规则和机制。同时，本游戏要求玩家具备入门级的python编程基础。

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

## 游戏特色

- **冒险世界 6**：5-10 为自定义僵王关，通关后进入世界 6（6-1~6-10）；6-1~6-4、6-6~6-9 为砸罐关卡（单批，无阶段推进），6-5 为普通关。所有关卡默认解锁，便于直接测试。
- **种子化砸罐**：罐子内容分布由随机种子决定（种子 = 玩家 ID + 启动随机种子 + 关卡号），同一关随种子不同布局不同。
- **关卡奖励**：5-10 通关得纸条（`VX_HINT_1`）+3 钻石；6-1 得纸条（`VX_HINT_2`）+2 钻石；6-2 得图鉴 +2 钻石。点击纸条/图鉴后钻石扇形飞出收入钱袋。
- **Python 脚本系统**：内嵌 CPython 3.12，玩家可在关卡内用 Python 操控游戏。
- **调试快捷键**（需 `PVZ_DEBUG=ON`）：主菜单按 `0` 直接进入 5-10，`1`~`9` 分别进入 6-1~6-9。

## Python 脚本

每关载入对应的关卡脚本：`scripts/script_adventure_<世界>_<关号>.py`（如 6-1 对应 `scripts/script_adventure_6_1.py`），Dave 开场对话结束后启动。脚本顶层异常会写入 `scripts/vx_script.log`，不影响游戏继续。

世界 6（6-1~6-10）改为玩家驱动：进入关卡后脚本不会自动运行，画面下方的 **Open / Reset / Run / Submit** 按钮控制流程。点击 **Open** 打开游戏内编辑器（窗口右侧延伸 250px，变为 1050x600），可直接编写/修改当前关脚本（首次打开会写入模板）；**Run** 为试玩（保存并重开本关，胜利不推进存档、不发奖励，编辑器保持原有开合状态），**Reset** 为同种子重开，**Submit** 为正式通关（胜利后推进存档并发奖）。编辑器面板内有 保存/运行/关闭，运行前自动保存到 `scripts/script_adventure_<世界>_<关号>.py`；若该文件在外部（VSCode/PyCharm）被修改而编辑器缓冲也有改动，会弹出覆盖/重新加载选择。

### vb 包

关卡脚本 `import vb` 即可使用：

| 函数 | 说明 |
| :--- | :--- |
| `vb.brk(row, col, delay=0)` | 打碎指定罐子，然后脚本阻塞 `delay` 秒 |
| `vb.slp(delay)` | 脚本暂停 `delay` 秒 |
| `vb.plt(row, col, card_id=0)` | 种下卡槽第 `card_id` 张卡片（0 基） |
| `vb.rmv(row, col)` | 铲除指定格植物 |
| `vb.get_zombies()` / `vb.get_plants()` | 场上僵尸 / 植物快照列表 |
| `vb.get_cards()` / `vb.get_vases()` | 掉落卡片 / 罐子快照列表 |
| `vb.consts` | 植物（`pt`）、僵尸（`zt`）、罐子内容（`vc`）等类型常量 |

动作类接口行列从 1 开始数；查询返回冻结 dataclass 快照（`typ` 为类型号），僵尸/植物/卡片/罐子对象分别提供 `rmv()`、`plc(row, col)` 等方法。快照默认不可修改，避免脚本污染游戏状态。

### 第三方库（userlibs）

游戏根目录下的 `userlibs/`（自动创建）会加入 Python 搜索路径：可放入与内置 Python 3.12 兼容的纯 Python 包或 `.pyd` 扩展，`import` 后即可在关卡脚本中使用。系统模块不做沙箱限制，风险自担。

### 关卡数据

砸罐阵容、场景、卡槽由 `Properties/adventure_info.csv` 驱动（罐子内容池会按随机种子洗牌），纸条图片为 `Properties/VX_HINT_1.png`、`Properties/VX_HINT_2.png`。开发模式下游戏直接从仓库的 `scripts/`、`src/python/` 读取脚本与数据，修改后重开游戏即生效，无需重新编译；发布版则读取 exe 旁的 `scripts/`。

## 快速开始

### 准备游戏数据

将合法购买的游戏数据放在可执行文件旁边（游戏默认从可执行文件所在目录读取资源）：

- `main.pak`
- `properties/` 目录（与 `main.pak` 同级）
- `scripts/` 目录（游戏自带脚本；发布版还需 `python312/`、`python312.dll`、`vcruntime140*.dll`，即内嵌 Python 运行时与标准库；`userlibs/` 会在运行时自动创建）

也可用命令行参数自定义路径：

- `-resdir <path>`：指定资源目录（`main.pak` 和 `properties/` 所在位置）
- `-savedir <path>`：指定存档目录（覆盖系统默认的应用数据路径）

### 编译

依赖：CMake、Ninja、支持 C++20 的 C/C++ 编译器，以及 SDL2、libopenmpt、libogg、libvorbis、mpg123、libpng、libjpeg-turbo、OpenGL ES 2.0（或 OpenGL 2.1+）。在 `CMakeLists.txt` 所在目录执行：

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Python 运行时默认自包含：首次构建前先运行 `scripts/fetch_python.ps1`（下载官方 NuGet python 3.12 到 `third_party/python/` 并生成 MinGW 导入库，已 git 忽略；离线环境可跳过，CMake 会回退到系统 Python 3.12）。

常用选项：

| 选项 | 默认 | 说明 |
| :--- | :--- | :--- |
| `PVZ_DEBUG` | `OFF` | 启用作弊键、调试显示等调试功能 |
| `VX_SCRIPT` | `ON` | 启用内嵌 Python 脚本系统与游戏内代码编辑器（优先使用 `scripts/fetch_python.ps1` 拉取的官方 NuGet Python 3.12，缺失时回退系统 Python 3.12 开发文件） |
| `DO_FIX_BUGS` | `OFF` | 应用社区对官方 1.2.0.1073 GOTY 版"Bug"的修复（多数玩家视为特性） |
| `CONSOLE` | `OFF` | 显示控制台窗口（仅 Windows） |

各平台依赖安装命令（Arch/Debian/MSYS2/macOS）以及 Android、iOS、WASM 的构建与资源导入说明，详见上游 [README](https://github.com/wszqkzqk/PvZ-Portable)。
