# Submit at 2026-08-14 16:24:21: 1640, 1651, 0
from vb import *

for r in (2, 3, 4):
	for c in (3, 4, 5, 6, 7):
		brk(r, c)

slp(0.1)

z = get_zombies()[0]
plt(z.row, z.col)

