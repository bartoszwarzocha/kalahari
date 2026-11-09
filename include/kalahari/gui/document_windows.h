/// @file document_windows.h
/// @brief The texts of the book window in its tabs and in the windows of their own the tabs are
/// moved to

#pragma once

#include <QList>
#include <QMainWindow>
#include <QObject>
#include <QPoint>
#include <QString>

#include <functional>
#include <optional>

class QAction;
class QCloseEvent;
class QEvent;
class QIcon;
class QLabel;
class QMenu;
class QTabBar;
class QTabWidget;
class QToolBar;

namespace kalahari {
namespace gui {

class CommandRegistry;
class DetachedWindow;
class StatusBarStatistics;

/// @brief The texts of the book window: its tabs and the windows their tabs are moved to
///
/// A tab of a text (an editor) goes to a window of its own when it is dragged off the bar of
/// the tabs, from the menu of its tab or with Window > Move Tab to New Window. Such a window
/// (DetachedWindow) has tabs of its own, so more texts can go there; a tab dragged onto a bar of
/// tabs of the book, or Return to Book Window, takes the text back. The panels stay in the
/// book window and show the text the keys went to last ("the page in front"), wherever it is.
///
/// The code that looks for the open texts asks pages() instead of the tabs of the book window,
/// and the one that works on the text in front asks frontPage(). One object per tabs of a book
/// window, made at the first of().
class DocumentWindows : public QObject {
    Q_OBJECT

public:
    /// @brief What the window of its own of a tab shows besides the text
    enum class Look {
        Full,     ///< The menus, the toolbars of the book window and the status bar (default)
        Writing   ///< The Format toolbar and the status bar
    };

    /// @brief What closing a window of its own does to its texts
    enum class Closing {
        Return,  ///< They go back to the book window (default)
        Close    ///< They close, as their tabs would, asking about unsaved changes
    };

    /// @brief The key of the look of the windows of their own (Look)
    static constexpr const char* LOOK_KEY = "window.detached.look";
    /// @brief The key of what closing a window of its own does (Closing)
    static constexpr const char* CLOSING_KEY = "window.detached.close";

    /// @brief The texts of the book window whose tabs are @p bookTabs (made at the first call)
    static DocumentWindows& of(QTabWidget& bookTabs);

    /// @brief The look of the windows of their own, from the settings
    static Look look();

    /// @brief What closing a window of its own does, from the settings
    static Closing closing();

    // ------------------------------------------------------------------------
    // The pages and their tabs
    // ------------------------------------------------------------------------

    /// @brief The pages of the tabs of the book window, then those of each window of its own
    QList<QWidget*> pages() const;

    /// @brief The page the keys went to last: the one in front of its tabs (none: no tab)
    QWidget* frontPage() const;

    /// @brief The tabs whose page is in front (the book's at first)
    QTabWidget* frontTabs() const;

    /// @brief The tabs of the book window
    QTabWidget& bookTabs() const { return *m_bookTabs; }

    /// @brief The windows of their own of the tabs, in the order they were made
    QList<DetachedWindow*> windows() const;

    /// @brief Bring @p page to the front of its tabs, and its window to the front
    void showPage(QWidget* page);

    /// @brief Whether @p page is in a window of its own
    bool isDetached(const QWidget* page) const;

    /// @brief Whether @p page can go to a window of its own: a text (editor)
    static bool canDetach(const QWidget* page);

    /// @brief The tabs @p page is in (none: in no tabs)
    static QTabWidget* tabsOf(const QWidget* page);

    /// @brief The text of the tab of @p page (empty: in no tabs)
    static QString tabText(const QWidget* page);

    /// @brief Set the text of the tab of @p page
    static void setTabText(QWidget* page, const QString& text);

    /// @brief Set the icon of the tab of @p page
    static void setTabIcon(QWidget* page, const QIcon& icon);

    /// @brief Take @p page out of its tabs, the keys going to the page in front
    /// (utils::removeTabKeepingFocus()); the page is not deleted
    static void removePage(QWidget* page);

    // ------------------------------------------------------------------------
    // Moving the tabs
    // ------------------------------------------------------------------------

    /// @brief Move @p page to a new window of its own, its title bar at @p at (none: next to
    /// the window it was in); its window comes to the front with the keys
    /// @return The new window; none when the page cannot go to one
    DetachedWindow* detach(QWidget* page, std::optional<QPoint> at = std::nullopt);

    /// @brief Move @p page to @p tabs at @p index (-1: last), in front with the keys
    void moveTo(QWidget* page, QTabWidget& tabs, int index = -1);

    /// @brief Move @p page back to the tabs of the book window
    void returnToBook(QWidget* page);

    /// @brief Bring the next window of the book (+1) or the previous one (-1) to the front:
    /// the book window, then its windows of their own
    void activateNextWindow(int step);

    // ------------------------------------------------------------------------
    // The Window menu
    // ------------------------------------------------------------------------

    /// @brief Register the commands of the Window menu in @p registry; they work on the texts
    /// @p windows gives when they run (none: they do nothing)
    /// @return The number of commands registered
    static int registerCommands(CommandRegistry& registry,
                                const std::function<DocumentWindows*()>& windows);

    /// @brief List the windows of the book in @p windowMenu each time it opens, before Window
    /// Settings...
    void listWindowsIn(QMenu* windowMenu);

signals:
    /// @brief The page in front changed (none: no tab)
    void frontChanged(QWidget* page);

    /// @brief The tab of @p page is to close, as a click on its cross asks: in a window of its
    /// own, or the window closing with Closing::Close
    void closeRequested(QWidget* page);

    /// @brief Window > Window Settings... or the gear of a window of its own: Settings > General
    void settingsRequested();

    /// @brief A window of its own was made or closed
    void windowsChanged();

protected:
    /// @brief The mouse on a bar of tabs: a tab dragged off it, a drop on it, its menu
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    explicit DocumentWindows(QTabWidget& bookTabs);

    /// @brief Whether the tabs of the book window are whole: their window, destroyed, destroys
    /// their pages and their bar after they are no longer a QTabWidget
    bool isWhole() const;

    /// @brief The tabs of the book window (while they are whole) and those of the windows of
    /// their own
    QList<QTabWidget*> allTabs() const;

    /// @brief Watch the bar of @p tabs: dragging off, drops, its menu, the page in front
    void watch(QTabWidget& tabs);

    /// @brief @p tabs became the ones in front: say so when the page in front changed
    void setFrontTabs(QTabWidget* tabs);

    /// @brief Say when the page in front of the tabs in front changed
    void updateFront();

    /// @brief The keys went to @p now: its tabs are the ones in front
    void onFocusChanged(QWidget* now);

    /// @brief Drag the tab at @p index off the bar of @p tabs
    void dragTab(QTabWidget& tabs, int index);

    /// @brief The menu of the tab at @p index of @p tabs at @p globalPos
    void showTabMenu(QTabWidget& tabs, int index, const QPoint& globalPos);

    /// @brief The windows of the book: the book window, then its windows of their own
    QList<QWidget*> bookWindows() const;

    QTabWidget* m_bookTabs;
    QTabWidget* m_frontTabs;               ///< Its window may be gone: see frontTabs()
    const QWidget* m_frontPage = nullptr;  ///< The page frontChanged() named last, to compare
    bool m_frontSaid = false;              ///< frontChanged() was emitted

    // A tab pressed on a bar: dragging it off the bar takes it to a window of its own
    const QTabBar* m_pressedBar = nullptr;  ///< To compare
    int m_pressedIndex = -1;
    QPoint m_pressPos;
    QWidget* m_draggedPage = nullptr;  ///< The page of the tab being dragged
    bool m_dropped = false;            ///< The tab being dragged was dropped on a bar of tabs

    QList<QAction*> m_windowActions;  ///< The windows listed in the Window menu
};

/// @brief A window of its own of tabs of the book window
///
/// Its title is that of the text in front and of the book window. With Look::Full it has the
/// menus of the book window and its toolbars that are shown, with Look::Writing the Format
/// toolbar; the commands are those of the book window, so their keys work here too. The
/// status bar shows the page, the zoom and the counts of the text in front. The gear at the
/// end of the bar of the tabs opens Settings > General at its options. It closes when its last
/// tab goes.
class DetachedWindow : public QMainWindow {
    Q_OBJECT

public:
    /// @brief A window of the book window @p bookWindow, whose texts @p owner keeps
    DetachedWindow(DocumentWindows& owner, QMainWindow& bookWindow);
    ~DetachedWindow() override;

    /// @brief Its tabs
    QTabWidget& tabs() const { return *m_tabs; }

    /// @brief The title, the status bar and the counts follow the text in front
    void followFront();

    /// @brief Whether it is open: not closing
    bool isOpen() const { return !m_closing; }

    /// @brief None: its toolbars are those of the book window, whose menu shows them
    QMenu* createPopupMenu() override;

public slots:
    /// @brief Show the menus and the toolbars of the look in the settings
    void applyLook();

protected:
    /// @brief The texts go back to the book window, or close (DocumentWindows::closing())
    void closeEvent(QCloseEvent* event) override;

private:
    /// @brief The page and the zoom of the text in front in the status bar
    void updatePageStatus();

    DocumentWindows& m_owner;
    QMainWindow& m_bookWindow;
    QTabWidget* m_tabs;
    QList<QToolBar*> m_toolbars;  ///< Mirrors of the toolbars of the book window
    QLabel* m_pageLabel;
    QLabel* m_zoomLabel;
    StatusBarStatistics* m_statistics;
    const QWidget* m_followed = nullptr;  ///< The page whose editor the status bar follows
    QList<QMetaObject::Connection> m_followConnections;  ///< To its editor
    int m_settingsListener = -1;
    bool m_closing = false;        ///< It is closing: its texts went back or closed
    bool m_closingTexts = false;   ///< Its texts are being closed by its closing
};

} // namespace gui
} // namespace kalahari
