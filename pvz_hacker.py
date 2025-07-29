import struct
import time
import logging
from contextlib import contextmanager

from pvztoolkit.pvz import PvzModifier
import pvztoolkit.data

game = PvzModifier()
game.wait_for_game()
data_board = game.data.lawn.board

def read_board(*args: int | pvztoolkit.data.Offset) -> int:
    return game.read_offset((game.data.lawn, data_board, *args))

def rescan_as_float(x: int) -> float:
    """四字节整数重新识别为单精度浮点数"""
    return struct.unpack('f', struct.pack('i', x))[0]

def delay(seconds: float) -> None:
    """按游戏速度校正的延迟
    :param seconds: 游戏速度为1.0时对应的等待秒数"""
    if seconds > 1:  # 太短就不输出了
        print(f"Waiting {seconds * game.get_frame_duration() / 10} seconds...")
    time.sleep(seconds * game.get_frame_duration() / 10)


class Object:
    def __init__(self, idt: int, name: str, typ: int):
        self.idt = idt
        self.name = name
        self.typ = typ


class Plant(Object):
    def __init__(self,
                 idt: int,  # 编号(自然数)(0, 1, ...)
                 name: str,  # 名称("Sunflower", "Snow Pea")
                 typ: int,  # 类型(自然数)(0, 1, ...)
                 row: int,  # 位置(0-4or0-5)
                 col: int,  # 位置(0-8)
                 hp: list[int, int],  # 剩余血量,血值上限([156,300])
                 cd: list[int, int],  # 子弹发射倒计时,子弹发射时间间隔
                 asleep: bool):  # 是否睡着(True/False)
        """
        植物对象。
        :param idt: 在同一时刻唯一，在整局游戏中不一定唯一，不一定连续。谨慎使用。
        :param name: 类型的名称，对人类玩家最友好的一集。
        :param typ: 与名称对应，不保证一一对应。对应关系可参考plants_zombies_info.csv。
        :param row: 从上往下，从零开始。
        :param col: 从左往右，从零开始。
        :param hp: 剩余血量/血值上限，分别用self.hp[0]和self.hp[1]表示。
        :param cd: 子弹发射倒计时/子弹发射时间间隔，分别用self.cd[0]和self.cd[1]表示。
        :param asleep: True则植物睡着。
        """
        super().__init__(idt=idt, name=name, typ=typ)
        self.row = row
        self.col = col
        self.hp = hp
        self.cd = cd
        self.asleep = asleep

    def __str__(self):
        return (f"Plant {self.idt}\n"
                f"{self.typ} {self.name}\t{self.row + 1}行{self.col + 1}列\n"
                f"Hp: {self.hp[0]}/{self.hp[1]}\tCd: {self.cd[0]}/{self.cd[1]}\tAsleep: {self.asleep}")


class Zombie(Object):
    def __init__(self,
                 idt: int,
                 name: str,
                 typ: int,
                 row: int,
                 dis: float,
                 hp: int,
                 hp_max: int,
                 slow: float,
                 butter: float,
                 frozen: float):
        super().__init__(idt=idt, name=name, typ=typ)
        self.row = row
        self.dis = dis  # accurate position
        self.hp = hp
        self.hp_max = hp_max
        self.slow = slow
        self.butter = butter
        self.frozen = frozen

    def __str__(self):
        return (f"Zombie {self.idt}\n"
                f"{self.typ} {self.name}\t{self.row + 1}行 距家{self.dis + 1}\n"
                f"Hp: {self.hp}/{self.hp_max}\tSlow: {self.slow}\tButter: {self.butter}\tFrozen: {self.frozen}")

class HypnoticZombie(Zombie):
    def __init__(self, z: Zombie):
        self.__dict__.update(z.__dict__)


class Card(Object):
    def __init__(self,
                 idt: int,
                 name: str,
                 typ: int,
                 x: int,
                 y: int,
                 lost_time: int):
        super().__init__(idt=idt, name=name, typ=typ)
        self.x = x
        self.y = y
        self.lost_time = lost_time

    def __str__(self):
        return (f"Card {self.idt}\n"
                f"{self.typ} {self.name}\t位置：{self.x}, {self.y}\n"
                f"消失倒计时：{self.lost_time}")

    def plc(self, x: int, y: int) -> bool:
        if not game.click_avai():
            return False
        # game.left_click(card.x + 25, card.y + 35)
        # print(card.x + 25, card.y + 35)
        game.left_click(self.x, self.y)
        game.left_click(*game.xy_to_pos(x, y))
        return   # TODO


class Sun:
    def __init__(self, value: int):
        self.value = value


class Vase(Object):
    def __init__(self,
                 idt: int,
                 name: str,
                 # typ: int,
                 row: int,
                 col: int,
                 vase_type: int,
                 content_type: int,
                 zombie_type: int,
                 plant_type: int,
                 sun_count: int):
        super().__init__(idt=idt, name=name,
                         typ={0: -1,  # ept
                              1: plant_type,
                              2: 100 + zombie_type,
                              3: 0 - sun_count}[content_type])
        self.row = row
        self.col = col
        self.container = {3: 'q', 4: 'p', 5: 'z'}[vase_type]
        self.content = {0: None,
                        1: Plant,
                        2: Zombie,
                        3: Sun}[content_type]
        self.content_str = {0: "e",
                            1: "p",
                            2: "z",
                            3: "s"}[content_type]

    def __str__(self):
        return (f"Vase {self.idt}\t{self.name}\t{self.row + 1}行{self.col + 1}列\n"
                f"Container: {self.container}\tContent Type: {self.content_str} {self.typ}")

    def brk(self, sleep: float = 0.25) -> bool:
        return brk(x=self.col, y=self.row, after_delay=sleep)  # TODO


# class Scene:
#     def __init__(self):
#         self.win = memory.WindowMem(process_name="popcapgame1.exe")
#         self.mem = self.win.mem
#         self.time = self.win.read(offsets=[0x768, 0x5568])
#         self.scene = self.win.read(offsets=[0x768, 0x554C])
#         self.sun = self.win.read(offsets=[0x768, 0x5560])
# 
#         self.plants = []
#         self.zombies = []
#         self.cards = []
#         self.vases = []
#         self.bdc = []  # buDongChan 不动产
# 
#     def update(self):  # important
#         # 更新植物信息
#         self.plants = []
#         CURRENT_PLANTS_NUM = self.win.read([0x768, 0xBC])
#         MAX_EVER_PLANTS_NUM = self.win.read([0x768, 0xB0])
#         PL_ADDR = self.win.read([0x768, 0xAC])
#         for i in range(0, MAX_EVER_PLANTS_NUM * 0x14C, 0x14C):
#             if self.mem.read_bool(PL_ADDR + 0x141 + i) \
#                     or self.mem.read_bool(PL_ADDR + 0x142 + i):
#                 continue
#             self.plants.append(Plant(idt=self.mem.read_int(PL_ADDR + 0x148 + i), name="null",
#                                      typ=self.mem.read_int(PL_ADDR + 0x24 + i),
#                                      row=self.mem.read_int(PL_ADDR + 0x1C + i),
#                                      col=self.mem.read_int(PL_ADDR + 0x28 + i),
#                                      hp=[self.mem.read_int(PL_ADDR + 0x40 + i),
#                                          self.mem.read_int(PL_ADDR + 0x44 + i)],
#                                      cd=[self.mem.read_int(PL_ADDR + 0x58 + i),
#                                          self.mem.read_int(PL_ADDR + 0x5C + i)],
#                                      asleep=self.mem.read_bool(offsets=[0x768, 0xAC, 0x143 + i])))
# 
#         # 更新僵尸信息
#         self.zombies = []
#         CURRENT_ZOMBIES_NUM = self.win.read(offsets=[0x768, 0xA0])
#         MAX_EVER_ZOMBIES_NUM = self.win.read([0x768, 0x94])
#         ZO_ADDR = self.win.read([0x768, 0x90])
#         for i in range(0, MAX_EVER_ZOMBIES_NUM * 0x15C, 0x15C):
#             if self.mem.read_bool(ZO_ADDR + 0xBA + i) \
#                     or self.mem.read_bool(ZO_ADDR + 0xEC + i) \
#                     or (self.mem.read_int(ZO_ADDR + 0x60 + i) < 10):  # 濒死or消失or不存在/在罐子里
#                 continue
#             self.zombies.append(Zombie(idt=self.mem.read_int(ZO_ADDR + 0x158 + i), name="null",
#                                        typ=self.mem.read_int(ZO_ADDR + 0x24 + i),
#                                        row=self.mem.read_int(ZO_ADDR + 0x30 + i) // 100 + 1,
#                                        dis=self.mem.read_int(ZO_ADDR + 0x2C + i),
#                                        hp=[self.mem.read_int(ZO_ADDR + 0xC8 + i),
#                                            self.mem.read_int(ZO_ADDR + 0xCC + i)],
#                                        hypnotic=self.mem.read_bool(ZO_ADDR + 0xB8 + i * 0x14C),
#                                        slow=self.mem.read_int(ZO_ADDR + 0xAC + i),
#                                        butter=self.mem.read_int(ZO_ADDR + 0xB0 + i),
#                                        frozen=self.mem.read_int(ZO_ADDR + 0xB4 + i)))
# 
#         # TO-DO: update self.vases, self.bdc


plants:   list[Plant]  = []
zombies:  list[Zombie] = []
hypnotic: list[HypnoticZombie] = []
cards:    list[Card]   = []
vases:    list[Vase]   = []

def update():
    # _update_plants()
    _update_zombies()
    _update_cards()
    _update_vases()

def _update_plants():
    global plants
    raise NotImplementedError  # TODO

def _update_zombies():  # TODO
    global zombies
    # 更新僵尸信息
    zombies = []
    cnt_current = read_board(data_board.zombie_count)
    cnt_max =     read_board(data_board.zombie_count_max)
    addr_base =   read_board(data_board.zombies)
    struct_size = data_board.zombies.StructSize
    for i in range(0, cnt_max * struct_size, struct_size):
        if game.read_memory(addr_base + data_board.zombies.dead + i, 1):
            continue  # 似了
        z = Zombie(idt=game.read_memory(addr_base + data_board.zombies.idt + i, 4),
                   name="null",
                   typ=game.read_memory(addr_base + data_board.zombies.type + i, 4),
                   row=game.read_memory(addr_base + data_board.zombies.row + i, 4),
                   dis=rescan_as_float(game.read_memory(addr_base + data_board.zombies.x + i, 4)),
                   hp=(game.read_memory(addr_base + data_board.zombies.hp_self + i, 4)
                       + game.read_memory(addr_base + data_board.zombies.hp_accessory_1 + i, 4)
                       + game.read_memory(addr_base + data_board.zombies.hp_accessory_2 + i, 4)),
                   hp_max=(game.read_memory(addr_base + data_board.zombies.hp_self_max + i, 4)
                           + game.read_memory(addr_base + data_board.zombies.hp_accessory_1_max + i, 4)
                           + game.read_memory(addr_base + data_board.zombies.hp_accessory_2_max + i, 4)),
                   slow=  game.read_memory(addr_base + data_board.zombies.slow + i, 4),
                   butter=game.read_memory(addr_base + data_board.zombies.butter + i, 4),
                   frozen=game.read_memory(addr_base + data_board.zombies.frozen + i, 4))
        match game.read_memory(addr_base + data_board.zombies.hypnotic + i, 1):
            case 0:  # 未被魅惑
                zombies.append(z)
            case 1:  # 魅惑中
                hypnotic.append(HypnoticZombie(z))


def _update_cards():
    global cards
    # 更新卡片信息
    cards = []
    cnt_current = read_board(data_board.item_count)
    cnt_max =     read_board(data_board.item_count_max)
    addr_base =   read_board(data_board.items)
    struct_size = data_board.items.StructSize
    for i in range(0, cnt_max * struct_size, struct_size):
        # if game.read_memory(addr_base + data_board.items.invisible + i, 1) or game.read_memory(addr_base + data_board.items.exist + i, 2) != b'k\xdf':
        if game.read_memory(addr_base + data_board.items.exist + i, 2) == 0:
            continue  # 不存在
        # print(f"隐形：{game.read_memory(addr_base + data_board.items.invisible + i, 1)}")
        # print(f"存在：{game.read_memory(addr_base + data_board.items.exist + i, 2)}")
        if game.read_memory(addr_base + 0x58 + i, 4) in (9, 16):  # 卡牌
            cards.append(Card(idt=       game.read_memory(addr_base + 0xD4 + i, 4),
                              name=      'null',
                              typ=       game.read_memory(addr_base + 0x68 + i, 4),
                              x=         int(rescan_as_float(game.read_memory(addr_base + 0x24 + i, 4))) + 25,
                              y=         int(rescan_as_float(game.read_memory(addr_base + 0x28 + i, 4))) + 35,
                              lost_time= game.read_memory(addr_base + 0x54 + i, 4)))

def _update_vases():
    global vases
    # 更新花瓶信息
    vases = []
    cnt_current = read_board(data_board.grid_item_count)
    cnt_max =     read_board(data_board.grid_item_count_max)
    addr_base =   read_board(data_board.grid_items)
    struct_size = data_board.grid_items.StructSize
    for i in range(0, cnt_max * struct_size, struct_size):
        addr = addr_base + i
        if game.read_memory(addr + data_board.grid_items.dead, 1):
            continue  # 已破坏的花瓶
        if game.read_memory(addr + data_board.grid_items.type, 4) != 7:  # 7表示花瓶
            continue
        vases.append(Vase(idt=          game.read_memory(addr + data_board.grid_items.idt, 4),
                          name=         'null',
                          row=          game.read_memory(addr + data_board.grid_items.row, 4),
                          col=          game.read_memory(addr + data_board.grid_items.col, 4),
                          vase_type=    game.read_memory(addr + data_board.grid_items.vase_type, 4),
                          content_type= game.read_memory(addr + data_board.grid_items.vase_content_type, 4),
                          zombie_type=  game.read_memory(addr + data_board.grid_items.zombie_in_vase, 4),
                          plant_type=   game.read_memory(addr + data_board.grid_items.plant_in_vase, 4),
                          sun_count=    game.read_memory(addr + data_board.grid_items.sun_shine_in_vase, 4)))

def brk(x: int, y: int, after_delay: float = 0.25) -> None:
    """
    To break a vase existing.
    :param x: which to break ([0-5, 0-8])
    :param y: which to break ([0-5, 0-8])
    :param after_delay: break the vase and wait how many seconds
    :return: (deleted) Object/None/False/int(sun)
    """
    if not game.click_avai():
        return
    game.left_click(*game.xy_to_pos(x, y))
    if after_delay > 0.001:
        delay(after_delay)
    return

def plt(card_id: int, x: int, y: int) -> None:
    if not game.click_avai():
        return
    game.left_click(x=10+read_board(data_board.slots, data_board.slots.x + data_board.slots.CardOffset + card_id * data_board.slots.StructSize),
                    y=10+read_board(data_board.slots, data_board.slots.y + data_board.slots.CardOffset + card_id * data_board.slots.StructSize))
    game.left_click(*game.xy_to_pos(x, y))

def rmv(x: int, y: int) -> None:
    if not game.click_avai():
        return
    game.left_click(*game.shovel_pos(slot_count=read_board(data_board.slots, data_board.slots.count)))
    game.left_click(*game.xy_to_pos(x, y))


def setting_up(background_running: bool = None,
               speed_rate: float = None) -> None:
    if isinstance(background_running, bool):
        game.background_running(background_running)
    if isinstance(speed_rate, float | int) and 0.01 <= speed_rate <= 10.0:
        game.set_speed_rate(speed_rate)
    else:
        print("Invalid speed rate. 0.05 <= speed_rate <= 10.0")

def check_winning() -> bool:  # 僵尸全死完
    zom_cnt_current = read_board(data_board.zombie_count)
    zom_cnt_max = read_board(data_board.zombie_count_max)
    zom_addr_base = read_board(data_board.zombies)
    zom_struct_size = data_board.zombies.StructSize
    for i in range(0, zom_cnt_max * zom_struct_size, zom_struct_size):
        if game.read_memory(zom_addr_base + data_board.zombies.dead + i, 1) == 0 \
                and game.read_memory(zom_addr_base + data_board.zombies.hypnotic + i, 1) == 0:
            return False
    vase_cnt_current = read_board(data_board.grid_item_count)
    vase_cnt_max =     read_board(data_board.grid_item_count_max)
    vase_addr_base =   read_board(data_board.grid_items)
    vase_struct_size = data_board.grid_items.StructSize
    for i in range(0, vase_cnt_max * vase_struct_size, vase_struct_size):
        addr = vase_addr_base + i
        if game.read_memory(addr + data_board.grid_items.dead, 1) == 0 \
                and game.read_memory(addr + data_board.grid_items.type, 4) == 7:  # 7表示花瓶
            return False
    print("-----WIN-----")
    return True

# @contextmanager
# def running(loop=20, speed_rate=5, after_delay=5):
#     # 正式计时运行
#     logging.info("开始测试")
#     setting_up(background_running=True,
#                speed_rate=speed_rate)
#     timings = []
#     for _ in range(loop):
#         logging.info(f"Loop {_} 开始运行")
#         start_time = time.time()
#         yield  # 运行代码
#         delay(after_delay)
#         while not check_winning():
#             delay(0.5)
#         elapsed = time.time() - start_time
#         logging.info(f"Loop {_} 用时: {elapsed * speed_rate:.1f}秒")
#         timings.append(elapsed * speed_rate)
#     setting_up(background_running=True,
#                speed_rate=1)
#     logging.info("测试结束")
#     # 计算结果并输出
#     if timings:
#         avg_time = sum(timings) / len(timings)
#         max_time = max(timings)
#         min_time = min(timings)
#         logging.info(f"平均用时: {avg_time:.1f}秒")
#         logging.info(f"最长用时: {max_time:.1f}秒")
#         logging.info(f"最短用时: {min_time:.1f}秒")
#     else:
#         logging.warning("未记录到有效计时数据")


if __name__ == '__main__':
    print("from pvz_hacker.py")
    # for i in range(2, 11):
    #     game.set_slot_count(i)
    #     time.sleep(3)
    #     print(game.mouse_pos())
    # game.left_click(262, 138)
    # print(read_board(data_board.plants, data_board.plants.type))
    # rmv(4, 2)
