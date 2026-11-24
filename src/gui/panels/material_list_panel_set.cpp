// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list_panel_set.cpp
/// @brief The set for a publisher, the second view of the list of the texts for publishers:
///        its name, Copy All, Export Set... and its page

#include "kalahari/gui/panels/material_list_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/publishing_set.h"
#include "kalahari/gui/widgets/cover_plan.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/publishing_set_view.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// The margin of the bar of the set at its sides, as that of the footer of the list
constexpr int BAR_MARGIN_X = 8;

/// The part @p part of @p items; nullptr: none
const PublishingSetItem* itemOf(const QList<PublishingSetItem>& items, const QString& part) {
    const auto found = std::find_if(items.cbegin(), items.cend(),
                                    [&part](const PublishingSetItem& item) {
                                        return item.part == part;
                                    });
    return found != items.cend() ? &*found : nullptr;
}

/// A button of the bar of the set with its icon and text
QToolButton* barButton(const char* name, const char* iconId, const QString& text,
                       QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setText(text);
    button->setIcon(core::ArtProvider::getInstance().getIcon(QString::fromLatin1(iconId),
                                                             core::IconContext::Menu));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(true);
    return button;
}

}  // namespace

void MaterialListPanel::createSetPage() {
    m_setPage = new QWidget(m_stack);
    m_setPage->setObjectName(QStringLiteral("materialListSetPage"));
    auto* layout = new QVBoxLayout(m_setPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* bar = new QWidget(m_setPage);
    bar->setObjectName(QStringLiteral("materialListSetBar"));
    auto* barLayout = new FlowLayout(bar, 4, 2);
    barLayout->setContentsMargins(BAR_MARGIN_X, 2, BAR_MARGIN_X, 2);
    m_setHeading = new QLabel(bar);
    m_setHeading->setObjectName(QStringLiteral("materialListSetHeading"));
    // No access keys: Alt with a letter is the menus' in the window
    //: Copies the whole set for a publisher. In Polish: Kopiuj wszystko
    m_copyAllButton = barButton("materialListCopyAll", "edit.copy",
                                MaterialList::tr("Copy All"), bar);
    //: Opens the window of the export with the set for a publisher. In Polish: Eksportuj
    //: zestaw...
    m_exportSetButton = barButton("materialListExportSet", "file.export.docx",
                                  MaterialList::tr("Export Set..."), bar);
    barLayout->addWidget(m_setHeading);
    barLayout->addWidget(m_copyAllButton);
    barLayout->addWidget(m_exportSetButton);
    layout->addWidget(bar);

    auto* area = new QScrollArea(m_setPage);
    area->setObjectName(QStringLiteral("materialListSetArea"));
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    m_set = new PublishingSetView(area);
    area->setWidget(m_set);
    layout->addWidget(area, 1);
    m_stack->addWidget(m_setPage);

    connect(m_copyAllButton, &QToolButton::clicked, this, &MaterialListPanel::copySet);
    connect(m_exportSetButton, &QToolButton::clicked, this,
            &MaterialListPanel::requestExportSet);
    connect(m_set, &PublishingSetView::currentChanged, this,
            &MaterialListPanel::requestSetProperties);
    connect(m_set, &PublishingSetView::activated, this, &MaterialListPanel::openSetPart);
    connect(m_set, &PublishingSetView::toggled, this, &MaterialListPanel::toggleSetPart);
    connect(m_set, &PublishingSetView::copyRequested, this, &MaterialListPanel::copySetPart);
    connect(m_set, &PublishingSetView::versionChosen, this,
            [this](const QString&, const QString& elementId) { emit requestSetPick(elementId); });
    connect(m_set, &PublishingSetView::menuRequested, this, &MaterialListPanel::showSetMenu);
    connect(&m_book, &core::ProjectManager::publishingSetChanged, this,
            &MaterialListPanel::updateSet);
    // The length of a text as written in its editor
    connect(&m_book, &core::ProjectManager::publishingTextChanged, this, &MaterialListPanel::updateSet);
    // Another volume of a series: its set, its cover
    connect(&m_book, &core::ProjectManager::currentBookChanged, this,
            &MaterialListPanel::updatePlan);
}

bool MaterialListPanel::planShowing() const {
    QWidget* shown = m_stack->currentWidget();
    return shown == m_planArea || shown == m_setPage;
}

QWidget* MaterialListPanel::planPage() const {
    return MaterialList::isPublishing(m_list) ? m_setPage : static_cast<QWidget*>(m_planArea);
}

QWidget* MaterialListPanel::planWidget() const {
    return MaterialList::isPublishing(m_list) ? static_cast<QWidget*>(m_set)
                                              : static_cast<QWidget*>(m_plan);
}

void MaterialListPanel::updateSet() {
    if (m_stack->currentWidget() != m_setPage) {
        return;
    }
    m_set->setHeader(PublishingSetContents::titleOf(m_book),
                     PublishingSetContents::aboutOf(m_book));
    m_set->setItems(PublishingSetContents::itemsOf(m_book));
    m_setHeading->setText(QStringLiteral("<b>%1</b>")
                              .arg(PublishingSetContents::headingOf(m_book).toHtmlEscaped()));
    m_copyAllButton->setEnabled(!PublishingSetContents::textsOf(m_book).isEmpty());
    m_exportSetButton->setEnabled(m_copyAllButton->isEnabled());
}

void MaterialListPanel::applySetShown() {
    const bool set = m_stack->currentWidget() == m_setPage;
    if (auto* filters = findChild<QWidget*>(QStringLiteral("materialListFilters"))) {
        filters->setVisible(!set);
    }
    if (set) {
        updateSet();
        emit requestSetProperties(m_set->current());
    }
}

bool MaterialListPanel::updateSetFooter() {
    if (m_stack->currentWidget() != m_setPage) {
        return false;
    }
    //: The footer of the set for a publisher: what its keys do. In Polish: Strzałki wybierają
    //: część, Enter otwiera jej tekst, Spacja włącza ją albo wyłącza
    m_footer->setText(MaterialList::tr("The arrows choose a part, Enter opens its text, Space "
                                       "includes it or leaves it out"));
    m_sorting->clear();
    return true;
}

void MaterialListPanel::showSetPart(const QString& part) {
    m_set->setCurrent(part);
    if (auto* area = m_setPage->findChild<QScrollArea*>(QStringLiteral("materialListSetArea"));
        area != nullptr && m_set->card(part) != nullptr) {
        area->ensureWidgetVisible(m_set->card(part));
    }
}

void MaterialListPanel::toggleSetPart(const QString& part) {
    const core::PublishingSet set = m_book.publishingSet();
    m_book.setPublishingSet(set.withPart(part, !set.includes(part)));
}

void MaterialListPanel::copySetPart(const QString& part) {
    const QList<PublishingSetItem> items = PublishingSetContents::itemsOf(m_book);
    const PublishingSetItem* item = itemOf(items, part);
    if (item == nullptr || item->text.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(item->text);
    emit message(MaterialList::tr("Copied: %1").arg(item->name));
}

void MaterialListPanel::copySet() {
    if (const QString done = PublishingSetContents::copyAll(m_book); !done.isEmpty()) {
        emit message(done);
    }
}

void MaterialListPanel::openSetPart(const QString& part) {
    const QList<PublishingSetItem> items = PublishingSetContents::itemsOf(m_book);
    if (const PublishingSetItem* item = itemOf(items, part);
        item != nullptr && !item->elementId.isEmpty()) {
        emit requestOpen(item->elementId);
    }
}

void MaterialListPanel::showSetMenu(const QString& part, const QPoint& globalPos) {
    const QList<PublishingSetItem> items = PublishingSetContents::itemsOf(m_book);
    const PublishingSetItem* item = itemOf(items, part);
    if (item == nullptr) {
        return;
    }
    auto& art = core::ArtProvider::getInstance();
    QMenu menu(this);
    //: Opens the text of a part of the set for a publisher. In Polish: &Otwórz
    QAction* open = menu.addAction(
        art.getIcon(QStringLiteral("common.openExternal"), core::IconContext::Menu),
        MaterialList::tr("&Open"));
    open->setEnabled(!item->elementId.isEmpty());
    //: Copies the text of a part of the set for a publisher. In Polish: &Kopiuj tekst
    QAction* copy = menu.addAction(
        art.getIcon(QStringLiteral("edit.copy"), core::IconContext::Menu),
        MaterialList::tr("Copy &Text"));
    copy->setEnabled(!item->text.isEmpty());
    menu.addSeparator();
    //: Includes a part in the set for a publisher, or leaves it out. In Polish: &W zestawie
    QAction* included = menu.addAction(MaterialList::tr("&In the Set"));
    included->setCheckable(true);
    included->setChecked(item->on);
    included->setEnabled(!m_book.isReadOnly());
    connect(open, &QAction::triggered, this, [this, part]() { openSetPart(part); });
    connect(copy, &QAction::triggered, this, [this, part]() { copySetPart(part); });
    connect(included, &QAction::triggered, this, [this, part]() { toggleSetPart(part); });
    menu.exec(globalPos);
}

}  // namespace kalahari::gui
