# This script is partly written by Lilold.
from vb import *
import math

def ctox(c):
	return 80*c-40

vel_list1 = (1.4, 1.4, 1.4, 1.5, 1.4, 1.4, 1.3, 1.4, 1.4, 1.4, 1.5, 1.4, 0.8, 0.9, 0.9, 0.8, 0.1, 0.2, 0.1, 0.1, 0.0, 0.0, 0.0, 0.0, 2.4, 2.4, 2.3, 2.4, 2.3, 2.4, 2.4, 2.3, 1.2, 1.2, 1.2, 1.1, 1.3, 1.1, 1.2, 1.2, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1)  # 两手摆动
vel_list2 = (1.3, 1.2, 1.3, 1.3, 1.3, 1.3, 1.2, 1.2, 1.3, 1.2, 1.3, 1.3, 1.3, 1.3, 1.2, 1.3, 0.1, 0.1, 0.0, 0.1, 0.0, 0.1, 0.1, 1.8, 1.7, 1.8, 1.8, 1.8, 1.7, 1.8, 1.8, 1.8, 1.8, 1.7, 1.8, 1.7, 1.9, 1.7, 1.8, 0.1, 0.0, 0.1, 0.2, 0.1, 0.0, 0.1)  # 两手在前

def predict_time(vel_list, gamma, distance):
    """
    预测僵尸走过指定距离所需的时间（秒）
    :param vel_list: ZOMBIE_VEL_LIST 中的一组元组（如两手摆动）
    :param gamma: 该僵尸的速度参数（如普通僵尸 0.3）
    :param distance: 需要走过的距离（像素）
    :return: 所需时间（秒）
    """
    L = len(vel_list)
    S = sum(vel_list)
    
    # 基础增量
    delta = 0.47 * gamma / S
    coeff = 47 * delta  # 每一帧速度 = coeff * vel_list[idx]
    
    # 1. 计算一个完整周期（进度从0到1）的总帧数和总位移
    T_cycle = math.ceil(1.0 / delta)
    D_cycle = 0.0
    for t in range(T_cycle):
        # 刚出生时 t=0 对应 (0+1)*delta，符合游戏初始化
        p = ((t + 1) * delta) % 1.0
        idx = int(L * p)
        # 防止浮点误差导致 idx == L
        if idx >= L:
            idx = L - 1
        D_cycle += coeff * vel_list[idx]
    
    # 2. 利用周期性缩放
    cycles = int(distance // D_cycle)
    remaining_dist = distance - cycles * D_cycle
    total_frames = cycles * T_cycle
    
    # 3. 逐帧模拟剩余距离（最多一个周期，计算量极小）
    t = 0
    dist = 0.0
    while dist < remaining_dist:
        p = ((t + 1) * delta) % 1.0
        idx = int(L * p)
        if idx >= L:
            idx = L - 1
        dist += coeff * vel_list[idx]
        t += 1
    
    total_frames += t
    return total_frames / 100.0  # 1帧 = 0.01秒

def maxv() -> Vase | None:
	return max(filter(lambda v: v.row == 3, get_vases()), key=lambda v: v.col, default=None)

def plc_card(t, r, c):
	for card in get_cards():
		if card.typ == t:
			card.plc(r, c)
			break

def main():
	zs = list(filter(lambda z: z.row == 3, get_zombies()))
	mv = maxv()
	if not mv:
		print('end')
		return
	if not zs:
		mv.brk()
		return
	time = sum(predict_time(vel_list1, z.v, z.x-180) for z in zs) / len(zs)
	if time <= predict_time(vel_list1, (0.23+0.37)/2, ctox(mv.col)-180):
		mv.brk()
	return

if __name__ == "__main__":
	for v in get_vases():
		if v.row == 2 or v.row == 4:
			v.brk()

	slp(3)
	plc_card(pt.squ, 2, 4)
	plt(4, 9)
	
	rmv(2, 2); rmv(4, 2)

	while True:
		main()
		slp(1)
