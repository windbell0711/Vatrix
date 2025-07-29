import pvz_hacker as vb
import adventure

def solve():
    adventure.import_data()
    adventure.levels_pool[3].show(vb.game)

    print("coding start")

    cnt = 0
    for x in (8, 7, 6, 5, 4):
        for y in range(5):
            vb.brk(x, y, after_delay=0.3)
        vb.update()
        for c in vb.cards:
            c.plc(cnt // 5, cnt % 5)
            cnt += 1
            vb.delay(0.05)

    print("coding end")


if __name__ == '__main__':
    solve()
    # vb.setting_up(background_running=True,
    #               speed_rate=5)
    # for loop in range(20):
    #     solve()
    #     vb.delay(10)
    #     while not vb.check_winning():
    #         vb.delay(2)
    # vb.setting_up(background_running=True,
    #               speed_rate=1)
