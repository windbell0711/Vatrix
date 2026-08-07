#include "SpawnLogic.h"

#include <algorithm>

#include "Board.h"

#include "../LawnApp.h"
#include "Challenge.h"
#include "SeedPacket.h"
#include "../PvzpLib/PvzpCommon.h"

// vx: boss summon pool moved here from Zombie.cpp (Vatrix mod)
namespace SpawnLogic
{
// 原版僵王战出怪不记点数，为纯随机，但那样也太没意思了，于是加上点数机制
// 为保证平衡，我修改了部分僵尸的点数
const ZombieType gBossZombieList[BOSS_ZOMBIE_LIST_COUNT] = {
	ZombieType::ZOMBIE_NORMAL,          // zom: 1 -> 0  增加了普僵，作为点数耗尽时的选择
	ZombieType::ZOMBIE_DOOR,            // dor: 4 -> 2 -
	ZombieType::ZOMBIE_TRAFFIC_CONE,    // con: 2 -> 2
	ZombieType::ZOMBIE_NEWSPAPER,       // pap: 2 -> 2
	ZombieType::ZOMBIE_POLEVAULTER,     // pol: 2 -> 3 +
	ZombieType::ZOMBIE_PAIL,            // bkt: 4 -> 4
	ZombieType::ZOMBIE_JACK_IN_THE_BOX, // jac: 3 -> 6 +
	ZombieType::ZOMBIE_CATAPULT,        // ctp: 5 -> 6 +
	ZombieType::ZOMBIE_LADDER,          // lad: 4 -> 6 +
	ZombieType::ZOMBIE_FOOTBALL,        // ftb: 7 -> 8
	ZombieType::ZOMBIE_POGO,            // pog: 4 -> 8 +
	ZombieType::ZOMBIE_ZAMBONI,         // zbn: 7 -> 8 +
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
	6,  // FOOTBALL
	6,  // CATAPULT
	8,  // ZAMBONI
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

const ZombieType gBossTacticCollapse[3] = {
	ZombieType::ZOMBIE_LADDER,
	ZombieType::ZOMBIE_POGO,
	ZombieType::ZOMBIE_POLEVAULTER
};

const ZombieType gBossTacticWeak[6] = {
	ZombieType::ZOMBIE_ZAMBONI,
	ZombieType::ZOMBIE_POGO,
	ZombieType::ZOMBIE_GARGANTUAR,
	ZombieType::ZOMBIE_FOOTBALL,
	ZombieType::ZOMBIE_POGO,
	ZombieType::ZOMBIE_PAIL
};

const ZombieType gBossTacticStrong[3] = {
	ZombieType::ZOMBIE_FOOTBALL,
	ZombieType::ZOMBIE_JACK_IN_THE_BOX,
	ZombieType::ZOMBIE_POLEVAULTER
};

template <typename T, typename TF>
T random_choice_if(const T* arr, size_t len, TF pred, T fallback) {
    std::vector<T> filtered;
    for (size_t i = 0; i < len; ++i) {
        if (pred(arr[i])) {
            filtered.push_back(arr[i]);
        }
    }
    if (filtered.empty()) {
        // vx: budget-floor fallback instead of throwing (Vatrix mod)
        return fallback;
    }
    int idx = RandRangeInt(0, static_cast<int>(filtered.size()) - 1);
    return filtered[idx];
}

short sign(int value) {
	return (value > 0) - (value < 0);
}

// vx: collect per-row plant/zombie stats for the row picker
std::array<RowState, MAX_GRID_SIZE_Y> GetRowStates(Board* theBoard)
{
	std::array<RowState, MAX_GRID_SIZE_Y> aRowState{};

	for (Plant* aPlant : theBoard->mPlants)
	{
		if (aPlant->mDead || aPlant->mSquished)
		{
			continue;
		}

		// vx: Imitater seeds count as their mimicked seed type
		SeedType aSeedType = aPlant->mSeedType == SeedType::SEED_IMITATER ? aPlant->mImitaterType : aPlant->mSeedType;
		RowState& aStats = aRowState[aPlant->mRow];
		aStats.mPlantCount++;
		if (aSeedType == SeedType::SEED_FLOWERPOT)
		{
			aStats.mPotCount++;
			aStats.mToughness += 0.2f;
		}
		if (aSeedType == SeedType::SEED_CABBAGEPULT || aSeedType == SeedType::SEED_KERNELPULT || aSeedType == SeedType::SEED_MELONPULT || aSeedType == SeedType::SEED_WINTERMELON)
		{
			aStats.mPultCount++;
			aStats.mToughness += 1.0f;
			if (aSeedType == SeedType::SEED_MELONPULT)
			{
				aStats.mMelonCount++;
				aStats.mToughness += 1.0f;
				if (aPlant->mPlantCol == 0)
				{
					aStats.mLastColMelon = true;
					aStats.mToughness += 0.6f;
				}
				else if (aPlant->mPlantCol == 1)
				{
					aStats.mToughness += 0.3f;
				}
			}
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
		// vx: bungees are airborne, not field zombies; exclude from row counts
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE)
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
		if (aZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
		{
			aRowState[aZombie->mRow].mGargantuarCount++;
		}
	}
	
	return aRowState;
}

// vx: score how much the boss wants to steal a plant with a bungee
int ScoreBungeeSteal(Plant* thePlant, const std::array<RowState, MAX_GRID_SIZE_Y>& theRowStats)
{
	int aScore = 0;
	int aRow = thePlant->mRow;
	SeedType aSeedType = thePlant->mSeedType == SeedType::SEED_IMITATER ? thePlant->mImitaterType : thePlant->mSeedType;
	bool aIsPult = aSeedType == SeedType::SEED_CABBAGEPULT || aSeedType == SeedType::SEED_KERNELPULT ||
		aSeedType == SeedType::SEED_MELONPULT || aSeedType == SeedType::SEED_WINTERMELON;

	if (theRowStats[aRow].mZombieCount >= 2)  // vx: row has 2+ effective zombies
	{
		aScore += 3;
	}
	if (aIsPult && theRowStats[aRow].mPultCount <= 3)  // vx: row has <=3 pults
	{
		aScore += 2;
	}
	if (aIsPult && thePlant->mPlantCol == 4)  // vx: last column of the 5-col boss board
	{
		aScore += 2;
	}
	if (aIsPult && thePlant->mPlantCol == 3)  // vx: second-to-last column
	{
		aScore += 1;
	}
	if (aSeedType == SeedType::SEED_MELONPULT)
	{
		aScore += 3;
	}
	if (aSeedType == SeedType::SEED_KERNELPULT)
	{
		aScore += theRowStats[aRow].mGargantuarCount > 0 ? 3 : 1;
	}
	return aScore;
}

template<typename TC, typename TF>
int PickRow(Board* theBoard, const std::array<RowState, MAX_GRID_SIZE_Y>& aRowStates, TC compare, TF filter)
{
	Sexy::PrintF("Toughness: %f %f %f %f %f\n", aRowStates[0].mToughness, aRowStates[1].mToughness, aRowStates[2].mToughness, aRowStates[3].mToughness, aRowStates[4].mToughness);

	int aBestRow = -1;
	int aTieCount = 0;
	int aRowCount = theBoard->StageHas6Rows() ? MAX_GRID_SIZE_Y : MAX_GRID_SIZE_Y - 1;
	for (int aRow = 0; aRow < aRowCount; aRow++)
	{
		if (!filter(aRow, aRowStates[aRow])) continue;

		if (aBestRow < 0)
		{
			aBestRow = aRow;
			aTieCount = 1;
			continue;
		}

		short aCompareRes = compare(aRowStates[aRow], aRowStates[aBestRow]);
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

// vanilla: wave-composition picker moved from Board::PickZombieType
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

// vx: fireball targets the row with the most plants + melons, minus 5 per effective zombie
BossFireballDecision PickBossFireball(Board* theBoard)
{
	BossFireballDecision aDecision;
	auto aRowStates = GetRowStates(theBoard);
	aDecision.mRow = PickRow(
		theBoard,
		aRowStates,
		[](const RowState& a, const RowState& b) {
			return sign((a.mPlantCount + a.mMelonCount - a.mZombieCount * 5) -
				(b.mPlantCount + b.mMelonCount - b.mZombieCount * 5));
		},
		[](int aRow, const RowState& aRowState) { return true; }
	);

	// vx: ball type depends on ice-shrooms on the conveyor belt
	int aIceShroomCount = 0;
	if (theBoard->mSeedBank)
	{
		for (int i = 0; i < theBoard->mSeedBank->mNumPackets; i++)
		{
			if (theBoard->mSeedBank->mSeedPackets[i].mPacketType == SeedType::SEED_ICESHROOM)
			{
				aIceShroomCount++;
			}
		}
	}
	if (aIceShroomCount == 0)
	{
		aDecision.mIsFireBall = true;
	}
	else if (aIceShroomCount == 1)
	{
		aDecision.mIsFireBall = (RandRangeInt(0, 4) == 0);
	}
	else
	{
		aDecision.mIsFireBall = false;
	}
	return aDecision;
}

// vanilla: stomp-row picker
int PickBossStompRow(Board* theBoard, const intptr_t* theRowArray, int theRowCount)
{
	return (int)PvzpPickFromArray(theRowArray, theRowCount);
}

// vx: smash the 3x2 block (target cell = top-left) scoring: plants smashed
// (flower pots included) + 2x melons + last-column melons + 2x effective
// zombies in the two rows
BossTargetDecision PickBossRVTarget(Board* theBoard)
{
	auto aRowStats = GetRowStates(theBoard);

	int aBestRow = -1;
	int aBestCol = -1;
	int aBestScore = 0;
	int aTieCount = 0;
	for (int aRow = 0; aRow <= 3; aRow++)
	{
		for (int aCol = 0; aCol <= 2; aCol++)
		{
			int aPlantCount = 0;
			int aMelonCount = 0;
			int aLastColMelons = 0;
			for (Plant* aPlant : theBoard->mPlants)
			{
				if (aPlant->mDead || aPlant->mSquished)
				{
					continue;
				}
				if (aPlant->mRow < aRow || aPlant->mRow > aRow + 1 ||
					aPlant->mPlantCol < aCol || aPlant->mPlantCol > aCol + 2)
				{
					continue;
				}

				aPlantCount++;
				SeedType aSeedType = aPlant->mSeedType == SeedType::SEED_IMITATER
					? aPlant->mImitaterType : aPlant->mSeedType;
				if (aSeedType == SeedType::SEED_MELONPULT)
				{
					aMelonCount++;
					if (aPlant->mPlantCol == 4)  // vx: last column of the 5-col boss board
					{
						aLastColMelons++;
					}
				}
			}
			int aScore = aPlantCount + aMelonCount * 2 + aLastColMelons +
				(aRowStats[aRow].mZombieCount + aRowStats[aRow + 1].mZombieCount) * 2;

			bool aIsBetter = aBestRow < 0 || aScore > aBestScore;
			if (aIsBetter)
			{
				aBestRow = aRow;
				aBestCol = aCol;
				aBestScore = aScore;
				aTieCount = 1;
			}
			else if (aScore == aBestScore)
			{
				// vx: random tie-break among equally good targets
				aTieCount++;
				if (Rand(aTieCount) == 0)
				{
					aBestRow = aRow;
					aBestCol = aCol;
				}
			}
		}
	}

	return BossTargetDecision{aBestRow, aBestCol};
}

int PickBossBungeeCol(Board* theBoard)
{
	// vx: pick the 3-column block whose 3 best steals (one per bungee
	// column) score the highest; first enumerated wins on ties
	auto aRowStats = GetRowStates(theBoard);

	int aBestCol = -1;
	int aBestScore = 0;
	for (int aCol = 0; aCol <= 2; aCol++)
	{
		int aBlockScore = 0;
		int aColBests[3] = { 0, 0, 0 };
		for (int i = 0; i < 3; i++)
		{
			int aColBest = 0;
			for (Plant* aPlant : theBoard->mPlants)
			{
				if (aPlant->mDead || aPlant->mSquished)
				{
					continue;
				}
				if (aPlant->mPlantCol != aCol + i)
				{
					continue;
				}
				int aScore = ScoreBungeeSteal(aPlant, aRowStats);
				if (aScore > aColBest)
				{
					aColBest = aScore;
				}
			}
			aColBests[i] = aColBest;
			aBlockScore += aColBest;
		}

		// vx: debug print
		Sexy::PrintF("BungeeCol: block %d cols %d/%d/%d total %d\n", aCol, aColBests[0], aColBests[1], aColBests[2], aBlockScore);

		if (aBestCol < 0 || aBlockScore > aBestScore)
		{
			aBestCol = aCol;
			aBestScore = aBlockScore;
		}
	}
	// vx: debug print
	Sexy::PrintF("BungeeCol: pick block %d\n", aBestCol);

	return aBestCol;
}

// vx: boss bungee cell picker, moved here from Zombie::PickBungeeZombieTarget
BossTargetDecision PickBossBungeeTarget(Board* theBoard, int theColumn, bool aAllowSunFlowerTarget)
{
	// vx: boss bungees take the highest-scoring steal, first enumerated on ties
	auto aRowStats = GetRowStates(theBoard);
	BossTargetDecision aTarget{ -1, -1 };
	int aBestScore = -1;
	for (int x = 0; x < MAX_GRID_SIZE_X; x++)
	{
		if (theColumn != -1 && theColumn != x)
		{
			continue;
		}
		for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
		{
			if (theBoard->GetGraveStoneAt(x, y) || theBoard->mGridSquareType[x][y] == GridSquareType::GRIDSQUARE_DIRT)
			{
				continue;
			}

			int aScore = 0;
			Plant* aPlant = theBoard->GetTopPlantAt(x, y, PlantPriority::TOPPLANT_BUNGEE_ORDER);
			if (aPlant)
			{
				if (aPlant->mSquished)
				{
					continue;
				}
				if (!aAllowSunFlowerTarget && aPlant->MakesSun())
				{
					continue;
				}
				if (aPlant->mSeedType == SeedType::SEED_GRAVEBUSTER || aPlant->mSeedType == SeedType::SEED_COBCANNON)
				{
					continue;
				}
				aScore = ScoreBungeeSteal(aPlant, aRowStats);
				// vx: debug print
				Sexy::PrintF("Bungee col %d cell(%d,%d) %s row%d z%d p%d g%d score%d\n",
					theColumn, x, y, Plant::GetNameString(aPlant->mSeedType, aPlant->mImitaterType).c_str(),
					aPlant->mRow, aRowStats[aPlant->mRow].mZombieCount, aRowStats[aPlant->mRow].mPultCount,
					aRowStats[aPlant->mRow].mGargantuarCount, aScore);
			}

			if (!theBoard->BungeeIsTargetingCell(x, y) && (aTarget.mRow < 0 || aScore > aBestScore))
			{
				aTarget.mRow = y;
				aTarget.mCol = x;
				aBestScore = aScore;
			}
		}
	}
	// vx: debug print
	Sexy::PrintF("Bungee col %d -> target(%d,%d) score %d\n", theColumn, aTarget.mCol, aTarget.mRow, aBestScore);
	return aTarget;
}

// vx: decide summon type and row together
BossSummonDecision PickBossSummon(Board* theBoard, int theZombieAge, int theSpawnPoints)
{
	BossSummonDecision aSummon{ZombieType::ZOMBIE_NORMAL, -1};

	// vx: one row-state snapshot per summon decision, shared by all branches
	auto aRowStates = GetRowStates(theBoard);

	if (theZombieAge < 3500)
	{
		// 普僵放投手最少的地方
		aSummon.mZombieType = ZombieType::ZOMBIE_NORMAL;
		aSummon.mRow = PickRow(
			theBoard,
			aRowStates,
			[](const RowState& a, const RowState& b) {
				return sign(b.mPultCount - a.mPultCount);
			},
			[](int aRow, const RowState& aRowState) { return true; }
		);
	}
	else if (theZombieAge < 8000)
	{
		// 路障放二四路已有僵尸最少的地方
		aSummon.mZombieType = ZombieType::ZOMBIE_TRAFFIC_CONE;
		aSummon.mRow = PickRow(
			theBoard,
			aRowStates,
			[](const RowState& a, const RowState& b) {
				return sign(b.mZombieCount - a.mZombieCount);
			},
			[](int aRow, const RowState& aRowState) { return aRow == 1 || aRow == 3; }
		);
	}
	else if (theZombieAge < 12500)
	{
		// 铁桶放一三五路已有僵尸最少的地方
		aSummon.mZombieType = ZombieType::ZOMBIE_PAIL;
		aSummon.mRow = PickRow(
			theBoard,
			aRowStates,
			[](const RowState& a, const RowState& b) {
				return sign(b.mZombieCount - a.mZombieCount);
			},
			[](int aRow, const RowState& aRowState) { return aRow == 0 || aRow == 2 || aRow == 4; }
		);
	}
	else
	{
		// 放在僵尸不多于一个的，且最薄弱的地方
		aSummon.mRow = PickRow(
			theBoard,
			aRowStates,
			[](const RowState& a, const RowState& b) {
				return sign(b.mToughness - a.mToughness);
			},
			[](int aRow, const RowState& aRowState) {
				return aRowState.mZombieCount <= 1;
			}
		);
		if (aSummon.mRow < 0)
		{
			aSummon.mRow = RandRangeInt(0, 4);
		}
		// 出一个适合的僵尸
		const ZombieType* aTactic;
		int aTacticCount;
		if (aRowStates[aSummon.mRow].mToughness < 2.0f)
		{
			aTactic = gBossTacticCollapse;
			aTacticCount = LENGTH(gBossTacticCollapse);
		}
		else if (aRowStates[aSummon.mRow].mToughness < 4.5f)
		{
			aTactic = gBossTacticWeak;
			aTacticCount = LENGTH(gBossTacticWeak);
		}
		else
		{
			aTactic = gBossTacticStrong;
			aTacticCount = LENGTH(gBossTacticStrong);
		}
		aSummon.mZombieType = random_choice_if(
			aTactic,
			aTacticCount,
			[aSummon, theBoard, theSpawnPoints](ZombieType theZombieType){
				return GetBossZombieCost(theZombieType) <= theSpawnPoints
					&& theBoard->RowCanHaveZombieType(aSummon.mRow, theZombieType);
			},
			ZombieType::ZOMBIE_TRAFFIC_CONE
		);
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
