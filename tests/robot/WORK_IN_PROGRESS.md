# Command robot – work in progress, paused

Status on 2026-11-16: paused before the robot was built, because building it was not
approved. None of these files is part of the build: `tests/CMakeLists.txt` does not list them.
The proposal is `przeglad/automatyzacja-testow-propozycja.md` in the project files.

## What exists

- `robot_report.h`, `robot_report.cpp` – the records of a run: `steps.jsonl` for each
  situation, written as each step goes, so that a crash or a hang still names its step and
  phase; the pages made from them: `index.html` with the screenshots, `summary.md` for the
  summary of a run on GitHub. Not compiled yet.
- `robot_watch.h` – the interface of the watchers: the log, Qt's warnings, the system's
  programs replaced, the settings, the watchdog. No implementation yet.

## The design of stage 1, if the robot is approved

- Hidden Catch2 test cases (`[.robot][situation]`) in `kalahari-tests`, one for each
  situation: no book; a chapter with the cursor in the text; a selection across two
  paragraphs; a book open read-only (as `tests/gui/test_read_only_book.cpp`); the cursor in a
  table; a selected image; a text file outside the book; a copy of `examples/ExampleNovel`.
  ctest does not run hidden test cases, so the usual run of the tests stays as it is.
- Each situation builds a real `MainWindow` (as `tests/gui/test_book_windows.cpp`) and
  triggers the `QAction` of every command, as the menus, the toolbars and the keys do, in the
  order of the menus. The window and its book are built again, from an untouched copy of the
  book and of the settings, when a step changed them.
- The checks of each step: the action is greyed exactly when its command is; a greyed command
  says why; a crash or a hang (the records and a watchdog thread); the longest time the event
  loop did not run (slow); errors in the log; Qt's warnings; windows of Qt or of the system
  (not `KalahariDialog`; `QFileDialog` aside); windows that do not close; exceptions
  (`CommandRegistry::setErrorHandler`); nothing changes when a question is cancelled; one Undo
  brings the text (KML) back; a toggle comes back. What the program asks the system to open
  is recorded, and nothing opens (`gui/utils/file_location.h`, `core/file_trash.h`,
  `QDesktopServices::setUrlHandler`).
- A 20 ms timer answers modal windows with Cancel, measures the stalls and feeds the
  watchdog; the windows a step leaves open are closed after it; every window gets a
  screenshot, and every step does in the full mode.
- CI: steps that only report, after the tests, in `ci-linux.yml` (Release, and the job with
  the sanitizers) and `ci-windows.yml`; the artifact `robot-report-<system>`; the summary in
  `GITHUB_STEP_SUMMARY`; the full run on a schedule.
- MSVC builds with `/W4 /WX`: the code must not hide names or narrow numbers implicitly
  (main was red on 2026-11-16 for C4456).

## The run in a browser (the user's requirement, 2026-11-16)

The user wants to see the course of a test in an HTML interface. `index.html` in the report
folder becomes that interface, in Polish:

- a summary: the situations, the commands run and greyed, the time, the state of the run
  (running, finished, ended by a crash), the findings;
- the steps in the order they ran: the command with its name in the menu, the time, the
  windows it showed with their screenshots, the checks, the log; steps with findings in colour;
- a player: the screenshots of the steps one after another, with Back, Next and Pause;
- filters: findings only, a check, a situation, the name of a command;
- the findings, with links to their steps.

How: the page and the screenshots are one folder that a browser opens from the disk
(`file://`), without a server. The robot writes the data of the page (`data.js`, loaded by a
script tag) after each step; while the run lasts, the page reloads itself every few seconds and
follows the current step. The robot runs the program in Polish (the tests have the
translation: `tests/CMakeLists.txt`, line 819), so the names and the screenshots are those the
user sees. On GitHub the folder is the artifact `robot-report-<system>`, a ZIP; `summary.md`
goes to the summary of the run. A fixed address (GitHub Pages) would make the page public: the
user decides. Not tried yet: the reload and the script of `data.js` under `file://`.

The present `robot_report.cpp` writes a static `index.html` after each situation; the
interface replaces it.
