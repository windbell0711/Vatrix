import pvz_hacker as vb
import adventure

for loop in range(20):
    adventure.import_data()
    adventure.levels_pool[1].show(vb.game)

    vb.setting_up(background_running=True,
                  speed_rate=10)

    print("coding start")

    for i in (6, 7):
        for j in (1, 2, 3):
            vb.brk(x=i, y=j)

    vb.update()
    for k in (0, 1, 2):
        vb.cards[k].plc(x=8, y=k+1)

    vb.delay(15)
    while not vb.check_winning():
        vb.delay(1)

    print("coding end")
