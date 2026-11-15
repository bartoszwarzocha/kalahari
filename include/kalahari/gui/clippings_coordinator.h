// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file clippings_coordinator.h
/// @brief Clippings: Edit > Cut to Clippings, Insert > Clipping..., and the way back to where
///        a clipping was cut from

#ifndef KALAHARI_GUI_CLIPPINGS_COORDINATOR_H
#define KALAHARI_GUI_CLIPPINGS_COORDINATOR_H

#include <kalahari/core/clipping.h>

#include <QDateTime>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>

#include <cstdint>
#include <functional>
#include <list>

class QStatusBar;
class QTabWidget;
class QTextDocument;
class QWidget;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

class CommandRegistry;
class EditorPanel;
class MaterialListCoordinator;
class ParagraphMarksCoordinator;
class NavigatorCoordinator;
class PropertiesPanel;

/// @brief A clipping as the window and the list of Insert > Clipping show it
struct ClippingEntry {
    QString id;            ///< The element
    QString title;         ///< Its title
    QString fromId;        ///< The text it was cut from; empty: added by hand
    QString from;          ///< The title of that text; "(deleted)" after it when it is gone
    bool originGone = false;  ///< That text is no longer in the book
    QDateTime cut;         ///< When it was cut; invalid: added by hand
    int words = 0;         ///< The words of its text
    QString note;          ///< Its note (the notes of the element)
    bool inserted = false; ///< It was inserted into a text
    QString search;        ///< Its title, text and note, folded for the search
};

/// @brief What the window or the list of Insert > Clipping was closed with
enum class ClippingChoice : std::uint8_t {
    None,     ///< Closed
    Insert,   ///< Insert at the cursor
    Restore,  ///< Restore to the place it came from
    Open,     ///< Open it in the editor
};

/// @brief Clippings: fragments cut from the texts to the Workshop, and inserted back
///
/// Edit > Cut to Clippings (Ctrl+Shift+X) takes the selection, or the paragraph with the cursor
/// without one (Settings > Editor > Clippings), from a text of the book or a working note to a
/// new clipping of the Workshop, titled with its first words (or in a small window first). The
/// clipping keeps where it was cut from (core::ClippingOrigin). A notice under the text says
/// where it went, with Undo; Ctrl+Z in the text takes the cut back too: the fragment comes
/// back and the clipping goes, while it has not changed since (and Ctrl+Y cuts it again).
///
/// Insert > Clipping... (Alt+Shift+X) opens the window with the clippings and the preview of
/// the one chosen, or the list at the cursor (the setting). The clipping goes in at the cursor
/// as a paste does; then it stays in the Workshop marked as inserted, or goes from it (the
/// setting; Ctrl+Z brings it back). Restore to the Place It Came From opens its text at the
/// place found again (core::Clippings::placeIn()) and inserts it there; where the words around
/// the place changed, the cursor goes to the paragraph and a notice offers Insert Here.
class ClippingsCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Id of Edit > Cut to Clippings in the registry
    static constexpr const char* CUT_COMMAND = "edit.cutToClippings";
    /// @brief Id of Insert > Clipping... in the registry
    static constexpr const char* INSERT_COMMAND = "insert.clipping";
    /// @brief The page of the Settings dialog with the clippings (Editor > Clippings)
    static constexpr const char* SETTINGS_PAGE = "editor.clippings";

    /// @brief Says something under the text of a tab, with a button
    ///        (DocumentCoordinator::showNotice())
    using NoticeShower = std::function<void(EditorPanel* editor, const QString& message,
                                            const QString& actionText,
                                            const QString& actionToolTip,
                                            std::function<void()> action)>;

    /// @brief The text that has the keys (none: the keys are elsewhere)
    using TextWithKeys = std::function<editor::BookEditor*()>;

    /// @brief Where the coordinator tells the writer what it did
    struct Feedback {
        /// Says under a text what was done, with a button (none: the status bar says it,
        /// without the button)
        NoticeShower showNotice;
        QStatusBar* statusBar = nullptr;  ///< Where short messages go (may be null)
        QWidget* window = nullptr;        ///< Over which the windows and the warnings open
    };

    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param navigator Opens and saves the texts, shows the clippings added and closes their
    ///        tabs
    /// @param textWithKeys The text that has the keys
    /// @param feedback The notices under the texts, the status bar and the window
    /// @param parent Parent object
    ClippingsCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                         QTabWidget* centralTabs, NavigatorCoordinator* navigator,
                         TextWithKeys textWithKeys, Feedback feedback,
                         QObject* parent = nullptr);
    /// @brief Stops following the texts
    ~ClippingsCoordinator() override;
    /// @brief Not copied: it belongs to its window
    ClippingsCoordinator(const ClippingsCoordinator&) = delete;
    /// @brief Not copied: it belongs to its window
    ClippingsCoordinator& operator=(const ClippingsCoordinator&) = delete;
    /// @brief Not moved: its texts are connected to it
    ClippingsCoordinator(ClippingsCoordinator&&) = delete;
    /// @brief Not moved: its texts are connected to it
    ClippingsCoordinator& operator=(ClippingsCoordinator&&) = delete;

    /// @brief Give Cut to Clippings and Insert > Clipping their actions and the reasons why
    ///        they are greyed out (their states follow the keys: EditCoordinator)
    void connectCommands();

    /// @brief Restore to Its Place and Go to Its Place of the group Origin of @p properties
    ///        and of the lists of @p lists do it here (either may be null)
    void connectPanels(PropertiesPanel* properties, MaterialListCoordinator* lists) const;

    /// @brief Restore Here of the scissors of @p marks does it here; the scissors follow the
    ///        clippings cut, inserted and gone (the Navigator shows those changes)
    void connectMarks(ParagraphMarksCoordinator& marks);

    // =========================================================================
    // Cut to Clippings
    // =========================================================================

    /// @brief Why the selection of @p text, or its paragraph, cannot be cut to the clippings
    ///        now (empty: it can)
    [[nodiscard]] QString whyNotCut(const editor::BookEditor* text) const;

    /// @brief Whether @p text can be cut to the clippings now (for EditCoordinator)
    [[nodiscard]] bool canCut(const editor::BookEditor& text) const {
        return whyNotCut(&text).isEmpty();
    }

    /// @brief Edit > Cut to Clippings in @p text: at once, or with the small window first
    ///        (core::Clippings::CUT_SETTING)
    /// @return The id of the new clipping; empty when nothing was cut
    QString cut(editor::BookEditor* text);

    /// @brief Cut the selection of @p text, or its paragraph, to a new clipping with @p title
    ///        (empty: its first words) and @p note
    /// @return The id of the new clipping; empty when it cannot be cut (the status bar says
    ///         why) or the book could not be changed (a warning says so)
    QString cutAs(editor::BookEditor* text, const QString& title, const QString& note);

    /// @brief Whether the last cut or insertion can still be taken back from the notice: its
    ///        text has not changed since
    [[nodiscard]] bool canUndoLast() const;

    /// @brief Undo of the notice: the last cut or insertion is taken back, as Ctrl+Z in its
    ///        text does
    bool undoLast();

    // =========================================================================
    // Insert > Clipping
    // =========================================================================

    /// @brief Why no clipping can be inserted into @p text now (empty: it can)
    [[nodiscard]] QString whyNotInsert(const editor::BookEditor* text) const;

    /// @brief Whether a clipping can be inserted into @p text now (for EditCoordinator)
    [[nodiscard]] bool canInsert(const editor::BookEditor& text) const {
        return whyNotInsert(&text).isEmpty();
    }

    /// @brief Insert > Clipping... in @p text: the window, or the list at the cursor
    ///        (core::Clippings::INSERT_SETTING)
    void insert(editor::BookEditor* text);

    /// @brief Do with clipping @p clippingId what the window or the list of Insert > Clipping
    ///        was closed with: insert it into @p text, restore it, or open it
    void act(editor::BookEditor* text, ClippingChoice choice, const QString& clippingId);

    /// @brief Insert clipping @p clippingId into @p text at its cursor, as a paste
    /// @return false when it cannot be inserted (the status bar says why)
    bool insertAt(editor::BookEditor* text, const QString& clippingId);

    /// @brief Restore to the Place It Came From: open the text of clipping @p clippingId at
    ///        its place and insert it there; where the place is not found as it was, the cursor
    ///        goes to its paragraph and a notice offers Insert Here
    /// @return true when the clipping was inserted
    bool restore(const QString& clippingId);

    /// @brief Go to the Place: open the text of clipping @p clippingId with the cursor at the
    ///        place it was cut from
    /// @return false when the clipping has no origin or its text is gone
    bool showOrigin(const QString& clippingId);

    /// @brief The clippings of the book, the last cut first; @p withInserted: also those
    ///        inserted already
    [[nodiscard]] QList<ClippingEntry> clippings(bool withInserted) const;

    /// @brief The text of clipping @p clippingId (KML): of its open tab, else of its file
    [[nodiscard]] QString kmlOf(const QString& clippingId) const;

private:
    /// @brief A cut or an insertion followed in the undo of its text, so that undo and redo
    ///        take the clipping back and forth with it
    struct Step {
        int id = 0;                         ///< Unique among the steps
        QPointer<editor::BookEditor> text;  ///< Where it was done
        const QTextDocument* document = nullptr;  ///< Its document then (a new one: forgotten)
        int level = 0;          ///< The level of undo of the step in the text
        bool done = true;       ///< false while undone
        bool isCut = true;      ///< A cut; false: an insertion
        QString clippingId;     ///< The clipping, also while it is not in the Workshop
        bool existsDone = true;     ///< Whether the clipping is in the Workshop when done
        bool existsUndone = false;  ///< ... and when undone
        QString title;          ///< The clipping as it was: its title,
        QString kml;            ///< its text,
        QString fileKml;        ///< its text as its file had it then (chapterText()),
        QString note;           ///< its note,
        core::ClippingData dataDone;    ///< its data when done
        core::ClippingData dataUndone;  ///< and when undone
    };

    /// @brief The selection to cut in @p text: its selection, else its paragraph (after
    ///        whyNotCut(), which checks NO_SELECTION_SETTING); false when there is nothing to
    ///        cut
    [[nodiscard]] static bool selectsWhatToCut(editor::BookEditor& text);

    /// @brief The title of a clipping cut: @p title, else the first words of @p kml, else the
    ///        default title of a clipping (an image alone)
    [[nodiscard]] QString clippingName(const QString& title, const QString& kml) const;

    /// @brief Add the clipping @p name with @p kml and @p note, cut from the text @p from
    /// @return Its id; empty when the book could not take it (a warning says so)
    QString addCutClipping(const QString& name, const QString& kml, const QString& note,
                           const QString& from);

    /// @brief Take the selection from @p text: its words deleted, or moved (KEEP_WORDS_SETTING)
    /// @return false when nothing was taken
    bool takeSelectionOf(editor::BookEditor& text);

    /// @brief Say @p message under @p text with Undo (@p undoToolTip), else in the status bar
    void notice(editor::BookEditor* text, const QString& message, const QString& undoToolTip);

    /// @brief After a change: save the text of @p text and show the Workshop as it is now
    void refreshAfter(const editor::BookEditor* text);

    /// @brief Clipping @p clippingId, when it can be inserted into @p text (else the status bar
    ///        says why)
    /// @return nullptr when it cannot
    [[nodiscard]] const core::ProjectElement* clippingToInsert(const editor::BookEditor* text,
                                                               const QString& clippingId) const;

    /// @brief Save the changes of the tab of @p clipping before it goes from the Workshop
    /// @return false when they could not be saved (a warning says so)
    bool savedBeforeGoing(const core::ProjectElement& clipping);

    /// @brief Insert @p kml into @p text at its cursor, as a paste
    /// @return false when the text did not change
    bool pasteInto(editor::BookEditor& text, const QString& kml);

    /// @brief After an insertion: mark @p clipping as inserted with @p inserted (@p keeps), or
    ///        take it from the Workshop
    void keepOrRemove(const core::ProjectElement& clipping, const core::ClippingData& inserted,
                      bool keeps);

    /// @brief The notice after inserting the clipping @p title of @p words; @p keeps: it stays
    ///        in the Workshop; @p undoKeys: the keys of Undo
    [[nodiscard]] static QString insertedMessage(const QString& title, int words, bool keeps,
                                                 const QString& undoKeys);

    /// @brief @p clipping as the window and the list of Insert > Clipping show it
    [[nodiscard]] ClippingEntry entryOf(const core::ProjectElement& clipping) const;

    /// @brief Keep @p step, followed in the undo of its text
    void addStep(Step step);

    /// @brief Follow the undo of @p text and its document: its steps undone and redone
    void follow(editor::BookEditor* text);

    /// @brief Stop following @p text, and forget its steps (its document is another one);
    ///        the texts destroyed too (null: only those)
    void unfollow(const editor::BookEditor* text);

    /// @brief The text of @p text changed: its steps undone or redone take their clippings
    ///        with them
    void textChanged(editor::BookEditor* text);

    /// @brief Bring the next step of @p text undone or redone to its new state
    /// @return false when no step of @p text changed
    bool applyNextStep(editor::BookEditor* text);

    /// @brief A new level of undo in @p text: its steps undone can no longer be redone
    void undoAdded(const editor::BookEditor* text);

    /// @brief The step with @p stepId; nullptr when it is forgotten
    [[nodiscard]] Step* stepWithId(int stepId);

    /// @brief Forget the step with @p stepId
    void forgetStep(int stepId);

    /// @brief Save the text of @p text to its file, so that the file and the Workshop agree
    void saveText(const editor::BookEditor* text);

    /// @brief Bring the clipping of @p step to its state when done or undone
    /// @return false when the clipping changed since and stays (the step is then forgotten)
    bool applyState(Step& step, bool done);

    /// @brief Add the clipping of @p step again, with @p data, and follow its new id
    /// @return false when it could not be added (the status bar says so)
    bool bringBack(Step& step, const core::ClippingData& data);

    /// @brief Take @p clipping, the clipping of @p step, from the Workshop, unless it changed
    ///        since
    /// @return false when it stays
    bool takeAway(const Step& step, const core::ProjectElement& clipping);

    /// @brief Whether the clipping of @p step is as the step left it (its title, text, note)
    [[nodiscard]] bool isAsLeft(const Step& step) const;

    /// @brief Take clipping @p clippingId from the Workshop with its file
    bool removeClipping(const QString& clippingId);

    /// @brief Add the clipping of @p step again
    /// @return Its new id; empty when it could not be added
    QString addAgain(const Step& step, const core::ClippingData& data);

    /// @brief The element of the text of @p text in the book; nullptr for a file outside it
    [[nodiscard]] const core::ProjectElement* elementOf(const editor::BookEditor* text) const;

    /// @brief Why the text of @p text cannot have clippings cut from it or inserted into it
    ///        (empty: it can)
    [[nodiscard]] QString whyNotText(const editor::BookEditor* text) const;

    /// @brief The tab of an editor of the book
    [[nodiscard]] EditorPanel* panelOf(const editor::BookEditor* text) const;

    /// @brief The editor of the open text of element @p elementId
    [[nodiscard]] editor::BookEditor* editorOf(const QString& elementId) const;

    /// @brief Open element @p elementId in the editor
    /// @return Its editor; nullptr when it could not be opened
    editor::BookEditor* open(const QString& elementId);

    /// @brief The keys of Edit > Undo as the writer sees them ("Ctrl+Z")
    [[nodiscard]] QString undoKeys() const;

    /// @brief Short message in the status bar
    void say(const QString& message) const;

    /// @brief Forget all the steps (another book)
    void forgetSteps();

    core::ProjectManager& m_book;                 ///< The book of its window
    CommandRegistry& m_commands;                  ///< The commands of its window
    QTabWidget* m_centralTabs = nullptr;          ///< The tabs of the open texts
    NavigatorCoordinator* m_navigator = nullptr;  ///< Refreshes the tree, saves open texts
    TextWithKeys m_textWithKeys;                  ///< The text that has the keyboard
    NoticeShower m_showNotice;                    ///< Says under a text what was done
    QStatusBar* m_statusBar = nullptr;            ///< Where short messages go (may be null)
    QWidget* m_window = nullptr;                  ///< Over which windows and warnings open

    /// The cuts and insertions their texts can undo or redo (a list: a step stays where it is
    /// while others go)
    std::list<Step> m_steps;
    int m_nextStepId = 1;       ///< The id of the next step
    int m_lastStepId = 0;       ///< The last cut or insertion (the Undo of its notice)
    /// The texts followed, each with its connections
    QList<QPair<QPointer<editor::BookEditor>, QList<QMetaObject::Connection>>> m_followed;
    bool m_changing = false;  ///< The coordinator changes a text itself: no step follows it
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_CLIPPINGS_COORDINATOR_H
