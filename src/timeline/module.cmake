# Timelines of the Workshop: a module of their own
#
# The program knows it only by its provider of the editor "timeline" of the window kinds
# (core::IWindowEditorProvider), which registerTimelineModule() registers; nothing else of
# the program names its classes.

# The model: the time of the story, the timeline, its file and its axis (Qt Core only)
set(KALAHARI_TIMELINE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/story_time.cpp        # Dates and days, their words and file
    ${CMAKE_CURRENT_LIST_DIR}/story_time_parse.cpp  # The time the writer types, read back
    ${CMAKE_CURRENT_LIST_DIR}/timeline.cpp          # Threads, events, the order, the .ktl file
    ${CMAKE_CURRENT_LIST_DIR}/time_axis.cpp         # The axis, its long gaps shortened
)

# The editor in a tab of the window
set(KALAHARI_TIMELINE_GUI_SOURCES
)
