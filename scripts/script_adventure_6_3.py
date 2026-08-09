import vb

print(__file__ + " begin")

for r in (2, 3, 4):
    for c in (7, 8):
        vb.brk(r, c)

vb.slp(0.5)

row = 2
for card in vb.get_cards():
    card.plc(row, 9)
    row += 1

print(__file__ + " end")
