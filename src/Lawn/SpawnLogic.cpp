#include "SpawnLogic.h"

#include <algorithm>

#include "Board.h"

#include "../LawnApp.h"
#include "Challenge.h"
#include "../PvzpLib/PvzpCommon.h"

// vx: boss summon pool moved here from Zombie.cpp (Vatrix mod)
namespace SpawnLogic
{
const ZombieType gBossZombieList[BOSS_ZOMBIE_LIST_COUNT] = {
	ZombieType::ZOMBIE_TRAFFIC_CONE,
	ZombieType::ZOMBIE_PAIL,
	ZombieType::ZOMBIE_FOOTBALL,
	ZombieType::ZOMBIE_POLEVAULTER,
	ZombieType::ZOMBIE_JACK_IN_THE_BOX,
	ZombieType::ZOMBIE_LADDER,
	ZombieType::ZOMBIE_ZAMBONI,
	ZombieType::ZOMBIE_CATAPULT,
	ZombieType::ZOMBIE_POGO,
	ZombieType::ZOMBIE_NEWSPAPER,
	ZombieType::ZOMBIE_DOOR,
	ZombieType::ZOMBIE_GARGANTUAR
};

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

ZombieType PickBossSummonType(Board* theBoard, int theZombieAge, int theTargetRow)
{
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
		int aZombieTypeCount = LENGTH(gBossZombieList);
		if (theTargetRow == 0)
		{
			PVZP_ASSERT(gBossZombieList[aZombieTypeCount - 1] == ZombieType::ZOMBIE_GARGANTUAR);
			aZombieTypeCount--;
		}
		return PvzpPickFromArray(gBossZombieList, aZombieTypeCount);
	}
}
}
