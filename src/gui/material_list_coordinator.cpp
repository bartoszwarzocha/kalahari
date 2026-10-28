/// @file material_list_coordinator.cpp
/// @brief The lists of the kinds of materials on their tabs in the middle of the window

#include "kalahari/gui/material_list_coordinator.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/add_target.h"
#include "kalahari/gui/dialogs/link_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/utils/file_location.h"

#include <QDir>
#include <QPointer>
#include <QSet>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>

#include <functional>
#include <optional>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief The property of a list behind another tab that missed a change of the book
constexpr const char* STALE_PROPERTY = "materialListStale";

/// @brief How long the status bar shows what a list did
constexpr int MESSAGE_TIMEOUT = 2000;

/// @brief The ids of all the elements of the Workshop of the open book
QSet<QString> workshopIds() {
    QSet<QString> ids;
    const core::BookProject* project = core::ProjectManager::getInstance().project();
    if (project == nullptr) {
        return ids;
    }
    const std::function<void(const QList<core::ProjectElement>&)> collect =
        [&](const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                ids.insert(element.id);
                collect(element.elements);
            }
        };
    collect(project->workshop.elements);
    return ids;
}

}  // namespace

MaterialListCoordinator::MaterialListCoordinator(QTabWidget* centralTabs,
                                                 NavigatorCoordinator* navigator,
                                                 QStatusBar* statusBar,
                                                 PropertiesShower showProperties, QObject* parent)
    : QObject(parent)
    , m_centralTabs(centralTabs)
    , m_navigator(navigator)
    , m_statusBar(statusBar)
    , m_showProperties(std::move(showProperties))
{
    // Many changes at once (a material added: the book, its links, the Navigator) fill the
    // lists once
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0);
    connect(m_refreshTimer, &QTimer::timeout, this, &MaterialListCoordinator::refreshNow);

    connect(m_navigator, &NavigatorCoordinator::refreshNavigatorRequested, this,
            &MaterialListCoordinator::refresh);
    connect(m_navigator, &NavigatorCoordinator::materialChanged, this,
            &MaterialListCoordinator::refresh);
    connect(&core::ProjectManager::getInstance(), &core::ProjectManager::linksChanged, this,
            &MaterialListCoordinator::refresh);
    connect(m_centralTabs, &QTabWidget::currentChanged, this,
            &MaterialListCoordinator::onCurrentTabChanged);

    // The setting may change on another thread: the Navigator is built on this one
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [self = QPointer<MaterialListCoordinator>(this)](const std::string& key) {
            if (key == NavigatorPanel::MATERIALS_TREE_SETTING && self != nullptr) {
                QMetaObject::invokeMethod(self, "onTreeSettingChanged", Qt::QueuedConnection);
            }
        });
}

MaterialListCoordinator::~MaterialListCoordinator() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

MaterialListPanel* MaterialListCoordinator::openList(const QString& list, bool keys) {
    if (!core::ProjectManager::getInstance().isProjectOpen()) {
        return nullptr;
    }
    MaterialListPanel* panel = nullptr;
    const QList<MaterialListPanel*> open = lists();
    if (MaterialList::ownTabs()) {
        for (MaterialListPanel* shown : open) {
            if (shown->list() == list) {
                panel = shown;
                break;
            }
        }
    } else if (!open.isEmpty()) {
        // The one tab of the lists: the one in front, else the first
        panel = currentList() != nullptr ? currentList() : open.constFirst();
    }

    if (panel == nullptr) {
        panel = new MaterialListPanel(list, m_centralTabs);
        connectList(panel);
        m_centralTabs->addTab(panel, QString());
    } else {
        if (panel->property(STALE_PROPERTY).toBool()) {
            panel->setProperty(STALE_PROPERTY, false);
            panel->refresh();
        }
        panel->setList(list);
    }
    updateTab(panel);
    m_centralTabs->setCurrentWidget(panel);
    if (keys) {
        panel->focusList();
    }
    return panel;
}

QList<MaterialListPanel*> MaterialListCoordinator::lists() const {
    QList<MaterialListPanel*> panels;
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        if (auto* panel = qobject_cast<MaterialListPanel*>(m_centralTabs->widget(i))) {
            panels << panel;
        }
    }
    return panels;
}

MaterialListPanel* MaterialListCoordinator::currentList() const {
    return qobject_cast<MaterialListPanel*>(m_centralTabs->currentWidget());
}

void MaterialListCoordinator::refresh() {
    m_refreshTimer->start();
}

void MaterialListCoordinator::refreshNow() {
    m_refreshTimer->stop();
    if (!core::ProjectManager::getInstance().isProjectOpen()) {
        return;
    }
    MaterialListPanel* current = currentList();
    for (MaterialListPanel* panel : lists()) {
        if (panel == current) {
            panel->setProperty(STALE_PROPERTY, false);
            panel->refresh();
            updateTab(panel);
        } else {
            panel->setProperty(STALE_PROPERTY, true);
        }
    }
}

void MaterialListCoordinator::onTreeSettingChanged() {
    m_navigator->refreshNavigator();
}

void MaterialListCoordinator::onCurrentTabChanged() {
    MaterialListPanel* panel = currentList();
    if (panel != nullptr && panel->property(STALE_PROPERTY).toBool() &&
        core::ProjectManager::getInstance().isProjectOpen()) {
        panel->setProperty(STALE_PROPERTY, false);
        panel->refresh();
        updateTab(panel);
    }
}

void MaterialListCoordinator::connectList(MaterialListPanel* panel) {
    // A material chosen shows in the Properties panel, brought to the front as a material
    // chosen in the Navigator
    connect(panel, &MaterialListPanel::materialChosen, this, [this](const QString& elementId) {
        m_navigator->onRequestProperties(elementId);
        if (m_showProperties) {
            m_showProperties(false);
        }
    });
    connect(panel, &MaterialListPanel::requestProperties, this, [this](const QString& elementId) {
        m_navigator->onRequestProperties(elementId);
        if (m_showProperties) {
            m_showProperties(true);
        }
    });
    connect(panel, &MaterialListPanel::requestEdit, this, &MaterialListCoordinator::edit);
    connect(panel, &MaterialListPanel::requestOpen, this, &MaterialListCoordinator::open);
    connect(panel, &MaterialListPanel::requestRename, this, &MaterialListCoordinator::rename);
    connect(panel, &MaterialListPanel::requestDelete, m_navigator,
            &NavigatorCoordinator::onRequestDelete);
    connect(panel, &MaterialListPanel::requestLink, this, &MaterialListCoordinator::link);
    connect(panel, &MaterialListPanel::requestShowInFolder, this,
            &MaterialListCoordinator::showInFolder);
    connect(panel, &MaterialListPanel::requestAdd, this,
            [this, panel = QPointer<MaterialListPanel>(panel)](const QString& list) {
                if (panel != nullptr) {
                    add(panel, list);
                }
            });
    connect(panel, &MaterialListPanel::message, this,
            [this](const QString& text) { m_statusBar->showMessage(text, MESSAGE_TIMEOUT); });
}

void MaterialListCoordinator::updateTab(MaterialListPanel* panel) {
    const int index = m_centralTabs->indexOf(panel);
    if (index < 0) {
        return;
    }
    // DockCoordinator::refreshTabIcons() gives the tab the icon of another theme
    const QString iconId = MaterialList::iconIdOf(panel->list());
    panel->setProperty("tabIconId", iconId);
    m_centralTabs->setTabIcon(index, core::ArtProvider::getInstance().getIcon(iconId));
    m_centralTabs->setTabText(index, MaterialList::nameOf(panel->list()));
    m_centralTabs->setTabToolTip(index, MaterialList::nameOf(panel->list()));
}

void MaterialListCoordinator::edit(const QString& elementId, bool text) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(elementId);
    if (element == nullptr) {
        return;
    }
    // A working note in the editor, a material on its card
    if (pm.isMaterial(*element)) {
        m_navigator->onRequestOpenMaterialCard(elementId, text);
    } else {
        m_navigator->onElementSelected(elementId, element->title);
    }
}

void MaterialListCoordinator::open(const QString& elementId) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(elementId);
    if (element == nullptr) {
        return;
    }
    if (pm.isMaterial(*element)) {
        m_navigator->onRequestOpenMaterial(elementId);
    } else {
        m_navigator->onElementSelected(elementId, element->title);
    }
}

void MaterialListCoordinator::rename(const QString& elementId) {
    const core::ProjectElement* element =
        core::ProjectManager::getInstance().findElement(elementId);
    if (element != nullptr) {
        m_navigator->onRequestRename(elementId, element->title);
    }
}

void MaterialListCoordinator::link(const QString& elementId) {
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isReadOnly() || pm.findElement(elementId) == nullptr) {
        return;
    }
    const std::optional<QList<core::ElementLink>> links =
        dialogs::chooseLinks(elementId, QString(), window());
    if (!links) {
        return;
    }
    if (!pm.setLinks(elementId, *links)) {
        //: In Polish: Nie zapisano powiązań
        dialogs::MessageDialog::warning(window(), MaterialList::tr("Links Not Saved"),
                                        MaterialList::tr("Failed to save changes."));
    }
    m_navigator->onMaterialChanged(elementId);
}

void MaterialListCoordinator::add(MaterialListPanel* panel, const QString& list) {
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen() || pm.isReadOnly()) {
        return;
    }
    // The Add Element window with the kind of the list, or with the kinds of all of them
    AddTarget target;
    target.scope = AddTarget::Scope::Workshop;
    const QStringList kinds = list.isEmpty() ? MaterialList::kinds() : QStringList{list};
    for (const QString& kindName : kinds) {
        const std::optional<core::KindReference> reference = core::KindReference::parse(kindName);
        const core::KindRef kind =
            reference ? pm.bookTypes().findKind(reference->packageId, reference->kindId)
                      : core::KindRef{};
        if (core::ProjectManager::canAdd(kind)) {
            target.kinds.append(kind);
        }
    }
    if (target.kinds.isEmpty()) {
        return;
    }
    if (!list.isEmpty()) {
        target.kind = target.kinds.constFirst();
    }
    const QSet<QString> before = workshopIds();
    m_navigator->onRequestAdd(target);

    // The new material is chosen in the list; a working note opens in the editor, which keeps
    // the keys
    QString added;
    for (const QString& id : workshopIds()) {
        if (!before.contains(id)) {
            added = id;
            break;
        }
    }
    if (added.isEmpty()) {
        return;
    }
    panel->setProperty(STALE_PROPERTY, false);
    panel->refresh();
    updateTab(panel);
    panel->selectMaterial(added);
    if (m_centralTabs->currentWidget() == panel) {
        panel->focusList();
    }
}

void MaterialListCoordinator::showInFolder(const QString& elementId) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(elementId);
    const QString path = element != nullptr ? pm.filePathOf(*element) : QString();
    if (path.isEmpty()) {
        return;
    }
    if (!gui::showInFolder(path)) {
        //: In Polish: Pokaż w folderze
        dialogs::MessageDialog::warning(
            window(), MaterialList::tr("Show in Folder"),
            //: %1: the path of the file. In Polish: Menedżer plików nie mógł pokazać tego
            //: miejsca: %1
            MaterialList::tr("The file manager of the system could not show:\n%1")
                .arg(QDir::toNativeSeparators(path)));
    }
}

QWidget* MaterialListCoordinator::window() const {
    return m_centralTabs->window();
}

}  // namespace kalahari::gui
