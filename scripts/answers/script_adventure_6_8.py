# Submit at 2026-08-16 10:05:02: 26310, 30759, 125
# This script is written by Lilold, tested by Windbell.
from vb import *
slp(1)
aal = [0,0,0,0,0]
for plant in get_plants():
	print(plant)
	if plant.typ == pt.rep:
		aal[plant.row-1]+=20
	elif plant.typ == pt.gat:
		aal[plant.row-1]+=40
	elif plant.typ == pt.thr:
		aal[plant.row-1]+=10
		if plant.row < 5:
			aal[plant.row]+=10
		if plant.row > 1:
			aal[plant.row-2]+=10
print(f"{aal = }")

slp(1)
zha = [0,0,0,0,0]
zcm = [0,0,0,0,0]
for zom in get_zombies():
	print(zom)
	ind = zom.row-1
	if zom.typ == zt.zom:
		zha[ind] += 5
	elif zom.typ == zt.bkt:
		zha[ind] += 40
	c = zom.col + 1
	if c > zcm[ind]:
		zcm[ind] = c
print(f"{zha = }")

pmc = [9,9,9,9,9]
def p(rc = 0):
	for row in [1,2,3,4,5]:
		ind = row - 1
		while zha[ind] - aal[ind] > 0 and pmc[ind] > zcm[ind] - rc:
			cs = get_cards()
			if len(cs) == 0:
				break
			cs[0].plc(row,pmc[ind])
			aal[ind] += 20
			pmc[ind] -= 1
			slp(0.05)
p()
slp(4)
p(1)
slp(4)
p(2)
slp(3)
p(3)
