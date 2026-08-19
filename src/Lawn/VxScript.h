#ifndef __VXSCRIPT_H__
#define __VXSCRIPT_H__

// vx: Python scripting bridge for player scripts (_vx module)

#include <vector>

class Board;

namespace VX
{
	// vx: one pot entry produced by the CSV level init (enum fields stored as ints)
	struct VxPotDef
	{
		int mRow;    // 0-4, or -1 for random among free cells
		int mCol;    // 0-8, or -1 for random
		int mType;   // ScaryPotType
		int mZombie; // ZombieType
		int mSeed;   // SeedType
		int mCount;  // number of pots when placing randomly
	};

	void Init();
	void Shutdown();
	void StartScripts(int theLevel, int theGameMode);
	void StopScripts();
	// vx: main thread feeds the script-visible game clock (seconds) every frame; pause freezes it
	void VxUpdateGameTime(double theGameTime);
	void ProcessBoardQueue(Board* theBoard);
	bool GetScaryPotLineup(int theLevel, int theSeed, std::vector<VxPotDef>& theOut);

	// vx: one scene entry produced by the CSV level init
	struct VxSceneDef
	{
		int mRow;
		int mCol;
		bool mIsPlant; // true = pre-placed plant, false = pre-placed zombie
		int mSeed;     // SeedType when mIsPlant
		int mZombie;   // ZombieType when !mIsPlant
	};
	bool GetSceneLayout(int theLevel, int theSeed, std::vector<VxSceneDef>& theOut);
	bool GetSlotSetup(int theLevel, int& theSun, std::vector<int>& theSlots);
	bool GetRandomSeeds(int theLevel, std::vector<int>& theOut);
	// vx: author best scores from the CSV statistics column (avg_ms;max_ms;sun); false when unset
	bool GetLevelStatistics(int theLevel, int& theAvgMs, int& theMaxMs, int& theSun);
	// vx: prepend the settlement log "# Submit at {ts}: {AvgMs}, {MaxMs}, {Sun}" to the level script
	void LogSubmitStats(int theLevel, int theAvgMs, int theMaxMs, int theSun);
	// vx: last player-script error (kind 1=compile CE, 2=runtime RE); consumes the error file
	bool GetScriptError(std::string& theText, int& theKind);

	// vx: player-driven world-6 runs (Run = trial, Submit = official)
	void RequestScriptRun(int theLevel, int theGameMode, bool theSubmit);
	bool ConsumePendingScriptRun();
	bool IsTrialRun();
	void ClearRunFlags();
	// vx: seed carried into the next board (Run = fresh non-repeating, Reset = same as the current board)
	void SetTrialSeed(int theSeed);
	int TakeTrialSeed();
	// vx: locked Submit runs freeze the script on disk (editor autosave suspended)
	void SetRunLocked(bool theLocked);
	bool IsRunLocked();
	bool IsScriptRunning();
	// vx: true once the current script driver has finished (error or not)
	bool IsScriptDone();
	void InterruptScript();
	std::wstring GetScriptsDir();
	std::wstring GetLevelScriptPath(int theLevel);
}

#endif // __VXSCRIPT_H__
