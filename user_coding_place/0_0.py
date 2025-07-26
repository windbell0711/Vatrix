import pvz_hacker as vb
import adventure

adventure.import_data()
adventure.levels_pool[20].show(vb.game)

vb.setting_up(background_running=True)

print("coding start")
ps: list[tuple[int, int]] = [
    (0, 3),
    (0, 7),
    (1, 3),
    (1, 7),
    (2, 3),
    (2, 4),
    (2, 6),
    (2, 7),
    (3, 4),
    (3, 5),
    (3, 6),
    (4, 5)
]

for p in ps:
    vb.brk(x=p[1] - 1, y=p[0])

print("coding end")
