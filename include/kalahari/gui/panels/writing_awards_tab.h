/// @file writing_awards_tab.h
/// @brief The tab Challenges and Badges in the middle of the window: the writer's challenges
/// and the badges in three parts, the writer's own, the open book's and the month's
///
/// Every threshold of a kind is a badge of its own: won, it has the color of its level
/// (bronze, silver, gold, platinum); not yet, it is grey with a dashed ring. Under each a bar
/// shows how far it is. The tooltip of a badge says what it counts and when it was won.

#pragma once

#include <QString>
#include <QWidget>

#include <functional>

class QBoxLayout;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace kalahari::gui {

class WritingAwardsTracker;
class WritingStatsModel;

namespace awards_tab {
class BadgeGrid;
class ChallengeList;
class LevelKey;
}  // namespace awards_tab

/// @brief The tab Challenges and Badges (Tools > Challenges and Badges)
///
/// It shows what the tracker has and follows it. The challenges are added with Add Challenge
/// and removed with Remove or Delete, after asking. Start Over makes the badges of the book
/// count again from today (WritingAwardsTracker::startBookOver()), after asking.
class WritingAwardsTab : public QWidget {
    Q_OBJECT

public:
    /// @brief The icon of the tab (ArtProvider)
    static constexpr const char* ICON_ID = "tools.challenges";

    /// @param tracker The badges and the challenges; it outlives the tab
    /// @param model The statistics, for the open book; it outlives the tab
    WritingAwardsTab(WritingAwardsTracker* tracker, WritingStatsModel* model,
                     QWidget* parent = nullptr);

    /// @brief Where the title of the open book comes from
    void setBookTitleSource(std::function<QString()> title);

    /// @brief The title of the tab
    static QString tabTitle();

    /// @brief Show what the tracker has now
    void refresh();

    /// @brief Remove the chosen challenge, after asking
    void removeChallenge();

    /// @brief Make the badges of the book count again from today, after asking
    void startBookOver();

signals:
    /// @brief Add Challenge was pressed
    void addChallengeRequested();

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The texts in the language of the program
    void retranslate();
    /// @brief The colors of the notes in the theme
    void updateColors();
    /// @brief The two columns side by side when they fit, else one under the other
    void updateColumns();
    /// @brief Remove only with a challenge chosen
    void updateChallengeButtons();

    WritingAwardsTracker* m_tracker = nullptr;
    WritingStatsModel* m_model = nullptr;
    std::function<QString()> m_bookTitle;
    QLabel* m_title = nullptr;
    QLabel* m_count = nullptr;
    QGroupBox* m_challengesGroup = nullptr;
    QPushButton* m_addChallenge = nullptr;
    QPushButton* m_removeChallenge = nullptr;
    awards_tab::ChallengeList* m_challenges = nullptr;
    QLabel* m_challengesNote = nullptr;
    awards_tab::LevelKey* m_key = nullptr;
    QLabel* m_badgesNote = nullptr;
    QBoxLayout* m_columns = nullptr;
    QWidget* m_left = nullptr;
    QWidget* m_right = nullptr;
    QGroupBox* m_yoursGroup = nullptr;
    awards_tab::BadgeGrid* m_yours = nullptr;
    QGroupBox* m_bookGroup = nullptr;
    awards_tab::BadgeGrid* m_book = nullptr;
    QLabel* m_bookNote = nullptr;
    QPushButton* m_startOver = nullptr;
    QGroupBox* m_monthGroup = nullptr;
    awards_tab::BadgeGrid* m_month = nullptr;
    QList<QLabel*> m_notes;
};

}  // namespace kalahari::gui
