import _vx
from .basic import *

# ===== actions =====

def brk(row: int, col: int, delay=0.02):
    """Break the vase at (row-1, col-1) now, then sleep for delay seconds."""
    if delay < 0:
        raise ValueError("delay must be >= 0")
    _vx.brk(row-1, col-1)
    _vx.slp(delay)

def plt(row: int, col: int, card_id=0):
    """Plant the card at seed bank slot card_id (0-based) at (row, col) (1-based)."""
    _vx.plt(row-1, col-1, card_id)

def rmv(row: int, col: int):
    """Shovel the plant at (row, col) (1-based)."""
    _vx.rmv(row-1, col-1)
