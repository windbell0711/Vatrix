# Submit at 2026-08-14 15:42:20: 24889, 26271, 0
# Submit at 2026-08-14 15:40:01: 26780, 27840, 0
from vb import *

slp(6)
brk(3, 9)
slp(6)
brk(3, 7)

loop = True
while loop:
	for z in get_zombies():
		if z.x < 380:
			loop = False
	slp(0.5)
	
brk(3, 6)

slp(3)
brk(3, 8)

