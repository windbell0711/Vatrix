import pvz_hacker as vb
import adventure

for loop in range(20):
    adventure.import_data()
    adventure.levels_pool[0].show(vb.game)

    vb.setting_up(background_running=True,
                  speed_rate=5)

    print("coding start")

    vb.brk(8, 2, after_delay=0.225)
    vb.brk(7, 2, after_delay=0.225)

    vb.delay(15)
    while not vb.check_winning():
        vb.delay(1)

    print("coding end")
