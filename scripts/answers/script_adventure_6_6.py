# Submit at 2026-08-15 18:11:24: 2539, 2551, 0
from vx import *

brk(3, 3)
brk(3, 7)

slp(0.1)
z = get_zombies()[0]
get_cards()[0].plc(z.row, z.col)
