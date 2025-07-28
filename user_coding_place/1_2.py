import pvz_hacker as vb
import adventure

def solve():
    adventure.import_data()
    adventure.levels_pool[1].show(vb.game)

    vb.setting_up(background_running=True,
                  speed_rate=1)

    print("coding start")



    print("coding end")


if __name__ == '__main__':
    solve()
