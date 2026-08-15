#ifndef __VXLEVELINIT_H__
#define __VXLEVELINIT_H__

// vx: world-6 level init data from Properties/adventure_info.csv.
// Ported from the removed src/python/vx_init_lvl.py so runtime Python is used only for
// player scripts; level setup is pure C++ data parsing. The seeded shuffles use
// std::mt19937, so pot/scene layouts differ from the old Python build (one-time change).

#include <filesystem>
#include <string>
#include <vector>

#include "Lawn/VxScript.h"

namespace VX
{
	struct VxLevelInfo
	{
		bool mValid = false; // the level row exists in the CSV
		int mCode1 = 0;
		int mCode2 = 0;
		bool mSpecial = false;
		int mSceneId = 0;
		std::string mSceneDesign;
		int mSceneRuleCode = 0;
		std::string mSceneRule;
		std::string mVaseDesign;
		int mVaseRuleCode = 0;
		std::string mVaseRule;
		std::string mSlotRule;
		std::string mRandomSeeds;
		std::string mStatistics;
	};

	// vx: false only when the CSV file itself is unreadable; a missing level row leaves mValid=false
	bool VxLoadLevelInfo(int theLevel, const std::filesystem::path& theCsvPath, VxLevelInfo& theOut);

	bool VxGeneratePots(const VxLevelInfo& theInfo, int theSeed, std::vector<VxPotDef>& theOut);
	bool VxBuildScene(const VxLevelInfo& theInfo, int theSeed, std::vector<VxSceneDef>& theOut);
	bool VxParseSlot(const VxLevelInfo& theInfo, int& theSun, std::vector<int>& theSlots);
	bool VxParseRandomSeeds(const VxLevelInfo& theInfo, std::vector<int>& theOut);
	bool VxParseStatistics(const VxLevelInfo& theInfo, int& theAvgMs, int& theMaxMs, int& theSun);
}

#endif // __VXLEVELINIT_H__