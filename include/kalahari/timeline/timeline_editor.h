// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_editor.h
/// @brief The tab of a timeline: its bar, the axis, the list or the time and the book, and the
/// bar of messages under them

#ifndef KALAHARI_TIMELINE_TIMELINE_EDITOR_H
#define KALAHARI_TIMELINE_TIMELINE_EDITOR_H

#include <kalahari/core/window_editor_provider.h>
#include <kalahari/timeline/timeline.h>
#include <kalahari/timeline/timeline_index.h>

#include <QString>
#include <QWidget>

#include <array>
#include <functional>

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QStackedWidget;
class QTimer;
class QToolButton;

namespace kalahari::gui {
class InfoBar;
}  // namespace kalahari::gui

namespace kalahari::timeline {

class TimelineAxisView;
class TimelineBookView;
class TimelineDocument;
class TimelineList;

/// @brief The tab of a timeline of the Workshop
///
/// +----------------------------------------------------------------------------------+
/// | [+ New Event] [Event…] [Chapter]  [<] [>] [^] [v]  [x Delete the Event]  [<-] [->]|
/// | View: [Axis|List|Time and book]  Scale: [Dates v]  Lanes: [Threads v]            |
/// | [Cards|Labels]  [x] Compress gaps  Highlight: [nothing v]  [-] 100% [+]          |
/// | [Day One and Threads…]                            14 events · 3 threads  [gear]  |
/// +----------------------------------------------------------------------------------+
/// | the axis, the list, or the time and the book                                     |
/// +----------------------------------------------------------------------------------+
/// | [i] Moved “Kompas przy studni”: 17–18 kwietnia 1931.             [Undo]  [X]     |
/// +----------------------------------------------------------------------------------+
///
/// The commands of the bar (gui::CardBar) are those of the menus and the keys of the views:
/// New Event (Insert; in the time and the book the window "Event"), the window "Event"
/// (Enter), Open the Chapter (Ctrl+Enter), on the axis Earlier and Later (Ctrl+Left,
/// Ctrl+Right) and the lane above and below (Ctrl+Up, Ctrl+Down), Delete the Event (Delete),
/// Undo (Ctrl+Z) and Redo (Ctrl+Y); each is greyed out where it can do nothing.
///
/// Each change is saved to the file of the timeline at once; the bar under it says what was
/// done, with Undo where it can be undone. The bar above wraps on a narrow window.
class TimelineEditor : public QWidget {
    Q_OBJECT

public:
    /// @brief The tab of element @p context.elementId
    TimelineEditor(const core::WindowEditorContext& context, QWidget* parent = nullptr);
    ~TimelineEditor() override;

    TimelineEditor(const TimelineEditor&) = delete;
    TimelineEditor& operator=(const TimelineEditor&) = delete;
    TimelineEditor(TimelineEditor&&) = delete;
    TimelineEditor& operator=(TimelineEditor&&) = delete;

    [[nodiscard]] TimelineDocument& document() { return *m_document; }
    [[nodiscard]] TimelineAxisView* axis() const { return m_axis; }
    [[nodiscard]] TimelineList* list() const { return m_list; }
    [[nodiscard]] TimelineBookView* bookView() const { return m_bookView; }
    [[nodiscard]] const TimelineIndex& index() const { return m_index; }

    /// @brief Say @p message in the bar under the timeline; with @p undoable it offers Undo
    void tell(const QString& message, bool undoable);

    /// @brief Open the window "Event" of event @p id
    /// @return Whether the window was closed with OK
    bool editEvent(const QString& id);
    /// @brief Open the window of day one and the threads
    void editTimeline();

    /// @brief The commands of the bar (New Event, Event…, Open the Chapter, Earlier, Later,
    ///        To the Lane Above, To the Lane Below, Delete the Event, Undo, Redo)
    enum class Command { New, Edit, Chapter, Earlier, Later, LaneUp, LaneDown, Delete, Undo, Redo };
    [[nodiscard]] QAction* command(Command command) const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void buildBar(QWidget* bar);
    void buildCommands(QWidget* bar);
    void updateCommands();
    [[nodiscard]] TimelineView::Mode shownMode() const;
    void addEvent();
    void deleteEvent();
    void undo(bool back);
    void finishTyping();
    void showMode(TimelineView::Mode mode, bool keep);
    void applyLook();
    void updateBar();
    void fillHighlight();
    void refreshIndex();
    void openChapter(const QString& id);
    void changeScale(TimelineView::Scale scale);

    core::ProjectManager& m_book;
    QString m_elementId;
    std::function<void()> m_openSettings;
    std::function<void(const QString&)> m_openElement;
    TimelineIndex m_index;
    TimelineDocument* m_document = nullptr;
    TimelineAxisView* m_axis = nullptr;
    TimelineList* m_list = nullptr;
    TimelineBookView* m_bookView = nullptr;
    QStackedWidget* m_stack = nullptr;
    QToolButton* m_axisButton = nullptr;
    QToolButton* m_listButton = nullptr;
    QToolButton* m_bookButton = nullptr;
    QComboBox* m_scale = nullptr;
    QWidget* m_lanesBox = nullptr;
    QComboBox* m_lanes = nullptr;
    QWidget* m_lookBox = nullptr;
    QToolButton* m_cardsButton = nullptr;
    QToolButton* m_labelsButton = nullptr;
    QCheckBox* m_compress = nullptr;
    QComboBox* m_highlight = nullptr;
    QWidget* m_zoomBox = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QLabel* m_counts = nullptr;
    QToolButton* m_gear = nullptr;
    QWidget* m_commands = nullptr;      ///< The row of the commands, above the view's
    QWidget* m_moveBox = nullptr;       ///< Earlier, Later and the lanes: on the axis only
    std::array<QAction*, 10> m_actions{};  ///< By Command
    gui::InfoBar* m_info = nullptr;
    QTimer* m_infoTimer = nullptr;
    int m_settingsListener = -1;
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_EDITOR_H
