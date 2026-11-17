"""Stopping a run at once.

The runner's stop sets one flag that every wait of the automaton looks at: a check watched
for ten seconds, a program that is starting, an element that has not appeared yet. Each of
them ends within a few hundredths of a second with Stopped, and the runner records the
scenario as stopped (not as a failure)."""

import threading


class Stopped(Exception):
    """The run was stopped while the automaton was waiting."""


_flag = threading.Event()


def watch(event: threading.Event) -> None:
    """Use the runner's stop event as the flag."""
    global _flag  # noqa: PLW0603 - one run at a time, one flag for every wait
    _flag = event


def requested() -> bool:
    return _flag.is_set()


def check() -> None:
    if _flag.is_set():
        raise Stopped("przebieg zatrzymany")


def sleep(seconds: float) -> None:
    """time.sleep that ends as soon as the run is stopped."""
    if _flag.wait(max(0.0, seconds)):
        raise Stopped("przebieg zatrzymany")
