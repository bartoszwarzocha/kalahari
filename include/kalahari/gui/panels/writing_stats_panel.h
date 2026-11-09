/// @file writing_stats_panel.h
/// @brief The Statistics panel: a list of small graphs of the writing, as in the Performance
/// tab of the Task Manager of Windows 11, and the view chosen in it, large
///
/// The views are today, the week, 30 days, the book and the times of writing. Each one has a
/// large graph on a fine grid, big numbers and a few details in plain words. Everything fits
/// the narrowest panel: the graphs are as wide as the panel, the numbers go to as many columns
/// as fit, and a detail that does not fit beside its name goes under it.

#pragma once

#include <QPointer>
#include <QWidget>

#include <functional>

class QLabel;
class QListWidget;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace kalahari::gui {

class WritingStatsModel;
class StatsGraph;
class StatsKpiGrid;
class StatsDetails;
class StatsProgress;

/// @brief The Statistics panel
class WritingStatsPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief The views, in the order of the list
    enum class View { Today, Week, Month, Book, Hours };

    /// @brief The height of the large graph
    static constexpr int GRAPH_HEIGHT = 112;
    /// @brief The size of the small graphs of the list
    static constexpr int MINI_WIDTH = 58;
    static constexpr int MINI_HEIGHT = 30;

    explicit WritingStatsPanel(QWidget* parent = nullptr);
    ~WritingStatsPanel() override;

    /// @brief The statistics it shows; nullptr: none (an empty panel)
    void setModel(WritingStatsModel* model);
    WritingStatsModel* model() const;

    /// @brief How the titles of the texts and of the book are found: by the id of a text, and
    /// the title of the book open
    void setTitleSources(std::function<QString(const QString&)> textTitle,
                         std::function<QString()> bookTitle);

    /// @brief The view in front
    View view() const { return m_view; }
    /// @brief Bring a view to the front (also when the settings hide it from the list)
    void setView(View view);

    /// @brief The views of the list, as the settings say (stats.panel.*)
    QList<View> listedViews() const;

    /// @brief The list of the views
    QListWidget* viewList() const { return m_list; }

    /// @brief The week or the 30 days shown: 0 the present ones, -1 those before, ...
    int periodOffset() const { return m_offset; }
    void setPeriodOffset(int offset);

    /// @brief What the view in front shows, for the tests and the screen readers: its title,
    /// its subtitle, its numbers ("Words today: 850") and its details ("Written: 972")
    QString titleText() const;
    QString subtitleText() const;
    QStringList numbers() const;
    QStringList details() const;
    /// @brief The texts of the list of views: "Today: 850 of 1,000 words · 2:35"
    QStringList listTexts() const;

    /// @brief The setting of a view in the list ("stats.panel.today")
    static const char* settingOf(View view);

public slots:
    /// @brief Count everything again and show it
    void refresh();

signals:
    /// @brief The tab Writing Goals and Deadlines was asked for
    void goalsRequested();

protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    struct Context;
    struct ViewData;

    /// @brief What every view counts from, once for each refresh
    Context context() const;
    /// @brief What a view shows
    void viewOf(View view, const Context& context, ViewData& data) const;
    void showToday(const Context& context, ViewData& data) const;
    void showWeek(const Context& context, ViewData& data) const;
    void showMonth(const Context& context, ViewData& data) const;
    void showBook(const Context& context, ViewData& data) const;
    void showHours(const Context& context, ViewData& data) const;
    /// @brief The list of views: those the settings show, with their small graphs and texts
    void fillList(const Context& context);
    /// @brief The view in front
    void fillView(const Context& context);
    void updateFonts();

    QPointer<WritingStatsModel> m_model;
    std::function<QString(const QString&)> m_textTitle;
    std::function<QString()> m_bookTitle;
    View m_view{View::Today};
    int m_offset{0};
    bool m_dirty{true};
    bool m_filling{false};
    QList<View> m_listed;
    QStringList m_listTexts;
    QStringList m_numberTexts;
    QStringList m_detailTexts;

    QListWidget* m_list{nullptr};
    QLabel* m_title{nullptr};
    QLabel* m_subtitle{nullptr};
    QToolButton* m_previous{nullptr};
    QToolButton* m_next{nullptr};
    StatsGraph* m_graph{nullptr};
    QLabel* m_note{nullptr};
    StatsKpiGrid* m_numbers{nullptr};
    StatsProgress* m_progress{nullptr};
    StatsDetails* m_details{nullptr};
    QPushButton* m_goals{nullptr};
};

}  // namespace kalahari::gui
