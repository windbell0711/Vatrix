#include "SpawnLogic.h"

#include <algorithm>

#include "Board.h"

#include "../LawnApp.h"
#include "Challenge.h"
#include "../PvzpLib/PvzpCommon.h"

// vx: boss summon pool moved here from Zombie.cpp (Vatrix mod)
namespace SpawnLogic
{
// 原版僵王战出怪不记点数，为纯随机，但那样也太没意思了，于是加上点数机制
// 为保证平衡，我修改了部分僵尸的点数
const ZombieType gBossZombieList[BOSS_ZOMBIE_LIST_COUNT] = {
	ZombieType::ZOMBIE_NORMAL,          // zom: 1 -> 0  增加了普僵，作为点数耗尽时的选择
	ZombieType::ZOMBIE_TRAFFIC_CONE,    // con: 2 -> 2
	ZombieType::ZOMBIE_PAIL,            // bkt: 4 -> 4
	ZombieType::ZOMBIE_FOOTBALL,        // ftb: 7 -> 7
	ZombieType::ZOMBIE_POLEVAULTER,     // pol: 2 -> 2
	ZombieType::ZOMBIE_JACK_IN_THE_BOX, // jac: 3 -> 4 +
	ZombieType::ZOMBIE_LADDER,          // lad: 4 -> 5 +
	ZombieType::ZOMBIE_ZAMBONI,         // zbn: 7 -> 8 +
	ZombieType::ZOMBIE_CATAPULT,        // ctp: 5 -> 6 +
	ZombieType::ZOMBIE_POGO,            // pog: 4 -> 6 +
	ZombieType::ZOMBIE_NEWSPAPER,       // pap: 2 -> 2
	ZombieType::ZOMBIE_DOOR,            // dor: 4 -> 2 -
	ZombieType::ZOMBIE_GARGANTUAR       // ggt: 10 -> 12 +
};

// vx: spawn cost per pool entry, aligned with gBossZombieList (Vatrix mod)
const int gBossZombieCost[BOSS_ZOMBIE_LIST_COUNT] = {
	0,  // NORMAL (free, budget-floor pick)
	2,  // TRAFFIC_CONE
	4,  // PAIL
	7,  // FOOTBALL
	2,  // POLEVAULTER
	4,  // JACK_IN_THE_BOX
	5,  // LADDER
	8,  // ZAMBONI
	6,  // CATAPULT
	6,  // POGO
	2,  // NEWSPAPER
	2,  // DOOR
	12, // GARGANTUAR
};

// vx: zombie cost per pool entry
int GetBossZombieCost(ZombieType theZombieType)
{
	for (int i = 0; i < BOSS_ZOMBIE_LIST_COUNT; i++)
	{
		if (gBossZombieList[i] == theZombieType)
		{
			return gBossZombieCost[i];
		}
	}
	return 0;
}

// vx: vanilla wave-composition picker moved from Board::PickZombieType
ZombieType PickWaveZombieType(Board* theBoard, int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker)
{
	int aPickCount = 0;
	PvzpWeightedArray aZombieWeightArray[ZombieType::NUM_ZOMBIE_TYPES];
	for (int aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType++)
	{
		if (!theBoard->mZombieAllowed[aZombieType])
		{
			continue;
		}

		const ZombieDefinition& aZombieDef = GetZombieDefinition((ZombieType)aZombieType);

		// skip zombie types barred by spawn rules or over budget
		GameMode aGameMode = theBoard->mApp->mGameMode;
		if (aZombieType == ZombieType::ZOMBIE_BUNGEE && theBoard->mApp->IsSurvivalEndless(aGameMode))
		{
			if (!theBoard->IsFlagWave(theWaveIndex))
			{
				continue;
			}
		}
		else if (aGameMode != GameMode::GAMEMODE_CHALLENGE_POGO_PARTY && aGameMode != GameMode::GAMEMODE_CHALLENGE_BOBSLED_BONANZA && aGameMode != GameMode::GAMEMODE_CHALLENGE_AIR_RAID)
		{
			int aFirstAllowedWave = aZombieDef.mFirstAllowedWave;
			// endless mode: zombies appear progressively earlier
			if (theBoard->mApp->IsSurvivalEndless(aGameMode))
			{
				int aFlags = theBoard->GetSurvivalFlagsCompleted();
				int aAllowedWave = aFirstAllowedWave - PvzpAnimateCurve(18, 50, aFlags, 0, 15, PvzpCurves::CURVE_LINEAR);
				aFirstAllowedWave = std::max(aAllowedWave, 1);
			}
			if (theWaveIndex + 1 < aFirstAllowedWave || theZombiePoints < aZombieDef.mZombieValue)
			{
				continue;
			}
		}

		// survival mode: reweight zombies by flags completed
		int aPickWeight = aZombieDef.mPickWeight;
		if (theBoard->mApp->IsSurvivalMode())
		{
			int aFlags = theBoard->GetSurvivalFlagsCompleted();
			if (aZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombieType == ZombieType::ZOMBIE_ZAMBONI)
			{
				if (theZombiePicker->mZombieTypeCount[aZombieType] >= PvzpAnimateCurve(10, 50, aFlags, 2, 50, PvzpCurves::CURVE_LINEAR))
				{
					continue;
				}
			}
			else if (aZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
			{
				if (theBoard->IsFlagWave(theWaveIndex))
				{
					if (theZombiePicker->mZombieTypeCount[aZombieType] >= PvzpAnimateCurve(14, 100, aFlags, 1, 50, PvzpCurves::CURVE_LINEAR))
					{
						continue;
					}
				}
				else
				{
					if (theZombiePicker->mAllWavesZombieTypeCount[aZombieType] >= PvzpAnimateCurve(10, 110, aFlags, 1, 50, PvzpCurves::CURVE_LINEAR))
					{
						continue;
					}
					aPickWeight = 1000;
				}
			}
			else if (aZombieType == ZombieType::ZOMBIE_NORMAL)
			{
				aPickWeight = PvzpAnimateCurve(10, 50, aFlags, aPickWeight, aPickWeight / 10, PvzpCurves::CURVE_LINEAR);
			}
			else if (aZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE)
			{
				aPickWeight = PvzpAnimateCurve(10, 50, aFlags, aPickWeight, aPickWeight / 4, PvzpCurves::CURVE_LINEAR);
			}
		}
		aZombieWeightArray[aPickCount].mItem = aZombieType;
		aZombieWeightArray[aPickCount].mWeight = aPickWeight;
		aPickCount++;
	}

	// weighted-random pick of a valid zombie type
	return (ZombieType)PvzpPickFromWeightedArray(aZombieWeightArray, aPickCount);
}

// vx: custom spawn-logic: pick the lane with the fewest alive plants
int PickCustomSpawnRow(Board* theBoard, ZombieType theZombieType)
{
	int aBestRow = -1;
	int aBestCount = 0;
	int aTieCount = 0;
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (!theBoard->RowCanHaveZombieType(aRow, theZombieType))
		{
			continue;
		}

		int aCount = 0;
		for (Plant* aPlant : theBoard->mPlants)
		{
			if (!aPlant->mDead && aPlant->mRow == aRow)
			{
				aCount++;
			}
		}

		if (aBestRow < 0 || aCount < aBestCount)
		{
			aBestRow = aRow;
			aBestCount = aCount;
			aTieCount = 1;
		}
		else if (aCount == aBestCount)
		{
			// vx: random tie-break among equally empty lanes
			aTieCount++;
			if (Rand(aTieCount) == 0)
			{
				aBestRow = aRow;
			}
		}
	}
	return aBestRow < 0 ? 0 : aBestRow;
}

BossFireballDecision PickBossFireball(Board* theBoard)
{
	BossFireballDecision aDecision;
#ifdef DO_FIX_BUGS
	aDecision.mRow = RandRangeInt(0, theBoard->StageHas6Rows() ? 5 : 4);  // pool-stage boss row range
#else
	aDecision.mRow = RandRangeInt(0, 4);
#endif
	aDecision.mIsFireBall = RandRangeInt(0, 1) == 0;
	return aDecision;
}

BossTargetDecision PickBossRVTarget(Board* theBoard)
{
	BossTargetDecision aTarget;
#ifdef DO_FIX_BUGS
	aTarget.mRow = RandRangeInt(0, theBoard->StageHas6Rows() ? 4 : 3);  // pool-stage boss row range
#else
	aTarget.mRow = RandRangeInt(0, 3);
#endif
	aTarget.mCol = RandRangeInt(0, 2);
	return aTarget;
}

int PickBossStompRow(Board* theBoard, const intptr_t* theRowArray, int theRowCount)
{
	return (int)PvzpPickFromArray(theRowArray, theRowCount);
}

int PickBossBungeeCol(Board* theBoard)
{
	return RandRangeInt(0, 2);
}

ZombieType PickBossSummonType(Board* theBoard, int theZombieAge, int theTargetRow, int theSpawnPoints)
{
	Sexy::PrintF("theSpawnPoints: %d\n", theSpawnPoints);
	// vanilla: fixed stage zombies for the first three waves, regardless of budget
	if (theZombieAge < 3500)
	{
		return ZombieType::ZOMBIE_NORMAL;
	}
	else if (theZombieAge < 8000)
	{
		return ZombieType::ZOMBIE_TRAFFIC_CONE;
	}
	else if (theZombieAge < 12500)
	{
		return ZombieType::ZOMBIE_PAIL;
	}
	else
	{
		ZombieType ret;

		// vx: budget-limited pool pick: only affordable zombies, random among them
		// ZombieType aAffordableList[BOSS_ZOMBIE_LIST_COUNT];
		// int aAffordableCount = 0;
		// for (int i = 0; i < BOSS_ZOMBIE_LIST_COUNT; i++)
		// {
		// 	ZombieType aType = gBossZombieList[i];
		// 	if (theTargetRow == 0 && aType == ZombieType::ZOMBIE_GARGANTUAR)
		// 	{
		// 		continue;  // vanilla: no Gargantuar on row 1
		// 	}
		// 	if (gBossZombieCost[i] <= theSpawnPoints)
		// 	{
		// 		aAffordableList[aAffordableCount++] = aType;
		// 	}
		// }
		// ret = aAffordableCount > 0 ? PvzpPickFromArray(aAffordableList, aAffordableCount) : ZombieType::ZOMBIE_NORMAL;
		
		ret = ZombieType::ZOMBIE_GARGANTUAR;

		return ret;
	}
}
}
