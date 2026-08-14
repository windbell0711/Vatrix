# Submit at 2026-08-14 17:32:01: 2471, 2487, 0
from vb import *

brk(3, 3)
brk(3, 7)

slp(0.1)
z = get_zombies()[0]
get_cards()[0].plc(z.row, z.col)

