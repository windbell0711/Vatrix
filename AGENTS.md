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
- Mark every Vatrix-modification site with a `// vx: <short description>` comment (ASCII only), placed directly above the changed code, except for codes in file `src/Lawn/SpawnLogic.cpp`.
- Tabs for indentation; keep the upstream `// GOTY @Patoke: 0x...` comment style.
- Source files are UTF-8 without BOM. On Windows, PowerShell's `Get-Content` may mis-decode UTF-8 Chinese comments (GBK console); prefer `rg`/Python/git for reading.
- After code changes, run `ninja -C build` to confirm compilation.
