# Submit at 2026-08-20 16:27:39: 2431, 2454, 0
from vx import *

for r in (2, 3, 4):
	for c in (3, 4, 5, 6, 7):
		brk(r, c, 0)

slp(0.1)

z = get_zombies()[0]
plt(z.row, z.col)
