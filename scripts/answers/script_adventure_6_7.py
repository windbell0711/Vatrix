# Submit at 2026-08-15 18:14:09: 26784, 27205, 0
# This script is written and tested by Lilold.
from vb import *

plt(3, 3)
slp(0.1)
for v in get_vases():
    if v.content_typ == 4:
        v.brk()
    elif v.content_typ == 104:
        bkt_row = v.row
        bkt_col = v.col
    elif v.col < 5:
        brk(v.row,v.col)
    slp(0.01)
rmv(3,3)
slp(0.05)
get_cards()[0].plc(bkt_row, bkt_col-1)
slp(5)
for v in get_vases():
	if v.col>5:
		v.brk()
slp(0.1)
for z in get_zombies():
    if z.typ == 102:
        get_cards()[0].plc(z.row, z.col+2)

slp(5)
for v in get_vases():
    v.brk()
