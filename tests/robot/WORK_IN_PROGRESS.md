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
