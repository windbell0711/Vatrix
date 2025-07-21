import pvz_hacker as pvz
import adventure

vb = pvz.VbHacker()
adventure.import_data()
adventure.levels_pool[0].show(vb.game)

print("coding start")

print("hello vatrix")
vb.brk(8, 2)

print("coding end")
