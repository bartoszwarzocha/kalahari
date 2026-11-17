"""A small Qt Widgets program for checking the automaton on a machine (not a test of Kalahari).

It has the kinds of widgets Kalahari has (menu bar, dialog, check buttons, search field,
table, editor) and, on purpose, a slow star column and a slow window, so the probe can
compare what it measures from outside with what the program itself knows. The program
prints its own moments (QueryPerformanceCounter, in seconds) as JSON lines on stdout.
"""

import ctypes
import json
import sys
import time

from PySide6.QtCore import QEvent, QObject, Qt, QTimer
from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtWidgets import (QApplication, QDialog, QHBoxLayout, QLabel, QLineEdit, QMainWindow,
                               QPlainTextEdit, QPushButton, QTableWidget, QTableWidgetItem,
                               QVBoxLayout, QWidget)

_kernel32 = ctypes.WinDLL("kernel32")
_freq = ctypes.c_longlong()
_kernel32.QueryPerformanceFrequency(ctypes.byref(_freq))


def qpc() -> float:
    now = ctypes.c_longlong()
    _kernel32.QueryPerformanceCounter(ctypes.byref(now))
    return now.value / _freq.value


def note(event: str, **data) -> None:
    print(json.dumps({"event": event, "qpc": qpc(), **data}, ensure_ascii=False), flush=True)


def busy(seconds: float) -> None:
    """Keep the window thread busy, like a slow operation in the program."""
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        pass


class KeyLog(QObject):
    """Notes every key press the program receives, with the widget that has the focus."""

    def eventFilter(self, watched, event):
        if event.type() == QEvent.KeyPress and watched is QApplication.focusWidget():
            note("key", key=event.key(), text=event.text(), modifiers=int(event.modifiers().value),
                 widget=type(watched).__name__, window=watched.window().windowTitle())
        return False


class CharDialog(QDialog):
    ROWS = [("—", "Pauza", "U+2014"), ("–", "Półpauza", "U+2013"), ("…", "Wielokropek", "U+2026")]

    def __init__(self, parent):
        super().__init__(parent)
        self.setWindowTitle("Znak specjalny")
        self._painted = False
        layout = QVBoxLayout(self)
        views = QHBoxLayout()
        self.grid_button = QPushButton("Siatka")
        self.list_button = QPushButton("Lista")
        for button in (self.grid_button, self.list_button):
            button.setCheckable(True)
            button.setAutoExclusive(True)
            views.addWidget(button)
        self.grid_button.setChecked(True)
        layout.addLayout(views)
        self.search = QLineEdit()
        self.search.setAccessibleName("Szukaj znaku")
        layout.addWidget(self.search)
        self.table = QTableWidget(len(self.ROWS), 4)
        self.table.setAccessibleName("Znaki")
        self.table.setHorizontalHeaderLabels(["Znak", "Nazwa", "Kod", "★"])
        for row, (char, name, code) in enumerate(self.ROWS):
            for column, text in enumerate((char, name, code, "☆")):
                item = QTableWidgetItem(text)
                item.setFlags(item.flags() & ~Qt.ItemIsEditable)
                self.table.setItem(row, column, item)
        self.table.cellClicked.connect(self._cell_clicked)
        layout.addWidget(self.table)
        self.resize(520, 360)

    def _cell_clicked(self, row: int, column: int) -> None:
        if column != 3:
            return
        note("star_clicked", row=row)
        busy(0.8)
        item = self.table.item(row, 3)
        item.setText("★" if item.text() == "☆" else "☆")
        note("star_set", row=row, text=item.text())

    def showEvent(self, event):
        super().showEvent(event)
        note("dialog_show_event", title=self.windowTitle())
        self._painted = False

    def hideEvent(self, event):
        super().hideEvent(event)
        note("dialog_hide_event", title=self.windowTitle())

    def paintEvent(self, event):
        super().paintEvent(event)
        if not self._painted:
            self._painted = True
            note("dialog_first_paint", title=self.windowTitle())


class SlowDialog(QDialog):
    def __init__(self, parent):
        super().__init__(parent)
        self.setWindowTitle("Wolne okno")
        QVBoxLayout(self).addWidget(QLabel("Pokazane po 0,4 s pracy"))


class Main(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Sonda automatu")
        insert = self.menuBar().addMenu("&Wstaw")
        action = QAction("&Znak specjalny...", self)
        action.setShortcut(QKeySequence("Ctrl+Shift+I"))
        action.triggered.connect(self.open_chars)
        insert.addAction(action)
        central = QWidget()
        layout = QVBoxLayout(central)
        self.slow_button = QPushButton("Wolne okno")
        self.slow_button.clicked.connect(self.open_slow)
        layout.addWidget(self.slow_button)
        self.editor = QPlainTextEdit()
        self.editor.setAccessibleName("Tekst")
        layout.addWidget(self.editor)
        self.setCentralWidget(central)
        self.chars = None
        self.resize(900, 640)

    def open_chars(self):
        note("menu_action")
        if self.chars is None:
            self.chars = CharDialog(self)
            note("dialog_constructed")
        self.chars.show()
        self.chars.raise_()
        self.chars.activateWindow()
        note("dialog_shown_call")

    def open_slow(self):
        note("slow_clicked")
        busy(0.4)
        dialog = SlowDialog(self)
        dialog.setAttribute(Qt.WA_DeleteOnClose)
        dialog.show()
        note("slow_shown_call")


def main() -> int:
    app = QApplication(sys.argv)
    keys = KeyLog()
    app.installEventFilter(keys)
    window = Main()
    window.show()
    QTimer.singleShot(0, lambda: note("ready"))
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
