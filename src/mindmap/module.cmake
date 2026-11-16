# Mind maps of the Workshop: a module of their own
#
# The program knows it only by its provider of the editor "mindmap" of the window kinds
# (core::IWindowEditorProvider), which registerMindMapModule() registers; nothing else of
# the program names its classes.

# The model: the map, its file and its layouts (Qt Core and Gui only)
set(KALAHARI_MINDMAP_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/mind_map.cpp         # The tree of nodes, loose nodes, connections
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_file.cpp    # Its .kmap file and its outlines
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_layout.cpp  # Its layouts: both sides, right, down, free
)

# The editor in a tab of the window (Qt Widgets)
set(KALAHARI_MINDMAP_GUI_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_look.cpp        # The settings and the colors of the theme
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_scene.cpp       # Where each node, line and badge is drawn
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_document.cpp    # The map open on its tab, Undo and Redo
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_menus.cpp       # What the map and the list share
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_view.cpp        # The map: coordinates, zoom, the text
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_view_paint.cpp  # The map: its painting
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_view_input.cpp  # The map: the keys, the mouse, the menu
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_list.cpp        # The map as a list with indents
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_editor.cpp      # The tab: its bar, the map, the list
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_settings_page.cpp  # Settings > Mind Maps
    ${CMAKE_CURRENT_LIST_DIR}/mind_map_module.cpp      # What the program knows of the module
    # Headers with Q_OBJECT (for AUTOMOC)
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/mindmap/mind_map_document.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/mindmap/mind_map_view.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/mindmap/mind_map_list.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/mindmap/mind_map_editor.h
    ${CMAKE_CURRENT_LIST_DIR}/../../include/kalahari/mindmap/mind_map_settings_page.h
)
