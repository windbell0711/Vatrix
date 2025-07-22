import struct
from pvz_toolkit.src.pvz import PvzModifier

game = PvzModifier()
game.wait_for_game()


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
                 asleep: bool  # 是否睡着(True/False)
                 ):
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
                 hp: list[int, int],
                 hypnotic: bool,
                 slow: float,
                 butter: float,
                 frozen: float
                 ):
        super().__init__(idt=idt, name=name, typ=typ)
        self.row = row
        self.dis = dis  # accurate position
        self.hp = hp
        self.hypnotic = hypnotic
        self.slow = slow
        self.butter = butter
        self.frozen = frozen

    def __str__(self):
        return (f"Zombie {self.idt}" + "\t!Hypnotic\n" if self.hypnotic else "\n" +
                                                                             f"{self.typ} {self.name}\t{self.row + 1}行 距家{self.dis + 1}\n"
                                                                             f"Hp: {self.hp[0]}/{self.hp[1]}\tSlow: {self.slow}\tButter: {self.butter}\tFrozen: {self.frozen}")


class Card(Object):
    def __init__(self,
                 idt: int,
                 name: str,
                 typ: int,
                 x: int,
                 y: int,
                 lost_time: int
                 ):
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


cards: list[Card] = []


def rescan_as_float(x: int) -> float:
    """四字节整数重新识别为单精度浮点数"""
    return struct.unpack('f', struct.pack('i', x))[0]


def update():
    _update_cards()


def _update_cards():
    global cards
    # 更新卡片信息
    cards = []
    cnt_item_current = game.read_offset((game.data.lawn, game.data.lawn.board, game.data.lawn.board.item_count))
    print(cnt_item_current)
    cnt_item_max_ever = game.read_offset((game.data.lawn, game.data.lawn.board, game.data.lawn.board.item_count_max))
    addr_item = game.read_offset((game.data.lawn, game.data.lawn.board, game.data.lawn.board.items))
    for i in range(0, cnt_item_max_ever * 0xD8, 0xD8):
        # if game.read_memory(addr_item + game.data.lawn.board.items.invisible + i, 1):
        #         # or game.read_memory(addr_item + game.data.lawn.board.items.exist + i, 2) != b'k\xdf':
        if game.read_memory(addr_item + game.data.lawn.board.items.exist + i, 2) == 0:
            continue  # 不存在
        # print(f"隐形：{game.read_memory(addr_item + game.data.lawn.board.items.invisible + i, 1)}")
        # print(f"存在：{game.read_memory(addr_item + game.data.lawn.board.items.exist + i, 2)}")
        if game.read_memory(addr_item + 0x58 + i, 4) in (9, 16):  # 卡牌
            cards.append(Card(idt=game.read_memory(addr_item + 0xD4 + i, 4),
                              name='null',
                              typ=game.read_memory(addr_item + 0x68 + i, 4),
                              x=int(rescan_as_float(game.read_memory(addr_item + 0x24 + i, 4))) + 25,
                              y=int(rescan_as_float(game.read_memory(addr_item + 0x28 + i, 4))) + 35,
                              lost_time=game.read_memory(addr_item + 0x54 + i, 4)))


def brk(x: int, y: int) -> None:
    """
    To break a vase existing.
    :param x: which to break ([0-5, 0-8])
    :param y: which to break ([0-5, 0-8])
    :param delay: (deleted) break the vase after how many seconds
    :return: (deleted) Object/None/False/int(sun)
    """
    if not game.click_avai():
        return
    game.left_click(*game.xy_to_pos(x, y))


if __name__ == '__main__':
    game.left_click(262, 138)
