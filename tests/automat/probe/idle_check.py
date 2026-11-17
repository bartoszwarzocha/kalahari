"""Does the automaton's input reset the idle timer that locks the screen?

The lock after idle time (and the screen saver) counts from the last input, as
GetLastInputInfo tells it. The check waits without input, then moves the cursor by a pixel
and back through SendInput (as the automaton does) and reads the timer again.

    python tests/automat/probe/idle_check.py
"""

import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from automat.win import input as inp, system  # noqa: E402

WAIT_S = 3


def main() -> int:
    first = system.idle_ms()
    time.sleep(WAIT_S)
    before = system.idle_ms()
    x, y = inp.cursor_position()
    inp.move_to(x + 1, y, steps=1)
    inp.move_to(x, y, steps=1)
    after_move = system.idle_ms()
    time.sleep(0.5)
    inp.press("Shift")
    after_key = system.idle_ms()
    report = {"idle_at_start_ms": first, "idle_after_wait_ms": before,
              "idle_after_mouse_ms": after_move, "idle_after_key_ms": after_key}
    reset_by_mouse = before is not None and after_move is not None and after_move < 200 <= before
    reset_by_key = after_key is not None and after_key < 200
    report["mouse_resets_idle"] = reset_by_mouse
    report["key_resets_idle"] = reset_by_key
    print(json.dumps(report, indent=2))
    print("Ruchy automatu " + ("odsuwają" if reset_by_mouse and reset_by_key else "NIE odsuwają")
          + " blokadę po bezczynności.")
    return 0 if reset_by_mouse and reset_by_key else 1


if __name__ == "__main__":
    sys.exit(main())
