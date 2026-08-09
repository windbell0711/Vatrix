#ifndef __VXSCRIPT_H__
#define __VXSCRIPT_H__

// vx: Python scripting bridge for player scripts (_vb module)

#include <vector>

class Board;

namespace VX
{
	// vx: one pot entry produced by scripts/vx_pots.py (enum fields stored as ints)
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
	void ProcessBoardQueue(Board* theBoard);
	bool GetScaryPotLineup(int theLevel, std::vector<VxPotDef>& theOut);

	// vx: one scene entry produced by vx_init_lvl.get_scene
	struct VxSceneDef
	{
		int mRow;
		int mCol;
		bool mIsPlant; // true = pre-placed plant, false = pre-placed zombie
		int mSeed;     // SeedType when mIsPlant
		int mZombie;   // ZombieType when !mIsPlant
	};
	bool GetSceneLayout(int theLevel, std::vector<VxSceneDef>& theOut);
	bool GetSlotSetup(int theLevel, int& theSun, std::vector<int>& theSlots);
}

#endif // __VXSCRIPT_H__
