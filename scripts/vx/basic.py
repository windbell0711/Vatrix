import _vx
from dataclasses import dataclass
from typing import Literal, Optional


# ===== dataclasses =====

@dataclass(frozen=True)
class Zombie:
    """Snapshot of a zombie; row/col are 1-based."""
    id: int
    typ: int
    row: int
    col: int
    x: float
    v: float
    age: int
    hp: int
    hp_max: int
    helm: int
    helm_max: int
    slow: int


@dataclass(frozen=True)
class Plant:
    """Snapshot of a plant; row/col are 1-based."""
    id: int
    typ: int
    row: int
    col: int
    age: int
    hp: int
    hp_max: int
    asleep: bool

    def rmv(self):
        """Shovel this plant."""
        _vx.rmv_plant(self.id)


@dataclass(frozen=True)
class Card:
    """Snapshot of a dropped seed card on the field (COIN_USABLE_SEED_PACKET)."""
    id: int
    typ: int
    age: int

    def plc(self, row, col):
        """Plant this dropped card at (row, col) (1-based)."""
        _vx.plc(self.id, row - 1, col - 1)


@dataclass(frozen=True)
class Vase:
    """Snapshot of a vase (scary pot); row/col are 1-based."""
    id: int
    vase_typ: int
    content_typ: int
    row: int
    col: int
    transparent: bool

    def brk(self, delay=0.02):
        """Break exactly this vase (even if other vases share the cell), then block for delay seconds."""
        if delay < 0:
            raise ValueError("delay must be >= 0")
        _vx.brk_vase(self.id)
        _vx.slp(delay)


# ===== get snapshot functions =====

def get_zombies() -> list[Zombie]:
    """Return the current zombies as a list of Zombie dataclasses."""
    return [Zombie(id=z["id"], typ=z["typ"] + 100, row=z["row"] + 1, col=z["col"] + 1, x=z["x"], v=z["v"],
                   age=z["age"], hp=z["hp"], hp_max=z["hp_max"], helm=z["helm"], helm_max=z["helm_max"],
                   slow=z["slow"]) for z in _vx.get_zombies()]  # typ converted from 0-based to consts.zt

def get_plants() -> list[Plant]:
    """Return the current plants as a list of Plant dataclasses."""
    return [Plant(id=p["id"], typ=p["typ"], row=p["row"] + 1, col=p["col"] + 1, age=p["age"],
                  hp=p["hp"], hp_max=p["hp_max"], asleep=p["asleep"]) for p in _vx.get_plants()]

def get_cards() -> list[Card]:
    """Return the dropped seed cards as a list of Card dataclasses."""
    return [Card(id=c["id"], typ=c["typ"], age=c["age"]) for c in _vx.get_cards()]

def get_vases() -> list[Vase]:
    """Return the vases on the field as a list of Vase dataclasses."""
    return [Vase(id=v["id"], row=v["row"] + 1, col=v["col"] + 1, transparent=v["transparent"],
                 vase_typ=v["vase_typ"], content_typ=v["content_typ"]) for v in _vx.get_vases()]


# ===== time control =====

def get_now() -> float:
    """Current level game time in seconds; frozen while the game is paused."""
    return _vx.time()

def slp(delay: float):
    """Sleep for delay seconds; the game keeps running."""
    if delay < 0:
        raise ValueError("delay must be >= 0")
    _vx.slp(delay)

def slp_until(ddl: float):
    """Sleep until the level game time reaches the given value (in seconds)."""
    _vx.slp_until(ddl)
