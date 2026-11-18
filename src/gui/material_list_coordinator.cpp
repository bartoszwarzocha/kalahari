// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list_coordinator.cpp
/// @brief The lists of the kinds of materials on their tabs in the middle of the window

#include "kalahari/gui/material_list_coordinator.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/source_description.h"
#include "kalahari/gui/add_target.h"
#include "kalahari/gui/card_list.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/dialogs/link_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/relation_dialog.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/publishing_set_page.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/source_names.h"
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

/// @brief How long the status bar shows what is longer to read
constexpr int LONG_MESSAGE_TIMEOUT = 6000;

/// @brief The beginning of the keys of the settings of the cover: the unit and the targets of
/// its texts (core::CoverMaterial::UNIT_SETTING, TARGETS_SETTING)
constexpr const char* COVER_SETTINGS = "materials.cover.";

/// @brief The beginning of the keys of the settings of the sources: the style of the
/// bibliography and the form of the references (core::Bibliography::STYLE_SETTING...)
constexpr const char* SOURCE_SETTINGS = "materials.sources.";

/// @brief The beginning of the keys of the settings of the texts for publishers: their target
/// lengths and the standard pages (core::PublishingTexts::TARGETS_SETTING...)
constexpr const char* PUBLISHING_SETTINGS = "materials.publishing.";

/// @brief The titles of the elements @p ids of the open book, those it has
QStringList titlesOf(const core::ProjectManager& book, const QStringList& ids) {
    QStringList titles;
    for (const QString& id : ids) {
        if (const core::ProjectElement* element = book.findElement(id)) {
            titles << element->title;
        }
    }
    return titles;
}

/// @brief The ids of all the elements of the Workshop of the open book
QSet<QString> workshopIds(const core::ProjectManager& book) {
    QSet<QString> ids;
    const core::BookProject* project = book.project();
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

MaterialListCoordinator::MaterialListCoordinator(core::ProjectManager& book,
                                                 QTabWidget* centralTabs,
                                                 NavigatorCoordinator* navigator,
                                                 QStatusBar* statusBar,
                                                 PropertiesShower showProperties, QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_navigator(navigator),
      m_statusBar(statusBar),
      m_showProperties(std::move(showProperties)),
      m_book(book) {
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
    connect(&m_book, &core::ProjectManager::linksChanged, this,
            &MaterialListCoordinator::refresh);
    // A description, the source of a quote or the style of the book: the list Sources, the
    // column Source of the quotes
    connect(&m_book, &core::ProjectManager::sourcesChanged, this,
            &MaterialListCoordinator::refresh);
    // The fields and the relations of a card, its color and portrait; the forms of the names
    // the search finds the cards by
    connect(&m_book, &core::ProjectManager::cardChanged, this, &MaterialListCoordinator::refresh);
    // The fields of the cards (the window Card Fields): the columns of their lists
    connect(&m_book, &core::ProjectManager::cardFieldsChanged, this,
            &MaterialListCoordinator::refresh);
    connect(&m_book, &core::ProjectManager::bookDictionaryChanged, this,
            &MaterialListCoordinator::refresh);
    connect(m_centralTabs, &QTabWidget::currentChanged, this,
            &MaterialListCoordinator::onCurrentTabChanged);
    connectSetPage();

    // The setting may change on another thread: the Navigator is built on this one
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [self = QPointer<MaterialListCoordinator>(this)](const std::string& key) {
            if (self == nullptr) {
                return;
            }
            if (key == NavigatorPanel::MATERIALS_TREE_SETTING) {
                QMetaObject::invokeMethod(self, "onTreeSettingChanged", Qt::QueuedConnection);
            } else if (key == MaterialList::HIDDEN_SETTING) {
                QMetaObject::invokeMethod(self, "onHiddenSettingChanged", Qt::QueuedConnection);
            } else if (key.starts_with(COVER_SETTINGS) || key.starts_with(SOURCE_SETTINGS) ||
                       key.starts_with(PUBLISHING_SETTINGS)) {
                // The unit and the targets of the texts of the cover and of the texts for
                // publishers; the style of the entries
                QMetaObject::invokeMethod(self, "refresh", Qt::QueuedConnection);
            }
        });
}

MaterialListCoordinator::~MaterialListCoordinator() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

MaterialListPanel* MaterialListCoordinator::openList(const QString& list, bool keys) {
    if (!m_book.isProjectOpen()) {
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
        panel = new MaterialListPanel(m_book, list, m_centralTabs);
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
    if (!m_book.isProjectOpen()) {
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
    // The set for a publisher in the Properties panel follows the same changes
    if (PropertiesPanel* properties = m_navigator->propertiesPanel();
        properties != nullptr &&
        properties->currentPage() == PropertiesPanel::Page::PublishingSet) {
        properties->publishingSetPage()->refresh();
    }
}

void MaterialListCoordinator::connectSetPage() {
    PropertiesPanel* properties = m_navigator->propertiesPanel();
    if (properties == nullptr) {
        return;
    }
    PublishingSetPage* page = properties->publishingSetPage();
    // A part chosen in the contents is chosen on the page of the set too
    connect(page, &PublishingSetPage::partChosen, this, [this](const QString& part) {
        for (MaterialListPanel* panel : lists()) {
            if (MaterialList::isPublishing(panel->list())) {
                panel->showSetPart(part);
            }
        }
    });
    connect(page, &PublishingSetPage::requestOpen, this, &MaterialListCoordinator::open);
    connect(page, &PublishingSetPage::requestExportSet, this,
            &MaterialListCoordinator::exportSetRequested);
    connect(page, &PublishingSetPage::requestSettings, this,
            &MaterialListCoordinator::settingsRequested);
    connect(page, &PublishingSetPage::message, this,
            [this](const QString& text) { m_statusBar->showMessage(text, MESSAGE_TIMEOUT); });
}

void MaterialListCoordinator::onTreeSettingChanged() {
    m_navigator->refreshNavigator();
}

void MaterialListCoordinator::onHiddenSettingChanged() {
    if (m_book.isProjectOpen()) {
        // The Navigator built again fills the lists again
        m_navigator->refreshNavigator();
    }
}

void MaterialListCoordinator::hideKind(const QString& list) {
    if (list.isEmpty() || MaterialList::isHidden(list)) {
        return;
    }
    MaterialList::setHidden(list, true);
    //: The status bar after a kind of material is hidden in the Navigator; %1 its name. In
    //: Polish: Ukryto typ „%1”. Jego materiały zostały w książce; przywrócisz go w Ustawieniach
    //: > Nawigator
    m_statusBar->showMessage(
        MaterialList::tr("The kind \"%1\" is hidden. Its materials stay in the book; you show it "
                         "again in Settings > Navigator.")
            .arg(MaterialList::nameOf(list)),
        LONG_MESSAGE_TIMEOUT);
}

void MaterialListCoordinator::showKind(const QString& list) {
    if (!MaterialList::isHidden(list)) {
        return;
    }
    MaterialList::setHidden(list, false);
    //: The status bar after a hidden kind of material is shown again; %1 its name. In Polish:
    //: Typ „%1” jest znów w Nawigatorze
    m_statusBar->showMessage(MaterialList::tr("The kind \"%1\" is in the Navigator again.")
                                 .arg(MaterialList::nameOf(list)),
                             MESSAGE_TIMEOUT);
}

void MaterialListCoordinator::onCurrentTabChanged() {
    MaterialListPanel* panel = currentList();
    if (panel != nullptr && panel->property(STALE_PROPERTY).toBool() &&
        m_book.isProjectOpen()) {
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
    connect(panel, &MaterialListPanel::requestAddCover, this,
            [this, panel = QPointer<MaterialListPanel>(panel)](const QString& list,
                                                               const QString& part) {
                if (panel != nullptr) {
                    add(panel, list, part);
                }
            });
    connect(panel, &MaterialListPanel::requestCoverState, this,
            &MaterialListCoordinator::setCoverState);
    connect(panel, &MaterialListPanel::requestPublishingState, this,
            &MaterialListCoordinator::setPublishingState);
    connect(panel, &MaterialListPanel::requestSetPick, this, &MaterialListCoordinator::pickForSet);
    // The set shown, a part chosen in it: the Properties panel shows the set, brought to the
    // front as a material chosen
    connect(panel, &MaterialListPanel::requestSetProperties, this,
            &MaterialListCoordinator::showSetProperties);
    connect(panel, &MaterialListPanel::requestExportSet, this,
            &MaterialListCoordinator::exportSetRequested);
    connect(panel, &MaterialListPanel::requestShowInCover, this,
            &MaterialListCoordinator::showInCover);
    connect(panel, &MaterialListPanel::requestShowKind, this, &MaterialListCoordinator::showKind);
    connect(panel, &MaterialListPanel::requestSettings, this,
            [this, shown = QPointer<MaterialListPanel>(panel)]() {
                if (shown != nullptr && CardList::isCardList(shown->list())) {
                    emit cardSettingsRequested();
                } else {
                    emit settingsRequested();
                }
            });
    connect(panel, &MaterialListPanel::requestRestore, this,
            &MaterialListCoordinator::clippingRestoreRequested);
    connect(panel, &MaterialListPanel::requestShowOrigin, this,
            &MaterialListCoordinator::clippingOriginRequested);
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
    const auto& pm = m_book;
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
    const auto& pm = m_book;
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
        m_book.findElement(elementId);
    if (element != nullptr) {
        m_navigator->onRequestRename(elementId, element->title);
    }
}

void MaterialListCoordinator::link(const QString& elementId) {
    auto& pm = m_book;
    const core::ProjectElement* element = pm.findElement(elementId);
    if (pm.isReadOnly() || element == nullptr) {
        return;
    }
    // A card has its relations with the other cards; both cards tell of a new one
    if (pm.isCard(*element)) {
        dialogs::editRelation(m_book, elementId, std::nullopt, window());
        return;
    }
    const std::optional<QList<core::ElementLink>> links =
        dialogs::chooseLinks(m_book, elementId, QString(), window());
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

void MaterialListCoordinator::add(MaterialListPanel* panel, const QString& list,
                                  const QString& coverPart) {
    auto& pm = m_book;
    if (!pm.isProjectOpen() || pm.isReadOnly()) {
        return;
    }
    // The Add Element window with the kind of the list, or with the kinds of all of them; the
    // list of the sources has those that can be a source, a publication chosen
    AddTarget target;
    target.scope = AddTarget::Scope::Workshop;
    const bool sources = list == QLatin1String(MaterialList::SOURCES);
    for (const QString& kindName : MaterialList::addKindsOf(m_book, list)) {
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
    // The part asked for: of a text for publishers, of a material of the cover
    if (MaterialList::isPublishing(list)) {
        target.publishingPart = coverPart;
    } else {
        target.coverPart = coverPart;
    }
    const QSet<QString> before = workshopIds(m_book);
    m_navigator->onRequestAdd(target);

    // The new material is chosen in the list; a working note opens in the editor, which keeps
    // the keys
    QString added;
    for (const QString& id : workshopIds(m_book)) {
        if (!before.contains(id)) {
            added = id;
            break;
        }
    }
    if (added.isEmpty()) {
        return;
    }
    // A material added to the sources is one: its kind and its title in its description
    if (const core::ProjectElement* element = pm.findElement(added);
        sources && element != nullptr && element->source.isEmpty()) {
        core::SourceDescription source;
        source.type = core::SourceDescription::kindFor(SourceNames::baseKindOf(m_book, *element));
        source.setValue(QString::fromLatin1(core::SourceDescription::TITLE), element->title);
        if (!pm.setSource(added, source)) {
            //: In Polish: Nie zapisano opisu bibliograficznego
            dialogs::MessageDialog::warning(window(),
                                            MaterialList::tr("Bibliographic Description Not Saved"),
                                            MaterialList::tr("Failed to save changes."));
        }
    }
    panel->setProperty(STALE_PROPERTY, false);
    panel->refresh();
    updateTab(panel);
    panel->selectMaterial(added);
    if (m_centralTabs->currentWidget() == panel) {
        panel->focusList();
    }
}

void MaterialListCoordinator::setCoverState(const QString& elementId, const QString& state) {
    auto& pm = m_book;
    const core::ProjectElement* element = pm.findElement(elementId);
    if (pm.isReadOnly() || element == nullptr) {
        return;
    }
    const QString title = element->title;
    QStringList drafts;
    if (!pm.setCoverState(elementId, state, &drafts)) {
        //: In Polish: Nie zapisano stanu
        dialogs::MessageDialog::warning(window(), MaterialList::tr("State Not Saved"),
                                        MaterialList::tr("Failed to save changes."));
        return;
    }
    const QString text = CoverNames::stateMessage(title, state, titlesOf(pm, drafts));
    m_statusBar->showMessage(text, LONG_MESSAGE_TIMEOUT);
    m_navigator->onMaterialChanged(elementId);
    for (const QString& id : drafts) {
        m_navigator->onMaterialChanged(id);
    }
}

void MaterialListCoordinator::setPublishingState(const QString& elementId,
                                                 const QString& state) {
    auto& pm = m_book;
    const core::ProjectElement* element = pm.findElement(elementId);
    if (pm.isReadOnly() || element == nullptr) {
        return;
    }
    const QString title = element->title;
    QStringList drafts;
    if (!pm.setPublishingState(elementId, state, &drafts)) {
        dialogs::MessageDialog::warning(window(), MaterialList::tr("State Not Saved"),
                                        MaterialList::tr("Failed to save changes."));
        return;
    }
    m_statusBar->showMessage(PublishingNames::stateMessage(title, state, titlesOf(pm, drafts)),
                             LONG_MESSAGE_TIMEOUT);
    m_navigator->onMaterialChanged(elementId);
    for (const QString& id : drafts) {
        m_navigator->onMaterialChanged(id);
    }
}

void MaterialListCoordinator::pickForSet(const QString& elementId) {
    auto& pm = m_book;
    const core::ProjectElement* element = pm.findElement(elementId);
    if (pm.isReadOnly() || element == nullptr) {
        return;
    }
    const QString title = element->title;
    if (!pm.pickForPublishingSet(elementId)) {
        //: In Polish: Nie zapisano zestawu
        dialogs::MessageDialog::warning(window(), MaterialList::tr("Set Not Saved"),
                                        MaterialList::tr("Failed to save changes."));
        return;
    }
    //: The status bar after a description or a tagline of the cover became the version of its
    //: part in the set for publishers; %1: its title. In Polish: „%1” jest teraz w zestawie dla
    //: wydawnictwa.
    m_statusBar->showMessage(
        MaterialList::tr("\"%1\" is now in the set for publishers.").arg(title),
        LONG_MESSAGE_TIMEOUT);
}

void MaterialListCoordinator::showSetProperties(const QString& part) {
    if (PropertiesPanel* properties = m_navigator->propertiesPanel()) {
        properties->showPublishingSetProperties(part);
    }
    if (m_showProperties) {
        m_showProperties(false);
    }
}

void MaterialListCoordinator::showInCover(const QString& elementId) {
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (element == nullptr) {
        return;
    }
    // The list of the kind of the material, which is the cover
    if (MaterialListPanel* panel = openList(m_book.kindOf(*element).reference(), true)) {
        panel->selectMaterial(elementId);
    }
}

void MaterialListCoordinator::showInFolder(const QString& elementId) {
    const auto& pm = m_book;
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
