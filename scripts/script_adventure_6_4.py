import vb

print(__file__ + " begin")

for row in (1, 2, 3, 4, 5):
    for col in (9, 8, 7, 6, 5):
        vb.brk(row, col)

vb.slp(0.2)
cnt = 0
for c in vb.get_cards():
    c.plc(cnt % 5 + 1, cnt // 5 + 1)
    cnt += 1

print(__file__ + " end")
