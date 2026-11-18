// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_set_page.cpp
/// @brief The set for a publisher in the Properties panel: its contents, the part chosen and
///        its export

#include "kalahari/gui/panels/publishing_set_page.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/publishing_set.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/length_meter.h"
#include "kalahari/gui/widgets/publishing_set_view.h"

#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui {

namespace {

using core::PublishingTexts;

/// The margins and the spacing of the page, as those of the other pages of the panel
constexpr int PAGE_MARGIN = 11;
/// The space between the buttons of the export
constexpr int BUTTON_SPACING = 6;

/// The part of row @p row of the contents
QString partOf(const QListWidgetItem* row) {
    return row != nullptr ? row->data(Qt::UserRole).toString() : QString();
}

/// The part @p part of @p items; nullptr: none
const PublishingSetItem* itemOf(const QList<PublishingSetItem>& items, const QString& part) {
    const auto found = std::find_if(items.cbegin(), items.cend(),
                                    [&part](const PublishingSetItem& item) {
                                        return item.part == part;
                                    });
    return found != items.cend() ? &*found : nullptr;
}

/// The words of a hint under a group: wrapped, in the color of a placeholder
QLabel* hintLabel(const QString& text, QWidget* parent) {
    auto* hint = new QLabel(text, parent);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    return hint;
}

/// What the keys @p keys do in the contents
enum class ContentsKey { None, Up, Down, Toggle, Open };

ContentsKey contentsKeyOf(QKeyCombination keys) {
    const Qt::KeyboardModifiers modifiers = keys.keyboardModifiers();
    if (modifiers == Qt::AltModifier) {
        if (keys.key() == Qt::Key_Up) {
            return ContentsKey::Up;
        }
        return keys.key() == Qt::Key_Down ? ContentsKey::Down : ContentsKey::None;
    }
    if (modifiers != Qt::NoModifier) {
        return ContentsKey::None;
    }
    if (keys.key() == Qt::Key_Space) {
        return ContentsKey::Toggle;
    }
    return keys.key() == Qt::Key_Return || keys.key() == Qt::Key_Enter ? ContentsKey::Open
                                                                       : ContentsKey::None;
}

/// Row @p line of the contents for part @p item: its name and length, checked when it is in
/// the set; checkable in a book that can change (@p editable); the length in words (@p words)
void showRow(QListWidgetItem& line, const PublishingSetItem& item, bool editable, bool words) {
    //: A part of the set for a publisher without a version, in its contents. In Polish: brak
    //: wersji
    const QString length =
        item.elementId.isEmpty()
            ? PublishingSetPage::tr("no version")
            : CoverNames::lengthText(words ? item.words : item.characters, words);
    line.setText(item.name + QStringLiteral(" · ") + length);
    line.setToolTip(line.text());  // the whole row in a narrow panel
    line.setData(Qt::UserRole, item.part);
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (editable) {
        flags |= Qt::ItemIsUserCheckable;
    }
    line.setFlags(flags);
    line.setCheckState(item.on ? Qt::Checked : Qt::Unchecked);
}

}  // namespace

PublishingSetPage::PublishingSetPage(core::ProjectManager& book, QWidget* parent)
    : QWidget(parent), m_book(book) {
    setObjectName(QStringLiteral("publishingSetPage"));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* area = new QScrollArea(this);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(area);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN);
    layout->setSpacing(PAGE_MARGIN);

    // The name of the set with the parts in it and their length in all
    auto* heading = new QHBoxLayout;
    m_icon = new QLabel(content);
    m_icon->setAlignment(Qt::AlignTop);
    //: The heading of the set for a publisher in the Properties panel. In Polish: Zestaw dla
    //: wydawnictwa
    auto* title = new QLabel(tr("Set for the Publisher"), content);
    title->setObjectName(QStringLiteral("publishingSetPageTitle"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    m_summary = hintLabel(QString(), content);
    m_summary->setObjectName(QStringLiteral("publishingSetSummary"));
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    titles->addWidget(title);
    titles->addWidget(m_summary);
    heading->addWidget(m_icon);
    heading->addLayout(titles, 1);
    layout->addLayout(heading);
    layout->addWidget(createContents(content));
    layout->addWidget(createPart(content));
    layout->addWidget(createExport(content));
    layout->addStretch();
    area->setWidget(content);
    outer->addWidget(area);

    refreshIcons();
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &PublishingSetPage::refreshIcons);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &PublishingSetPage::refreshIcons);
    // A part included, left out or moved here or on the page of the set
    connect(&m_book, &core::ProjectManager::publishingSetChanged, this,
            &PublishingSetPage::refresh);
}

QWidget* PublishingSetPage::createContents(QWidget* parent) {
    //: The group of the parts of the set for a publisher in the Properties panel. In Polish:
    //: Zawartość zestawu
    auto* group = new QGroupBox(tr("Contents of the Set"), parent);
    group->setObjectName(QStringLiteral("publishingSetContents"));
    auto* layout = new QVBoxLayout(group);
    m_contents = new QListWidget(group);
    m_contents->setObjectName(QStringLiteral("publishingSetContentsList"));
    m_contents->setAccessibleName(group->title());
    m_contents->setSelectionMode(QAbstractItemView::SingleSelection);
    m_contents->setUniformItemSizes(true);
    m_contents->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_contents->installEventFilter(this);
    m_upButton = new QToolButton(group);
    m_upButton->setObjectName(QStringLiteral("publishingSetUp"));
    m_upButton->setAutoRaise(true);
    //: Moves a part of the set for a publisher up. In Polish: W górę (Alt+↑)
    m_upButton->setToolTip(tr("Move Up (%1)")
                               .arg(ShortcutRules::keysText(Qt::AltModifier | Qt::Key_Up)));
    m_upButton->setAccessibleName(m_upButton->toolTip());
    m_downButton = new QToolButton(group);
    m_downButton->setObjectName(QStringLiteral("publishingSetDown"));
    m_downButton->setAutoRaise(true);
    //: Moves a part of the set for a publisher down. In Polish: W dół (Alt+↓)
    m_downButton->setToolTip(tr("Move Down (%1)")
                                 .arg(ShortcutRules::keysText(Qt::AltModifier | Qt::Key_Down)));
    m_downButton->setAccessibleName(m_downButton->toolTip());
    auto* buttons = new QVBoxLayout;
    buttons->addWidget(m_upButton);
    buttons->addWidget(m_downButton);
    buttons->addStretch();
    auto* row = new QHBoxLayout;
    row->addWidget(m_contents, 1);
    row->addLayout(buttons);
    layout->addLayout(row);
    //: Under the contents of the set for a publisher. In Polish: Spacja włącza i wyłącza
    //: część, Alt+↑ i Alt+↓ ją przestawiają. Zestaw zapamiętuje to w książce; zawartość dla
    //: nowych książek ustawia się w Ustawieniach.
    layout->addWidget(hintLabel(tr("Space includes a part or leaves it out, Alt+Up and Alt+Down "
                                   "move it. The set keeps this in the book; the contents for "
                                   "new books are in the Settings."),
                                group));

    connect(m_contents, &QListWidget::currentRowChanged, this,
            &PublishingSetPage::onCurrentRowChanged);
    connect(m_contents, &QListWidget::itemChanged, this, &PublishingSetPage::onItemChanged);
    connect(m_contents, &QListWidget::itemDoubleClicked, this, &PublishingSetPage::openPart);
    connect(m_upButton, &QToolButton::clicked, this, [this]() { movePart(-1); });
    connect(m_downButton, &QToolButton::clicked, this, [this]() { movePart(1); });
    return group;
}

QWidget* PublishingSetPage::createPart(QWidget* parent) {
    m_partGroup = new QGroupBox(parent);
    m_partGroup->setObjectName(QStringLiteral("publishingSetPart"));
    auto* layout = new QVBoxLayout(m_partGroup);
    // Its version, where it comes from, its length and Open; hidden for a part without one
    m_details = new QWidget(m_partGroup);
    auto* details = new QVBoxLayout(m_details);
    details->setContentsMargins(0, 0, 0, 0);
    auto* form = new QFormLayout;
    m_version = new QLabel(m_details);
    m_version->setObjectName(QStringLiteral("publishingSetVersion"));
    m_version->setWordWrap(true);
    m_version->setTextFormat(Qt::PlainText);
    m_source = new QLabel(m_details);
    m_source->setObjectName(QStringLiteral("publishingSetSource"));
    m_source->setWordWrap(true);
    //: The version of the part chosen of the set for a publisher. In Polish: Wersja:
    form->addRow(tr("Version:"), m_version);
    //: Where the text of the part chosen of the set comes from. In Polish: Skąd:
    form->addRow(tr("Source:"), m_source);
    details->addLayout(form);
    m_meter = new LengthMeter(m_details);
    details->addWidget(m_meter);
    //: Opens the version of the part chosen of the set for a publisher. In Polish: Otwórz
    //: (%1)
    m_openButton = new QPushButton(tr("Open (%1)").arg(ShortcutRules::keysText(Qt::Key_Enter)),
                                   m_details);
    m_openButton->setObjectName(QStringLiteral("publishingSetOpen"));
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_openButton);
    buttons->addStretch();
    details->addLayout(buttons);
    layout->addWidget(m_details);
    //: The part chosen of the set for a publisher has no version. In Polish: Ta część nie ma
    //: jeszcze wybranej wersji.
    m_noVersion = hintLabel(tr("This part has no version chosen yet."), m_partGroup);
    m_noVersion->setObjectName(QStringLiteral("publishingSetNoVersion"));
    layout->addWidget(m_noVersion);
    connect(m_openButton, &QPushButton::clicked, this, &PublishingSetPage::openPart);
    return m_partGroup;
}

QWidget* PublishingSetPage::createExport(QWidget* parent) {
    //: The group of the export of the set for a publisher. In Polish: Eksport
    auto* group = new QGroupBox(tr("Export"), parent);
    group->setObjectName(QStringLiteral("publishingSetExport"));
    auto* layout = new QVBoxLayout(group);
    m_settingsButton = new QToolButton(group);
    m_settingsButton->setObjectName(QStringLiteral("publishingSetSettings"));
    m_settingsButton->setAutoRaise(true);
    //: The gear of the set for a publisher: Settings > Navigator. In Polish: Ustawienia tekstów
    //: dla wydawnictwa
    m_settingsButton->setToolTip(tr("Texts for Publishers Settings"));
    m_settingsButton->setAccessibleName(m_settingsButton->toolTip());
    auto* top = new QHBoxLayout;
    //: Under Export of the set for a publisher. In Polish: Okno eksportu to to samo okno co
    //: Plik › Eksportuj; zestaw jest w nim czwartym źródłem obok całej książki, wybranych
    //: tekstów i tekstu na wierzchu. Pliki DOCX i Markdown dostają nagłówki części zestawu.
    top->addWidget(hintLabel(tr("The window of the export is that of File > Export; the set is "
                                "its fourth source beside the whole book, the texts chosen and "
                                "the text in front. DOCX and Markdown files get the headings "
                                "of the parts of the set."),
                             group),
                   1);
    top->addWidget(m_settingsButton, 0, Qt::AlignTop);
    layout->addLayout(top);
    auto* buttons = new QWidget(group);
    auto* flow = new FlowLayout(buttons, BUTTON_SPACING, BUTTON_SPACING);
    flow->setContentsMargins(0, 0, 0, 0);
    //: Copies the whole set for a publisher. In Polish: Kopiuj wszystko
    m_copyAllButton = new QPushButton(tr("Copy All"), buttons);
    m_copyAllButton->setObjectName(QStringLiteral("publishingSetCopyAll"));
    //: Opens the window of the export with the set for a publisher. In Polish: Eksportuj
    //: zestaw...
    m_exportButton = new QPushButton(tr("Export Set..."), buttons);
    m_exportButton->setObjectName(QStringLiteral("publishingSetExportButton"));
    flow->addWidget(m_copyAllButton);
    flow->addWidget(m_exportButton);
    layout->addWidget(buttons);
    connect(m_settingsButton, &QToolButton::clicked, this, &PublishingSetPage::requestSettings);
    connect(m_copyAllButton, &QPushButton::clicked, this, &PublishingSetPage::copyAll);
    connect(m_exportButton, &QPushButton::clicked, this, &PublishingSetPage::requestExportSet);
    return group;
}

void PublishingSetPage::showSet(const QString& part) {
    m_part = part;
    refresh();
}

void PublishingSetPage::refresh() {
    if (!m_book.isProjectOpen()) {
        return;
    }
    fillContents();
    showPart();
    m_summary->setText(PublishingSetContents::summaryOf(m_book));
    const bool texts = !PublishingSetContents::textsOf(m_book).isEmpty();
    m_copyAllButton->setEnabled(texts);
    m_exportButton->setEnabled(texts);
}

void PublishingSetPage::focusContents() {
    m_contents->setFocus(Qt::OtherFocusReason);
}

void PublishingSetPage::fillContents() {
    const QList<PublishingSetItem> items = PublishingSetContents::itemsOf(m_book);
    if (itemOf(items, m_part) == nullptr) {
        m_part = items.isEmpty() ? QString() : items.constFirst().part;
    }
    // The rows stay: the row with the keys keeps them, also after its part moved
    const QSignalBlocker blocker(m_contents);
    while (m_contents->count() > items.size()) {
        delete m_contents->takeItem(m_contents->count() - 1);
    }
    while (m_contents->count() < items.size()) {
        (void)new QListWidgetItem(m_contents);
    }
    const bool editable = !m_book.isReadOnly();
    const bool words = core::CoverMaterial::countsWords();
    int current = -1;
    for (int row = 0; row < items.size(); ++row) {
        showRow(*m_contents->item(row), items.at(row), editable, words);
        if (items.at(row).part == m_part) {
            current = row;
        }
    }
    m_contents->setCurrentRow(current);
    updateMoveButtons();
}

void PublishingSetPage::updateMoveButtons() {
    const int row = m_contents->currentRow();
    const bool editable = !m_book.isReadOnly();
    m_upButton->setEnabled(editable && row > 0);
    m_downButton->setEnabled(editable && row >= 0 && row < m_contents->count() - 1);
}

void PublishingSetPage::showPart() {
    const QList<PublishingSetItem> items = PublishingSetContents::itemsOf(m_book);
    const PublishingSetItem* item = itemOf(items, m_part);
    //: The group of the part chosen of the set for a publisher; %1 its name. In Polish:
    //: Wybrana część: %1
    m_partGroup->setTitle(item != nullptr ? tr("Part Chosen: %1").arg(item->name) : QString());
    const core::ProjectElement* element =
        item != nullptr ? m_book.findElement(item->elementId) : nullptr;
    m_details->setVisible(element != nullptr);
    m_noVersion->setVisible(item != nullptr && element == nullptr);
    if (element == nullptr) {
        return;
    }
    m_version->setText(element->title);
    m_source->setText(PublishingSetContents::sourceOf(item->part));
    const bool pages = item->part == QLatin1String(PublishingTexts::SYNOPSIS) &&
                       PublishingTexts::showsPages();
    m_meter->setLength(item->characters, item->words, item->target,
                       core::CoverMaterial::countsWords(), pages);
}

void PublishingSetPage::refreshIcons() {
    auto& art = core::ArtProvider::getInstance();
    const int size = art.getIconSize(core::IconContext::Toolbar);
    m_icon->setPixmap(art.getIcon(QStringLiteral("materials.publishingSet"),
                                  core::IconContext::Toolbar)
                          .pixmap(size, size));
    m_upButton->setIcon(art.getIcon(QStringLiteral("navigation.up"), core::IconContext::Menu));
    m_downButton->setIcon(
        art.getIcon(QStringLiteral("navigation.down"), core::IconContext::Menu));
    m_settingsButton->setIcon(
        art.getIcon(QStringLiteral("dock.settings"), core::IconContext::Menu));
    m_openButton->setIcon(
        art.getIcon(QStringLiteral("common.openExternal"), core::IconContext::Menu));
    m_copyAllButton->setIcon(art.getIcon(QStringLiteral("edit.copy"), core::IconContext::Menu));
    m_exportButton->setIcon(
        art.getIcon(QStringLiteral("file.export.docx"), core::IconContext::Menu));
}

void PublishingSetPage::onCurrentRowChanged(int row) {
    const QString part = partOf(m_contents->item(row));
    if (part.isEmpty() || part == m_part) {
        return;
    }
    m_part = part;
    updateMoveButtons();
    showPart();
    emit partChosen(m_part);
}

void PublishingSetPage::onItemChanged(const QListWidgetItem* item) {
    const QString part = partOf(item);
    const core::PublishingSet set = m_book.publishingSet();
    const bool on = item->checkState() == Qt::Checked;
    if (part.isEmpty() || on == set.includes(part)) {
        return;
    }
    if (m_book.isReadOnly() || !m_book.setPublishingSet(set.withPart(part, on))) {
        refresh();  // the check as the book has it
    }
}

void PublishingSetPage::movePart(int step) {
    if (m_part.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const core::PublishingSet set = m_book.publishingSet();
    const core::PublishingSet moved = set.withPartMoved(m_part, step);
    if (moved.order != set.parts()) {
        (void)m_book.setPublishingSet(moved);
    }
}

void PublishingSetPage::openPart() {
    const QList<PublishingSetItem> items = PublishingSetContents::itemsOf(m_book);
    if (const PublishingSetItem* item = itemOf(items, m_part);
        item != nullptr && !item->elementId.isEmpty()) {
        emit requestOpen(item->elementId);
    }
}

void PublishingSetPage::copyAll() {
    if (const QString done = PublishingSetContents::copyAll(m_book); !done.isEmpty()) {
        emit message(done);
    }
}

bool PublishingSetPage::runContentsKey(QKeyCombination keys, bool run) {
    const ContentsKey action = contentsKeyOf(keys);
    if (!run) {
        return action != ContentsKey::None;
    }
    switch (action) {
        case ContentsKey::Up:
            movePart(-1);
            break;
        case ContentsKey::Down:
            movePart(1);
            break;
        case ContentsKey::Toggle:
            toggleCurrent();
            break;
        case ContentsKey::Open:
            openPart();
            break;
        case ContentsKey::None:
            return false;
    }
    return true;
}

void PublishingSetPage::toggleCurrent() {
    if (QListWidgetItem* row = m_contents->currentItem();
        row != nullptr && row->flags().testFlag(Qt::ItemIsUserCheckable)) {
        row->setCheckState(row->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    }
}

bool PublishingSetPage::eventFilter(QObject* watched, QEvent* event) {
    const bool keys =
        event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress;
    if (watched == m_contents && keys) {
        auto* key = static_cast<QKeyEvent*>(event);
        // The keys are the contents', not the window's shortcuts
        if (runContentsKey(ShortcutRules::keysOf(key), event->type() == QEvent::KeyPress)) {
            key->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace kalahari::gui
