# AGENTS.md

## Project
Vatrix: a non-official mod of [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) (a cross-platform port of Plants vs. Zombies GOTY). Code follows the upstream license **LGPL-3.0-or-later**. The repo contains **no** PopCap/EA copyrighted assets: game data such as `main.pak` and `properties/` must be supplied by the player; never commit them.

## Build & Run
Dependencies: CMake, Ninja, a C++20 compiler, SDL2/OpenGL ES 2.0, etc. (see `README.md`).

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
ninja -C build
```

- A `build/` dir (Ninja + MinGW UCRT64 + Debug) already exists; for incremental builds just run `ninja -C build`.
- Run `build/Vatrix.exe` with `main.pak` and `properties/` next to the executable, or pass `-resdir <path>` / `-savedir <path>`.
- Useful CMake options: `PVZ_DEBUG` (debug keys/display), `CONSOLE` (Windows console).
- No unit tests; verify by running the game manually through the main flow.

## Version (版本号)
版本号手动维护固定值（不随 git describe 推导）。升级版本时需同步修改三处:
- `CMake/ProjectVersion.cmake` 顶部: `PVZP_VERSION` 和 `PVZP_VERSION_PLAIN`
- `android/app/build.gradle`: `def gitVersionName = '0.0.0'`
- `archlinux/PKGBUILD` 和 `archlinux/PKGBUILD-AUR` 顶部: `pkgver=0.0.0`

窗口标题由 `PVZP_VERSION` 自动生成(`src/LawnApp.cpp` 中 `"Vatrix v" PVZP_VERSION`),无需手动改。Android 的 `versionCode` 仍由 git 提交数自动递增。

## 发布流程 (vX.Y.Z)
准备发布时按顺序执行：
1. 升级版本号（三处同步，见上），并把 `wasm/shell.html` 的 `<title>` 改为 `Vatrix vX.Y.Z — Web`（窗口标题由 PVZP_VERSION 自动生成，无需手改）。
2. 在 `CHANGELOG.md` 顶部新增 `## [vX.Y.Z] - <日期>` 条目，按 Keep a Changelog 风格写中文变更说明（注意中文编码，见 Conventions）。
3. 询问用户是否进行构建打包，如果是则继续执行。
4. Release 构建并打包到 `dist/vatrix-win64-X.Y.Z/`（见 `## Packaging (Windows 发布包)`）；重打包时只重跑 `ninja -C build-release` 并覆盖 exe即可。不要压缩文件夹，用户会自己压缩。
5. 不要自己执行 git 提交；把改动总结与建议 commit message（如 `release. vX.Y.Z`）交给用户。

## Packaging (Windows 发布包)
When the user asks to 打包/发布, produce a Release build and bundle it:

```bash
cmake -G Ninja -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe \
  -DCMAKE_CXX_COMPILER=D:/msys64/ucrt64/bin/g++.exe
ninja -C build-release
```

- 显式指定 MinGW 编译器(默认 cmake 可能误选 MSVC)。Release 默认 CONSOLE=OFF、PVZ_DEBUG=OFF;`build-release/` 已被 git 忽略。
- Python 运行时（VX_SCRIPT）来自 `third_party/python/tools/`（由 `fetch_python.ps1` 拉取的官方 NuGet python 3.12，git 忽略）：首次构建前先运行该脚本；打包时带 `python312.dll`、`vcruntime140.dll`、`vcruntime140_1.dll`、`python312/`（标准库，构建时自动同步）与空的 `userlibs/`（运行时自动创建）。
- 运行时 DLL 来自 `D:/msys64/ucrt64/bin/`:`SDL2.dll`、`libopenmpt-0.dll`、`libpng16-16.dll`、`libjpeg-8.dll`、`zlib1.dll`、`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`、`libmpg123-0.dll`、`libvorbis-0.dll`、`libvorbisfile-3.dll`、`libogg-0.dll`。后 4 个是 `libopenmpt-0.dll` 的传递依赖,漏带会报“找不到 libmpg123-0.dll”。用 `objdump -p build-release/Vatrix.exe | Select-String "DLL Name"` 核对 exe,并对包内每个 DLL 各跑一次 `objdump -p` 检查传递依赖是否都在包内;SDL Mixer X 为静态链接,无需带 mixer DLL。`VatrixSetup.exe` 只额外依赖上述 MinGW 运行时(libgcc_s_seh-1.dll、libstdc++-6.dll),系统库无需打包。
- 组装 `dist/vatrix-win64/`(`dist/` 已被 git 忽略):exe + `VatrixSetup.exe`(首次运行配置向导) + `resources.sha256`(启动时校验玩家资源的哈希清单,需与 exe 同目录)+ 上述 DLL + LICENSE + COPYING + `使用说明.txt`(写明玩家需自备 `main.pak` 和 `properties/`,包内不得包含;可先运行 `VatrixSetup.exe`,游戏缺资源时也会自动拉起)。
- 有新提交后重新打包:只重跑 `ninja -C build-release` 并覆盖 exe、重新压缩即可;DLL 未变则无需重拷。

## Python 运行时获取

首次配置前运行 `fetch_python.ps1`（下载官方 NuGet python 3.12.10 到 `third_party/python/` 并生成 MinGW 导入库 `libpython312.a`）。CMake 检测到 `third_party/python/tools/` 时使用它，否则回退 `find_package(Python3 3.12)`（如 MSYS2 UCRT64）。

## Development
When user requests you to "commit" or "提交", please follow the following steps:
1. Check `git diff` to summarize the changes.
2. Write a commit message to describe the changes. You can use English for proper nouns, but keep the rest **in Chinese**. The commit prefix (like `feat.`, `fix.`, `refactor.`, etc.) is crucial.
3. Check if any changes are not proper or may lead to bugs, and notify the user.
4. DO NOT run git command yourself. The user will do it.

## Layout
- `src/LawnApp.cpp/.h`: app flow, title screen, game modes, menu/level transitions, `StartBossFromTitle()`
- `src/Lawn/`: game logic: `Board`, `CutScene`, `Challenge`, `Widget/` (`TitleScreen`, `NewOptionsDialog`, `ChallengeScreen`, ...), `System/` (`ProfileMgr`, `SaveGame`, `Music`)
- `src/SexyAppFramework/`: engine base (Widget/Dialog/ResourceManager); upstream code, avoid touching
- `src/PvzpLib/`: internal helper library
- `src/Resources.cpp`, `src/GameConstants.h`, `src/ConstEnums.h`: resources and enums

## Mod: boss level & world 6
- No boss-on-launch: the title screen goes straight to the main menu (`LoadingCompleted` -> `ShowGameSelector`). The old launch-boss code (`mStartBossOnLaunch`, `StartBossFromTitle`, `Board::mIsBossOnLaunch`) has been removed.
- Adventure level 50 (5-10) is the final boss (`LawnApp::IsFinalBossLevel`): conveyor-belt loadout, `BACKGROUND_6_BOSS`, custom Crazy Dave dialogue (index 9000 in `properties/VatrixStrings.txt`, `CutScene::StartLevelIntro`), and the level-end paper-note award (`Zombie::DropLoot` coin `COIN_NOTE` at level 50 + `ShowAwardScreen(AWARD_FORLEVEL, false, true)` in `CheckForGameEnd`). Beating it advances the save to level 51.
- Adventure has 6 areas (`ADVENTURE_AREAS = 6`, `GameConstants.h`), `FINAL_LEVEL = 60`: world 6 levels 6-1..6-9 (51..59) exist; 6-1..6-4 and 6-6..6-9 are Scary Potter (`IsScaryPotterLevel` also covers level 35), 6-5 is a normal level, 6-10 (60) is the current world-final placeholder.
- `GameSelector::KeyDown` debug hotkeys (`PVZ_DEBUG` only): `0` enters adventure 5-10, `1`-`9` enter 6-1..6-9 (sets the save level, then starts `GAMEMODE_ADVENTURE`).
- `ChallengeScreen::MoreTrophiesNeeded` returns 0 and `GameSelector` mode buttons are force-unlocked: all mini-game / puzzle / survival levels are always available.

## Conventions
- Mark every Vatrix-modification site with a `// vx: <short description>` comment (ASCII only), placed directly above the changed code. Comments in file `src/Lawn/SpawnLogic.cpp` can omit the vx prefix.
- Tabs for indentation; keep the upstream `// GOTY @Patoke: 0x...` comment style.
- Source files are UTF-8 without BOM. On Windows, PowerShell's `Get-Content` may mis-decode UTF-8 Chinese comments (GBK console); prefer `rg`/Python/git for reading.
- 写含中文的文件（CHANGELOG.md、使用说明.txt 等）时，禁止用 PowerShell 管道（@'...'@ | python -）把中文传给 Python：管道按 GBK 编码传输，中文会被破坏成 ?（已发生多次）。改用 .NET API 直接写 UTF-8 无 BOM（[System.IO.File]::WriteAllText(path, text, New-Object System.Text.UTF8Encoding $false)），或把 Python 源码中的中文写成 \uXXXX 转义。
- When writing files, avoid using Chinese comments. Existing Chinese comments should not be modified.
- After code changes, run `ninja -C build` to confirm compilation.
