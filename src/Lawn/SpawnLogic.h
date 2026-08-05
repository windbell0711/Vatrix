#ifndef __SPAWNLOGIC_H__
#define __SPAWNLOGIC_H__

#include <cstdint>

#include "../ConstEnums.h"

class Board;
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

	// vx: pool includes a free NORMAL zombie as the budget-floor pick (Vatrix mod)
	constexpr int BOSS_ZOMBIE_LIST_COUNT = 13;
	extern const ZombieType			gBossZombieList[BOSS_ZOMBIE_LIST_COUNT];
	extern const int				gBossZombieCost[BOSS_ZOMBIE_LIST_COUNT];
	constexpr int					BOSS_SUMMON_POINTS_PER_ACTION = 30;

	ZombieType						PickWaveZombieType(Board* theBoard, int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker);
	int								PickCustomSpawnRow(Board* theBoard, ZombieType theZombieType);
	BossFireballDecision			PickBossFireball(Board* theBoard);
	BossTargetDecision				PickBossRVTarget(Board* theBoard);
	int								PickBossStompRow(Board* theBoard, const intptr_t* theRowArray, int theRowCount);
	int								PickBossBungeeCol(Board* theBoard);
	BossSummonDecision				PickBossSummon(Board* theBoard, int theZombieAge, int theSpawnPoints);
	int								GetBossZombieCost(ZombieType theZombieType);
}

#endif // __SPAWNLOGIC_H__
