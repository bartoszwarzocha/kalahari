# Kalahari Core Library (shared with Python bindings)

# Core library source files
set(KALAHARI_CORE_SOURCES
    core/logger.cpp
    core/settings_manager.cpp
    core/settings_schema.cpp         # Default values of all settings
    # OpenSpec #00042: Custom Text Editor
    editor/editor_types.cpp
    editor/kml_serializer.cpp  # Phase 11.2: QTextDocument to KML serializer
    editor/kml_format_registry.cpp  # OpenSpec #00043: Shared KML format registry
    editor/annotation.cpp  # Comments, TODOs and notes anchored to the text
    editor/text_link.cpp   # Links in the text: the address on its characters
    editor/paragraph_styles.cpp  # Paragraph styles of a book, the objects of its text
    editor/paragraph_format.cpp  # A paragraph's own formatting: indents, spacing, breaks
    editor/character_format.cpp  # The character formatting of Format > Font, Change Case
    editor/text_lists.cpp        # Bulleted and numbered lists
    editor/text_tables.cpp       # Tables: a table of the document, its cells of paragraphs
    editor/text_images.cpp       # Images: a picture that is a character of its line
    editor/text_captions.cpp     # Numbered captions, cross-references and the list of figures
    editor/text_fields.cpp       # Fields: the book's title, the page, the words, the date
    editor/text_skeleton.cpp     # The skeleton of a text for publishers: headings of points, hints
    editor/text_footnotes.cpp    # Footnotes and endnotes: the number in the text, the note inside it
    editor/image_store.cpp       # Images: the files of a book, their sizes and pictures
    editor/table_editing.cpp     # Tables: rows, columns, cells, moving, sorting, conversions
    editor/text_sorting.cpp      # Sorting rows and paragraphs: text, numbers, dates
    editor/kml_document_model.cpp  # KML reader (the clipboard reads the lists of KML)
    editor/clipboard_handler.cpp       # Phase 4.13-4.16: Clipboard operations
    editor/editor_appearance.cpp       # Phase 5: Editor appearance configuration
    editor/book_export.cpp             # Export of a book: the texts read as the editor reads them
    editor/docx_writer.cpp             # Export of a book to a Word document (DOCX)
    editor/markdown_writer.cpp         # Export of a book to Markdown (GitHub Flavored)
    editor/book_import.cpp             # Import of a file into a book: texts, parts, kinds, styles
    editor/auto_correct.cpp            # Autocorrect: the changes of the text as it is typed
    editor/book_import_kml.cpp         # Import of a file into a book: the KML of a text, cleanups
    editor/docx_reader.cpp             # Import of a Word document (DOCX)
    editor/markdown_reader.cpp         # Import of Markdown
    editor/text_reader.cpp             # Import of plain text: encodings, paragraphs, titles
    editor/epub_writer.cpp             # Export of a book to an EPUB 3 e-book
    core/database_schema_manager.cpp
    core/project_lock.cpp
    core/backup_manager.cpp
    core/project_database.cpp
    core/python_interpreter.cpp
    core/cmd_line_parser.cpp
    core/plugin_manager.cpp
    core/plugin_manifest.cpp
    core/plugin_archive.cpp
    core/plugin_signature.cpp   # OpenSpec #00038: Plugin signature verification (Ed25519)
    core/trusted_keys.cpp       # OpenSpec #00038: Trusted publisher key management
    core/plugin_host.cpp        # What the program gives the plugins (the book, messages)
    core/plugin_api.cpp         # The kalahari_api module of the plugins (its contents)
    core/plugin_settings.cpp    # The settings plugins declare and their values
    core/python_plugin.cpp      # A Python plugin behind the extension points
    core/extension_points.cpp
    core/window_editor_provider.cpp # The editors of the window kinds that modules bring
    core/event_bus.cpp
    core/busy_pulse.cpp       # The beats of long work, for a spinner that waits for it
    core/chapter_document.cpp
    core/chapter_scenes.cpp     # The scenes of a text: what their breaks say, moved, added, deleted
    core/chapter_versions.cpp   # Versions of the texts of the chapter files
    core/text_diff.cpp          # Differences between two texts, word by word
    core/text_statistics.cpp    # Word/character counting shared by the whole application
    core/writing_activity.cpp   # The record of writing in 5-minute slots, and its recorder
    core/writing_statistics.cpp # The statistics of writing: writing days, days in a row, hours
    core/writing_goals.cpp      # The goals of a book, its deadlines and their reckoning
    core/writing_awards.cpp     # The badges of writing and the writer's challenges
    core/writing_journal.cpp    # The record of writing in a database; the writer's journal
    core/text_file.cpp          # Plain text files in their own encoding and line ends
    core/standalone_file.cpp    # Chapters and text files opened outside a book project
    core/file_trash.cpp         # Files to the Trash of the system
    core/book_type_package.cpp  # Book type packages: kinds, structure and styles as data
    core/package_json.cpp       # The parts of booktype.json that its readers share
    core/card_package.cpp       # The fields of the kinds of cards and the relations of a package
    core/card_field_set.cpp     # The fields of the cards as a book has them
    core/card_names.cpp         # The names of the cards in the book's dictionary
    core/card_name_index.cpp    # Which card a name at a place of a text is
    core/book_type_registry.cpp # Book type packages loaded from folders
    core/kind_words.cpp         # Forms of the names of kinds in the program's sentences
    core/affix_expander.cpp     # The forms a Hunspell dictionary gives its words
    core/name_declension.cpp    # The forms of names in the grammatical cases
    core/name_declension_polish.cpp # The Polish models of declension
    core/book_dictionary.cpp    # The dictionary of a book: names, rules of spelling, words
    core/book_dictionary_rules.cpp # Its words of a text, the places against its rules
    core/book_dictionary_file.cpp  # Its file
    core/material_kinds.cpp     # The kinds of materials, and the kind a file or an address gives
    core/cover_material.cpp     # The materials of the cover: parts, places, states and lengths
    core/publishing_texts.cpp   # The texts for publishers: data, templates, lengths, the set
    core/volume_materials.cpp   # The cover and the texts for publishers of each volume
    core/card_data.cpp          # The cards of the Workshop: the values of their fields, colors
    core/source_description.cpp # The bibliographic description of a source, its bookmarks
    core/bibliography.cpp       # The entries of a bibliography and the references in the text
    core/bibliography_records.cpp  # The descriptions pasted as BibTeX and RIS
    core/source_catalogs.cpp    # The catalogs on the web of an ISBN and a DOI, their answers
    core/link_roles.cpp         # The roles of links, and the paragraph a link names
    core/book_project.cpp       # Book project as its .klh file saves it
    core/clipping.cpp           # Clippings: their titles and the places they were cut from
    core/old_book_conversion.cpp # Books of the first .klh format, converted to this version
    core/unicode_paths.cpp      # Paths with any characters, as UTF-8 text, and their ZIP archives
    core/utils/svg_converter.cpp
    core/theme.cpp
    # NOTE: icon_downloader.cpp, theme_manager.cpp, icon_registry.cpp moved to kalahari executable
    # (avoid Q_OBJECT DLL export issues with MOC-generated symbols)
)

# Create core library as SHARED for Python module compatibility
# SHARED library ensures Python module can link correctly across all platforms
add_library(kalahari_core SHARED ${KALAHARI_CORE_SOURCES})

# Precompiled headers — activate the function defined in cmake/PrecompiledHeaders.cmake.
# Covers stdlib + spdlog + nlohmann_json. Qt6 headers are intentionally left out of the
# PCH set in Sub-Project D1 (they carry higher risk of MOC macro collisions and will be
# tuned in a later phase). Guarded with if(COMMAND ...) so the build still works if
# PrecompiledHeaders.cmake is ever removed or renamed.
if(COMMAND kalahari_add_pch)
    kalahari_add_pch(kalahari_core)
endif()

# Core library properties
set_target_properties(kalahari_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)

# MSVC Release: Disable LTCG (Link-Time Code Generation) to avoid corrupted .obj files
# Issue: vcpkg Python3 + LTCG produces "unrecognized file format" errors in Release builds
if(MSVC)
    target_compile_options(kalahari_core PRIVATE
        $<$<CONFIG:Release>:/GL->  # Disable whole program optimization
    )
    target_link_options(kalahari_core PRIVATE
        $<$<CONFIG:Release>:/LTCG:OFF>  # Disable link-time code generation
    )
endif()

# On Windows with multi-config generators (Visual Studio), explicitly set output dirs
# This is needed because CMAKE_<CONFIG>_POSTFIX doesn't affect import library placement
if(MSVC AND CMAKE_CONFIGURATION_TYPES)
    set_target_properties(kalahari_core PROPERTIES
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    # For multi-config, we need to handle each config
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER ${config} config_upper)
        set_target_properties(kalahari_core PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib"
            RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib"
            ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib"
        )
    endforeach()
endif()

# Core library include directories
target_include_directories(kalahari_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

# Core library dependencies (includes Qt6::Core for QSize/QPoint in SettingsManager)
# Task #00020: Qt6::Network for IconDownloader, Qt6::Xml for SvgConverter
# Task #00021: Qt6::Svg for IconRegistry (QSvgRenderer), Qt6::Gui for QPixmap/QIcon
# OpenSpec #00041: Qt6::Sql for ProjectDatabase (SQLite)
target_link_libraries(kalahari_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Svg
    Qt6::Network
    Qt6::Xml
    Qt6::Sql
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    libzip::zip
    Python3::Python
    pybind11::embed
    ${CMAKE_DL_LIBS}  # dladdr (Python home detection on Linux)
    OpenSSL::SSL      # OpenSpec #00038: Ed25519 signature verification
    OpenSSL::Crypto   # OpenSpec #00038: SHA256 hashing
)

# Kalahari executable target

# Executable source files
set(KALAHARI_SOURCES
    main.cpp
    core/plugin_api_embedded.cpp  # kalahari_api built into the program (not into kalahari_core)
    core/utils/icon_downloader.cpp  # IconDownloader moved to executable (avoid moc DLL export issues)
    core/theme_manager.cpp  # ThemeManager moved to executable (avoid moc DLL export issues)
    core/icon_registry.cpp  # IconRegistry moved to executable (Task #00025: Q_OBJECT for theme signals)
    core/art_provider.cpp   # ArtProvider - Central Visual Resource Manager (OpenSpec #00026)
    core/log_panel_sink.cpp # LogPanelSink - spdlog Qt sink (OpenSpec #00024)
    core/resource_paths.cpp # ResourcePaths - Platform-aware resource locator (OpenSpec #00029)
    core/recent_books_manager.cpp # RecentBooksManager - Recent files management (OpenSpec #00030)
    core/project_manager.cpp # ProjectManager - Project lifecycle management (OpenSpec #00033)
    core/card_relations.cpp # The relations between the cards of a book (uses ProjectManager)
    core/volume_copies.cpp  # The volumes of a series as books, and books as volumes (card_relations)
    core/project_manager_card_fields.cpp # ProjectManager: the fields of the cards of the book
    core/project_manager_publishing.cpp # ProjectManager: the texts for publishers of the book
    core/project_manager_series.cpp     # ProjectManager: a series, several books of one project
    core/package_card.cpp   # Cards of the packages a book uses: names and icons (IconRegistry)
    gui/kalahari_style.cpp  # KalahariStyle - Dynamic icon sizing (OpenSpec #00026)
    gui/widget_wheel.cpp  # The mouse wheel changes only the field with the focus
    gui/command.cpp
    gui/command_registry.cpp
    gui/dialogs/about_dialog.cpp
    gui/dialogs/icon_downloader_dialog.cpp
    gui/dialogs/item_templates.cpp  # TemplateRegistry - Project/file templates (OpenSpec #00033)
    gui/dialogs/new_item_dialog.cpp  # NewItemDialog - New project/file dialog (OpenSpec #00033)
    gui/dialogs/toolbar_manager_dialog.cpp  # ToolbarManagerDialog - Visual Studio-style toolbar customization (OpenSpec #00031)
    gui/dialogs/kalahari_dialog.cpp  # KalahariDialog - base of the program's own dialogs
    gui/dialogs/message_dialog.cpp  # MessageDialog, TextInputDialog - own messages, questions, typed texts
    gui/dialogs/open_book_dialog.cpp  # Opening a Book: in a new window or in this one
    gui/dialogs/grammar_setup_dialog.cpp  # GrammarSetupDialog - the window of the grammar preparation
    gui/dialogs/progress_dialog.cpp  # ProgressDialog - own window for the progress of a long task
    gui/dialogs/color_dialog.cpp  # ColorDialog - own window for choosing a color
    gui/dialogs/color_palettes.cpp  # The palettes of the color window
    gui/dialogs/add_to_book_dialog.cpp  # AddToBookDialog - a file as a new element of the book
    gui/dialogs/new_file_dialog.cpp  # NewFileDialog - New File: a new element of the open book
    gui/dialogs/new_volume_dialog.cpp  # NewVolumeDialog - Book > New Volume...: a new book of a series
    gui/dialogs/take_out_volume_dialog.cpp  # TakeOutVolumeDialog - a volume as a book of its own
    gui/dialogs/join_book_dialog.cpp  # JoinBookDialog - a book joined as a volume
    gui/dialogs/publishing_choice.cpp  # PublishingChoice - the part and the template of a new text for publishers
    gui/dialogs/publishing_templates_dialog.cpp  # PublishingTemplatesDialog - the templates of the texts for publishers
    gui/dialogs/move_element_dialog.cpp  # MoveElementDialog - a new place in the book for an element
    gui/dialogs/delete_dialogs.cpp  # DeleteFromBookDialog, DeleteFilesDialog - what goes, and its files
    gui/dialogs/dialog_parts.cpp  # Parts of the own dialogs: a list as high as its rows, a note, Copy
    gui/dialogs/archive_dialogs.cpp  # ExportArchiveDialog, ImportArchiveDialog - one window each
    gui/dialogs/book_output_dialog.cpp  # BookOutputDialog - the tabs of the texts and the layout of the export and the print
    gui/dialogs/print_book_dialog.cpp  # PrintBookDialog - File > Print
    gui/dialogs/export_book_dialog.cpp  # ExportBookDialog - File > Export > DOCX, PDF, Markdown and EPUB
    gui/dialogs/import_book_dialog.cpp  # ImportBookDialog - File > Import > DOCX, Markdown and Plain Text
    gui/dialogs/file_preview_dialog.cpp  # FilePreviewDialog - the text of a file no element has
    gui/dialogs/user_project_dialog.cpp  # UserProjectDialog - the kinds of elements of a user project
    gui/section_words.cpp  # SectionWords - the front, main and back part of a book in sentences
    gui/kind_names.cpp     # The names of kinds of elements in sentences, in the program's language
    gui/avatar.cpp         # Avatar - the parts of a portrait put together from parts
    gui/avatar_art.cpp     # AvatarArt - the drawings of the parts (resources/portraits)
    gui/avatar_palettes.cpp  # AvatarPalettes - the colors of the portraits and their names
    gui/avatar_words.cpp   # The look of a portrait in words
    gui/name_labels.cpp    # The cases and kinds of the names of the book dictionary, in the program's language
    gui/cover_names.cpp    # The materials of the cover in the program's words
    gui/publishing_names.cpp # The texts for publishers in the program's words
    gui/publishing_set.cpp   # The set for a publisher in the program's words
    gui/volume_names.cpp     # The volumes of the cover and the texts for publishers in words
    gui/source_names.cpp   # The sources in the program's words
    gui/pdf_files.cpp      # PDF files: whether the program shows them, their first page and data
    gui/quick_insert_items.cpp  # What the list of quick insert offers: cards, fragments, characters
    gui/read_only_book.cpp # What the program says of a book open read-only
    gui/dialogs/rename_element_dialog.cpp  # RenameElementDialog - new name of an element in the Navigator
    gui/dialogs/special_character_dialog.cpp  # SpecialCharacterDialog - Insert > Special Character...
    gui/dialogs/list_dialogs.cpp           # Bullets and Numbering, Set Numbering Value
    gui/dialogs/font_dialog.cpp            # Format > Font...
    gui/dialogs/paragraph_dialog.cpp       # Format > Paragraph...
    gui/dialogs/hyperlink_dialog.cpp       # Insert > Link...
    gui/dialogs/insert_table_dialog.cpp    # Insert > Table...
    gui/dialogs/caption_dialogs.cpp        # Insert Caption, Caption Numbering, Insert Cross-Reference
    gui/dialogs/footnote_settings_dialog.cpp  # Format > Footnote Layout...: the numbering of the notes
    gui/dialogs/field_dialogs.cpp          # Insert > Date && Time... and Insert > Field...
    gui/dialogs/edit_dialogs.cpp           # Edit > Paste Special... and Edit > Go To...
    gui/dialogs/table_dialogs.cpp          # The windows of the Table menu
    gui/dialogs/image_properties_dialog.cpp  # Image > Image Properties...
    gui/widgets/character_grid.cpp  # CharacterGrid - characters in cells, in the font of the text
    gui/widgets/quick_character_palette.cpp  # QuickCharacterPalette - Insert > Quick Character
    gui/dialogs/character_dialogs.cpp  # The keys of a character, a replacement of auto-correct
    gui/dialogs/link_dialog.cpp  # LinkDialog - what a material is about, or the materials of an element
    gui/dialogs/relation_dialog.cpp  # RelationDialog - a relation of a card with another card
    gui/dialogs/portrait_dialog.cpp  # PortraitDialog - a portrait of a card put together from parts
    gui/dialogs/portrait_palettes_dialog.cpp  # PortraitPalettesDialog - the palettes of the portraits
    gui/dialogs/paragraph_card_dialog.cpp  # ParagraphCardDialog - a paragraph linked with a card
    gui/dialogs/card_fields_dialog.cpp  # CardFieldsDialog - the fields of the cards of each kind
    gui/dialogs/card_field_dialogs.cpp  # CardFieldDialog, CardChoicesDialog - a field of the writer, its list
    gui/material_file.cpp  # MaterialFile - the file of a material: its name, format, size and picture
    gui/material_source.cpp  # MaterialSource - a file, a web page, a picture on the web or none
    gui/material_downloader.cpp  # MaterialDownloader - the title of a page, the copy of a picture
    gui/source_lookup.cpp  # SourceLookup - the description of a source from a catalog on the web
    gui/link_names.cpp  # The titles, icons and places of the links of a material
    gui/material_list.cpp  # MaterialList - the lists of the materials: their kinds, columns and rows
    gui/material_list_clippings.cpp  # MaterialList - the columns of the clippings
    gui/material_list_publishing.cpp  # MaterialList - the columns of the texts for publishers
    gui/clipping_texts.cpp  # What the program says of a clipping
    gui/card_list.cpp  # CardList - the lists of the cards: their columns and rows
    gui/version_texts.cpp  # What the program says of a version of a text
    gui/versions_coordinator.cpp  # VersionsCoordinator - Save Version and Version History
    gui/dialogs/version_name_dialog.cpp  # VersionNameDialog - the description of a version
    gui/dialogs/element_choice_dialog.cpp  # ElementChoiceDialog - choose an element of the book
    gui/dialogs/quick_insert_item_dialog.cpp  # QuickInsertItemDialog - a fragment or a character of quick insert
    gui/dialogs/name_dialog.cpp  # NameDialog - a name of the book dictionary with its forms
    gui/dialogs/rule_dialog.cpp  # RuleDialog - a spelling rule of the book dictionary
    gui/dialogs/deadline_dialog.cpp  # DeadlineDialog - a deadline of the book
    gui/dialogs/new_quote_dialog.cpp  # NewQuoteDialog - a quote from a selection of a PDF
    gui/dialogs/source_lookup_dialog.cpp  # SourceLookupDialog - From the Web by ISBN or DOI...
    gui/dialogs/challenge_dialog.cpp  # ChallengeDialog - Add Challenge
    gui/dialogs/session_summary_dialog.cpp  # SessionSummaryDialog - the summary of the session
    gui/dialogs/version_history_dialog.cpp  # VersionHistoryDialog - the versions of a text
    gui/dialogs/clipping_dialogs.cpp  # Cut to Clippings, Insert > Clipping: the windows and the list
    gui/kalahari_syntax_highlighter.cpp
    gui/icon_registrar.cpp  # IconRegistrar - Icon registration extracted from MainWindow (OpenSpec #00038)
    gui/command_registrar.cpp  # CommandRegistrar - Command registration extracted from MainWindow (OpenSpec #00038)
    gui/diagnostic_controller.cpp  # DiagnosticController - Diagnostic/dev mode extracted from MainWindow (OpenSpec #00038)
    gui/diagnostic_menus.cpp  # The items of the Diagnostics and Dev Tools menus
    gui/dock_coordinator.cpp  # DockCoordinator - Dock/panel management extracted from MainWindow (OpenSpec #00038 Phase 4)
    gui/panel_tabs.cpp  # The program's own tabs of the panels docked one over another
    gui/settings_coordinator.cpp  # SettingsCoordinator - Settings management extracted from MainWindow (OpenSpec #00038 Phase 5)
    gui/navigator_coordinator.cpp  # NavigatorCoordinator - Navigator handlers extracted from MainWindow (OpenSpec #00038 Phase 6)
    gui/navigator_coordinator_series.cpp  # The volumes of a series: new, in order, deleted
    gui/document_coordinator.cpp  # DocumentCoordinator - Document operations extracted from MainWindow (OpenSpec #00038 Phase 7)
    gui/document_windows.cpp  # The texts of the book window in its tabs and in windows of their own
    gui/annotations_coordinator.cpp  # AnnotationsCoordinator - annotation commands and the Annotations panel
    gui/paragraph_marks_coordinator.cpp  # ParagraphMarksCoordinator - paperclips of linked paragraphs
    gui/paragraph_marks_clippings.cpp  # ParagraphMarksCoordinator - scissors of the clippings
    gui/writing_stats_coordinator.cpp  # WritingStatsCoordinator - the statistics bar and goals
    gui/writing_awards_tracker.cpp  # WritingAwardsTracker - the badges and challenges won
    gui/writing_stats_model.cpp  # WritingStatsModel - the statistics of writing shown
    gui/material_list_coordinator.cpp  # MaterialListCoordinator - the lists of the materials on tabs
    gui/plot_board.cpp  # PlotBoard - what the plot board shows of the book, where its cards move
    gui/plot_board_titles.cpp  # PlotBoard - the numbers in the titles after a move
    gui/plot_board_coordinator.cpp  # PlotBoardCoordinator - the plot board on its tab, its changes
    gui/plot_board_coordinator_arrange.cpp  # PlotBoardCoordinator - cards moved, added, cut
    gui/plot_board_coordinator_plan.cpp  # PlotBoardCoordinator - the scenes moved, the plans
    gui/plot_board_coordinator_scenes.cpp  # PlotBoardCoordinator - the scenes changed
    gui/plot_outline_model.cpp  # PlotOutlineModel - the outline: the texts of the book in a table
    gui/edit_coordinator.cpp  # EditCoordinator - clipboard and undo on the element with the keys
    gui/paragraph_style_menu.cpp  # ParagraphStyleMenu - Format > Text Style shows the text's styles
    gui/book_styles.cpp  # The paragraph styles of the open book: its type's and its own
    gui/style_draft.cpp  # StyleDraft - a style as the window of a style changes it
    gui/style_coordinator.cpp  # StyleCoordinator - new, changed, deleted styles of the book
    gui/caption_settings.cpp  # The settings of new images and of the captions of new books
    gui/book_texts.cpp  # The texts of the book in reading order, the saved text of one not open
    gui/caption_coordinator.cpp  # CaptionCoordinator - the numbered captions of the book
    gui/book_export_coordinator.cpp  # BookExportCoordinator - the export of the book to DOCX and Markdown
    gui/printer_driver.cpp  # The settings of the driver of a printer (Windows)
    gui/plugin_coordinator.cpp  # The commands and panels of the plugins in a book window
    gui/book_import_texts.cpp  # The texts of an imported file added to the book, and Undo import
    gui/book_import_plan.cpp  # The plan of an import: the files read, the texts, their kinds and styles
    gui/book_import_coordinator.cpp  # BookImportCoordinator - the import of DOCX, Markdown and plain text
    gui/footnote_coordinator.cpp  # FootnoteCoordinator - Insert > Footnote, Endnote and the frame of a note
    gui/image_filter_coordinator.cpp  # ImageFilterCoordinator - one filter for every image of the book
    gui/book_search_coordinator.cpp  # BookSearchCoordinator - Find in Book and Replace All in Book
    gui/chapter_break_coordinator.cpp  # ChapterBreakCoordinator - Book > Chapter Break with Undo
    gui/clippings_coordinator.cpp  # ClippingsCoordinator - Cut to Clippings and Insert > Clipping
    gui/clippings_coordinator_steps.cpp  # ClippingsCoordinator - the undo of the texts, the tabs
    gui/cards_in_text_coordinator.cpp  # CardsInTextCoordinator - the cards in the texts
    gui/field_coordinator.cpp  # FieldCoordinator - the fields of the open texts, Insert > Field...
    gui/own_fields.cpp  # The writer's own fields of a book and of its series
    gui/list_sets.cpp  # The sets of list formats: built-in, of the book and of the program
    gui/list_sets_menu.cpp  # ListSetsMenu - Format > Pick List Set, the arrows of Numbering and Bullets
    gui/go_to_coordinator.cpp  # GoToCoordinator - Edit > Go To..., the window of the places of the text
    gui/dialogs/style_dialog.cpp  # StyleDialog - New Style and Modify Style, in two variants
    gui/dialogs/manage_styles_dialog.cpp  # ManageStylesDialog - Format > Text Style > Manage Styles
    gui/widgets/style_tag_delegate.cpp  # The names of styles in a list, with "own" or "changed"
    gui/auto_correct_coordinator.cpp  # AutoCorrectCoordinator - auto-correct turned on and off as a whole
    gui/toolbar_keys.cpp  # ToolbarKeys - a toolbar gives the keys back after a command
    gui/spelling_coordinator.cpp  # SpellingCoordinator - spelling as you type: the editors' dictionary
    gui/grammar_coordinator.cpp  # GrammarCoordinator - grammar as you type: the editors' LanguageTool server
    gui/spelling_panel_coordinator.cpp  # SpellingPanelCoordinator - the Spelling and Grammar panel and the text in front
    gui/book_dictionary_coordinator.cpp  # BookDictionaryCoordinator - the book dictionary on its tab and its windows
    gui/card_names_coordinator.cpp  # CardNamesCoordinator - the names of the cards in the book's dictionary
    gui/repetitions_coordinator.cpp  # RepetitionsCoordinator - the Repetitions panel, the marks in the text, Next Repetition
    gui/grammar_components.cpp  # GrammarComponents - Java and LanguageTool on this computer
    gui/language_tool_server.cpp  # LanguageToolServer - the LanguageTool server the program starts
    gui/grammar_setup.cpp  # GrammarSetup - Java and LanguageTool downloaded and prepared
    gui/main_window.cpp
    gui/menu_builder.cpp
    gui/program_instance.cpp  # ProgramInstance - a later start hands its request to the program that runs
    gui/recent_books_menu.cpp
    gui/settings_dialog.cpp
    gui/settings/settings_page.cpp
    gui/settings/general_pages.cpp
    gui/settings/spelling_page.cpp
    gui/settings/grammar_page.cpp
    gui/settings/appearance_pages.cpp
    gui/settings/editor_pages.cpp
    gui/settings/images_page.cpp
    gui/settings/tables_page.cpp
    gui/settings/auto_correct_page.cpp  # Settings > Editor > Auto-correct
    gui/settings/quick_insert_page.cpp  # Settings > Editor > Quick Insert
    gui/settings/repetitions_page.cpp  # Settings > Editor > Repetitions
    gui/settings/versions_page.cpp  # Settings > Editor > Versions
    gui/settings/clippings_page.cpp  # Settings > Editor > Clippings
    gui/settings/lists_page.cpp  # Settings > Editor > Lists
    gui/settings/annotations_page.cpp
    gui/settings/navigator_page.cpp  # Settings > Navigator
    gui/settings/cards_page.cpp  # Settings > Cards
    gui/settings/materials_page.cpp  # Settings > Materials
    gui/settings/plot_board_page.cpp  # Settings > Plot Board
    gui/settings/network_page.cpp  # Settings > Network > Materials
    gui/settings/folders_page.cpp
    gui/settings/import_page.cpp
    gui/settings/shortcuts_page.cpp  # Settings > Keyboard Shortcuts
    gui/settings/stats_pages.cpp  # Settings > Statistics and Goals
    gui/shortcut_rules.cpp  # Which keys a command can have
    gui/shortcut_settings.cpp  # The user's keys in the settings and in the files
    gui/special_characters.cpp  # The characters of Insert > Special Character, by group, with names
    gui/special_character_choices.cpp  # The favorite characters and those inserted last
    gui/character_commands.cpp  # The commands of the characters given keys
    gui/auto_correct_replacements.cpp  # The replacement table of auto-correct in the settings
    gui/special_character_coordinator.cpp  # SpecialCharacterCoordinator - Insert > Special Character...
    gui/widget_keys.cpp  # Qt's widgets have the keys of Windows, also on Linux
    gui/widgets/color_config_widget.cpp
    gui/widgets/path_label.cpp  # A file path that wraps also inside a long name
    gui/widgets/length_spin_box.cpp
    gui/widgets/color_menu.cpp             # The submenus Font Color and Highlight Color
    gui/widgets/list_set_tile.cpp          # A tile of a set of list formats
    gui/widgets/info_bar.cpp  # InfoBar - a message across the text area, with a button
    gui/widgets/plot_board_view.cpp  # PlotBoardView - the cards of the texts of the book
    gui/widgets/plot_board_view_input.cpp  # PlotBoardView - its keys, the mouse, a card dragged
    gui/widgets/plot_board_view_layout.cpp  # PlotBoardView - where the cards are
    gui/widgets/plot_board_view_paint.cpp  # PlotBoardView - the cards painted
    gui/widgets/plot_card_editor.cpp  # PlotCardEditor - a card of the plot board edited (F2)
    gui/widgets/cover_plan.cpp  # CoverPlan - the plan of the cover with its materials in their places
    gui/widgets/publishing_set_view.cpp  # PublishingSetView - the set for a publisher as a page
    gui/widgets/wrapping_label.cpp  # A label that wraps only where its line does not fit
    gui/widgets/collapsible_section.cpp  # Fields under a header that opens and closes them
    gui/widgets/wrapping_button.cpp  # A button that wraps its text only where it does not fit
    gui/widgets/flow_layout.cpp  # A row of buttons that wraps on a narrow panel
    gui/widgets/length_meter.cpp  # LengthMeter - the length of a text against its target
    gui/widgets/segmented_layout.cpp  # Joined buttons, one under another on a narrow panel
    gui/widgets/height_for_width_scroll_area.cpp  # Rows that wrap, and scroll in a low panel
    gui/widgets/shortcut_recorder.cpp  # A field that records key presses
    gui/widgets/tool_button_columns.cpp  # Panel buttons that wrap into columns
    gui/widgets/panel_tab_bar.cpp  # The tabs of a group of panels: icons, names, rows
    gui/widgets/central_tabs.cpp  # The tabs in the middle of the window: one row or more
    gui/widgets/fitting_scroll_area.cpp  # Content that scrolls only on a small screen
    gui/widgets/standalone_info_bar.cpp  # StandaloneInfoBar widget (OpenSpec #00033 Phase F)
    gui/widgets/element_place_picker.cpp  # ElementPlacePicker - the place of an element in the book
    gui/widgets/flow_layout.cpp  # FlowLayout - items in more rows in a narrow panel
    gui/widgets/sections_field.cpp  # Fields of the sections of a book: their names or none
    gui/widgets/kind_palette.cpp  # KindPalette - the kinds of elements of all book types
    gui/widgets/folder_field.cpp  # FolderField - the path of a folder, Browse... and Restore Default
    gui/widgets/name_forms_table.cpp  # NameFormsTable - the forms of a name by case and number
    gui/widgets/card_name_box.cpp  # CardNameBox - the declension of the name of a card
    gui/toolbar_manager.cpp
    gui/widgets/status_bar_statistics.cpp  # Counts of the document in front in the status bar
    gui/widgets/stats_painter.cpp  # Colors and drawing of the statistics graphs
    gui/widgets/badge_painter.cpp  # The look of the badges and their texts
    gui/widgets/status_bar_writing.cpp  # The time of writing and the days in a row in the status bar
    gui/widgets/writing_stats_bar.cpp  # The statistics bar above the tabs of the texts
    gui/panels/annotation_card.cpp
    gui/panels/annotation_colors.cpp
    gui/panels/annotation_entry.cpp
    gui/panels/annotation_frame.cpp
    gui/panels/note_frame.cpp  # The frame a note's text is written in
    gui/panels/writing_frame.cpp  # A frame over the editor a text is written in
    gui/panels/annotations_panel.cpp
    gui/panels/assistant_panel.cpp
    gui/panels/dashboard_panel.cpp
    gui/widgets/loading_overlay.cpp       # The card of a book being opened: darkened, a spinner
    gui/panels/dashboard_layouts.cpp  # The Dashboard on a narrow or a low panel
    gui/panels/editor_panel.cpp
    gui/panels/log_panel.cpp
    gui/panels/navigator_panel.cpp
    gui/panels/navigator_panel_series.cpp
    gui/panels/properties_panel.cpp
    gui/panels/plot_fields_box.cpp   # PlotFieldsBox - the plot of a text in the Properties panel
    gui/panels/book_fields_box.cpp   # BookFieldsBox - the own fields of the book in the Properties panel
    gui/panels/footnotes_panel.cpp   # FootnotesPanel - the footnotes and endnotes of a text or the book
    gui/footnotes_panel_coordinator.cpp  # The Footnotes panel and the texts
    gui/panels/material_page.cpp  # MaterialPage, LinkedMaterialsBox - the card of a material in Properties
    gui/widgets/own_fields_box.cpp  # OwnFieldsBox - the writer's own fields of a material or a card
    gui/widgets/relations_box.cpp  # RelationsBox - the relations of a card with the other cards
    gui/widgets/card_chapters_box.cpp  # CardChaptersBox - the chapters a card is in
    gui/widgets/card_cell_delegate.cpp  # CardCellDelegate - the cells of a list of cards that F2 edits
    gui/widgets/palette_edit.cpp  # PaletteEdit - a palette of colors to change in the Settings
    gui/panels/card_page.cpp      # CardPage - the card of a character, a location or an item
    gui/panels/card_page_portraits.cpp      # The portraits of a card
    gui/panels/cursor_card_box.cpp  # CursorCardBox - the card at the cursor in the Properties panel
    gui/panels/paragraph_cards_box.cpp  # ParagraphCardsBox - the cards of the paragraph at the cursor
    gui/panels/card_panel.cpp     # CardPanel - the full card on a tab beside the chapters
    gui/panels/pitch_panel.cpp  # PitchPanel - the card of a sentence about the book
    gui/panels/publishing_set_page.cpp  # PublishingSetPage - the set for a publisher in Properties
    gui/panels/series_page.cpp  # SeriesPage - the name and the volumes of a series in Properties
    gui/panels/publishing_text_box.cpp  # PublishingTextBox - a text for publishers in Properties
    gui/panels/card_parts.cpp  # The small parts of the cards of the materials
    gui/panels/source_boxes.cpp  # The groups of the card of a source and of a quote
    gui/panels/source_description_box.cpp  # SourceDescriptionBox - the description of a source on its card
    gui/panels/preview_panel.cpp  # PreviewPanel - what the window asks of a preview of a file
    gui/panels/image_preview_panel.cpp  # ImagePreviewPanel - the preview of a material's picture on a tab
    gui/panels/material_list_panel.cpp  # MaterialListPanel - the list of a kind of materials on a tab
    gui/panels/material_list_panel_clippings.cpp  # MaterialListPanel - the commands of the clippings
    gui/panels/material_list_panel_publishing.cpp  # MaterialListPanel - the commands of the texts for publishers
    gui/panels/material_list_panel_set.cpp  # MaterialListPanel - the set for a publisher
    gui/panels/clipping_origin_box.cpp  # ClippingOriginBox - where a clipping comes from, in the Properties
    gui/panels/book_dictionary_panel.cpp  # BookDictionaryPanel - the book dictionary on a tab
    gui/panels/plot_board_panel.cpp  # PlotBoardPanel - the tab of the plot board and the outline
    gui/panels/plot_board_panel_header.cpp  # PlotBoardPanel - its switches and look from the settings
    gui/panels/paragraph_panel.cpp
    gui/panels/search_panel.cpp
    gui/panels/spelling_issue_list.cpp  # Spelling and Grammar panel: the cards, made only in view
    gui/panels/spelling_panel.cpp  # Spelling and Grammar panel: the counts, the state, the keys
    gui/panels/repetitions_panel.cpp  # Repetitions panel: the words too close, the most frequent, the keys
    gui/panels/writing_stats_panel.cpp  # Statistics panel: the views of the writing
    gui/panels/writing_goals_tab.cpp  # The tab Writing Goals and Deadlines
    gui/panels/writing_awards_tab.cpp  # The tab Challenges and Badges
    gui/panels/tags_panel.cpp       # TagsPanel - Tags panel (OpenSpec #00042 Task 7.10)
    gui/utils/access_keys.cpp  # The access keys of menu texts
    gui/utils/language_names.cpp  # Names of languages in the language of the user interface
    gui/utils/layout_utils.cpp  # OpenSpec #00038: Layout utilities (clearLayout)
    gui/utils/panel_toggle.cpp  # Show/hide toggles of the dock panels
    gui/utils/reading_time.cpp  # The reading time of a text as the program shows it
    gui/utils/setting_toggle.cpp  # Toggles that show an on/off setting
    gui/utils/program_folders.cpp  # The folders of the program and of the windows choosing files
    gui/utils/file_location.cpp  # A file or a folder shown in the file manager of the system
    gui/utils/caps_lock.cpp  # Caps Lock turned off after autocorrect changed a word typed with it
    gui/utils/document_commands.cpp  # The commands that work on the document in front
    gui/utils/distraction_free_layout.cpp  # The main window's layout for Distraction-Free writing
    gui/utils/tab_focus.cpp  # The keys go to the page in front of the tabs
    gui/utils/chapter_cursor.cpp  # A chapter opens where its cursor was
    gui/utils/qt_standard_texts.cpp  # Qt's own texts (standard buttons) for the translations
    gui/utils/core_texts.cpp  # The core library's texts for the translations
    # OpenSpec #00042: BookEditor (Q_OBJECT requires executable, not DLL)
    editor/book_editor.cpp      # Phase 3.1: BookEditor widget
    editor/annotation_cards.cpp  # The cards of the annotations under their paragraphs
    editor/book_editor_annotations.cpp
    editor/book_editor_cards.cpp
    editor/book_editor_checking.cpp
    editor/book_editor_cursor.cpp
    editor/book_editor_editing.cpp
    editor/book_editor_formatting.cpp
    editor/book_editor_lists.cpp
    editor/book_editor_links.cpp
    editor/book_editor_notes.cpp  # BookEditor: footnotes and endnotes
    editor/book_editor_images.cpp
    editor/book_editor_captions.cpp
    editor/book_editor_fields.cpp  # BookEditor: the fields of the book, the page, the words, the date
    editor/book_editor_skeleton.cpp  # BookEditor: the skeleton of a text for publishers
    editor/book_editor_places.cpp  # BookEditor: Edit > Go To..., the places of the text
    editor/book_editor_image_look.cpp
    editor/book_editor_input.cpp
    editor/book_editor_search.cpp
    editor/book_editor_tables.cpp
    editor/book_editor_table_commands.cpp
    editor/book_editor_view.cpp
    editor/find_replace_bar.cpp  # Find/Replace bar of the editor
    editor/version_comparison.cpp  # A version of a text compared with the text now
    editor/book_editor_accessible.cpp  # Task 7.16: Accessibility interface
    editor/buffer_commands.cpp  # Cursor position helpers
    editor/text_keys.cpp  # The keys of the text on each system
    editor/split_editor_panel.cpp  # Phase 5.8: Split view panel
    editor/statistics_collector.cpp  # Phase 6.10-6.13: Statistics collector
    editor/style_resolver.cpp        # Phase 6.1-6.3: Style resolver with inheritance
    editor/spell_check_service.cpp   # Phase 6.4-6.9: Spell check service with Hunspell
    editor/grammar_check_service.cpp # Phase 6.14-6.17: Grammar check service with LanguageTool
    editor/word_repetitions.cpp  # The repetitions of words: too close and most frequent
    editor/text_to_speech_service.cpp   # Phase 6.21-6.24: Text-to-speech service (optional Qt Speech)
    editor/tag_detector.cpp              # Task 7.10: Tag detector service
    editor/quick_insert.cpp              # Quick insert: the items the typed letters find
    editor/quick_insert_controller.cpp   # Quick insert: the list at the cursor of an editor
    editor/viewport_manager.cpp          # OpenSpec #00043: Viewport manager for Word/Writer-style scrolling
    editor/search_engine.cpp             # OpenSpec #00044 Task 9.4: Search engine for Find/Replace
    editor/search_forms.cpp              # The forms the search of all forms finds ("Odm")
    editor/kalahari_text_document_layout.cpp  # OpenSpec #00043 Phase 11.11: Custom layout without Qt leading gaps
    editor/kalahari_text_document_layout_notes.cpp  # The layout of the notes: numbers, under paragraphs, at the bottom of pages
    editor/kalahari_text_document_layout_floats.cpp  # The images the text flows around
    editor/formatting_marks.cpp               # The formatting marks of a paragraph (¶ · ° → ↵ ¬), drawn by the layout
    editor/text_source_adapter.cpp            # OpenSpec #00043 Phase 12.1: Text source adapters
    editor/editor_render_pipeline.cpp         # OpenSpec #00043 Phase 12.1: Unified render pipeline
    # The pages of a book use the layout of the editor above, so they are not in kalahari_core
    editor/book_pages.cpp                     # The pages of a book as the page view shows them (PDF, printer)
    editor/book_print.cpp                     # The print of a book: the pages on the sheets, their drawing
    editor/pdf_writer.cpp                     # Export of a book to a PDF document
    editor/editor_benchmark.cpp               # OpenSpec #00043 Phase 11.11: Performance benchmarks
    # Headers with Q_OBJECT (needed for AUTOMOC to generate moc files)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/utils/icon_downloader.h  # IconDownloader with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/theme_manager.h  # ThemeManager with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/icon_registry.h  # IconRegistry with Q_OBJECT (Task #00025)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/art_provider.h   # ArtProvider with Q_OBJECT (OpenSpec #00026)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/recent_books_manager.h  # RecentBooksManager with Q_OBJECT (OpenSpec #00030)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/project_manager.h  # ProjectManager with Q_OBJECT (OpenSpec #00033)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/log_panel_sink.h # LogPanelSink with Q_OBJECT (OpenSpec #00024)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/kalahari_style.h  # KalahariStyle with Q_OBJECT (OpenSpec #00026)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/recent_books_menu.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/program_instance.h  # ProgramInstance with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dock_coordinator.h  # DockCoordinator with Q_OBJECT (OpenSpec #00038 Phase 4)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/document_windows.h  # DocumentWindows and DetachedWindow with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/element_choice_dialog.h  # ElementChoiceDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panel_tabs.h  # PanelTabs with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/panel_tab_bar.h  # PanelTabBar with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/central_tabs.h  # CentralTabs with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/about_dialog.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/icon_downloader_dialog.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/toolbar_manager_dialog.h  # ToolbarManagerDialog with Q_OBJECT (OpenSpec #00031)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/new_item_dialog.h  # NewItemDialog with Q_OBJECT (OpenSpec #00033)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/kalahari_dialog.h  # KalahariDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/message_dialog.h  # MessageDialog, TextInputDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/open_book_dialog.h  # OpenBookDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/grammar_setup_dialog.h  # GrammarSetupDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/progress_dialog.h  # ProgressDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/color_dialog.h  # ColorDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/add_to_book_dialog.h  # AddToBookDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/new_file_dialog.h  # NewFileDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/new_volume_dialog.h  # NewVolumeDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/take_out_volume_dialog.h  # Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/join_book_dialog.h  # Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/publishing_choice.h  # PublishingChoice with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/publishing_templates_dialog.h  # PublishingTemplatesDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/pitch_panel.h  # PitchPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/publishing_set_page.h  # PublishingSetPage with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/series_page.h  # SeriesPage with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/publishing_text_box.h  # PublishingTextBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/length_meter.h  # LengthMeter with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/move_element_dialog.h  # MoveElementDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/delete_dialogs.h  # Delete dialogs with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/archive_dialogs.h  # Archive dialogs with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/book_output_dialog.h  # BookOutputDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/print_book_dialog.h  # PrintBookDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/export_book_dialog.h  # ExportBookDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/import_book_dialog.h  # ImportBookDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/file_preview_dialog.h  # FilePreviewDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/user_project_dialog.h  # UserProjectDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/rename_element_dialog.h  # RenameElementDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/special_character_dialog.h  # SpecialCharacterDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/list_dialogs.h  # the list windows with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/font_dialog.h  # Format > Font... with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/paragraph_dialog.h  # Format > Paragraph... with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/hyperlink_dialog.h  # Insert > Link... with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/insert_table_dialog.h  # Insert > Table... with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/caption_dialogs.h  # The windows of captions, with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/footnote_settings_dialog.h  # Footnote Settings, with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/field_dialogs.h  # The windows of the fields, with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/edit_dialogs.h  # Paste Special... and Go To..., with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/table_dialogs.h  # The windows of the Table menu, with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/image_properties_dialog.h  # Image Properties, with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/quick_character_palette.h  # QuickCharacterPalette with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/character_dialogs.h  # CharacterShortcutDialog, ReplacementDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/link_dialog.h  # LinkDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/relation_dialog.h  # RelationDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/portrait_dialog.h  # PortraitDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/portrait_palettes_dialog.h  # With Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/paragraph_card_dialog.h  # ParagraphCardDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/card_fields_dialog.h  # CardFieldsDialog, CardFieldDialog, CardChoicesDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/version_name_dialog.h  # VersionNameDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/quick_insert_item_dialog.h  # QuickInsertItemDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/name_dialog.h  # NameDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/rule_dialog.h  # RuleDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/deadline_dialog.h  # DeadlineDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/new_quote_dialog.h  # NewQuoteDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/source_lookup_dialog.h  # SourceLookupDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/challenge_dialog.h  # ChallengeDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/session_summary_dialog.h  # SessionSummaryDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/version_history_dialog.h  # VersionHistoryDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/clipping_dialogs.h  # The windows of the clippings with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/versions_coordinator.h  # VersionsCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/style_coordinator.h  # StyleCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/caption_coordinator.h  # CaptionCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/book_export_coordinator.h  # BookExportCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/plugin_coordinator.h  # PluginCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/book_import_coordinator.h  # BookImportCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/footnote_coordinator.h  # FootnoteCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/image_filter_coordinator.h  # ImageFilterCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/book_search_coordinator.h  # BookSearchCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/chapter_break_coordinator.h  # ChapterBreakCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/clippings_coordinator.h  # ClippingsCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/cards_in_text_coordinator.h  # CardsInTextCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/field_coordinator.h  # FieldCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/go_to_coordinator.h  # GoToCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/style_dialog.h  # StyleDialog, StylePreview with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/manage_styles_dialog.h  # ManageStylesDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/kalahari_syntax_highlighter.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/main_window.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/diagnostic_controller.h  # DiagnosticController with Q_OBJECT (OpenSpec #00038)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings_coordinator.h  # SettingsCoordinator with Q_OBJECT (OpenSpec #00038 Phase 5)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/navigator_coordinator.h  # NavigatorCoordinator with Q_OBJECT (OpenSpec #00038 Phase 6)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/document_coordinator.h  # DocumentCoordinator with Q_OBJECT (OpenSpec #00038 Phase 7)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/annotations_coordinator.h  # AnnotationsCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/paragraph_marks_coordinator.h  # ParagraphMarksCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/writing_stats_coordinator.h  # WritingStatsCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/writing_awards_tracker.h  # WritingAwardsTracker with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/writing_stats_model.h  # WritingStatsModel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/material_list_coordinator.h  # MaterialListCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/plot_board_coordinator.h  # PlotBoardCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/plot_outline_model.h  # PlotOutlineModel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/edit_coordinator.h  # EditCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/auto_correct_coordinator.h  # AutoCorrectCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/special_character_coordinator.h  # SpecialCharacterCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/toolbar_keys.h  # ToolbarKeys with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/spelling_coordinator.h  # SpellingCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/grammar_coordinator.h  # GrammarCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/spelling_panel_coordinator.h  # SpellingPanelCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/book_dictionary_coordinator.h  # BookDictionaryCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/card_names_coordinator.h  # CardNamesCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/repetitions_coordinator.h  # RepetitionsCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/language_tool_server.h  # LanguageToolServer with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/grammar_setup.h  # GrammarSetup with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings_dialog.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/settings_page.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/settings_pages.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/grammar_page.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/shortcuts_page.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/stats_pages.h  # Statistics pages with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/color_config_widget.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/length_spin_box.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/color_menu.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/list_sets_menu.h  # ListSetsMenu with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/info_bar.h  # InfoBar with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/plot_board_view.h  # PlotBoardView with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/plot_card_editor.h  # PlotCardEditor with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/cover_plan.h  # CoverPlan with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/publishing_set_view.h  # PublishingSetView with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/shortcut_recorder.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/tool_button_columns.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/fitting_scroll_area.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/collapsible_section.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/standalone_info_bar.h  # StandaloneInfoBar with Q_OBJECT (OpenSpec #00033)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/element_place_picker.h  # ElementPlacePicker with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/flow_layout.h  # FlowLayout with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/sections_field.h  # SectionsComboBox, SectionNamesEdit with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/kind_palette.h  # KindPalette with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/folder_field.h  # FolderField with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/name_forms_table.h  # NameFormsTable with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/card_name_box.h  # CardNameBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widget_keys.h  # WidgetKeys with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/utils/distraction_free_layout.h  # DistractionFreeLayout with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/status_bar_statistics.h  # StatusBarStatistics with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/status_bar_writing.h  # StatusBarWriting with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/writing_stats_bar.h  # WritingStatsBar with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/find_replace_bar.h  # FindReplaceBar with Q_OBJECT (OpenSpec #00044 Task 9.6)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/annotation_card.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/annotation_frame.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/note_frame.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/writing_frame.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/annotations_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/assistant_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/dashboard_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/loading_overlay.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/editor_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/log_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/navigator_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/paragraph_panel.h  # ParagraphPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/properties_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/plot_fields_box.h  # PlotFieldsBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/book_fields_box.h  # BookFieldsBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/footnotes_panel.h  # FootnotesPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/footnotes_panel_coordinator.h  # FootnotesPanelCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/material_page.h  # MaterialPage, LinkedMaterialsBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/own_fields_box.h  # OwnFieldsBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/relations_box.h  # RelationsBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/card_chapters_box.h  # CardChaptersBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/card_page.h  # CardPage with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/cursor_card_box.h  # CursorCardBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/paragraph_cards_box.h  # ParagraphCardsBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/card_panel.h  # CardPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/palette_edit.h  # PaletteEdit with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/source_boxes.h  # The groups of the card of a source with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/preview_panel.h  # PreviewPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/image_preview_panel.h  # ImagePreviewPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/material_list_panel.h  # MaterialListPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/clipping_origin_box.h  # ClippingOriginBox with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/book_dictionary_panel.h  # BookDictionaryPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/plot_board_panel.h  # PlotBoardPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/material_downloader.h  # MaterialDownloader with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/source_lookup.h  # SourceLookup with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/search_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/spelling_issue_list.h  # SpellingIssueCard, SpellingIssueList with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/spelling_panel.h  # SpellingPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/repetitions_panel.h  # RepetitionsPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/writing_stats_panel.h  # WritingStatsPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/writing_goals_tab.h  # WritingGoalsTab with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/writing_awards_tab.h  # WritingAwardsTab with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/tags_panel.h     # TagsPanel with Q_OBJECT (OpenSpec #00042 Task 7.10)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/command_registry.h  # CommandRegistry with Q_OBJECT (OpenSpec #00040)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/book_editor.h  # BookEditor with Q_OBJECT (OpenSpec #00042 Phase 3.1)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/split_editor_panel.h  # SplitEditorPanel with Q_OBJECT (OpenSpec #00042 Phase 5.8)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/statistics_collector.h  # StatisticsCollector with Q_OBJECT (OpenSpec #00042 Phase 6)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/style_resolver.h  # StyleResolver with Q_OBJECT (OpenSpec #00042 Phase 6.1-6.3)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/spell_check_service.h  # SpellCheckService with Q_OBJECT (OpenSpec #00042 Phase 6.4-6.9)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/grammar_check_service.h  # GrammarCheckService with Q_OBJECT (OpenSpec #00042 Phase 6.14-6.17)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/text_to_speech_service.h  # TextToSpeechService with Q_OBJECT (OpenSpec #00042 Phase 6.21-6.24)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/tag_detector.h  # TagDetector with Q_OBJECT (OpenSpec #00042 Task 7.10)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/quick_insert_controller.h  # QuickInsertList, QuickInsertController with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/viewport_manager.h     # ViewportManager with Q_OBJECT (OpenSpec #00043 Phase 4)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/search_engine.h       # SearchEngine with Q_OBJECT (OpenSpec #00044 Task 9.4)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/kalahari_text_document_layout.h  # KalahariTextDocumentLayout with Q_OBJECT (OpenSpec #00043 Phase 11.11)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/editor_render_pipeline.h       # EditorRenderPipeline with Q_OBJECT (OpenSpec #00043 Phase 12.1)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/editor_benchmark.h            # EditorBenchmark with Q_OBJECT (OpenSpec #00043 Phase 11.11)
)

# The preview of PDF files: only with PDFium (cmake/Pdfium.cmake)
if(KALAHARI_PDF_AVAILABLE)
    list(APPEND KALAHARI_SOURCES
        gui/pdf_document.cpp              # PdfDocument - a PDF file read by PDFium
        gui/pdf_models.cpp                # PdfSearchModel, PdfContentsModel - its search and contents
        gui/panels/pdf_preview_panel.cpp  # PdfPreviewPanel - a PDF on a tab: pages, side panel, search
        gui/panels/pdf_preview_parts.cpp  # PdfPreviewPanel - its tool bar, side panel, selection bar
        gui/panels/pdf_preview_highlights.cpp  # PdfPreviewPanel - the highlights kept in the book
        gui/panels/pdf_pages_view.cpp     # PdfPagesView - the pages of a PDF, drawn by the program
        gui/panels/pdf_pages_layout.cpp   # PdfPagesView - the rows, the zoom and the page shown
        ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/pdf_preview_panel.h  # with Q_OBJECT
        ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/pdf_pages_view.h     # with Q_OBJECT
        ${CMAKE_SOURCE_DIR}/include/kalahari/gui/pdf_document.h              # with Q_OBJECT
        ${CMAKE_SOURCE_DIR}/include/kalahari/gui/pdf_models.h                # with Q_OBJECT
    )
endif()

# The modules of the Workshop's tools: each lists its own sources and talks to the program
# only through core::IWindowEditorProvider (core/window_editor_provider.h), so that it can
# leave the program as an add-on of its own
include(${CMAKE_CURRENT_SOURCE_DIR}/mindmap/module.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/timeline/module.cmake)
list(APPEND KALAHARI_SOURCES
    ${KALAHARI_MINDMAP_SOURCES} ${KALAHARI_MINDMAP_GUI_SOURCES}
    ${KALAHARI_TIMELINE_SOURCES} ${KALAHARI_TIMELINE_GUI_SOURCES}
)

# Create executable
add_executable(kalahari ${KALAHARI_SOURCES})

# Precompiled headers for the executable target (same rationale as kalahari_core above).
if(COMMAND kalahari_add_pch)
    kalahari_add_pch(kalahari)
endif()

# UI translations: lrelease compiles the .ts files into .qm files embedded under :/i18n.
# After changing UI strings, refresh the .ts files with: cmake --build <dir> --target update_translations
# lupdate reads the executable's sources and the core's that have texts the writer sees: the
# names of the fields and where a caption is ("above")
qt_add_translations(kalahari
    TS_FILES ${CMAKE_SOURCE_DIR}/translations/kalahari_pl.ts
    SOURCES ${KALAHARI_SOURCES}
        editor/text_captions.cpp
        editor/text_fields.cpp
    LUPDATE_OPTIONS -no-obsolete -locations none
)

# Target properties
set_target_properties(kalahari PROPERTIES
    OUTPUT_NAME "kalahari"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories (for this target)
target_include_directories(kalahari PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

# Link libraries (Qt6 Hello World - Task #00001)
# Task #00020: Added SvgWidgets for IconDownloaderDialog preview
# OpenSpec #00042: Added Hunspell for spell checking (via pkg-config)
target_link_libraries(kalahari PRIVATE
    kalahari_core
    kalahari_build_info  # What the program is built from (kalahari/build_info.h)
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::SvgWidgets
    Qt6::PrintSupport  # The print of the book
    ${HUNSPELL_LIBRARIES}
)
target_include_directories(kalahari PRIVATE ${HUNSPELL_INCLUDE_DIRS})
target_link_directories(kalahari PRIVATE ${HUNSPELL_LIBRARY_DIRS})

# Windows: the settings of the driver of the printer go to the print engine of Qt (its
# private API) and come from the driver itself (src/gui/printer_driver.cpp)
if(WIN32)
    target_link_libraries(kalahari PRIVATE Qt6::PrintSupportPrivate winspool)
endif()

# The preview of PDF files (PDFium)
if(KALAHARI_PDF_AVAILABLE)
    target_link_libraries(kalahari PRIVATE PDFium::pdfium)
    target_compile_definitions(kalahari PRIVATE KALAHARI_HAS_PDF)
    if(WIN32)
        # One release build of pdfium.dll serves every configuration: the Debug deployment of
        # vcpkg (debug/bin) has none
        add_custom_command(TARGET kalahari POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:PDFium::pdfium>" "$<TARGET_FILE_DIR:kalahari>/"
            COMMENT "Copying pdfium.dll to output directory"
        )
    endif()
    message(STATUS "PDF preview: Enabled (linked PDFium::pdfium)")
else()
    message(STATUS "PDF preview: Disabled (PDF files open in the system program)")
endif()

# OpenSpec #00042: Text-to-Speech (optional Qt Speech module)
if(QT_TEXTTOSPEECH_AVAILABLE)
    target_link_libraries(kalahari PRIVATE Qt6::TextToSpeech)
    target_compile_definitions(kalahari PRIVATE QT_TEXTTOSPEECH_LIB)
    message(STATUS "Text-to-Speech: Enabled (linked Qt6::TextToSpeech)")
else()
    message(STATUS "Text-to-Speech: Disabled (graceful degradation)")
endif()

# Copy resources to output directory after each build (OpenSpec #00029)
# This ensures resources are always up-to-date, unlike file(COPY) which only runs at configure time
add_custom_command(TARGET kalahari POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:kalahari>/resources"
    COMMENT "Copying resources to output directory"
)

# Platform-specific settings
if(WIN32)
    # Windows: GUI application (not console)
    set_target_properties(kalahari PROPERTIES WIN32_EXECUTABLE TRUE)

    # Unicode support
    target_compile_definitions(kalahari PRIVATE UNICODE _UNICODE)
elseif(APPLE)
    # macOS: Bundle structure
    set_target_properties(kalahari PROPERTIES MACOSX_BUNDLE TRUE)

    # macOS: Copy resources to bundle (OpenSpec #00029)
    add_custom_command(TARGET kalahari POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources"
            "$<TARGET_BUNDLE_CONTENT_DIR:kalahari>/Resources/resources"
        COMMENT "Copying resources to macOS bundle"
    )
endif()

# Copy Qt plugins after build (Windows/Linux/macOS)
# This fixes "no Qt platform plugin could be initialized" error
if(WIN32)
    # Windows: Copy Debug plugins from vcpkg_installed
    add_custom_command(TARGET kalahari POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/platforms"
            "$<TARGET_FILE_DIR:kalahari>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/styles"
            "$<TARGET_FILE_DIR:kalahari>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/imageformats"
            "$<TARGET_FILE_DIR:kalahari>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/tls"
            "$<TARGET_FILE_DIR:kalahari>/tls"
        # OpenSpec #00041: SQLite database support
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/sqldrivers"
            "$<TARGET_FILE_DIR:kalahari>/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/bin/libcrypto-3-x64.dll"
            "$<TARGET_FILE_DIR:kalahari>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/bin/libssl-3-x64.dll"
            "$<TARGET_FILE_DIR:kalahari>/"
        # The libraries the plugins load (libjpeg of the JPEG plugin, libtiff, libwebp), which
        # nothing of the program links: without them the plugins do not load
        COMMAND ${CMAKE_COMMAND}
            "-DFROM_DIR=${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/bin"
            "-DTO_DIR=$<TARGET_FILE_DIR:kalahari>"
            -P "${CMAKE_SOURCE_DIR}/cmake/CopyDlls.cmake"
        COMMENT "Copying Qt plugins, the libraries they load and OpenSSL DLLs to output directory"
    )
elseif(UNIX AND NOT APPLE)
    # Linux: Find Qt plugins directory dynamically
    # Check if using system Qt6 (from apt) - skip plugin copying in that case
    # System Qt6 knows where its plugins are at runtime via built-in paths
    if(USE_SYSTEM_PACKAGES)
        message(STATUS "Using system Qt6 - plugins handled by system at runtime (skipping copy)")
    else()
        # vcpkg on Linux places plugins in different locations depending on triplet:
        # - x64-linux-dynamic: lib/qt6/plugins/
        # - system Qt: lib/qt6/plugins/ or lib/x86_64-linux-gnu/qt6/plugins/
        #
        # Qt6_DIR points to: <prefix>/share/Qt6 (cmake config location)
        # Need to find: <prefix>/lib/qt6/plugins/ or <prefix>/plugins/

        # Get vcpkg installed directory (Qt6_DIR is <prefix>/share/Qt6)
        get_filename_component(_qt6_share_dir "${Qt6_DIR}" DIRECTORY)  # <prefix>/share
        get_filename_component(_qt6_prefix "${_qt6_share_dir}" DIRECTORY)  # <prefix>

        message(STATUS "Qt6_DIR: ${Qt6_DIR}")
        message(STATUS "Qt6 prefix (calculated): ${_qt6_prefix}")

        # Try common plugin locations (ordered by likelihood)
        set(_qt6_plugin_candidates
            "${_qt6_prefix}/Qt6/plugins"           # vcpkg x64-linux-dynamic actual location
            "${_qt6_prefix}/lib/qt6/plugins"       # vcpkg x64-linux-dynamic alternative
            "${_qt6_prefix}/plugins"               # some vcpkg configurations
            "${_qt6_prefix}/lib/plugins"           # fallback
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/qt6/plugins"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/plugins"
            # System Qt6 from apt (Ubuntu/Debian)
            "/usr/lib/x86_64-linux-gnu/qt6/plugins"
            "/usr/lib/qt6/plugins"
            "/usr/lib64/qt6/plugins"
        )

    set(_qt6_plugins_dir "")
    foreach(_candidate IN LISTS _qt6_plugin_candidates)
        message(STATUS "Checking Qt6 plugins at: ${_candidate}")
        if(EXISTS "${_candidate}/platforms")
            set(_qt6_plugins_dir "${_candidate}")
            message(STATUS "  -> Found platforms/ subdirectory!")
            break()
        endif()
    endforeach()

    if(_qt6_plugins_dir)
        message(STATUS "Using Qt6 plugins directory: ${_qt6_plugins_dir}")

        # REQUIRED: platforms plugin (application cannot start without it)
        if(EXISTS "${_qt6_plugins_dir}/platforms")
            add_custom_command(TARGET kalahari POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt6_plugins_dir}/platforms"
                    "$<TARGET_FILE_DIR:kalahari>/platforms"
                COMMENT "Copying Qt6 platforms plugins"
            )
        else()
            message(FATAL_ERROR "Qt6 platforms plugin not found at ${_qt6_plugins_dir}/platforms - required for application to start")
        endif()

        # OPTIONAL: styles plugin (application works without it, just uses default style)
        if(EXISTS "${_qt6_plugins_dir}/styles")
            add_custom_command(TARGET kalahari POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt6_plugins_dir}/styles"
                    "$<TARGET_FILE_DIR:kalahari>/styles"
                COMMENT "Copying Qt6 styles plugins"
            )
        else()
            message(STATUS "Qt6 styles plugin not found at ${_qt6_plugins_dir}/styles (optional, skipping)")
        endif()

        # OPTIONAL: imageformats plugin (application works without it, just limited image format support)
        if(EXISTS "${_qt6_plugins_dir}/imageformats")
            add_custom_command(TARGET kalahari POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt6_plugins_dir}/imageformats"
                    "$<TARGET_FILE_DIR:kalahari>/imageformats"
                COMMENT "Copying Qt6 imageformats plugins"
            )
        else()
            message(STATUS "Qt6 imageformats plugin not found at ${_qt6_plugins_dir}/imageformats (optional, skipping)")
        endif()
    else()
        message(WARNING "Could not find Qt6 plugins directory!")
        message(WARNING "Searched locations:")
        foreach(_candidate IN LISTS _qt6_plugin_candidates)
            message(WARNING "  - ${_candidate}")
        endforeach()
        message(WARNING "Qt6_DIR: ${Qt6_DIR}")
        message(WARNING "VCPKG_TARGET_TRIPLET: ${VCPKG_TARGET_TRIPLET}")
        message(FATAL_ERROR "Qt6 plugins not found. Cannot build without platform plugin.")
    endif()
    endif()  # Close else() for USE_SYSTEM_PACKAGES
elseif(APPLE)
    # macOS: Plugins are embedded in .app bundle automatically by Qt
    # No manual copying needed
endif()

message(STATUS "Configured executable target: kalahari")
