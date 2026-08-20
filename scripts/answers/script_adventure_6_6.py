# Submit at 2026-08-20 16:26:50: 1709, 1729, 0
from vx import *

brk(3, 3)
brk(3, 7)

slp(0.1)
z = get_zombies()[0]
get_cards()[0].plc(z.row, z.col)
