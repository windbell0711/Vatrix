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
- Run `build/pvz-portable.exe` with `main.pak` and `properties/` next to the executable, or pass `-resdir <path>` / `-savedir <path>`.
- Useful CMake options: `PVZ_DEBUG` (debug keys/display), `CONSOLE` (Windows console).
- No unit tests; verify by running the game manually through the main flow.

## Packaging (Windows 发布包)
When the user asks to 打包/发布, produce a Release build and bundle it:

```bash
cmake -G Ninja -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe \
  -DCMAKE_CXX_COMPILER=D:/msys64/ucrt64/bin/g++.exe
ninja -C build-release
```

- 显式指定 MinGW 编译器(默认 cmake 可能误选 MSVC)。Release 默认 CONSOLE=OFF、PVZ_DEBUG=OFF;`build-release/` 已被 git 忽略。
- 运行时 DLL 来自 `D:/msys64/ucrt64/bin/`:`SDL2.dll`、`libopenmpt-0.dll`、`libpng16-16.dll`、`libjpeg-8.dll`、`zlib1.dll`、`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`。用 `objdump -p build-release/pvz-portable.exe | Select-String "DLL Name"` 核对;SDL Mixer X 为静态链接,无需带 mixer DLL。
- 组装 `dist/pvz-portable-win64/`(`dist/` 已被 git 忽略):exe + 上述 DLL + LICENSE + COPYING + `使用说明.txt`(写明玩家需自备 `main.pak` 和 `properties/`,包内不得包含)。
- 有新提交后重新打包:只重跑 `ninja -C build-release` 并覆盖 exe、重新压缩即可;DLL 未变则无需重拷。

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

## Mod: launch straight into the boss level (Level 0-0)
- `LawnApp::mStartBossOnLaunch`: initialized `true` in the constructor (every launch); reset to `false` when entering the challenge screen / main menu (`ShowChallengeScreen` / `ShowGameSelector`).
- `LawnApp::StartBossFromTitle()`: after clicking "Start" on the title screen, skips the main menu and opens a new game with `GAMEMODE_CHALLENGE_FINAL_BOSS`; auto-creates a `Player 1` profile when none exists.
- `Board::mIsBossOnLaunch`: captured from `mStartBossOnLaunch` in the Board constructor; lives for the whole level and distinguishes the launch-straight-into-boss path from manually selecting the same mini-game from the challenge screen.
- "Level 0-0" is shown only when `mIsBossOnLaunch`: in the `CutScene::StartLevelIntro` intro banner and the yellow level-name plaque in `Board::DrawLevel`. Manual selection keeps the original challenge name.
- `NewOptionsDialog`: hides the "Restart Level" and "Main Menu" buttons while `mIsBossOnLaunch` (both ESC and the in-game menu button open this dialog).

## Conventions
- Mark every Vatrix-modification site with a `// vx: <short description>` comment (ASCII only), placed directly above the changed code. Comments in file `src/Lawn/SpawnLogic.cpp` can omit the vx prefix.
- Tabs for indentation; keep the upstream `// GOTY @Patoke: 0x...` comment style.
- Source files are UTF-8 without BOM. On Windows, PowerShell's `Get-Content` may mis-decode UTF-8 Chinese comments (GBK console); prefer `rg`/Python/git for reading.
- When writing files, avoid using Chinese comments. Existing Chinese comments should not be modified.
- After code changes, run `ninja -C build` to confirm compilation.
