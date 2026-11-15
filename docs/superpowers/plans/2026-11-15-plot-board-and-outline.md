# Plot board and outline – implementation plan

Date: 2026-11-15 · Mockup: [Tablica fabuły](https://claude.ai/artifact/2hUGbUcieTKUg617832iiq), accepted
with the recommended defaults (scenes: variant C by default, A and B in Settings; layout: Columns by
default, Grid switchable) · Roadmap: `plan-faz.md`, "Tablica fabuły i konspekt".

## Goal

The texts of the book (chapters, prologue, epilogue) as cards with a short description, moved on the
board, and the same texts in a table (the outline): description, point of view, place and time of the
action, words, state. Everything works from the keyboard, in both themes, in the program's own windows.

## Stage 1 – the board of the texts and the outline (done)

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
Settings. Three steps, each a commit of the one pull request of the board:

### 2a – the scenes of the texts on the board (done)

- **A scene** is the text of a chapter from its start or from a scene break (`<scene-break/>`, at the
  level of the paragraphs) to the next break. A text without breaks is one scene and has no cards of
  scenes.
- **What a scene says**: title, description, point of view, place and time of the action, color and
  state. A break keeps them as its attributes (`<scene-break title="…" description="…" pov="…" place="…"
  time="…" color="…" status="…"/>`), so they move with its text, also when it is cut and pasted in the
  editor; the editor reads and writes them (`SCENE_ATTRIBUTES_PROPERTY` on the character of the break).
  The first scene has no break: its fields are in the `.klh` file, `"plot": { "opening": {…} }`. A scene
  without a state has the state of its text.
- **The book** (`core`): `ChapterDocument::kmlScenes()` – the scenes of a KML with their words (the
  words of the scenes add up to those of the text), `withScenePlot()` – the KML with the fields of a
  break changed; `ProjectManager::scenesOf()` from the files (read with the words), `setScenePlot()` for
  a text whose file has its text (the first scene in the `.klh` file, the others in the chapter file).
  An open text with unsaved changes gives its scenes from its editor, and the fields of its scenes
  change in its editor, where Ctrl+Z also undoes them.
- **The board**: the cards of the scenes under the card of their text in Columns, in a drawer under
  the card in Grid (Space opens and closes it, the card says "▸ 3 scenes"); "Scenes" in the header of the
  board shows or hides them (`plotBoard.showScenes`). A scene card: "Scene 2" with its words and state,
  its title, description, point of view, place and time. The arrows go through the scenes; F2 edits a
  scene, Enter opens its text at the scene, the menu gives it a state and a color, Ctrl+Z undoes it.
- **The outline**: the scenes as rows under their text, edited in the cells.
- Until 2b a scene card moved, added, cut or deleted said that scenes are moved, added and deleted in
  the text, in the editor; Ctrl+V on a scene card put the text marked after its text.

### 2b – scenes moved and added (done)

Ctrl+Up and Ctrl+Down (Columns) or Ctrl+Left and Ctrl+Right (the drawer) move a scene with its text
among the scenes of its text; Ctrl+X and Ctrl+V and the mouse too; Insert adds a scene break after the
scene, Alt+Insert a scene at the end of the text; Delete asks "Delete Scene": with its text, or the
break only (Keep the Text: its text joins the scene before it). One undo step on the board.

- **The book** (`core`): `ChapterDocument::withSceneMoved()`, `withSceneAdded()` and `withoutScene()`
  take and give a `SceneText` (the KML and the first scene): the first scene goes to the `.klh` file
  when another one takes its place. `ProjectManager::setSceneText()` writes a closed text and its first
  scene.
- **An open text** changes in its editor (`BookEditor::replaceWithKml()`, one step of its Ctrl+Z), to
  be saved with it; a text changed in the editor since is not undone on the board.
- **To another text** a scene moves in the editor for now: the board says so (Ctrl with the arrows
  across the texts, the mouse, Ctrl+V after another text). The annotations of a scene are kept with
  its text, and a move across two texts, one open and one closed, needs one undo step of both; that
  comes with 2c.

### 2c – scenes in the plan

Variant C (default): planned scenes beside the scenes of the text, with a dashed frame, in the `.klh`
file; Enter on a planned scene ("Write the Scene") puts a scene break with its fields at its place in
the text and opens the text there. Variant A (only the scenes of the text) and B (only planned scenes)
in Settings > Plot Board (`plotBoard.scenes`).

## Stage 3 – settings and the other panels

Settings > Plot Board: the colors per theme, the fields of the cards, the front and back sections, the
renumbering of "Chapter N" after a move; the plot fields in the Properties panel; "Show on the Plot Board"
in the Navigator; Edit > Undo and Redo of the menu working on the board.

## Every stage

Tests (`tests/gui/test_plot_board.cpp`), the Polish translations, a SonarQube analysis of the branch, a
PR of its own.
