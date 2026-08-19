# Foreword

*By Dr. Edgar George Zomboss, PhD in Necro-Engineering, Master of the Algorithm*

My loyal, decomposing legions.

For decades, we have thrown our flesh against green fortresses - only to be thwarted by sunflower-generated sunbeams and utterly irrational stubbornness of a suburban gardener. Why? Because we relied on instinct. Instinct is a bug. Instinct is the float that rounds down victory to a decimal point of chance.

I have spent countless nights perfecting the solution. I call it Vatrix. It is not merely a programming language; it is a philosophy of certainty. With Vatrix, every zombie becomes a deterministic node in an invincible graph. Every attack is a pre-computed instruction. Every plant's death is a scheduled event.

While the grand theory and final architecture are mine alone, the actual writing was carried out by my most capable engineering underlings. You will find their names stamped on the marginal notes. They are the ones who translated my divine abstractions into bite‑sized instructions for your spongy cerebra.

This manual is our weapon. Read it. Digest it - yes, even the parts you cannot chew. You will learn to read the garden's memory, to overwrite the soil's very properties, and to replace the chaos of photosynthesis with the cold beauty of a while-loop.

And to any human who dares to peek at these pages - go ahead. Stare at the symbols. They will whisper nothing but your own irrelevance. For Vatrix speaks only to the dead. And the dead never miss a deadline.

Now turn the page. The algorithm is hungry.

-- *E.G.Z., with editorial contributions from the Vatrix Engineering Corps*
*Secret Sub‑Basement Laboratory, Year of the Eternal Grin*



# Catalog

[Foreword 前言](#Foreword-前言)

[vAPIx 接口文档](#vAPIx-接口文档)

## About Vatrix

1 [Open Source or Not? 开源那些事儿](#Open-Source-or-Not?-开源那些事儿)

3 [We Are the Undead 我们是不死族](#We-Are-the-Undead-我们是不死族)

5 [Fast and Furious 速度与激情](#Fast-and-Furious-速度与激情)

7 [Research on Squash 对于瓜杀一路相关定式的研究](#Research-on-Squash-对于瓜杀一路相关定式的研究)

## Programming in Vatrix

2 [Counting Stars 观星](#Counting-Stars-观星)

4 [Loop 圈圈圆圆圈圈](#Loop-圈圈圆圆圈圈)

6 [More Than a Dot 一点之外](#More-Than-a-Dot-一点之外)

8 [Coroutines 协程](#Coroutines-协程)



# vAPIx 接口文档

Vatrix 是一个基于 Python 的嵌入式脚本框架。请在程序开头加上 `from vx import *` 以访问 Vatrix API。游戏控制台会自动读取对应的程序文件 "script_adventure_6_X.py" 并将其注入到游戏中。

## Actions 操作

- brk(row, col, delay=0.02)

- plt(row, col, card_id=0)

- rmv(row, col)

## Time Control 时间控制

- get_now() -> float

- slp(delay: float)

- slp_until(ddl: float)

## Get Snapshot Functions 获取快照函数

- get_zombies() -> list[Zombie]

- get_plants() -> list[Plant]

- get_cards() -> list[Card]

- get_vases() -> list[Vase]

## Snapshot Dataclasses 快照数据类

### Zombie 僵尸

- id: int

- typ: int

- row: int

- col: int

- x: float

- v: float

- age: int

- hp: int

- hp_max: int

- helm: int

- helm_max: int

- slow: int

### Plant 植物

- id: int

- typ: int

- row: int

- col: int

- age: int

- hp: int

- hp_max: int

- asleep: bool

- rmv(self)

### Card 卡片

- id: int

- typ: int

- age: int

- plc(self, row, col)

### Vase 罐子

- id: int

- vase_typ: int

- content_typ: int

- row: int

- col: int

- transparent: bool

- brk(self, delay=0.02)



# Open Source or Not? 开源那些事儿

开源还是闭源？这是个问题。毕竟，要是自己辛辛苦苦写好的代码，被某个疯子改装利用，甚至用来对付自己，那可就**糟糕了！**

Vatrix 是一件伟大的艺术品，而他最伟大之处就在于，他是一群素不相识的程序员共同建造的。博士高中毕业后，创作并开源了 Vatrix 的原型，而整个框架都是基于已有的另一个开源项目 Pvz-Portable 编写的。代码被创作出来，便拥有了生命；而当他被开源出来并应用在另一个系统里，生命的意义随之转移、拓展、延续。

但记得选好许可证。

博士在复制了 Pvz-Portable 仓库并完成后续开发后，保留了原有的 LGPL 许可证，以至于任何人都可以自由地使用、修改、分发 Vatrix。而邪恶的疯狂戴夫先生，竟然胆敢对 Vatrix 进行改装，加上了操作植物的相关功能，企图利用这一不朽的发明对抗僵尸。不过，援引博士的话，Vatrix 系统无惧任何反击，包括私自改装和反作用，因为它的算法会将植物方的任何操作视作随机的节点，并推导出走向胜利的策略。

戴夫沿用了我们设计的 (row, col) 格式来指代格子，并分别引入`brk`、`plt`、`rmv`分别完成砸碎罐子、种植植物、铲去植物的动作，参数均为两个整数，分别代表所在行和所在列。时机控制沿用了`slp`函数，参数为一个小数，代表需要等待的时间。

如果戴夫必须用 Vatrix 才能反制我们的攻击，这恰恰证明了 *v*A*tr*I*x* 对于人类的无情碾压。既然代码已经开源，就让我们看看，谁才是这场算法之战最后的赢家。



# Counting Stars 观星

仰望夜空，星光点点。

我们遥望星空旋转，看星星腾挪变换。

于是我们把左上角称作 (0.0, 0.0)。

横着走，是 x，向右生长；

纵着落，是 y，向土埋葬。

像素一粒一粒，铺满屏幕的荒原。

每一颗像素都有编号，

每一行泥土都是尺规。

---

当自由的不死者在草坪上翩翩起舞，

戴夫却在混沌之上铺开网格。

行与列，像棋盘，像祭坛。

而左上角，是 (1, 1)，是万物伊始。

五条横线，九个竖格，50像素为一列，每一格都是被驯服的坐标。

row 从 1 到 5，那是你的阶层；

col 从 1 到 9，那是你的序位。

(1, 1) 是北极星，

(5, 9) 是南十字，

它们永恒地亮着，

像倒在绿茵场上的死者不曾闭上的眼睛。

---

然而，当数据从二维空间归入一维列表，

当你的灵魂从大地抽象出来，进入代码，

便进入了从零开始的秩序。

0, 1, 2, 3... 借下标找寻 list 中的自己。

那是数字最原始的母语，

是代码世界最优雅的艺术。

---

(row, col) 属于大地，从一开始；

list[index] 属于代码，从零开始。

两者都是坐标，但出生不同，

像左手的指节与右手的琴弦，

奏出的乐章，不可混淆。

---

而时间，时间是什么？

是厘秒。

一秒劈成一百瓣，

每一瓣都是 0.01。

那是大脑皮层的神经末梢

能感知的最小刻痕。

你可以 slp(0.05)，沉睡五厘秒，

让脚本在时间的河床上打盹；

你也可以 slp_until(2.00)，

精准醒来，像僵尸从墓穴中准时坐起。

那时，阳光正好，植物正绿，

而你的指令，掐着时间的咽喉。

---

当我们数星星时，

我们在数什么？

是屏幕坐标里的尘埃，

是网格祭坛上的刻度？

是列表下标中的次序，

还是那无止尽的厘秒？

每一只僵尸都有它的 y 和 x，

每一株植物都有它的 row 和 col，

每一具灵体都有它在列表中的位置。

而我们，不死族的程序员，

是星空的测绘师，

是时间的记账员。

---

这，就是我们的十四行诗，

一行一行，

直到胜利的

return 0;

*by Zombies*



# We Are the Undead 我们是不死族

鲁迅曾经说过，兵者，贵在自知。植物与僵尸之间的战争已经持续了将近十八年，而我们真正了解过自己吗？

可以说，Vatrix 第一次做出了这样的尝试。博士把僵尸抽象为一个 Python 类，并称做 Zombie。通过这个数据类，我们可以快速获知自身的状态信息，并以此为基础做出决策。

在战场上，`zs = get_zombies()`可以在变量`zs`中储存一个成员为 Zombie 的列表，表示为`list[Zombie]`，在保证场上有至少一只僵尸（`len(zs) > 0`）的情况下，可以通过`zs[0]`锁定这只僵尸。

而僵尸所有属性的获取方式与 Python 对象完全一致，例如`zs[0].row`获取所在行、`zs[0].x`获取精确的x坐标等等，其所有属性都已经在[vAPIx](#vAPIx-接口文档)中列出，随时查阅。

值得注意的是，`get_zombies()`返回的数据是一张静态的截图，而非实时更新的对象。如果在`slp(2.00)`后访问过去储存的`zs`，可能导致信息过时，需要再次调用`get_zombies()`重新获取。



# Loop 圈圈圆圆圈圈

> 没写呢

> for row in (2, 3, 4)

> for z in get_zombies()

> while z.x > 500:  slp(0.20)

