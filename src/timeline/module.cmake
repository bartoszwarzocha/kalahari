# Timelines of the Workshop: a module of their own
#
# The program knows it only by its provider of the editor "timeline" of the window kinds
# (core::IWindowEditorProvider) and by its chronology of the Book Plan
# (gui::IPlanChronologyProvider), which registerTimelineModule() registers; nothing else of
# the program names its classes.

# The model: the time of the story, the timeline, its file and its axis (Qt Core only)
set(KALAHARI_TIMELINE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/story_time.cpp        # Dates and days, their words and file
    ${CMAKE_CURRENT_LIST_DIR}/story_time_parse.cpp  # The time the writer types, read back
    ${CMAKE_CURRENT_LIST_DIR}/timeline.cpp          # Threads, events, the order, the .ktl file
    ${CMAKE_CURRENT_LIST_DIR}/time_axis.cpp         # The axis, its long gaps shortened
)

# The editor in a tab of the window (Qt Widgets)
set(KALAHARI_TIMELINE_GUI_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/timeline_look.cpp       # The settings and the colors of the theme
    ${CMAKE_CURRENT_LIST_DIR}/timeline_index.cpp      # What it knows of the book, its words
    ${CMAKE_CURRENT_LIST_DIR}/timeline_document.cpp   # The timeline open on its tab, Undo
    ${CMAKE_CURRENT_LIST_DIR}/timeline_scene.cpp      # Where the axis draws each event
    ${CMAKE_CURRENT_LIST_DIR}/timeline_edits.cpp      # The changes its views share
    ${CMAKE_CURRENT_LIST_DIR}/timeline_axis_view.cpp        # The axis: coordinates, zoom
    ${CMAKE_CURRENT_LIST_DIR}/timeline_axis_view_paint.cpp  # The axis: its painting
    ${CMAKE_CURRENT_LIST_DIR}/timeline_axis_view_input.cpp  # The axis: keys, mouse, menus
    ${CMAKE_CURRENT_LIST_DIR}/timeline_list.cpp       # The timeline as a table
    ${CMAKE_CURRENT_LIST_DIR}/timeline_book_view.cpp  # The time and the book
    ${CMAKE_CURRENT_LIST_DIR}/timeline_dialogs.cpp    # The windows "Event" and "Timeline"
    ${CMAKE_CURRENT_LIST_DIR}/timeline_editor.cpp     # The tab: its bar, views, messages
    ${CMAKE_CURRENT_LIST_DIR}/timeline_settings_page.cpp  # Settings > Timelines
    ${CMAKE_CURRENT_LIST_DIR}/plan_chronology_layout.cpp   # The chronology of the Book Plan
    ${CMAKE_CURRENT_LIST_DIR}/plan_chronology_grid.cpp     # Its cards: painting, keys
    ${CMAKE_CURRENT_LIST_DIR}/plan_chronology_dialogs.cpp  # Its windows: a card, the periods
    ${CMAKE_CURRENT_LIST_DIR}/plan_chronology_view.cpp     # Its options and commands
    ${CMAKE_CURRENT_LIST_DIR}/timeline_module.cpp     # The provider of the editor "timeline"
    # Headers with Q_OBJECT (for AUTOMOC)
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/timeline/timeline_document.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/timeline/timeline_axis_view.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/timeline/timeline_list.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/timeline/timeline_book_view.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/timeline/timeline_dialogs.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/timeline/timeline_editor.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/timeline/timeline_settings_page.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/timeline/plan_chronology_view.h
)
