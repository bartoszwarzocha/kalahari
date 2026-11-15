// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_stats_bar.h
/// @brief The statistics bar above the tabs of the texts: the graph of today's writing, or one
/// line

#pragma once

#include <kalahari/gui/widgets/stats_painter.h>
#include <kalahari/gui/writing_stats_model.h>

#include <QList>
#include <QWidget>

#include <utility>

class QToolButton;

namespace kalahari::gui {

/// @brief The statistics bar
///
/// One graph on the whole bar, from the top to the bottom, in layers: the grid of the hours,
/// the words of each piece of the day (a thin line with a translucent fill), the words of today
/// adding up toward the dashed line of the daily goal (in the color of the pace of work, or of
/// the goal reached), "now", and in small print the words of today with the goal and the pace
/// of work, the time of writing and the days in a row. The texts and the buttons never cover
/// the data: the hours of the graph end before them, and the grid runs on under them.
///
/// The graph shows the day from the left, filling up toward the right as the day goes on, or
/// "now" in the middle, with the graph moving through it from the right to the left; the
/// button beside the fold button switches them (stats.bar.graphMode).
///
/// Folded, the bar is one line with the same texts and a mini graph (today, the last 7 or 30
/// days; a click goes on to the next), filling up with the color of the pace of work as the
/// words of the day grow.
///
/// The button that folds and unfolds it is always in its top right corner; beside it are the
/// buttons of the goals, the Statistics panel and the settings. The settings
/// "stats.bar.*" say how it looks; it follows them and the colors of the theme.
class WritingStatsBar : public QWidget {
    Q_OBJECT

public:
    /// @brief The least height of the graph: all the texts and buttons fit
    static constexpr int MIN_GRAPH_HEIGHT = 83;
    /// @brief The height of the folded bar
    static constexpr int LINE_HEIGHT = 24;
    /// @brief The size of the buttons
    static constexpr int BUTTON_SIZE = 20;

    /// @brief What the mini graph of the line shows
    enum class MiniGraph { Today, Week, Month };

    /// @brief How the graph shows the day: from the left ("day"), or with now in the middle
    /// and the hours moving to the left ("now")
    enum class GraphMode { Day, NowInMiddle };

    explicit WritingStatsBar(WritingStatsModel* model, QWidget* parent = nullptr);

    /// @brief Read the settings and the numbers again and repaint
    void refresh();

    /// @brief The bar is folded to one line (stats.bar.mode is "line")
    bool isFolded() const { return m_folded; }

    /// @brief What the mini graph shows (stats.bar.miniGraph)
    MiniGraph miniGraph() const { return m_miniGraph; }

    /// @brief How the graph shows the day (stats.bar.graphMode)
    GraphMode graphMode() const { return m_graphMode; }

    /// @brief The texts of the bar, as they are shown now (for the tests and the readers of
    /// the screen): words and the pace of work, time, days in a row
    QStringList texts() const;

    /// @brief What the texts mean, with the numbers of now: the tooltip of the texts
    QString textsToolTip() const;

    /// @brief The minutes of the day at the left and the right edges of the data of the graph
    /// (now in the middle: before the start and after the end of the day too)
    std::pair<double, double> shownMinutes() const { return {m_fromMinute, m_toMinute}; }

    /// @brief Now, in minutes from the start of the writing day
    qint64 nowMinute() const { return m_nowMinute; }

    /// @brief The x of @p minute of the day on the graph
    double xOfMinute(double minute) const;

    /// @brief The rectangle of the data of the graph, without the texts and the buttons
    QRectF plotRect() const { return m_plot; }

    /// @brief The rectangle of the texts on the graph
    QRectF textsRect() const { return m_textsRect; }

    /// @brief The button that folds and unfolds the bar
    QToolButton* foldButton() const { return m_fold; }

    /// @brief The button of the mini graph of the line
    QToolButton* miniGraphButton() const { return m_mini; }

    /// @brief The button that puts now in the middle of the graph, and back (checked: now in
    /// the middle)
    QToolButton* graphModeButton() const { return m_graphModeButton; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// @brief The button of the goals was clicked
    void goalsRequested();
    /// @brief The button of the Statistics panel was clicked
    void panelRequested();
    /// @brief The button of the settings was clicked
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    /// @brief A part of a text in one color
    struct Run {
        QString text;
        QColor color;
        bool bold = false;
    };

    /// @brief One text of the bar, with its mark
    struct Text {
        QList<Run> runs;
        bool hasGlyph = false;
        stats_painter::Glyph glyph = stats_painter::Glyph::Clock;
        QColor glyphColor;

        QString plain() const;
    };

    /// @brief The settings of the bar
    void readSettings();
    /// @brief The numbers of now, from the model, and the texts
    void readNumbers();
    /// @brief The buttons and the places of the parts of the bar
    void layoutParts();
    void updateIcons();
    void paintGraph(QPainter& painter);
    void paintLine(QPainter& painter);
    /// @brief The mini graph of the line and its text, in its button
    void paintMiniGraph(QPainter& painter, const QRect& area) const;
    /// @brief Draw a text with its mark, from @p x on the line of @p baseline
    /// @return The x after it
    double drawText(QPainter& painter, const Text& text, double x, double baseline) const;
    double textWidth(const Text& text) const;
    QFont smallFont(bool bold = false) const;
    /// @brief The height of a row of the texts on the graph
    int rowHeight() const;
    /// @brief The height the graph needs: the settings, the buttons and the texts
    int graphHeight() const;
    /// @brief The piece under @p x on the graph; -1 outside
    int pieceAt(double x) const;
    QString pieceToolTip(int piece) const;
    QString miniGraphText() const;
    /// @brief The texts in the order the line shows them, or the rows of the graph
    QList<const Text*> lineParts() const;
    void cycleMiniGraph();

    // Plain, cleared when the statistics go: clang-tidy's analyzer reports a QPointer as a use
    // of freed memory
    WritingStatsModel* m_model{nullptr};
    QToolButton* m_fold{nullptr};
    QToolButton* m_goals{nullptr};
    QToolButton* m_panel{nullptr};
    QToolButton* m_settings{nullptr};
    QToolButton* m_mini{nullptr};
    QToolButton* m_graphModeButton{nullptr};

    // Settings
    bool m_folded = false;
    int m_graphHeight = MIN_GRAPH_HEIGHT;
    bool m_textsLeft = false;
    bool m_showWords = true;
    bool m_showTime = true;
    bool m_showStreak = true;
    bool m_showPieces = true;
    bool m_showCumulative = true;
    MiniGraph m_miniGraph = MiniGraph::Week;
    GraphMode m_graphMode = GraphMode::Day;
    int m_pieceMinutes = 10;
    int m_fromHour = 6;
    int m_toHour = 24;

    // Numbers
    StatsColors m_colors;
    QList<DayPiece> m_pieces;      ///< The pieces of today
    int m_dayStart = 240;          ///< The minutes of the clock a writing day starts at
    qint64 m_nowMinute = 0;        ///< Now, in minutes from the start of the day
    int m_firstPiece = 0;          ///< The first piece on the graph
    int m_endPiece = 0;            ///< The piece after the last on the graph
    int m_dayFirstPiece = 0;       ///< The first piece of the hours of the settings
    double m_fromMinute = 0;       ///< The minute of the day at the left edge of the data
    double m_toMinute = 0;         ///< At the right edge
    int m_words = 0;               ///< The words of today that count
    int m_goal = 0;                ///< The daily goal of today; 0: none
    qint64 m_activeMs = 0;
    int m_streak = 0;
    core::WritingTemperature m_temperature = core::WritingTemperature::Unknown;
    QList<int> m_miniValues;
    int m_miniSum = 0;
    core::GoalCounting m_counting = core::GoalCounting::Net;
    QColor m_stateColor;           ///< The pace of work, or the goal reached
    QList<Text> m_texts;           ///< The rows of the texts on the graph
    Text m_lineText;               ///< The words and the pace of work in one line

    // Places
    QRectF m_plot;
    QRectF m_textsRect;
    QRect m_miniRect;
};

}  // namespace kalahari::gui
