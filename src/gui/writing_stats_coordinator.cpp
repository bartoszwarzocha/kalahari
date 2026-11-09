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
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/document_coordinator.h"
#include "kalahari/gui/panels/writing_goals_tab.h"
#include "kalahari/gui/panels/writing_stats_panel.h"
#include "kalahari/gui/utils/panel_toggle.h"
#include "kalahari/gui/utils/setting_toggle.h"
#include "kalahari/gui/widgets/writing_stats_bar.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QAction>
#include <QDockWidget>
#include <QPointer>
#include <QTabWidget>
#include <QVBoxLayout>

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

/// @brief The open book, as the tab of the goals needs it
WritingGoalsTab::BookSource bookSource()
{
    WritingGoalsTab::BookSource source;
    source.title = []() {
        const core::ProjectBook* book = core::ProjectManager::getInstance().book();
        return book != nullptr ? book->title : QString();
    };
    source.parts = []() {
        QList<dialogs::BookPart> parts;
        if (const core::ProjectBook* book = core::ProjectManager::getInstance().book()) {
            addParts(book->frontElements, 0, parts);
            addParts(book->mainElements, 0, parts);
            addParts(book->backElements, 0, parts);
        }
        return parts;
    };
    source.words = [](const QString& elementId) {
        auto& projects = core::ProjectManager::getInstance();
        const core::ProjectElement* element = projects.findElement(elementId);
        return element != nullptr ? projects.statisticsOf({*element}).words : 0;
    };
    return source;
}

}  // namespace

WritingStatsCoordinator::WritingStatsCoordinator(DockCoordinator* docks,
                                                 DocumentCoordinator* documents,
                                                 std::function<void(const QString&)> openSettings,
                                                 QObject* parent, const QString& journalPath)
    : QObject(parent)
    , m_docks(docks)
    , m_documents(documents)
    , m_openSettings(std::move(openSettings))
    , m_model(new WritingStatsModel(this))
{
    m_model->openJournal(journalPath.isEmpty() ? core::WritingJournal::defaultPath()
                                               : journalPath);
    m_model->setBookWordsSource([]() {
        auto& projects = core::ProjectManager::getInstance();
        const core::ProjectBook* book = projects.book();
        return book != nullptr ? projects.statisticsOf(book->mainElements).words : 0;
    });

    // The bar on top of the middle of the window, above the tabs of the texts
    QWidget* middle = m_docks != nullptr ? m_docks->centralWrapper() : nullptr;
    m_bar = new WritingStatsBar(m_model, middle);
    if (auto* layout = middle != nullptr ? qobject_cast<QVBoxLayout*>(middle->layout())
                                         : nullptr) {
        layout->insertWidget(0, m_bar);
    }
    connect(m_bar, &WritingStatsBar::goalsRequested, this, []() {
        CommandRegistry::getInstance().executeCommand("tools.writingGoals");
    });
    connect(m_bar, &WritingStatsBar::settingsRequested, this, [this]() {
        if (m_openSettings) {
            m_openSettings(QString::fromLatin1(SETTINGS_PAGE));
        }
    });
    applyBarVisibility();

    // The Statistics panel: the views of the same statistics, with the titles of the book
    if (WritingStatsPanel* panel = m_docks != nullptr ? m_docks->statisticsPanel() : nullptr) {
        panel->setModel(m_model);
        panel->setTitleSources(
            [](const QString& elementId) {
                const core::ProjectElement* element =
                    core::ProjectManager::getInstance().findElement(elementId);
                return element != nullptr ? element->title : QString();
            },
            []() {
                const core::ProjectBook* book = core::ProjectManager::getInstance().book();
                return book != nullptr ? book->title : QString();
            });
        connect(panel, &WritingStatsPanel::goalsRequested, this, []() {
            CommandRegistry::getInstance().executeCommand("tools.writingGoals");
        });
    }
    // The bar's button opens the panel, brings it on top or closes it, as View > Panels does
    connect(m_bar, &WritingStatsBar::panelRequested, this, [this]() {
        if (m_docks != nullptr && m_docks->statisticsDock() != nullptr) {
            utils::togglePanel(m_docks->statisticsDock());
        }
    });

    // The setting may change on another thread: the bar is shown or hidden on this one
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [coordinator = QPointer<WritingStatsCoordinator>(this)](const std::string& key) {
            if (key == SHOW_BAR_SETTING && coordinator != nullptr) {
                QMetaObject::invokeMethod(coordinator,
                                          &WritingStatsCoordinator::applyBarVisibility,
                                          Qt::QueuedConnection);
            }
        });

    // After the documents, which make the collector of the book's editors when it opens
    auto& projects = core::ProjectManager::getInstance();
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
    // A bar without the middle of a window is the coordinator's own
    if (m_bar != nullptr && m_bar->parent() == nullptr) {
        delete m_bar;
    }
}

void WritingStatsCoordinator::connectCommands()
{
    auto& registry = CommandRegistry::getInstance();
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
        tab->setBookSource(bookSource());
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
    auto& projects = core::ProjectManager::getInstance();
    if (!projects.isProjectOpen()) {
        return;
    }
    const core::BookProject* project = projects.project();
    m_model->setBook(projects.getDatabase(), project != nullptr ? project->id : QString());
    if (m_documents != nullptr) {
        m_model->setCollector(m_documents->statisticsCollector());
    }
}

void WritingStatsCoordinator::onProjectClosing()
{
    // The collector saves the last of the record when its session ends; it stays followed, so
    // that the journal gets it
    m_model->setBook(nullptr, QString());
}

}  // namespace kalahari::gui
