# Plot board and outline – implementation plan

Date: 2026-11-15 · Mockup: [Tablica fabuły](https://claude.ai/artifact/2hUGbUcieTKUg617832iiq), accepted
with the recommended defaults (scenes: variant C by default, A and B in Settings; layout: Columns by
default, Grid switchable) · Roadmap: `plan-faz.md`, "Tablica fabuły i konspekt".

## Goal

The texts of the book (chapters, prologue, epilogue) as cards with a short description, moved on the
board, and the same texts in a table (the outline): description, point of view, place and time of the
action, words, state. Everything works from the keyboard, in both themes, in the program's own windows.

## Stage 1 – the board of the texts and the outline (this branch)

- **Data** (`core/book_project`): a text keeps its short description (`ProjectElement::description`) and
  its plot (`ElementPlot`: `pov`, `place`, `time`, `color`, unknown fields kept) in the `.klh` file under
  `"plot"`. `ProjectManager::setPlot()` saves both at once; `projectChanged()` tells that the `.klh` file
  was saved after a change.
- **What the board shows** (`gui/plot_board`): `PlotBoard::bandsOf()` – the texts of the main section in
  bands (each part with its texts, the texts outside the parts between them); `stepPlace()`,
  `placeBeside()`, `placeInPart()` – the places a card moves to, the same as in the Navigator
  (`ProjectManager::movePlacesOf()`); the characters and the places of the Workshop as suggestions;
  `PlotColors` – the colors of the states and the six colors of the writer in the theme files
  (`editor.plot*`).
- **The board** (`gui/widgets/plot_board_view`): Columns (the parts side by side) or Grid (rows under the
  titles of the parts), three sizes of the cards, colors by state, point of view, own color or none.
  Keys: arrows, Home/End, Ctrl+arrows move, Enter opens, F2 edits the card over it
  (`plot_card_editor`, Ctrl+Enter keeps, Esc leaves), Insert adds a text after the card, Delete,
  Ctrl+X/Ctrl+V, Ctrl+Z/Ctrl+Y, Alt+Enter (Properties), menu key/Shift+F10. Drag and drop. Screen reader
  texts.
- **The outline** (`gui/plot_outline_model`): the parts over their texts; title, description, point of
  view, place, time and state edited in the cells; the same keys where a table allows them.
- **The tab** (`gui/panels/plot_board_panel`): Board | Outline, Columns | Grid, the size of the cards,
  what colors them, the texts and the words of the book; the bar under the board says what was done,
  with Undo.
- **The coordinator** (`gui/plot_board_coordinator`): one tab per window, filled again when the book
  changes; the changes go through `ProjectManager`, and the Navigator's commands open, add and delete.
  Ctrl+Z undoes a move, an edited card, a state and a color; a change of the book made elsewhere forgets
  what can be undone, since the places remembered may be no more.
- **Commands**: Book > Plot Board (Ctrl+Shift+T), Book > Outline (Ctrl+Shift+L); in the Book menu,
  since every letter of their names has a command of the View menu. Settings:
  `plotBoard.view`, `plotBoard.layout`, `plotBoard.cardSize`, `plotBoard.colorBy`.

## Stage 2 – scenes

Scenes from the scene breaks of the texts (variant C by default): the cards of the scenes under the card
of their chapter, a scene moved with its text, planned scenes added on the board; variants A and B in
Settings.

## Stage 3 – settings and the other panels

Settings > Plot Board: the colors per theme, the fields of the cards, the front and back sections, the
renumbering of "Chapter N" after a move; the plot fields in the Properties panel; "Show on the Plot Board"
in the Navigator; Edit > Undo and Redo of the menu working on the board.

## Every stage

Tests (`tests/gui/test_plot_board.cpp`), the Polish translations, a SonarQube analysis of the branch, a
PR of its own.
