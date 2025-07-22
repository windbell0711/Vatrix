import pvz_hacker as vb
import adventure
import time

adventure.import_data()
adventure.levels_pool[1].show(vb.game)

print("coding start")

for i in (6, 7):
    for j in (1, 2, 3):
        vb.brk(x=i, y=j)
        time.sleep(0.25)

vb.update()
for k in (0, 1, 2):
    vb.cards[k].plc(x=8, y=k+1)

print("coding end")
