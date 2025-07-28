import pvz_hacker as vb
import adventure

def solve():
    adventure.import_data()
    adventure.levels_pool[1].show(vb.game)

    print("coding start")

    vb.plt(4, 3, 2)
    vb.brk(6, 2)
    vb.brk(7, 2)
    vb.delay(15)
    vb.rmv(4, 2)
    vb.brk(5, 2)

    print("coding end")


if __name__ == '__main__':
    vb.setting_up(background_running=True,
                  speed_rate=5)
    for loop in range(20):
        solve()
        vb.delay(10)
        while not vb.check_winning():
            vb.delay(1)
