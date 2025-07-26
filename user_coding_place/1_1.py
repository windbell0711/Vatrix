import pvz_hacker as vb
import adventure

adventure.import_data()
adventure.levels_pool[0].show(vb.game)

vb.setting_up(background_running=True,
              speed_rate=2)

print("coding start")

vb.brk(8, 2, after_delay=0.225)
vb.brk(7, 2, after_delay=0.225)

print("coding end")
