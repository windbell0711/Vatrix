from pvztoolkit.src.pvz import PvzModifier

class VbHacker:
    def __init__(self):
        self.game = PvzModifier()
        self.game.wait_for_game()

    def brk(self, x: int, y: int) -> None:
        self.game.break_vase_mouse(x, y)
