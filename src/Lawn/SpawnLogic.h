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

	constexpr int BOSS_ZOMBIE_LIST_COUNT = 12;
	extern const ZombieType			gBossZombieList[BOSS_ZOMBIE_LIST_COUNT];

	ZombieType						PickWaveZombieType(Board* theBoard, int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker);
	int								PickCustomSpawnRow(Board* theBoard, ZombieType theZombieType);
	BossFireballDecision			PickBossFireball(Board* theBoard);
	BossTargetDecision				PickBossRVTarget(Board* theBoard);
	int								PickBossStompRow(Board* theBoard, const intptr_t* theRowArray, int theRowCount);
	int								PickBossBungeeCol(Board* theBoard);
	ZombieType						PickBossSummonType(Board* theBoard, int theZombieAge, int theTargetRow);
}

#endif // __SPAWNLOGIC_H__
