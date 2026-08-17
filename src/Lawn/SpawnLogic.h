#ifndef __SPAWNLOGIC_H__
#define __SPAWNLOGIC_H__

#include <array>
#include <vector>
#include <cstdint>

#include "../ConstEnums.h"
#include "Board.h"
struct ZombiePicker;

// vx: SpawnLogic: centralized zombie-spawn / boss-target decision layer (Vatrix mod)
namespace SpawnLogic
{
	struct BossFireballDecision
	{
		int							mRow;
		bool						mIsFireBall;
	};

	struct BossTargetDecision
	{
		int							mRow;
		int							mCol;
	};

	struct BossSummonDecision
	{
		ZombieType					mZombieType;
		int							mRow;
	};

	// vx: per-boss variant selection; V1 is the vanilla random config, V5/V6 share
	// the smart RV/bungee pickers and per-bungee boss scoring
	struct BossSpawnConfig
	{
		BossSummonDecision (*mPickSummon)(Board* theBoard, int theZombieAge, int theSpawnPoints);
		BossFireballDecision (*mPickFireball)(Board* theBoard);
		BossTargetDecision (*mPickRVTarget)(Board* theBoard);
		int (*mPickBungeeCol)(Board* theBoard);
		bool mUseBossBungeeScoring;  // vx: V1 falls back to the vanilla per-bungee pick
	};

	struct RowState
	{
		int							mPlantCount;
		int							mPultCount;
		int							mMelonCount;
		int                         mPotCount;
		bool                        mLastColMelon;
		float                       mToughness;
		int							mZombieCount;
		int							mGargantuarCount;
	};

	// vx: pool includes a free NORMAL zombie as the budget-floor pick (Vatrix mod)
	constexpr int BOSS_ZOMBIE_LIST_COUNT = 13;
	extern const ZombieType			gBossZombieList[BOSS_ZOMBIE_LIST_COUNT];
	extern const int				gBossZombieCost[BOSS_ZOMBIE_LIST_COUNT];
	
	constexpr int					BOSS_SUMMON_POINTS_INIT = 8;
	constexpr int					BOSS_SUMMON_POINTS_ADD  = 6;

	int								GetBossZombieCost(ZombieType theZombieType);
	std::array<RowState, MAX_GRID_SIZE_Y>	GetRowStates(Board* theBoard);
	int								ScoreBungeeSteal(Plant* thePlant, const std::array<RowState, MAX_GRID_SIZE_Y>& theRowStats);
	ZombieType						PickWaveZombieType(Board* theBoard, int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker);
	BossTargetDecision				PickBossRVTarget(Board* theBoard);
	int								PickBossStompRow(Board* theBoard, const intptr_t* theRowArray, int theRowCount);
	int								PickBossBungeeCol(Board* theBoard);
	BossTargetDecision				PickBossBungeeTarget(Board* theBoard, int theColumn, bool aAllowSunFlowerTarget);
	BossSpawnConfig					GetBossConfig(Board* theBoard);
	BossFireballDecision			PickBossFireball(Board* theBoard);
	BossSummonDecision				PickBossSummon(Board* theBoard, int theZombieAge, int theSpawnPoints);
}

#endif // __SPAWNLOGIC_H__
