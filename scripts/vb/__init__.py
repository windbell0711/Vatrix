# vx: Vatrix scripting API
import _vb
import time


def brk(row: int, col: int, delay=0.0):
    """Break the vase at (row-1, col-1) now, then block the script for delay seconds."""
    if delay < 0:
        raise ValueError("delay must be >= 0")
    _vb.brk(row-1, col-1)
    time.sleep(delay)

def slp(delay: float):
    """Sleep for delay seconds; the game keeps running (script thread blocks)."""
    if delay < 0:
        raise ValueError("delay must be >= 0")
    time.sleep(delay)

def plt(row: int, col: int, card_id=0):
    """Plant the card at seed bank slot card_id (0-based) at (row, col) (1-based)."""
    _vb.plt(row-1, col-1, card_id)

def rmv(row: int, col: int):
    """Shovel the plant at (row, col) (1-based)."""
    _vb.rmv(row-1, col-1)
