/// @file writing_stats_coordinator.cpp
/// @brief The statistics and goals of writing in the main window

#include "kalahari/gui/writing_stats_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/writing_journal.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/editor/statistics_collector.h"
#include "kalahari/gui/dialogs/challenge_dialog.h"
#include "kalahari/gui/dialogs/session_summary_dialog.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/document_coordinator.h"
#include "kalahari/gui/panels/writing_awards_tab.h"
#include "kalahari/gui/panels/writing_goals_tab.h"
#include "kalahari/gui/panels/writing_stats_panel.h"
#include "kalahari/gui/utils/panel_toggle.h"
#include "kalahari/gui/utils/setting_toggle.h"
#include "kalahari/gui/widgets/badge_painter.h"
#include "kalahari/gui/widgets/info_bar.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/widgets/status_bar_writing.h"
#include "kalahari/gui/widgets/writing_stats_bar.h"
#include "kalahari/gui/writing_awards_tracker.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QLocale>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// @brief The groups and texts of @p elements and of their groups, in reading order
void addParts(const QList<core::ProjectElement>& elements, int depth,
              QList<dialogs::BookPart>& parts)
{
    for (const core::ProjectElement& element : elements) {
        parts.append({element.id, element.title, depth});
        addParts(element.elements, depth + 1, parts);
    }
}

/// @brief The title of the book open in @p projects; empty without one
QString bookTitle(const core::ProjectManager& projects)
{
    const core::ProjectBook* book = projects.book();
    return book != nullptr ? book->title : QString();
}

/// @brief The book open in @p projects, as the badges need it: its chapters, those final, and
/// the words of its parts
WritingAwardsTracker::BookSource awardsSource(core::ProjectManager& projects)
{
    WritingAwardsTracker::BookSource source;
    source.chapters = [&projects]() -> std::pair<int, int> {
        const core::ProjectBook* book = projects.book();
        if (book == nullptr) {
            return {0, 0};
        }
        const core::TextStatistics statistics = projects.statisticsOf(book->mainElements);
        const auto found = statistics.statuses.find(QStringLiteral("final"));
        return {found != statistics.statuses.end() ? found->second : 0, statistics.elements};
    };
    source.words = [&projects](const QString& elementId) {
        const core::ProjectElement* element = projects.findElement(elementId);
        return element != nullptr ? projects.statisticsOf({*element}).words : 0;
    };
    return source;
}

/// @brief The book open in @p projects, as the tab of the goals needs it
WritingGoalsTab::BookSource bookSource(core::ProjectManager& projects)
{
    WritingGoalsTab::BookSource source;
    source.title = [&projects]() {
        const core::ProjectBook* book = projects.book();
        return book != nullptr ? book->title : QString();
    };
    source.parts = [&projects]() {
        QList<dialogs::BookPart> parts;
        if (const core::ProjectBook* book = projects.book()) {
            addParts(book->frontElements, 0, parts);
            addParts(book->mainElements, 0, parts);
            addParts(book->backElements, 0, parts);
        }
        return parts;
    };
    source.words = [&projects](const QString& elementId) {
        const core::ProjectElement* element = projects.findElement(elementId);
        return element != nullptr ? projects.statisticsOf({*element}).words : 0;
    };
    return source;
}

}  // namespace

WritingStatsCoordinator::WritingStatsCoordinator(core::ProjectManager& book,
                                                 CommandRegistry& commands, DockCoordinator* docks,
                                                 DocumentCoordinator* documents,
                                                 std::function<void(const QString&)> openSettings,
                                                 QObject* parent, const QString& journalPath)
    : QObject(parent),
      m_docks(docks),
      m_documents(documents),
      m_openSettings(std::move(openSettings)),
      m_model(new WritingStatsModel(this)),
      m_book(book),
      m_commands(commands) {
    m_model->openJournal(journalPath.isEmpty() ? core::WritingJournal::defaultPath()
                                               : journalPath);
    m_model->setBookWordsSource([this]() {
        const core::ProjectBook* openBook = m_book.book();
        return openBook != nullptr ? m_book.statisticsOf(openBook->mainElements).words : 0;
    });
    m_tracker = new WritingAwardsTracker(m_model, this);
    m_tracker->setBookSource(awardsSource(m_book));
    m_sessionStart = m_model->now();

    // The bar on top of the middle of the window, above the tabs of the texts
    QWidget* middle = m_docks != nullptr ? m_docks->centralWrapper() : nullptr;
    m_bar = new WritingStatsBar(m_model, middle);
    // The window deletes its middle, with the bars, before this coordinator, made later
    connect(m_bar, &QObject::destroyed, this, [this]() { m_bar = nullptr; });
    if (auto* layout = middle != nullptr ? qobject_cast<QVBoxLayout*>(middle->layout())
                                         : nullptr) {
        layout->insertWidget(0, m_bar);
    }
    connect(m_bar, &WritingStatsBar::goalsRequested, this, [this]() {
        m_commands.executeCommand("tools.writingGoals");
    });
    connect(m_bar, &WritingStatsBar::settingsRequested, this, [this]() {
        if (m_openSettings) {
            m_openSettings(QString::fromLatin1(SETTINGS_PAGE));
        }
    });
    applyBarVisibility();

    // The bar of the messages at the bottom of the middle, under the tabs of the texts
    m_messageBar = new InfoBar(middle);
    m_messageBar->setObjectName(QStringLiteral("statsMessageBar"));
    connect(m_messageBar, &QObject::destroyed, this, [this]() { m_messageBar = nullptr; });
    //: The button of a message at the bottom of the window. In Polish: Kopiuj
    m_messageBar->setActionText(tr("Copy"));
    //: In Polish: Kopiuj tę wiadomość
    m_messageBar->setActionToolTip(tr("Copy this message"));
    m_messageBar->hide();
    if (auto* layout = middle != nullptr ? qobject_cast<QVBoxLayout*>(middle->layout())
                                         : nullptr) {
        layout->addWidget(m_messageBar);
    }
    connect(m_messageBar, &InfoBar::actionClicked, this, [this]() {
        QApplication::clipboard()->setText(m_messageBar->message());
    });
    connect(m_messageBar, &InfoBar::dismissed, m_messageBar, &QWidget::hide);
    m_messageTimer = new QTimer(this);
    m_messageTimer->setSingleShot(true);
    m_messageTimer->setInterval(0);
    connect(m_messageTimer, &QTimer::timeout, this, &WritingStatsCoordinator::flushMessages);
    connectMessages();

    // The Statistics panel: the views of the same statistics, with the titles of the book
    if (WritingStatsPanel* panel = m_docks != nullptr ? m_docks->statisticsPanel() : nullptr) {
        panel->setModel(m_model);
        panel->setTitleSources(
            [this](const QString& elementId) {
                const core::ProjectElement* element =
                    m_book.findElement(elementId);
                return element != nullptr ? element->title : QString();
            },
            [this]() {
                const core::ProjectBook* openBook = m_book.book();
                return openBook != nullptr ? openBook->title : QString();
            });
        connect(panel, &WritingStatsPanel::goalsRequested, this, [this]() {
            m_commands.executeCommand("tools.writingGoals");
        });
        panel->setAwardsTracker(m_tracker);
        connect(panel, &WritingStatsPanel::awardsRequested, this, [this]() {
            m_commands.executeCommand("tools.challenges");
        });
        connect(panel, &WritingStatsPanel::addChallengeRequested, this, [this]() {
            m_commands.executeCommand("tools.addChallenge");
        });
    }
    // The bar's button opens the panel, brings it on top or closes it, as View > Panels does
    connect(m_bar, &WritingStatsBar::panelRequested, this, [this]() {
        if (m_docks != nullptr && m_docks->statisticsDock() != nullptr) {
            utils::togglePanel(m_docks->statisticsDock());
        }
    });

    // The setting may change on another thread: the bar is shown or hidden on this one
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key == SHOW_BAR_SETTING) {
                QMetaObject::invokeMethod(this, "applyBarVisibility", Qt::QueuedConnection);
            }
        });

    // After the documents, which make the collector of the book's editors when it opens
    auto& projects = m_book;
    connect(&projects, &core::ProjectManager::projectOpened, this,
            &WritingStatsCoordinator::onProjectOpened, Qt::QueuedConnection);
    connect(&projects, &core::ProjectManager::projectAboutToClose, this,
            &WritingStatsCoordinator::onProjectClosing);
    if (projects.isProjectOpen()) {
        onProjectOpened();
    }
}

WritingStatsCoordinator::~WritingStatsCoordinator()
{
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    // The bars without the middle of a window are the coordinator's own; those in the middle
    // went with it, if the window went first
    if (m_bar != nullptr && m_bar->parent() == nullptr) {
        delete m_bar;
    }
    if (m_messageBar != nullptr && m_messageBar->parent() == nullptr) {
        delete m_messageBar;
    }
}

void WritingStatsCoordinator::connectCommands()
{
    auto& registry = m_commands;
    if (Command* command = registry.getCommand("view.showStatsBar")) {
        command->execute = [this]() { toggleBar(); };
        command->isEnabled = []() { return true; };
        registry.updateActionState("view.showStatsBar");
    }
    if (QAction* action = registry.getAction(std::string("view.showStatsBar"))) {
        utils::followSetting(action, SHOW_BAR_SETTING);
    }
    // The tab of the goals, from the Tools menu, the bar, the panel and the Statistics toolbar
    if (Command* command = registry.getCommand("tools.writingGoals")) {
        command->execute = [this]() { openGoals(); };
        command->isEnabled = []() { return true; };
        registry.updateActionState("tools.writingGoals");
    }
    // The tab of the badges, from the Tools menu, the panel and the Statistics toolbar
    if (Command* command = registry.getCommand("tools.challenges")) {
        command->execute = [this]() { openAwards(); };
        command->isEnabled = []() { return true; };
        registry.updateActionState("tools.challenges");
    }
    if (Command* command = registry.getCommand("tools.addChallenge")) {
        command->execute = [this]() { addChallenge(); };
        command->isEnabled = []() { return true; };
        registry.updateActionState("tools.addChallenge");
    }
    // Settings > Statistics and Goals, from the Statistics toolbar and the Tools menu
    if (Command* command = registry.getCommand("tools.statsSettings")) {
        command->execute = [this]() {
            if (m_openSettings) {
                m_openSettings(QString::fromLatin1(SETTINGS_PAGE));
            }
        };
        command->isEnabled = []() { return true; };
        registry.updateActionState("tools.statsSettings");
    }
}

WritingGoalsTab* WritingStatsCoordinator::openGoals()
{
    QTabWidget* tabs = m_docks != nullptr ? m_docks->centralTabs() : nullptr;
    if (tabs == nullptr) {
        return nullptr;
    }
    WritingGoalsTab* tab = nullptr;
    for (int index = 0; index < tabs->count() && tab == nullptr; ++index) {
        tab = qobject_cast<WritingGoalsTab*>(tabs->widget(index));
    }
    if (tab == nullptr) {
        tab = new WritingGoalsTab(m_model, tabs);
        tab->setBookSource(bookSource(m_book));
        // DockCoordinator::refreshTabIcons() gives the tab the icon of another theme
        tab->setProperty("tabIconId", QString::fromLatin1(WritingGoalsTab::ICON_ID));
        connect(tab, &WritingGoalsTab::settingsRequested, this, [this]() {
            if (m_openSettings) {
                m_openSettings(QString::fromLatin1(GOALS_SETTINGS_PAGE));
            }
        });
        const int index = tabs->addTab(
            tab, core::ArtProvider::getInstance().getIcon(QString::fromLatin1(WritingGoalsTab::ICON_ID)),
            WritingGoalsTab::tabTitle());
        tabs->setTabToolTip(index, WritingGoalsTab::tabTitle());
        core::Logger::getInstance().info("WritingStatsCoordinator: Writing goals opened");
    }
    tabs->setCurrentWidget(tab);
    tab->setFocus();
    return tab;
}

WritingAwardsTab* WritingStatsCoordinator::openAwards()
{
    QTabWidget* tabs = m_docks != nullptr ? m_docks->centralTabs() : nullptr;
    if (tabs == nullptr) {
        return nullptr;
    }
    WritingAwardsTab* tab = nullptr;
    for (int index = 0; index < tabs->count() && tab == nullptr; ++index) {
        tab = qobject_cast<WritingAwardsTab*>(tabs->widget(index));
    }
    if (tab == nullptr) {
        tab = new WritingAwardsTab(m_tracker, m_model, tabs);
        tab->setBookTitleSource([this]() { return bookTitle(m_book); });
        // DockCoordinator::refreshTabIcons() gives the tab the icon of another theme
        tab->setProperty("tabIconId", QString::fromLatin1(WritingAwardsTab::ICON_ID));
        connect(tab, &WritingAwardsTab::addChallengeRequested, this,
                &WritingStatsCoordinator::addChallenge);
        const int index = tabs->addTab(
            tab,
            core::ArtProvider::getInstance().getIcon(QString::fromLatin1(WritingAwardsTab::ICON_ID)),
            WritingAwardsTab::tabTitle());
        tabs->setTabToolTip(index, WritingAwardsTab::tabTitle());
        core::Logger::getInstance().info("WritingStatsCoordinator: Challenges and badges opened");
    }
    tabs->setCurrentWidget(tab);
    tab->setFocus();
    return tab;
}

int WritingStatsCoordinator::addChallenge()
{
    QWidget* window = m_docks != nullptr && m_docks->centralWrapper() != nullptr
                          ? m_docks->centralWrapper()->window()
                          : nullptr;
    dialogs::ChallengeDialog dialog(m_model->today(), window);
    if (dialog.exec() != QDialog::Accepted) {
        return 0;
    }
    const int id = m_tracker->addChallenge(dialog.challenge());
    if (id > 0) {
        openAwards();
    }
    return id;
}

void WritingStatsCoordinator::showMessage(const QString& message)
{
    if (message.isEmpty()) {
        return;
    }
    m_pendingMessages.append(message);
    m_messageTimer->start();
}

void WritingStatsCoordinator::flushMessages()
{
    if (m_pendingMessages.isEmpty() || m_messageBar == nullptr) {
        return;
    }
    // The messages of one moment together, in the order they came
    m_messageBar->setMessage(m_pendingMessages.join(QLatin1Char(' ')));
    m_pendingMessages.clear();
    m_messageBar->show();
}

void WritingStatsCoordinator::connectMessages()
{
    const auto on = [](const char* key, bool fallback) {
        return core::SettingsManager::getInstance().get<bool>(key, fallback);
    };
    connect(m_tracker, &WritingAwardsTracker::dailyGoalReached, this,
            [this, on](int words, int goal) {
                if (!on("stats.notify.dailyGoal", true)) {
                    return;
                }
                //: %1 and %2: numbers. In Polish: Cel dzienny osiągnięty: %1 z %2 słów.
                QString text = tr("Daily goal reached: %1 of %2 words.")
                                   .arg(stats_painter::number(words), stats_painter::number(goal));
                const core::WritingStatistics statistics = m_model->goalStatistics();
                const QDate today = m_model->today();
                const QDate monday = today.addDays(1 - today.dayOfWeek());
                const int week = statistics.period(monday, today).counted(m_model->rules().counting);
                const int weekGoal = m_model->rules().weeklyGoalWords();
                if (weekGoal > week) {
                    //: In Polish: Do celu tygodniowego zostało: %1.
                    text += QLatin1Char(' ') +
                            tr("Left to the weekly goal: %1.").arg(stats_painter::words(weekGoal - week));
                }
                showMessage(text);
            });
    // The day at once in the Statistics panel, when the settings say so; later the assistant
    // is to congratulate the writer there too
    connect(m_tracker, &WritingAwardsTracker::dailyGoalReached, this, [this, on]() {
        if (!on("stats.notify.showPanel", true) || m_docks == nullptr) {
            return;
        }
        QDockWidget* dock = m_docks->statisticsDock();
        WritingStatsPanel* panel = m_docks->statisticsPanel();
        if (dock == nullptr || panel == nullptr) {
            return;
        }
        panel->setView(WritingStatsPanel::View::Today);
        dock->show();
        dock->raise();
    });
    connect(m_tracker, &WritingAwardsTracker::weeklyGoalReached, this,
            [this, on](int words, int goal) {
                if (on("stats.notify.weeklyGoal", true)) {
                    //: In Polish: Cel tygodniowy osiągnięty: %1 z %2 słów.
                    showMessage(tr("Weekly goal reached: %1 of %2 words.")
                                    .arg(stats_painter::number(words), stats_painter::number(goal)));
                }
            });
    connect(m_tracker, &WritingAwardsTracker::deadlineComing, this,
            [this, on](const QString& name, const QDate& date, int days, int words, int target) {
                if (!on("stats.notify.deadline", true)) {
                    return;
                }
                const QString day = QLocale().toString(date, QStringLiteral("d MMMM"));
                //: The deadline of the whole book. In Polish: Termin książki
                const QString what = name.isEmpty() ? tr("The deadline of the book")
                                                    //: In Polish: Termin „%1”
                                                    : tr("The deadline “%1”").arg(name);
                QString text;
                if (days == 0) {
                    //: In Polish: %1 jest dziś (%2).
                    text = tr("%1 is today (%2).").arg(what, day);
                } else if (days == 1) {
                    //: In Polish: %1 jest jutro (%2).
                    text = tr("%1 is tomorrow (%2).").arg(what, day);
                } else {
                    //: In Polish: %1 jest za %n dni (%2).
                    text = tr("%1 is in %n days (%2).", nullptr, days).arg(what, day);
                }
                if (target > 0) {
                    //: In Polish: Jest %1 z %2 słów.
                    text += QLatin1Char(' ') +
                            tr("There are %1 of %2 words.").arg(stats_painter::number(words),
                                                                stats_painter::number(target));
                }
                showMessage(text);
            });
    connect(m_tracker, &WritingAwardsTracker::badgesWon, this,
            [this](const QList<core::EarnedBadge>& badges) {
                if (badges.isEmpty()) {
                    return;
                }
                // The highest of them is named; the others are on the tab
                const core::EarnedBadge best = *std::max_element(
                    badges.cbegin(), badges.cend(),
                    [](const core::EarnedBadge& a, const core::EarnedBadge& b) {
                        return a.level < b.level;
                    });
                const QString name =
                    tr("%1 (%2)").arg(badge_painter::title(m_tracker->progressOf(best.kind),
                                                           best.level),
                                      badge_painter::levelName(best.level));
                if (badges.size() == 1) {
                    //: In Polish: Nowa odznaka: %1.
                    showMessage(tr("New badge: %1.").arg(name));
                } else {
                    //: In Polish: Nowe odznaki: %n, w tym %1.
                    showMessage(tr("New badges: %n, among them %1.", nullptr, int(badges.size()))
                                    .arg(name));
                }
            });
    connect(m_tracker, &WritingAwardsTracker::challengeDone, this,
            [this](const core::WritingChallenge& challenge, int /*value*/) {
                //: In Polish: Wyzwanie ukończone: %1 (%2).
                showMessage(tr("Challenge done: %1 (%2).")
                                .arg(badge_painter::challengeTitle(challenge),
                                     badge_painter::challengePeriod(challenge)));
            });
}

void WritingStatsCoordinator::attachStatusBar(QStatusBar* statusBar)
{
    if (statusBar == nullptr || m_statusBarWriting != nullptr) {
        return;
    }
    m_statusBarWriting = new StatusBarWriting(statusBar, m_model, this);
}

void WritingStatsCoordinator::setSummaryWindow(QWidget* window)
{
    if (m_summaryWindow != nullptr) {
        disconnect(m_summaryWindow, nullptr, this, nullptr);
    }
    m_summaryWindow = window;
    if (m_summaryWindow != nullptr) {
        connect(m_summaryWindow, &QObject::destroyed, this,
                [this]() { m_summaryWindow = nullptr; });
    }
}

void WritingStatsCoordinator::showSessionSummary()
{
    if (m_summaryWindow == nullptr || m_summaryShown ||
        !core::SettingsManager::getInstance().get<bool>("stats.sessionSummary", true)) {
        return;
    }
    // The last of the writing, in the record
    if (m_documents != nullptr && m_documents->statisticsCollector() != nullptr) {
        m_documents->statisticsCollector()->flush();
    }
    const dialogs::SessionSummary summary =
        dialogs::SessionSummary::of(*m_model, m_sessionStart, bookTitle(m_book));
    if (!summary.writtenSince) {
        return;
    }
    m_summaryShown = true;
    dialogs::SessionSummaryDialog dialog(summary, m_summaryWindow);
    dialog.exec();
}

void WritingStatsCoordinator::toggleBar()
{
    auto& settings = core::SettingsManager::getInstance();
    const bool show = !settings.get<bool>(SHOW_BAR_SETTING, true);
    settings.set<bool>(SHOW_BAR_SETTING, show);
    core::Logger::getInstance().info("Action triggered: Statistics Bar {}", show ? "on" : "off");
    applyBarVisibility();
}

void WritingStatsCoordinator::applyBarVisibility()
{
    if (m_bar != nullptr) {
        m_bar->setVisible(core::SettingsManager::getInstance().get<bool>(SHOW_BAR_SETTING, true));
    }
}

void WritingStatsCoordinator::onProjectOpened()
{
    auto& projects = m_book;
    if (!projects.isProjectOpen()) {
        return;
    }
    const core::BookProject* project = projects.project();
    m_model->setBook(projects.getDatabase(), project != nullptr ? project->id : QString(),
                     projects.isReadOnly());
    // A new session with the book
    m_sessionStart = m_model->now();
    m_summaryShown = false;
    if (m_documents != nullptr) {
        m_model->setCollector(m_documents->statisticsCollector());
    }
}

void WritingStatsCoordinator::onProjectClosing()
{
    showSessionSummary();
    // The collector saves the last of the record when its session ends; it stays followed, so
    // that the journal gets it
    m_model->setBook(nullptr, QString());
}

}  // namespace kalahari::gui
