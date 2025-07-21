import pvz_hacker as pvz
import adventure
import time

vb = pvz.VbHacker()
adventure.import_data()
adventure.levels_pool[1].show(vb.game)

print("coding start")

for i in (6, 7):
    for j in (1, 2, 3):
        vb.brk(x=i, y=j)
        time.sleep(0.25)

vb.update()
for k in (0, 1, 2):
    vb.plc(vb.cards[k], x=8, y=k+1)

print("coding end")
