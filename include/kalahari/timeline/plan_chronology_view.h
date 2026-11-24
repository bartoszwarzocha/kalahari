// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plan_chronology_view.h
/// @brief The chronology of the Book Plan: the scenes of the book in the order of the story,
/// with the events outside the text, in bands of the points of view or of the places

#ifndef KALAHARI_TIMELINE_PLAN_CHRONOLOGY_VIEW_H
#define KALAHARI_TIMELINE_PLAN_CHRONOLOGY_VIEW_H

#include <kalahari/gui/dialogs/kalahari_dialog.h>
#include <kalahari/gui/plan_chronology.h>
#include <kalahari/timeline/plan_chronology_layout.h>
#include <kalahari/timeline/timeline.h>
#include <kalahari/timeline/timeline_look.h>

#include <QAbstractScrollArea>
#include <QRectF>
#include <QStringList>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

class QAction;
class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPainter;
class QPlainTextEdit;
class QTableWidget;
class QToolButton;

namespace kalahari::timeline {

/// @brief The cards of the chronology, painted: the times in columns, the bands in rows, the
/// cards without a time under them
///
/// The arrows choose a card (Left and Right in its band, Up and Down in its column and to the
/// band above or below), Home and End the first and the last of the band. What the keys and
/// the mouse do with a card is the view's (PlanChronologyView): it gets activated() for Enter
/// and a double click, menuRequested() for the menu.
class PlanChronologyGrid : public QAbstractScrollArea {
    Q_OBJECT

public:
    static constexpr double HEAD_WIDTH = 128.0;    ///< The names of the bands
    static constexpr double CARD_WIDTH = 156.0;
    static constexpr double CARD_SPACING = 6.0;
    static constexpr double PADDING = 8.0;
    static constexpr double GAP_WIDTH = 34.0;      ///< A gap between two times

    explicit PlanChronologyGrid(QWidget* parent = nullptr);

    /// @brief Show @p layout; @p noLaneName names the band of the cards without a point of view
    /// (a place), @p oneLaneName the one band
    void setChronology(std::shared_ptr<const ChronologyLayout> layout, const TimelineLook& look,
                       const QString& noLaneName, const QString& oneLaneName);
    [[nodiscard]] const ChronologyLayout* chronology() const { return m_layout.get(); }

    /// @brief The card chosen (an index of the layout); -1: none
    [[nodiscard]] int current() const { return m_current; }
    void setCurrent(int item);

    /// @brief Where card @p item is, in the coordinates of the content
    [[nodiscard]] QRectF rectOf(int item) const;

    /// @brief The card at @p point of the viewport; -1: none
    [[nodiscard]] int itemAt(const QPointF& point) const;

    /// @brief What the line of the cards without a time says before them
    void setUntimedTitle(const QString& title) { m_untimedTitle = title; }

signals:
    void currentChanged();
    /// @brief Enter or a double click on card @p item
    void activated(int item);
    /// @brief The menu of card @p item (-1: none) at @p globalPos
    void menuRequested(int item, const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    /// A row of the keys: the cards of a band left to right, or the cards without a time
    [[nodiscard]] QList<int> rowOf(int item) const;
    [[nodiscard]] QList<QList<int>> rows() const;
    void moveInRow(int step, bool toEnd);
    void moveToRow(int step);

    void layOut();
    void updateScrollBars();
    [[nodiscard]] QPointF offset() const;
    void paintCard(QPainter& painter, int item, const QRectF& rect) const;
    void paintHeads(QPainter& painter) const;

    std::shared_ptr<const ChronologyLayout> m_layout;
    TimelineLook m_look;
    QString m_noLaneName;
    QString m_oneLaneName;
    QString m_untimedTitle;
    int m_current = -1;
    double m_headerHeight = 0.0;
    double m_cardHeight = 0.0;
    QList<double> m_columnX;      ///< Of each column, in the content
    QList<double> m_columnWidth;
    QList<double> m_laneY;        ///< Of each band, in the content
    QList<double> m_laneHeight;
    QList<QRectF> m_rects;        ///< Of each card, in the content
    QRectF m_untimedTitleRect;
    double m_width = 0.0;
    double m_height = 0.0;
};

/// @brief The window of a card of the chronology: its title, when, the point of view, the
/// place and the description
///
/// Under "When" it says at once what the program reads in it, as the window "Event" of a
/// timeline does; a time it does not read leaves the card without a place in time.
class PlanCardDialog : public gui::dialogs::KalahariDialog {
    Q_OBJECT

public:
    /// @param outside An event outside the text, else a card of the book
    PlanCardDialog(const gui::PlanChronologyEdit& card, bool outside, qint64 dayOne,
                   const QList<PlanPeriod>& periods, const QStringList& povs,
                   const QStringList& places, const TimelineLook& look,
                   QWidget* parent = nullptr);

    [[nodiscard]] gui::PlanChronologyEdit result() const;

private:
    void updateNote();

    qint64 m_dayOne;
    QList<PlanPeriod> m_periods;
    TimelineLook m_look;
    QLineEdit* m_title = nullptr;
    QLineEdit* m_when = nullptr;
    QLabel* m_note = nullptr;
    QLineEdit* m_pov = nullptr;
    QLineEdit* m_place = nullptr;
    QPlainTextEdit* m_description = nullptr;
};

/// @brief The window "Periods": day one of the story and the periods the writer names
/// ("Wyprawa" is "zima 1931"), which a time of the action may say
class PlanPeriodsDialog : public gui::dialogs::KalahariDialog {
    Q_OBJECT

public:
    PlanPeriodsDialog(qint64 dayOne, const QList<PlanPeriod>& periods, const TimelineLook& look,
                      QWidget* parent = nullptr);

    /// @brief Julian day of day one, as the window leaves it
    [[nodiscard]] qint64 dayOne() const { return m_dayOne; }
    /// @brief The periods with a name and a time the program reads, in their order
    [[nodiscard]] QList<PlanPeriod> periods() const;

private:
    void addRow(const QString& name, const QString& when);
    void updateNote();

    TimelineLook m_look;
    qint64 m_dayOne;
    QLineEdit* m_day = nullptr;
    QLabel* m_note = nullptr;
    QTableWidget* m_periods = nullptr;
    QLabel* m_periodsNote = nullptr;
};

/// @brief The chronology of the Book Plan, as the module of the timelines gives it to the board
///
/// Over the cards: the scale (days of the story, dates, the order of the story), the bands
/// (points of view, places, one), Shorten Gaps and Periods..., then the commands: a new event
/// outside the text (Insert), Edit the Card (F2), Open the Text (Enter), a day earlier and later
/// (Ctrl+Left, Ctrl+Right), Delete (an event, or the time of the action of a card), Undo and
/// Redo. Under the cards a note says what the numbers and the frames mean.
class PlanChronologyView : public gui::PlanChronology {
    Q_OBJECT

public:
    /// @brief The commands of the bar
    enum class Command { NewEvent, Edit, Open, Earlier, Later, Delete, Undo, Redo, Count };

    explicit PlanChronologyView(QWidget* parent = nullptr);

    void setCards(const QList<gui::PlanChronologyCard>& cards) override;
    void setPlan(const QJsonObject& plan) override;
    void setReadOnly(bool readOnly) override;
    void setUndoState(bool canUndo, bool canRedo) override;
    [[nodiscard]] QString currentCard() const override;
    bool setCurrentCard(const QString& id) override;
    [[nodiscard]] QString hint() const override;
    void focusCards() override;

    [[nodiscard]] PlanChronologyGrid* grid() const { return m_grid; }
    [[nodiscard]] QAction* command(Command which) const;
    /// @brief The plan of the time as the view has it now, as its file keeps it
    [[nodiscard]] QJsonObject plan() const;
    /// @brief What the commands do, as the keys and the menu ask
    void runCommand(Command which);

    /// @brief The new time of @p text one step later (@p steps > 0) or earlier: a day, a
    /// month, a season, a half of a year or a year, as exactly as the time is known; written as
    /// the writer wrote it, in days of the story or in dates; nullopt for a text the program
    /// does not read
    [[nodiscard]] static std::optional<QString> shiftedTime(const QString& text, int steps,
                                                            qint64 dayOne,
                                                            const QList<PlanPeriod>& periods,
                                                            const StoryTimeStyle& style);

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildOptions(QWidget* row);
    void buildCommands(QWidget* bar);
    void refresh();
    void updateCommands();
    void showMenu(int item, const QPoint& globalPos);
    void editCard(int item);
    void addEvent();
    void shift(int steps);
    void removeCurrent();
    void editPeriods();
    void changePlan(const std::function<void(Timeline&, QList<PlanPeriod>&)>& change,
                    const QString& undoText, const QString& message);
    void changeView(const std::function<void(TimelineView&)>& change);
    [[nodiscard]] const ChronologyItem* currentItem() const;
    [[nodiscard]] QString eventIdOf(const ChronologyItem& item) const;
    [[nodiscard]] QStringList suggestions(bool places) const;

    QList<gui::PlanChronologyCard> m_cards;
    Timeline m_plan;
    QList<PlanPeriod> m_periods;
    TimelineLook m_look;
    std::shared_ptr<ChronologyLayout> m_layout;
    bool m_readOnly = false;
    bool m_canUndo = false;
    bool m_canRedo = false;
    QString m_wanted;  ///< The card to choose again after the cards change

    QComboBox* m_scale = nullptr;
    QComboBox* m_lanes = nullptr;
    QToolButton* m_compress = nullptr;
    QToolButton* m_periodsButton = nullptr;
    QLabel* m_totals = nullptr;
    PlanChronologyGrid* m_grid = nullptr;
    QLabel* m_note = nullptr;
    std::array<QAction*, static_cast<std::size_t>(Command::Count)> m_commands{};
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_PLAN_CHRONOLOGY_VIEW_H
