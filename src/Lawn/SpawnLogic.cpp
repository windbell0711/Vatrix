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
	ZombieType::ZOMBIE_DOOR,            // dor: 4 -> 1 -
	ZombieType::ZOMBIE_TRAFFIC_CONE,    // con: 2 -> 2
	ZombieType::ZOMBIE_NEWSPAPER,       // pap: 2 -> 2
	ZombieType::ZOMBIE_POLEVAULTER,     // pol: 2 -> 3
	ZombieType::ZOMBIE_PAIL,            // bkt: 4 -> 4
	ZombieType::ZOMBIE_JACK_IN_THE_BOX, // jac: 3 -> 5 +
	ZombieType::ZOMBIE_LADDER,          // lad: 4 -> 5 +
	ZombieType::ZOMBIE_POGO,            // pog: 4 -> 6 +
	ZombieType::ZOMBIE_CATAPULT,        // ctp: 5 -> 6 +
	ZombieType::ZOMBIE_ZAMBONI,         // zbn: 7 -> 7
	ZombieType::ZOMBIE_FOOTBALL,        // ftb: 7 -> 7
	ZombieType::ZOMBIE_GARGANTUAR       // ggt: 10 -> 12 +
};

// vx: spawn cost per pool entry, aligned with gBossZombieList (Vatrix mod)
const int gBossZombieCost[BOSS_ZOMBIE_LIST_COUNT] = {
	0,  // NORMAL (free, budget-floor pick)
	1,  // DOOR
	2,  // TRAFFIC_CONE
	2,  // NEWSPAPER
	3,  // POLEVAULTER
	4,  // PAIL
	5,  // JACK_IN_THE_BOX
	5,  // LADDER
	6,  // POGO
	6,  // CATAPULT
	7,  // ZAMBONI
	7,  // FOOTBALL
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

// vx: collect per-row plant/zombie stats for the row picker
std::array<RowState, MAX_GRID_SIZE_Y> GetRowPlantStates(Board* theBoard)
{
	std::array<RowState, MAX_GRID_SIZE_Y> aRowState{};

	for (Plant* aPlant : theBoard->mPlants)
	{
		if (aPlant->mDead)
		{
			continue;
		}

		// vx: Imitater seeds count as their mimicked seed type
		SeedType aSeedType = aPlant->mSeedType == SeedType::SEED_IMITATER ? aPlant->mImitaterType : aPlant->mSeedType;
		RowState& aStats = aRowState[aPlant->mRow];
		aStats.mPlantCount++;
		if (aSeedType == SeedType::SEED_CABBAGEPULT || aSeedType == SeedType::SEED_KERNELPULT || aSeedType == SeedType::SEED_MELONPULT || aSeedType == SeedType::SEED_WINTERMELON)
		{
			aStats.mPultCount++;
		}
		if (aSeedType == SeedType::SEED_MELONPULT)
		{
			aStats.mMelonCount++;
		}
	}

	for (Zombie* aZombie : theBoard->mZombies)
	{
		if (aZombie->mDead || aZombie->mMindControlled)
		{
			continue;
		}
		// vx: skip the boss itself
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
		{
			continue;
		}
		// vx: ignore zombies below 40% health, except Gargantuars
		if (aZombie->mZombieType != ZombieType::ZOMBIE_GARGANTUAR && aZombie->mZombieType != ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
		{
			int aHealth = aZombie->mBodyHealth + aZombie->mFlyingHealth;
			int aMaxHealth = aZombie->mBodyMaxHealth + aZombie->mFlyingMaxHealth;
			if (aHealth * 5 < aMaxHealth * 2)  // vx: below 40% health
			{
				continue;
			}
		}
		aRowState[aZombie->mRow].mZombieCount++;
	}
	
	return aRowState;
}

template<typename TC, typename TF>
int PickRow(Board* theBoard, TC compare, TF filter)
{
	auto aRowState = GetRowPlantStates(theBoard);

	int aBestRow = -1;
	int aTieCount = 0;
	int aRowCount = theBoard->StageHas6Rows() ? MAX_GRID_SIZE_Y : MAX_GRID_SIZE_Y - 1;
	for (int aRow = 0; aRow < aRowCount; aRow++)
	{
		if (!filter(aRow)) continue;

		if (aBestRow < 0)
		{
			aBestRow = aRow;
			aTieCount = 1;
			continue;
		}

		short aCompareRes = compare(aRowState[aRow], aRowState[aBestRow]);
		if (aCompareRes == 1)
		{
			aBestRow = aRow;
			aTieCount = 1;
		}
		else if (aCompareRes == 0)
		{
			// vx: random tie-break among equally empty lanes
			aTieCount++;
			if (Rand(aTieCount) == 0) aBestRow = aRow;
		}
	}
	return aBestRow;
}

short sign(int value) {
	return (value > 0) - (value < 0);
}

// vx: decide summon type and row together
BossSummonDecision PickBossSummon(Board* theBoard, int theZombieAge, int theSpawnPoints)
{
	BossSummonDecision aSummon{ZombieType::ZOMBIE_NORMAL, -1};

	if (theZombieAge < 3500)
	{
		// 普僵放投手最少的地方
		aSummon.mZombieType = ZombieType::ZOMBIE_NORMAL;
		aSummon.mRow = PickRow(
			theBoard,
			[](const RowState& a, const RowState& b) {
				return sign(b.mPultCount - a.mPultCount);
			},
			[](int aRow) { return true; }
		);
	}
	else if (theZombieAge < 8000)
	{
		// 路障放二四路已有僵尸最少的地方
		aSummon.mZombieType = ZombieType::ZOMBIE_TRAFFIC_CONE;
		aSummon.mRow = PickRow(
			theBoard, 
			[](const RowState& a, const RowState& b) {
				return sign(b.mZombieCount - a.mZombieCount);
			},
			[](int aRow) { return aRow == 1 || aRow == 3; }
		);
	}
	else if (theZombieAge < 12500)
	{
		// 铁桶放一三五路已有僵尸最少的地方
		aSummon.mZombieType = ZombieType::ZOMBIE_PAIL;
		aSummon.mRow = PickRow(
			theBoard, 
			[](const RowState& a, const RowState& b) {
				return sign(b.mZombieCount - a.mZombieCount);
			},
			[](int aRow) { return aRow == 0 || aRow == 2 || aRow == 4; }
			// [theBoard, aSummon](int aRow) {
			// 	return theBoard->RowCanHaveZombieType(aRow, aSummon.mZombieType); 
			// }
		);
	}
	else
	{
		aSummon.mZombieType = ZombieType::ZOMBIE_GARGANTUAR;
	}

	// 校验合法性
	if (GetBossZombieCost(aSummon.mZombieType) > theSpawnPoints)
	{
		Sexy::PrintF("PickBossSummon: points %d < cost %d, downgrade to NORMAL\n", theSpawnPoints, GetBossZombieCost(aSummon.mZombieType));
		aSummon.mZombieType = ZombieType::ZOMBIE_NORMAL;
	}
	if (aSummon.mRow < 0)
	{
		Sexy::PrintF("PickBossSummon: row %d invalid, use default logic\n", aSummon.mRow);
		aSummon.mRow = theBoard->PickRowForNewZombie(aSummon.mZombieType);  // 原版逻辑
	}
	return aSummon;
}

}
