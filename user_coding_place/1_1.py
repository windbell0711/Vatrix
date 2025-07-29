import pvz_hacker as vb
import adventure

lvl_name = "1-1"
lvl_num = {"1-1": 0, "1-2": 1, "1-3": 2, "1-4": 3, "1-6": 5, "1-7": 6, "1-8": 7}

def solve():
    adventure.import_data()
    adventure.levels_pool[lvl_num[lvl_name]].show(vb.game)

    print("coding start")

    vb.brk(8, 2, after_delay=0.23)
    vb.brk(7, 2, after_delay=0.23)

    print("coding end")


if __name__ == '__main__':
    # vb.setting_up(background_running=True,
    #               speed_rate=5)
    # for loop in range(20):
        solve()
    #     vb.delay(10)
    #     while not vb.check_winning():
    #         vb.delay(2)
    # vb.setting_up(background_running=True,
    #               speed_rate=1)
