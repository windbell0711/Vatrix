# Submit at 2026-08-14 17:40:58: 33649, 34295, 0
from vb import *

plt(3, 3)
slp(0.5)
for v in get_vases():
    if v.content_typ == 4:
        v.brk()
        slp(0.5)
    elif v.content_typ == 104:
        bkt_row = v.row

get_cards()[0].plc(bkt_row, 1)
rmv(3, 3)

slp(11)
for v in get_vases():
    if v.col < 5:
        v.brk()
for v in get_vases():
    if v.col > 5:
        v.brk()
slp(0.1)
for z in get_zombies():
    if z.typ == 102:
        get_cards()[0].plc(z.row, 9)

