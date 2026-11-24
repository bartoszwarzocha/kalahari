// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file properties_panel.h
/// @brief Contextual properties panel for project and chapter properties (Qt6)
///
/// This file defines the PropertiesPanel class, displaying different content
/// based on context: project properties (default), chapter properties (when
/// chapter selected), editor statistics (when editing), or placeholder
/// (when no project open).
///
/// OpenSpec #00033 Phase G: Full implementation with QStackedWidget.
/// OpenSpec #00042 Task 7.4: PropertiesPanel Integration with BookEditor.

#ifndef KALAHARI_GUI_PANELS_PROPERTIES_PANEL_H
#define KALAHARI_GUI_PANELS_PROPERTIES_PANEL_H

#include "kalahari/core/book_project.h"
#include "kalahari/gui/material_source.h"

#include <QWidget>
#include <QString>

class QDateTime;
class QStackedWidget;
class QLabel;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QTextEdit;
class QFormLayout;
class QVBoxLayout;
class QGroupBox;
class QPushButton;
class QTimer;
class QToolButton;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
} // namespace kalahari::editor

namespace kalahari::gui {

class BookFieldsBox;
class CardPage;
class ClippingOriginBox;
class CursorCardBox;
class EditorPanel;
class LinkedMaterialsBox;
class ParagraphCardsBox;
class PlotFieldsBox;
class SeriesPage;
class MaterialPage;
class SectionNamesEdit;
class SectionsComboBox;

/// @brief Contextual properties panel with three views
///
/// Shows different content based on current context:
/// - NoProject: Placeholder message when no project is open
/// - Project: Project metadata (title, author, language, genre, sections, statistics); in a
///   series those of the current volume, with its number
/// - Series: The name of a series and its volumes (SeriesPage)
/// - Chapter: Chapter properties (title, word count, status, notes, linked materials)
/// - Material: The short card of a material (MaterialPage)
/// - Editor: Text selection statistics (word/char count); the paragraph at the cursor is
///   the Paragraph and Styles panel's (ParagraphPanel)
///
/// Connects to ProjectManager signals for automatic updates.
/// Connects to BookEditor for real-time selection statistics.
class PropertiesPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Page indices for QStackedWidget
    enum class Page {
        NoProject = 0,  ///< Placeholder when no project open
        Project = 1,    ///< Project properties view
        Chapter = 2,    ///< Chapter properties view
        Section = 3,    ///< Section aggregate statistics view
        Part = 4,       ///< Part aggregate statistics view
        Editor = 5,     ///< Editor statistics view (selection/document stats)
        Material = 6,   ///< The short card of a material of the Workshop
        Card = 7,       ///< The short card of a character, a location or an item
        Series = 8      ///< The name and the volumes of a series (SeriesPage)
    };

    /// @brief Constructor
    /// @param book The book of its window
    /// @param parent Parent widget
    explicit PropertiesPanel(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Destructor
    ~PropertiesPanel() override;

    /// @brief The page the panel shows
    [[nodiscard]] Page currentPage() const;

    /// @brief The card of a material
    [[nodiscard]] MaterialPage* materialPage() const { return m_materialPage; }

    /// @brief The short card of a character, a location or an item
    [[nodiscard]] CardPage* cardPage() const { return m_cardPage; }

    /// @brief The page of the series
    [[nodiscard]] SeriesPage* seriesPage() const { return m_seriesPage; }

    /// @brief The group of the own fields of the book, on the page of the book
    [[nodiscard]] BookFieldsBox* bookFields() const { return m_bookFieldsBox; }

    /// @brief The card at the cursor of a text of the book, on the Editor page or the Chapter
    ///        page of the text in front
    [[nodiscard]] CursorCardBox* cursorCard() const { return m_cursorCard; }

    /// @brief The cards of the paragraph at the cursor of a chapter, on the Editor page or the
    ///        Chapter page of the text in front
    [[nodiscard]] ParagraphCardsBox* paragraphCards() const { return m_paragraphCards; }

    /// @brief The materials of the element of the chapter page, of the section and of the part
    [[nodiscard]] LinkedMaterialsBox* chapterMaterials() const { return m_chapterMaterials; }
    [[nodiscard]] LinkedMaterialsBox* sectionMaterials() const { return m_sectionMaterials; }
    [[nodiscard]] LinkedMaterialsBox* partMaterials() const { return m_partMaterials; }

    /// @brief Where the clipping of the chapter page comes from (hidden for another element)
    [[nodiscard]] ClippingOriginBox* clippingOrigin() const { return m_clippingOrigin; }




public slots:
    /// @brief Show project properties view
    ///
    /// Switches to project properties page and populates fields from
    /// the current project in ProjectManager.
    void showProjectProperties();

    /// @brief Show the book's properties with the keys in the first field, the title (Book >
    /// Book Properties...)
    void editProjectProperties();

    /// @brief Show chapter properties view
    /// @param elementId Element ID of the chapter to display
    ///
    /// Switches to chapter properties page and populates fields from
    /// the specified chapter element; a material shows its card (showMaterialProperties()).
    void showChapterProperties(const QString& elementId);

    /// @brief Show the card of material @p elementId of the Workshop
    void showMaterialProperties(const QString& elementId);

    /// @brief Show the short card of card @p elementId of the Workshop: a character, a
    /// location or an item
    void showCardProperties(const QString& elementId);

    /// @brief Show the name and the volumes of the series; the book's properties in a project
    /// with one book
    void showSeriesProperties();

    /// @brief Give the keys to the first field of the page shown, scrolled to it (Alt+Enter in
    /// the Navigator): the title of the book, of an element or of a material; the materials of
    /// a section or a part
    void focusFirstField();

    /// @brief Show no project placeholder
    ///
    /// Switches to placeholder page when no project is open.
    void showNoProject();

    /// @brief Show section properties view (aggregate statistics)
    /// @param sectionType Section type ("section_frontmatter", "section_body", "section_backmatter")
    void showSectionProperties(const QString& sectionType);

    /// @brief Show part properties view (aggregate statistics)
    /// @param partId Part ID
    void showPartProperties(const QString& partId);

    /// @brief Show editor statistics view
    ///
    /// Switches to editor page showing text statistics, and for a text of the book the
    /// group of its versions under them.
    /// Called when user starts editing in BookEditor.
    void showEditorProperties();

    /// @brief Connect to an EditorPanel for statistics updates
    /// @param editorPanel EditorPanel to track (nullptr to disconnect)
    ///
    /// Connects to BookEditor's selectionChanged signal for real-time
    /// updates of selection statistics. Without an editor (another tab in front, or none),
    /// the page of the editor gives way to the page of the book, or to the page without one.
    void setActiveEditor(EditorPanel* editorPanel);

    /// @brief Refresh current view with latest data
    ///
    /// Re-reads data from ProjectManager and updates displayed values.
    void refresh();

    /// @brief Show again what the current page shows of material @p elementId: its card, or
    /// its title among the linked materials; the fields the writer edits stay as they are
    void refreshMaterial(const QString& elementId);

    /// @brief Show the words ProjectManager counts now on the current page, also of the
    /// changes that are not saved; the fields the writer edits stay as they are
    void updateWordCounts();

private slots:
    /// @brief Handle project opened signal
    /// @param projectPath Path to opened project
    void onProjectOpened(const QString& projectPath);

    /// @brief Handle project closed signal
    void onProjectClosed();

    /// @brief Handle project title changed
    void onProjectTitleChanged();

    /// @brief Handle project author changed
    void onProjectAuthorChanged();

    /// @brief Handle project language changed
    void onProjectLanguageChanged(int index);

    /// @brief Handle project genre changed
    void onProjectGenreChanged();

    /// @brief The writer chose another item of the Sections field
    void onProjectSectionsChanged();

    /// @brief The writer changed the own names of the sections
    void onProjectSectionNamesChanged();

    /// @brief Handle chapter title changed
    void onChapterTitleChanged();

    /// @brief Handle chapter status changed
    void onChapterStatusChanged(int index);

    /// @brief The writer chose the page the text element starts on (item @p index of the list)
    void onChapterFirstPageChanged(int index);

    /// @brief Handle chapter notes changed (called on focus lost)
    void onChapterNotesChanged();

    /// @brief Handle selection changed in BookEditor
    void onEditorSelectionChanged();

    /// @brief Handle cursor position changed in BookEditor
    void onEditorCursorChanged();

    /// @brief The group of the versions again, when the page with it is shown
    void refreshVersions();

    /// @brief The card whose name the cursor of the text in front is in and the cards of its
    /// paragraph, on the Editor page or on the Chapter page of its chapter, as Settings >
    /// Cards shows them (CursorCardBox::cardsAtCursor())
    void updateCursorCard();

protected:
    /// @brief Event filter for handling focus events
    /// @param obj Object that received the event
    /// @param event The event
    /// @return true if event was handled
    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    /// @brief Emitted when chapter title or status is changed in the panel
    /// @param elementId Element ID of the chapter
    /// @note Used to notify Navigator to refresh the item's display title (status suffix)
    void chapterStatusChanged(const QString& elementId);

    /// @brief The page a text element starts on changed in the panel: the pages of the book
    ///        are counted anew, and its open editor places the margins of its pages anew
    void chapterFirstPageChanged(const QString& elementId);

    /// @brief The writer chose how the Navigator divides the book: sections @p sections
    /// @note The Navigator's coordinator saves them, and refreshes the Navigator and the panel
    void requestSections(const kalahari::core::BookSections& sections);

    /// @brief The title, the author or the language of the book changed in the panel: the
    /// Navigator shows the title, and the names of the sections in the language of the book;
    /// the fields of the texts show them (FieldCoordinator)
    void bookChanged();

    /// @brief Emitted when the book's language is changed in the panel
    /// @param language Its code (pl, en...)
    void bookLanguageChanged(const QString& language);

    /// @brief Open the preview of material @p elementId
    void requestPreview(const QString& elementId);

    /// @brief Open material @p elementId in the program the system gives its file
    void requestOpenWithSystem(const QString& elementId);

    /// @brief Open material @p elementId as the settings say
    void requestOpenMaterial(const QString& elementId);

    /// @brief Open the full card of material @p elementId: the source of a quote
    void requestOpenMaterialCard(const QString& elementId);

    /// @brief Open the settings of the sources (the gear of the entry of a source)
    void requestSourceSettings();

    /// @brief Open what link @p link of a material names: an element, or the section in the
    /// Navigator
    void requestOpenLink(const QString& link);

    /// @brief Open the web address of material @p elementId in the browser
    void requestOpenInBrowser(const QString& elementId);

    /// @brief Download the copy of the picture of material @p elementId
    void requestSaveCopy(const QString& elementId);

    /// @brief Material @p elementId changed on its card in the panel: its other cards show it
    void materialChanged(const QString& elementId);

    /// @brief Open Card on the short card: the full card of @p elementId beside the chapters
    void requestOpenCard(const QString& elementId);

    /// @brief The gear of the short card: Settings > Cards
    void requestCardSettings();

    /// @brief "+" of the cards of the paragraph: the window Link Paragraph with Card for the
    /// paragraph at the cursor of the text in front
    void requestLinkParagraphCard();

    /// @brief Change... of the declension of the short card: the window "Name" of the book's
    /// dictionary for the name of card @p elementId
    void requestCardNameWindow(const QString& elementId);

    /// @brief Material @p materialId was added on the short card (a portrait from a file): the
    /// Navigator shows it
    void cardMaterialAdded(const QString& materialId);

    /// @brief The short card shows card @p elementId: the other card of a relation; the
    /// Navigator shows it as the current element
    void requestSelectCard(const QString& elementId);

    /// @brief Card @p elementId was made with a relation on the short card: the Navigator
    /// shows it
    void cardAdded(const QString& elementId);

    /// @brief Files, web addresses or pictures @p incoming were dropped on the short card: new
    /// materials linked with card @p link
    void requestAddCardMaterials(const kalahari::gui::IncomingMaterials& incoming,
                                 const QString& link);

    /// @brief Something to say on the status bar: "Address copied: ..."
    void message(const QString& text);

    /// @brief New Volume... of the page of the series: the window of a new volume
    void requestNewVolume();

    /// @brief Open of the page of the series: volume @p bookId becomes the current one, with
    /// its first chapter open
    void requestOpenVolume(const QString& bookId);

    /// @brief Save Version in the group of the versions: keep the text of element
    /// @p elementId as a version
    void requestSaveVersion(const QString& elementId);

    /// @brief Version History... in the group of the versions: the versions of the text of
    /// element @p elementId
    void requestVersionHistory(const QString& elementId);

    /// @brief The gear of the group of the versions: Settings > Editor > Versions
    void requestVersionSettings();

    /// @brief Add Field... of the group of the own fields of the book
    void requestNewOwnField();

    /// @brief Insert in Text... of the group of the own fields: Insert > Field..., its tab Own,
    ///        for the text in front, field @p id chosen (empty: none)
    void requestInsertOwnField(const QString& id);

    /// @brief The gear of the group of the own fields: Settings > Editor > General, the fields
    void requestFieldSettings();

    /// @brief Restore to Its Place of the group Origin: insert clipping @p elementId where it
    /// was cut from
    void requestClippingRestore(const QString& elementId);

    /// @brief Go to Its Place of the group Origin: open the text clipping @p elementId was cut
    /// from at its place
    void requestClippingOrigin(const QString& elementId);

private:
    /// @brief Setup UI components
    void setupUI();

    /// @brief Setup "No Project" page
    /// @return Created widget
    QWidget* createNoProjectPage();

    /// @brief Setup "Project Properties" page
    /// @return Created widget
    QWidget* createProjectPage();

    /// @brief Setup "Chapter Properties" page
    /// @return Created widget
    QWidget* createChapterPage();

    /// @brief Setup "Section Properties" page (aggregate statistics)
    /// @return Created widget
    QWidget* createSectionPage();

    /// @brief Setup "Part Properties" page (aggregate statistics)
    /// @return Created widget
    QWidget* createPartPage();

    /// @brief Setup "Editor Properties" page (selection/document statistics)
    /// @return Created widget
    QWidget* createEditorPage();

    /// @brief Setup the page of the card of a material
    /// @return Created widget
    QWidget* createMaterialPage();

    /// @brief Setup the page of the short card of a character, a location or an item
    /// @return Created widget
    QWidget* createCardPage();

    /// @brief Show the short card again as the book has it, or the book's page when the card
    /// was deleted
    void refreshCard();

    /// @brief Show the part again as the book has it, or the book's page when the part was
    /// deleted
    void refreshPart();

    /// @brief A box of the materials linked with an element or a section, which forwards their
    /// opening
    LinkedMaterialsBox* createMaterialsBox(QWidget* parent);

    /// @brief Show the materials linked with the section of the section page
    void showSectionMaterials();

    /// @brief Show the materials linked with the part of the part page
    void showPartMaterials();

    /// @brief Connect signals to ProjectManager
    void connectSignals();

    /// @brief Disconnect from current editor
    void disconnectFromEditor();

    /// @brief Update editor statistics from current BookEditor
    void updateEditorStatistics();

    /// @brief Put the group of the versions, the card at the cursor and the cards of its
    ///        paragraph on the page of @p layout (of the Chapter page or the Editor page): the
    ///        one set of them follows the page shown
    void placeTextGroups(QVBoxLayout* layout);

    /// @brief The element of the book whose text is in front, when the page shown describes
    ///        it: the Editor page, or the Chapter page of its chapter; empty for none
    [[nodiscard]] QString describedTextId() const;

    /// @brief The cards of the paragraph at the cursor of @p text, the text of element
    ///        @p elementId: shown for a chapter of the book (nullptr: none shown)
    void showParagraphCards(const editor::BookEditor* text, const QString& elementId);

    /// @brief Populate project fields from current project
    void populateProjectFields();

    /// @brief Populate chapter fields from specified element
    /// @param elementId Element ID of the chapter
    void populateChapterFields(const QString& elementId);

    /// @brief Populate section fields (aggregate statistics)
    /// @param sectionType Section type
    void populateSectionFields(const QString& sectionType);

    /// @brief Populate part fields (aggregate statistics)
    /// @param partId Part ID
    void populatePartFields(const QString& partId);

    /// @brief Update project statistics
    void updateProjectStatistics();

    /// @brief Fill the Sections field and the own names of the sections of book @p book
    void populateBookSections(const core::ProjectBook& book);

    /// @brief Fill the style of the bibliography of the book: its own, or that of the Settings
    void populateBibliographyStyle();

    /// @brief Show the row of own names when the Sections field says own names
    void showSectionNamesRow();

    /// @brief Format date for display
    /// @param dateTime Date and time; invalid when unknown
    /// @return Formatted date string, in local time
    [[nodiscard]] QString formatDate(const QDateTime& dateTime) const;

    /// @brief Select @p status in the chapter's status combo; "draft" when it is not there
    void selectChapterStatus(const QString& status);

    /// @brief Show the first page of @p element in its field: Automatic named after the page
    ///        of its kind or of the settings (ProjectManager::kindPageStartOf()), or its own
    void showChapterFirstPage(const core::ProjectElement& element);

    /// @brief The group of the versions of the text of element @p elementId: how many, the
    /// last one and its description; hidden for an element whose text has none. While
    /// versions are turned off it says so, and how many can be read in the history.
    void showVersions(const QString& elementId);

    /// @brief Put the gear of the group of the versions at the end of the line of its title
    void placeVersionSettingsButton();

    /// @brief The icons of the buttons of the group of the versions, in the icon colors
    void updateVersionIcons();

    /// @brief Show the type of the open book @p project: its icon and its name, greyed out
    /// when the program does not have it, as the Dashboard shows it
    void showBookType(const core::BookProject& project, const core::ProjectBook& book);

    /// @brief In a series the page of the book is that of the current volume: its group says
    /// so, with the number of the volume; a book has neither
    void showVolumeRow(const core::BookProject& project);

    // Main widget
    QStackedWidget* m_stackedWidget = nullptr;

    // No Project Page
    QLabel* m_noProjectLabel = nullptr;

    // Project Page widgets
    QLabel* m_projectTypeIcon = nullptr;   ///< Icon of the book's type (showBookType())
    QLabel* m_projectTypeLabel = nullptr;  ///< Name of the book's type: "Novel", "Novel (not in
                                           ///< the program)"
    QLineEdit* m_projectTitleEdit = nullptr;
    QLineEdit* m_projectAuthorEdit = nullptr;
    QComboBox* m_projectLanguageCombo = nullptr;
    BookFieldsBox* m_bookFieldsBox = nullptr;  ///< The own fields of the book
    QComboBox* m_projectBibliographyCombo = nullptr;  ///< Its own style, or the Settings' one
    QLineEdit* m_projectGenreEdit = nullptr;
    QFormLayout* m_projectInfoLayout = nullptr;          ///< Rows of the book's information
    SectionsComboBox* m_projectSectionsCombo = nullptr;  ///< The sections of the book, or none
    SectionNamesEdit* m_projectSectionNames = nullptr;   ///< Own names of the sections
    QLabel* m_projectChaptersTitle = nullptr;  ///< "Total Chapters:", "Total Stories:": the main
                                               ///< texts
    QLabel* m_projectChaptersLabel = nullptr;
    QLabel* m_projectWordsLabel = nullptr;
    QLabel* m_projectCreatedLabel = nullptr;
    QLabel* m_projectModifiedLabel = nullptr;
    QLabel* m_projectDraftCountLabel = nullptr;
    QLabel* m_projectRevisionCountLabel = nullptr;
    QLabel* m_projectFinalCountLabel = nullptr;

    // Chapter Page widgets: a text element of any kind, or a file of the book outside its
    // text, which has only its title and its words
    QGroupBox* m_chapterInfoGroup = nullptr;  ///< "Chapter Information", "Story Information"
    QFormLayout* m_chapterInfoLayout = nullptr;  ///< Rows of the element's information
    QLineEdit* m_chapterTitleEdit = nullptr;
    QLabel* m_chapterWordCountLabel = nullptr;
    QComboBox* m_chapterStatusCombo = nullptr;
    QComboBox* m_chapterFirstPage = nullptr;  ///< The page the text starts on (page_start.h)
    QGroupBox* m_chapterNotesGroup = nullptr;
    QTextEdit* m_chapterNotesEdit = nullptr;
    LinkedMaterialsBox* m_chapterMaterials = nullptr;  ///< The materials of the element
    ClippingOriginBox* m_clippingOrigin = nullptr;     ///< Where a clipping comes from
    PlotFieldsBox* m_chapterPlot = nullptr;  ///< What the plot board says of a text
    QGroupBox* m_chapterVersionsGroup = nullptr;  ///< The versions of the text (showVersions())
    QLabel* m_versionsCountLabel = nullptr;       ///< How many, and when the last one was kept
    QLabel* m_versionsLastLabel = nullptr;        ///< The description of the last one
    QPushButton* m_saveVersionButton = nullptr;
    QPushButton* m_versionHistoryButton = nullptr;
    QToolButton* m_versionSettingsButton = nullptr;  ///< The gear at the end of the title
    QString m_versionsElementId;  ///< The text whose versions the group shows
    QVBoxLayout* m_chapterPageLayout = nullptr;  ///< The Chapter page, for the versions group
    QVBoxLayout* m_editorPageLayout = nullptr;   ///< The Editor page, for the versions group

    // Section Page widgets
    QLabel* m_sectionTitleLabel = nullptr;
    QLabel* m_sectionChapterCountTitle = nullptr;  ///< "Chapters:", "Stories:", or "Elements:"
                                                   ///< outside the main one
    QLabel* m_sectionChapterCountLabel = nullptr;
    QLabel* m_sectionWordCountLabel = nullptr;
    QLabel* m_sectionDraftCountLabel = nullptr;
    QLabel* m_sectionRevisionCountLabel = nullptr;
    QLabel* m_sectionFinalCountLabel = nullptr;
    LinkedMaterialsBox* m_sectionMaterials = nullptr;  ///< The materials of the section

    // Part Page widgets
    QLabel* m_partTitleLabel = nullptr;
    QLabel* m_partChapterCountTitle = nullptr;  ///< "Chapters:", "Stories:": the main texts of the
                                                ///< book
    QLabel* m_partChapterCountLabel = nullptr;
    QLabel* m_partWordCountLabel = nullptr;
    QLabel* m_partDraftCountLabel = nullptr;
    QLabel* m_partRevisionCountLabel = nullptr;
    QLabel* m_partFinalCountLabel = nullptr;
    LinkedMaterialsBox* m_partMaterials = nullptr;  ///< The materials of the part
    // The card of a material
    MaterialPage* m_materialPage = nullptr;
    CardPage* m_cardPage = nullptr;  ///< The short card of a character, a location or an item
    SeriesPage* m_seriesPage = nullptr;  ///< The name and the volumes of a series
    QLabel* m_projectVolumeLabel = nullptr;  ///< "2 of 3": the number of the volume of a series
    CursorCardBox* m_cursorCard = nullptr;  ///< The card at the cursor (placeTextGroups())
    ParagraphCardsBox* m_paragraphCards = nullptr;  ///< The paragraph's cards (placeTextGroups())

    // Editor Page widgets (OpenSpec #00042 Task 7.4)
    QLabel* m_editorTitleLabel = nullptr;           ///< "Selection" or "Document"
    QLabel* m_editorWordCountLabel = nullptr;       ///< Word count
    QLabel* m_editorCharCountLabel = nullptr;       ///< Character count (with spaces)
    QLabel* m_editorCharNoSpaceLabel = nullptr;     ///< Character count (without spaces)
    QLabel* m_editorParagraphCountLabel = nullptr;  ///< Paragraph count
    QLabel* m_editorReadingTimeLabel = nullptr;     ///< Estimated reading time

    // State tracking
    QString m_currentChapterId;    ///< Currently displayed chapter ID
    QString m_currentSectionType;  ///< Currently displayed section type
    QString m_currentPartId;       ///< Currently displayed part ID
    bool m_isUpdating = false;     ///< Flag to prevent recursive updates

    // Active editor tracking (OpenSpec #00042 Task 7.4)
    EditorPanel* m_activeEditorPanel{nullptr};  ///< Currently tracked editor panel

    // OpenSpec #00043: Debounce timer for cursor change updates
    QTimer* m_cursorDebounceTimer{nullptr};     ///< Debounce rapid cursor changes

    int m_settingsListener{0};  ///< SettingsManager::subscribe() id: the rules of counting,
                                ///< versions turned on or off, the cards at the cursor
    core::ProjectManager& m_book;  ///< The book of its window
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PROPERTIES_PANEL_H
