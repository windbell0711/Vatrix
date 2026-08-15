# vx: Vatrix scripting API
import _vb
from . import consts
from dataclasses import dataclass

@dataclass(frozen=True)
class Zombie:
    """Snapshot of a zombie; row/col are 1-based (feed them back into brk/plt/rmv directly)."""
    row: int
    col: int
    x: float
    v: float  # mVelX
    hp: int
    helm: int
    hp_max: int
    helm_max: int
    slow: int  # freeze countdown
    typ: int  # ZombieType
    id: int  # internal zombie handle


def brk(row: int, col: int, delay=0.0):
    """Break the vase at (row-1, col-1) now, then block the script for delay seconds."""
    if delay < 0:
        raise ValueError("delay must be >= 0")
    _vb.brk(row-1, col-1)
    _vb.slp(delay)

def slp(delay: float):
    """Sleep for delay seconds; the game keeps running (script thread blocks)."""
    if delay < 0:
        raise ValueError("delay must be >= 0")
    _vb.slp(delay)

def plt(row: int, col: int, card_id=0):
    """Plant the card at seed bank slot card_id (0-based) at (row, col) (1-based)."""
    _vb.plt(row-1, col-1, card_id)

def rmv(row: int, col: int):
    """Shovel the plant at (row, col) (1-based)."""
    _vb.rmv(row-1, col-1)


@dataclass(frozen=True)
class Plant:
    """Snapshot of a plant; row/col are 1-based."""
    row: int
    col: int
    hp: int
    hp_max: int
    age: int  # ticks since planted
    asleep: bool
    typ: int  # SeedType
    id: int  # internal plant handle

    def rmv(self):
        """Shovel this plant."""
        _vb.rmv_plant(self.id)


@dataclass(frozen=True)
class Card:
    """Snapshot of a dropped seed card on the field (a COIN_USABLE_SEED_PACKET)."""
    age: int  # ticks since it was dropped
    typ: int  # SeedType the card grants
    id: int  # internal coin handle

    def plc(self, row, col):
        """Plant this dropped card at (row, col) (1-based)."""
        _vb.plc(self.id, row - 1, col - 1)


@dataclass(frozen=True)
class Vase:
    """Snapshot of a vase (scary pot); row/col are 1-based."""
    row: int
    col: int
    vase_typ: int     # ????:QUESTION/LEAF/ZOMBIE
    content_typ: int  # ??????(SCARYPOT_*)
    transparent: bool
    id: int  # internal grid-item handle

    def brk(self, delay=0.0):
        """Break exactly this vase (even if other vases share the cell), then block for delay seconds."""
        if delay < 0:
            raise ValueError("delay must be >= 0")
        _vb.brk_vase(self.id)
        _vb.slp(delay)


def get_zombies():
    """Return the current zombies as a list of Zombie dataclasses (row/col 1-based)."""
    return [Zombie(row=z["row"] + 1, col=z["col"] + 1, x=z["x"], v=z["v"], hp=z["hp"], helm=z["helm"],
                   hp_max=z["hp_max"], helm_max=z["helm_max"], slow=z["slow"],
                   typ=z["typ"] + 100, id=z["id"]) for z in _vb.get_zombies()]  # 0-based -> consts.zt


def get_plants():
    """Return the current plants as a list of Plant dataclasses (row/col 1-based)."""
    return [Plant(row=p["row"] + 1, col=p["col"] + 1, hp=p["hp"], hp_max=p["hp_max"],
                  age=p["age"], asleep=p["asleep"], typ=p["typ"], id=p["id"]) for p in _vb.get_plants()]


def get_cards():
    """Return the dropped seed cards as a list of Card dataclasses."""
    return [Card(age=c["age"], typ=c["typ"], id=c["id"]) for c in _vb.get_cards()]


def get_vases():
    """Return the vases on the field as a list of Vase dataclasses (row/col 1-based)."""
    return [Vase(row=v["row"] + 1, col=v["col"] + 1, vase_typ=v["vase_typ"],
                 content_typ=v["content_typ"], transparent=v["transparent"], id=v["id"]) for v in _vb.get_vases()]
