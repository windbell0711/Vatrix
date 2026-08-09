# vx: Vatrix scripting API - player scripts in this folder can `import vb`
import _vb
import time


def brk(row, col, delay=0.0):
    """Break the vase at (row, col) now, then block the script for delay seconds."""
    if delay < 0:
        raise ValueError("delay must be >= 0")
    _vb.brk(row, col)
    time.sleep(delay)
