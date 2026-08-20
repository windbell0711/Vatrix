# Submit at 2026-08-20 16:22:03: 24195, 25085, 0
from vx import *

slp(5.6)
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
