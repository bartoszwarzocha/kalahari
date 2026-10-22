/// @file versions_coordinator.h
/// @brief The versions of the texts of the book: File > Save Version and File > Version
/// History, from the text in front, the Navigator and the Properties panel

#pragma once

#include <kalahari/core/chapter_versions.h>

#include <QObject>
#include <QString>

#include <functional>

class QStatusBar;
class QTabWidget;
class QWidget;

namespace kalahari::gui {

class EditorPanel;

/// @brief Keeps the texts of the book as versions and brings them back
///
/// Save Version (Alt+Shift+S) keeps the text in front as it is in its editor, its changes not
/// saved yet too, as the writer's own version: after a small window with its description, or
/// at once when the setting editor.versions.askName says so; the status bar says it is kept.
/// Version History (Alt+Shift+H) shows the versions of the text in front
/// (dialogs::VersionHistoryDialog). Both work for the texts of the book with a file in its
/// folder; for any other document they are greyed out and say why (in their tooltips, and in
/// the status bar after their keys). In a book open read-only only the history works, to read
/// and compare.
///
/// What the history window chose is done in the text's editor, each as one step of its undo
/// history: a version restored takes the place of the text, after the text was kept as a
/// version (VersionOrigin::Restore), and the bar under the text says so, with Undo, until the
/// text changes; a fragment of a version is inserted at the cursor and selected, and the
/// status bar says so.
class VersionsCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Opens a text of the book in a tab of its own, or brings its tab to the front
    using ChapterOpener = std::function<void(const QString& elementId)>;

    /// @brief Says something under the text of a tab, with a button
    /// (DocumentCoordinator::showNotice())
    using NoticeShower = std::function<void(EditorPanel* editor, const QString& message,
                                            const QString& actionText,
                                            const QString& actionToolTip,
                                            std::function<void()> action)>;

    /// @brief Constructor
    /// @param centralTabs The tabs of the documents
    /// @param openChapter Opens a text of the book (none: only texts open in tabs)
    /// @param showNotice Says that a version was restored (none: nothing says it)
    /// @param statusBar Where short messages go (may be null)
    /// @param window The window of the program, over which the windows of the versions open
    /// @param parent Parent object
    VersionsCoordinator(QTabWidget* centralTabs, ChapterOpener openChapter,
                        NoticeShower showNotice, QStatusBar* statusBar, QWidget* window,
                        QObject* parent = nullptr);

    /// @brief Give Save Version and Version History their callbacks
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief Why Save Version cannot be used for the text in front; empty: it can
    [[nodiscard]] QString whyNotSave() const;

    /// @brief Why Version History cannot be used for the text in front; empty: it can
    [[nodiscard]] QString whyNotShowHistory() const;

    /// @brief Save Version: keep the text of element @p elementId as a version, as it is in
    /// its editor
    /// @param elementId The text; empty: the text in front. Its tab is brought to the front,
    ///        the text opened when it is not.
    /// @param parent Over which the window of its description opens (null: the window of the
    ///        program)
    /// @return Whether the version is kept
    bool saveVersion(const QString& elementId = QString(), QWidget* parent = nullptr);

    /// @brief Version History: the window of the versions of element @p elementId, and what
    /// the writer chose in it done
    /// @param elementId The text; empty: the text in front. Its tab is brought to the front,
    ///        the text opened when it is not: a version restored goes into its editor, where
    ///        Ctrl+Z undoes it.
    /// @return Whether the window was shown
    bool showHistory(const QString& elementId = QString());

    /// @brief Bring version @p version of the text of @p editor back: the text as it is now
    /// is kept as a version first, then the version takes its place, as one step of its undo
    /// history
    /// @param kml The text of the version
    /// @return false in a book open read-only, and when the text as it is now cannot be kept
    ///         (it says why)
    bool restoreVersion(EditorPanel* editor, const core::ChapterVersion& version,
                        const QString& kml);

    /// @brief Insert @p fragment (KML) of version @p version at the cursor of @p editor, in
    /// place of its selection, as one step of its undo history; the fragment is selected then
    /// @return false in a book open read-only
    bool insertFragment(EditorPanel* editor, const core::ChapterVersion& version,
                        const QString& fragment);

private:
    /// @brief The tab in front, when it has an editor
    EditorPanel* currentPanel() const;

    /// @brief The tab of element @p elementId, if it is open (empty: the tab in front)
    EditorPanel* openPanel(const QString& elementId) const;

    /// @brief The tab of element @p elementId in front, the text opened when it is not
    /// (empty: the tab in front)
    EditorPanel* panelFor(const QString& elementId);

    /// @brief The element of the text of @p panel; empty: a document outside the book
    static QString elementIdOf(const EditorPanel* panel);

    /// @brief Why the versions of the text of @p panel cannot be kept (@p saving) or shown;
    /// empty: they can
    QString whyNot(const EditorPanel* panel, bool saving) const;

    /// @brief Save Version for the text of @p panel, the window of its description over
    /// @p parent
    bool saveVersionOf(EditorPanel* panel, QWidget* parent);

    /// @brief Show @p message in the status bar for a while
    void say(const QString& message) const;

    /// @brief Enable the commands for the tab in front
    void updateCommandStates() const;

    QTabWidget* m_centralTabs;
    ChapterOpener m_openChapter;
    NoticeShower m_showNotice;
    QStatusBar* m_statusBar;
    QWidget* m_window;
};

}  // namespace kalahari::gui
