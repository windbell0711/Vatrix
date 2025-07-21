import pvz_hacker as pvz
import adventure
import time

vb = pvz.VbHacker()
adventure.import_data()
adventure.levels_pool[20].show(vb.game)

print("coding start")
ps: list[tuple[int, int]] = [
    (0, 3),
    (0, 7),
    (1, 3),
    (1, 7),
    (2, 3),
    (2, 4),
    (2, 6),
    (2, 7),
    (3, 4),
    (3, 5),
    (3, 6),
    (4, 5)
]

for p in ps:
    time.sleep(0.3)
    vb.brk(x=p[1]-1, y=p[0])
# vb.brk(3, 3)

print("coding end")
