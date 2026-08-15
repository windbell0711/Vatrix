# Submit at 2026-08-15 16:02:29: 21531, 22323, 125
from vb import *
slp(0.1)
aal = [0,0,0,0,0]
for plt in get_plants():
	if plt.typ == 7:
		aal[plt.row-1]+=2
	elif plt.typ == 40:
		aal[plt.row-1]+=4
	elif plt.typ == 18:
		aal[plt.row-1]+=1
		if plt.row < 5:
			aal[plt.row]+=1
		if  plt.row > 1:
			aal[plt.row-2]+=1

slp(1)
zha = [0,0,0,0,0]
zcm = [0,0,0,0,0]
for zom in get_zombies():
	ind = zom.row-1
	if zom.typ == 100:
		zha[ind] += 1
	else:
		zha[ind] += 5
	c = zom.col + 1
	if c > zcm[ind]:
		zcm[ind] = c

pmc = [9,9,9,9,9]
def p(rc = 0):
	for row in [1,2,3,4,5]:
		ind = row - 1
		while zha[ind] - aal[ind] > 0 and pmc[ind] > zcm[ind] - rc:
			get_cards()[0].plc(row,pmc[ind])
			aal[ind] += 2
			pmc[ind] -= 1
			slp(0.05)
p()
slp(4)
p(1)
slp(4)
p(2)
