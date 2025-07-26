"""
-*- coding: utf-8 -*-
@Time    : 2024/6/14 15:36
@Author  : TheWindbell07
@File    : adventure.py
"""
import os
import random
import time

# from memory import WindowMem
from pvztoolkit.pvz import PvzModifier
import csv


levels_pool = []
alias2code = {}
with open(file="../plants_zombies_info.csv" if os.path.exists("../plants_zombies_info.csv") else "plants_zombies_info.csv",
          mode='r', encoding='utf-8', newline='') as f_i:
    for line in csv.reader(f_i):
        alias2code[line[2]] = int(line[0])


class Level:
    def __init__(self, data):
        # self.__dict__.update(data)  # 这种写法过于模糊，不利于程序的维护
        try:
            # 基本信息
            self.code = [data['code1'], data['code2']]  # 关卡编号
            self.special = (data['special'] == "1")  # 若为1则特判，否则为普通关
            # 场地信息
            self.scene_id = int(data['scene_id'])
            self.scene_design = data['scene_design'].split("/")
            self.scene_rule_code = data['scene_rule_code']
            self.scene_rule = data['scene_rule']
            self.scene_array = None  # 生成self.scene_array
            if self.scene_rule_code == "2":
                rules = {"0": []}
                for j in self.scene_rule.split(";"):
                    if j[1] != ":":
                        print(f"!Cannot read {self.code[0]}-{self.code[1]}: {j}")
                        continue
                    rules[j[0]] = j[2:].split("&")
                array = []
                for k0 in self.scene_design:
                    array.append([rules[k1] for k1 in k0])
                self.scene_array = array
            # 罐子信息
            self.vase_design = data["vase_design"]
            self.vase_rule_code = data["vase_rule_code"]
            self.vase_rule = data["vase_rule"]
            self.vase_pools = None  # 生成self.vase_pools
            match self.vase_rule_code:
                case "1":
                    p = []
                    for item in self.vase_rule.split("+"):
                        if item[3] != "*":
                            print(f"!Cannot read {self.code[0]}-{self.code[1]}: {item}")
                            continue
                        for _ in range(int(item[4:])):
                            p.append([item[:3], "q"])
                    self.vase_pools = {"1": p}
                case "2":
                    ps = {}
                    for j in self.vase_rule.split(";"):
                        if j[1] != "@" or j[3] != ":":
                            print(f"!Cannot read {self.code[0]}-{self.code[1]}: {j}")
                            continue
                        p = []
                        for item in j[4:].split("+"):
                            if item[3] != "*":
                                print(f"!Cannot read {self.code[0]}-{self.code[1]}: {item}")
                                continue
                            for _ in range(int(item[4:])):
                                p.append([item[:3], j[2]])
                        ps[j[0]] = p
                    self.vase_pools = ps
                case "3":
                    ps = {}
                    for j in self.vase_rule.split(";"):
                        if j[1] != ":":
                            print(f"!Cannot read {self.code[0]}-{self.code[1]}: {j}")
                            continue
                        p = []
                        for item in j[2:].split("+"):
                            if item[3] != "*" or item[-2] != "@":
                                print(f"!Cannot read {self.code[0]}-{self.code[1]}: {item}")
                                continue
                            for _ in range(int(item[4:-2])):
                                p.append([item[:3], item[-1]])
                        ps[j[0]] = p
                    self.vase_pools = ps
            # 卡槽信息
            self.slot_rule = data["slot_rule"]
            # 备注
            self.beizhu = data["beizhu"]
        except IndexError or KeyError or ValueError as e:
            print("\n\n!File adventure_info.csv not available, please check.\n"
                  "Data leading to an error: " + str(data))
            raise e  # 导入的文件数据格式错误

    def show(self, game: PvzModifier):
        print(f"Initiating level {self.code[0]}-{self.code[1]}...")
        # 更改场景类型
        game.set_scene(self.scene_id)
        # 创造再隐藏钱袋，以实现永不过关
        game.put_zombie(row=0, col=0, zombie_type=0)
        game.kill_all_zombies()
        item_addr_base = game.read_offset((game.data.lawn, game.data.lawn.board, game.data.lawn.board.items))
        item_struct_size = game.data.lawn.board.items.StructSize
        time.sleep(0.05)
        for i in range(0, game.read_offset((game.data.lawn, game.data.lawn.board, game.data.lawn.board.item_count_max)) * item_struct_size, item_struct_size):
            if game.read_memory(item_addr_base + game.data.lawn.board.items.exist + i, 2) == 0:
                continue  # 不存在
            if game.read_memory(item_addr_base + game.data.lawn.board.items.item_type + i, 4) == 18:  # 通关钱袋子
                # print(game.read_memory(item_addr_base + game.data.lawn.board.items.missing + i))
                game.write_memory(address=item_addr_base + game.data.lawn.board.items.missing + i,
                                  data=1, length=4)
                break
        # 开始布置场景
        if not self.special:  # 非特殊关
            game.sun_shine(25)  # TODO: 设置阳光数
            try:
                vase_pools = self.vase_pools.copy()
                for _ in vase_pools.keys():
                    random.shuffle(vase_pools[_])
                # print(self.vase_design)
                for row in range(6):
                    for col in range(9):
                        # 放置罐子
                        vase_category = self.vase_design[row * 10 + col]
                        if vase_category != '0':
                            name, style = vase_pools[vase_category].pop()  # 取出并删除最后一项
                            code = alias2code[name]
                            style = {'q': 3, 'p': 4, 'z': 5, 'e': 3}[style]
                            game.put_vase(row=row, col=col, vase_type=style, vase_content_type=((code // 100) + 1),
                                          plant_type=code, zombie_type=code-100, sun_shine_count=50)
                        # 放置场景植物/僵尸
                        if self.scene_array is not None:
                            names = self.scene_array[row][col]  # e.g. 'lil'
                            for name in names:
                                code = alias2code[name]  # e.g. 16
                                if 0 <= code < 100:  # 添加植物
                                    game.put_plant(row=row, col=col, plant_type=code, imitator=False)
                                elif code >= 100:  # TODO: 添加僵尸
                                    pass
                                else:
                                    print(f"!Warning: Code of {name} Not Found. Row: {row}; Col: {col}.")
            except IndexError:
                pass
        else:
            match self.code:  # 玩玩时髦的新用法match
                case ['0', '4']:
                    pass
                case ['0', '9']:
                    pass
                case ['1', '4']:
                    pass
                case ['1', '9']:
                    pass
                case _:
                    raise KeyError("!Level Code Not Found or Not Set To Special Category.")


def import_data() -> bool:  # 将.csv文件中的信息转化成对象Level存入列表levels_pool
    global levels_pool
    levels_pool = []
    with open(file="../adventure_info.csv" if os.path.exists("../adventure_info.csv") else "adventure_info.csv",
              mode='r', encoding='utf-8', newline='') as f:
        info = csv.DictReader(f)  # 将.csv文件以dict形式导入
        for dat in info:
            levels_pool.append(Level(data=dat))  # 存入列表levels_pool
    return True


if __name__ == '__main__':
    import_data()
    # for i in levels_pool:
    #     print(i.__dict__)

    # levels_pool[0].show(WindowMem(process_name="popcapgame1.exe"))
    g = PvzModifier()
    g.wait_for_game()
    levels_pool[1].show(g)
