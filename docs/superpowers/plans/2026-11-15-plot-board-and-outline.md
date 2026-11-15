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
- **To another text** a scene moved in the editor at first; the board moves it since 2c-2 (below).

### 2c – scenes in the plan (done, with 2c-2: a scene of a text moved to another text)

Variant C (default): planned scenes beside the scenes of the text, with a dashed frame, in the `.klh`
file; Enter on a planned scene ("Write the Scene") puts a scene break with its fields at its place in
the text and opens the text there. Variant A (only the scenes of the text) and B (only planned scenes)
in Settings > Plot Board (`plotBoard.scenes`).

- **The book** (`core`): `ElementPlot::planned`, a list of `PlannedScene` (a `ScenePlot` without a
  state and `after`, how many scenes of the text come before it), in the `.klh` file as
  `"plot": { "planned": [ {…, "after": 2} ] }`; `ChapterDocument::withSceneAdded()` takes the fields of
  the new scene.
- **The order**: `PlotBoard::sceneOrder()` puts each scene in the plan after the scenes of the text it
  comes after, in the order of the plan; `planInOrder()` gives the plan of an order rearranged (each
  scene after the scenes of the text before it; without them, in B, the places the plan had). A card of
  a scene in the plan is `<text>#p<n>`.
- **The board**: in C a scene in the plan is "In the Plan", in a dashed frame, lighter than the
  scenes written, its description in italics, without words and a state; colored by state, it has the
  color of the plan (`editor.plotPlanned`). In B the scenes of the plan are "Scene 1"… with a full
  frame, without words and a state. Insert adds a scene in the plan after a
  scene (Insert on the card of a text still adds a text), Alt+Insert one at the end of the text; F2 and
  the color change it; Ctrl with the arrows, Ctrl+X and Ctrl+V and the mouse move it among the scenes
  of its text, and to another text (Ctrl with the arrows across the texts: to the end of the text
  before, to the start of the text after); Delete deletes it at once; a state is not given to it.
  A scene of the text moved past a scene in the plan changes only the plan; moved past a scene of the
  text, the text, and the plan keeps the places of its scenes. Enter in C writes it (in a text not
  written yet, the first scene in the plan is the text itself), in B opens the text. Each of these is
  one Ctrl+Z on the board.
- **The outline**: a row of each scene in the plan ("–" for the number and the words, the state "In
  the Plan", in italics in C), edited in its cells.
- **Settings > Plot Board**: the scenes (`plotBoard.scenes`: `both`, `text`, `plan`) and the colors of
  the cards of the theme (the states, the scenes in the plan, the own colors).
- **2c-2, a scene of a text moved to another text** (done): Ctrl with the arrows across the texts
  (to the end of the text before, to the start of the text after), dropped on a scene of another
  text (before or after it) or on the card of a text (first among its scenes), Ctrl+V after it.
  `ChapterDocument::withSceneMovedTo()` gives both texts: the scene with its text and what it says,
  and the annotations anchored in it moved from the `<annotations>` section of one to that of the
  other (one anchored also in what stays, or with an id the other text has, goes as a copy with a
  new id). Into an empty text the scene takes the place of its empty scene; the only scene of a
  text stays. The scenes in the plans of both texts keep their places among the scenes of the
  texts. One step of Ctrl+Z on the board changes both texts back, each in its editor when it is
  open, else in its file.

## Stage 3 – settings and the other panels

### 3a – the Edit menu, the Navigator and the Properties panel (done)

- Edit > Undo, Redo, Cut, Paste and Delete work on the board and in the outline as their keys do:
  `PlotBoardPanel` is an `EditTarget` (`include/kalahari/gui/edit_target.h`), which
  `EditCoordinator` asks, as it asks a field, outside a field being written in (a field of the
  card editor keeps its own commands). Copy and Select All stay the outline's (the text of a
  cell); the status bar says nothing of a command the board says itself under it.
- "Show on the Plot Board" in the menu of the book, a part and a text of the main section in the
  Navigator (`PlotBoardCoordinator::showOnBoard()`): the board with the card of the text, or of the
  first text of the part, chosen, with the keys.
- The Properties panel of a text of the main section: the group "Plot Board" (`PlotFieldsBox`)
  with its description, point of view, place, time of the action and card color; the characters
  and places of the Workshop suggested; saved through `ProjectManager::setPlot()` when the keys
  leave a field, and filled again when the book changes elsewhere. A scene is changed on the
  board and in the outline.

### 3b – Settings > Plot Board (done)

The page came with 2c, with the scenes and the colors; now, as in the mockup, in five groups:

- View: the view a new tab opens on (`plotBoard.startView`: the board, the outline or the one
  used last, the default; Book > Plot Board and Book > Outline open their own), the layout and
  the size of the cards.
- Scenes: shown under the texts or not, and which scenes there are (2c).
- Cards: what they show besides their titles (`plotBoard.fields.*`: description, point of
  view, place, time of the action (off by default), words, state, the number of scenes; the
  view had `PlotBoardView::Fields` since stage 1).
- Colors: what colors the cards, as a strip at their top (the default) or their background
  (`plotBoard.colorPlace`), the background of the board (`plotBoard.background`: the theme's,
  cork or the writer's own; theme colors `plotCork` and `plotBackground`), and the colors of
  the theme.
- Outline: its columns besides the title (`plotBoard.outline.*`); the button "Table Columns"
  over the outline chooses them too. Without the description the last column takes the room.

`PlotBoardPanel` keeps its switches and the look from the settings in
`plot_board_panel_header.cpp`.

### 3c – the front and back sections, the numbers in the titles (done)

As in the mockup:

- The front and back sections on the board (`plotBoard.showSections`, off by default; Settings
  > Plot Board > View): a band with the name of the section before the main section and one
  after it, in columns and in the grid, their texts (also those in groups) dimmed in a dashed
  frame (`PlotBoard::sectionBand()`, `PlotBoardView::setSections()`). A preview only: their
  cards cannot be chosen, moved or opened, the tooltip says that the Navigator opens them. A
  section without texts is not shown; the outline shows the main section only.
- The numbers in the titles the program gave ("Chapter 3", also "Chapter 3. Storm";
  `PlotBoard::numberedTitle()`: the default title of the kind in the language of the book, its
  numbers arabic or roman, with any rest after them) when a move leaves them out of the order
  of the book (`PlotBoard::renumbered()`: the n-th text of a kind in the main section has number
  n). `plotBoard.renumber`: "ask" (the default) – the bar under the board says so and offers
  "Renumber", a step of its own for Ctrl+Z; "auto" – renumbered in the step of the move;
  "never". Only the number changes; the writer's own titles stay.

## Every stage

Tests (`tests/gui/test_plot_board.cpp`), the Polish translations, a SonarQube analysis of the branch, a
PR of its own.
