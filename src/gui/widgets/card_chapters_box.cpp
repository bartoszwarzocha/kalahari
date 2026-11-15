// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_chapters_box.cpp
/// @brief The chapters a card of a character, a location or an item is in

#include "kalahari/gui/widgets/card_chapters_box.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/link_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/panels/card_parts.h"

#include <QAction>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMetaObject>
#include <QPalette>
#include <QPointer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <string>

namespace kalahari::gui {

namespace {

/// The chapter of a row
constexpr int CHAPTER_ROLE = Qt::UserRole;
/// The columns: the chapter, the role of the card with the paragraph, and the note
constexpr int CHAPTER_COLUMN = 0;
constexpr int ROLE_COLUMN = 1;
constexpr int NOTE_COLUMN = 2;

/// What a key does in the list of the chapters
enum class ChapterKey {
    None,    ///< Nothing of the list
    Edit,    ///< Insert or F2: the window Link
    Remove,  ///< Delete: the chapter goes
    Open     ///< Enter: the chapter
};

/// What @p key does in the list
ChapterKey keyOf(const QKeyEvent* key) {
    if (card::isListKey(key, Qt::Key_Insert) || card::isListKey(key, Qt::Key_F2)) {
        return ChapterKey::Edit;
    }
    if (card::isListKey(key, Qt::Key_Delete)) {
        return ChapterKey::Remove;
    }
    return card::isOpenKey(key) ? ChapterKey::Open : ChapterKey::None;
}

/// The elements of the books of @p project in reading order
QList<const core::ProjectElement*> bookElements(const core::BookProject& project) {
    QList<const core::ProjectElement*> elements;
    for (qsizetype book = 0; book < project.books.size(); ++book) {
        elements << project.readingOrder(book);
    }
    return elements;
}

}  // namespace

CardChaptersBox::CardChaptersBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_list(new QTreeWidget(this)), m_menu(new QMenu(this)), m_book(book) {
    setObjectName(QStringLiteral("cardChapters"));
    //: The group of the chapters a card of a character, a location or an item is in. In
    //: Polish: W rozdziałach
    setTitle(tr("In Chapters"));
    QVBoxLayout* layout = card::groupLayout(this);
    //: In Polish: Powiąż z rozdziałami... (Insert)
    m_addButton = card::listButton(this, "common.add", tr("Link with Chapters... (Insert)"),
                                   QStringLiteral("cardChaptersAdd"));
    //: In Polish: Usuń z rozdziału (Delete)
    m_removeButton = card::listButton(this, "common.remove", tr("Remove from Chapter (Delete)"),
                                      QStringLiteral("cardChaptersRemove"));
    layout->addLayout(card::buttonRow(m_addButton, m_removeButton));

    m_list->setObjectName(QStringLiteral("cardChaptersList"));
    m_list->setColumnCount(NOTE_COLUMN + 1);
    m_list->setHeaderHidden(true);
    m_list->setRootIsDecorated(false);
    m_list->setItemsExpandable(false);
    m_list->setIndentation(0);
    m_list->setUniformRowHeights(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->header()->setStretchLastSection(true);
    m_list->header()->setSectionResizeMode(CHAPTER_COLUMN, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(ROLE_COLUMN, QHeaderView::ResizeToContents);
    //: In Polish: Rozdziały karty
    m_list->setAccessibleName(tr("Chapters of the card"));
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->installEventFilter(this);
    layout->addWidget(m_list);
    //: In Polish: Karta nie jest jeszcze w żadnym rozdziale. „+” albo Insert ją powiąże.
    m_emptyLabel = card::hintLabel(this, tr("The card is in no chapter yet. \"+\" or Insert "
                                            "links it."),
                                   QStringLiteral("cardChaptersEmpty"));
    layout->addWidget(m_emptyLabel);
    m_menu->setObjectName(QStringLiteral("cardChaptersMenu"));

    connect(m_addButton, &QToolButton::clicked, this, &CardChaptersBox::editChapters);
    connect(m_removeButton, &QToolButton::clicked, this, &CardChaptersBox::removeChapter);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &CardChaptersBox::updateButtons);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this]() {
        if (const QString chapter = currentChapter(); !chapter.isEmpty()) {
            emit requestOpenChapter(chapter);
        }
    });
    connect(m_list, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& at) {
        fillMenu();
        m_menu->popup(m_list->viewport()->mapToGlobal(at));
    });
    // The links and the titles change elsewhere too, and the paragraphs move when a chapter
    // is saved
    const auto changed = [this]() {
        if (!m_cardId.isEmpty()) {
            refresh();
        }
    };
    connect(&m_book, &core::ProjectManager::linksChanged, this, changed);
    connect(&m_book, &core::ProjectManager::elementRenamed, this, changed);
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            changed);
    // The paragraphs of the links shown or not (core::LinkRoles::PARAGRAPHS_SETTING): the
    // setting may change on another thread, the list changes on this one
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [box = QPointer<CardChaptersBox>(this)](const std::string& key) {
            if (key != core::LinkRoles::PARAGRAPHS_SETTING || box == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                box,
                [box]() {
                    if (box != nullptr && !box->m_cardId.isEmpty()) {
                        box->refresh();
                    }
                },
                Qt::QueuedConnection);
        });
    fitList();
    updateButtons();
}

CardChaptersBox::~CardChaptersBox() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    card::cutOffChildren(this);
}

void CardChaptersBox::showCard(const QString& cardId) {
    if (cardId != m_cardId) {
        m_cardId = cardId;
        m_list->clear();
    }
    refresh();
}

void CardChaptersBox::refresh() {
    const QString current = currentChapter();
    m_list->clear();
    const core::BookProject* project = m_book.project();
    const core::ProjectElement* card = m_book.findElement(m_cardId);
    if (project != nullptr && card != nullptr) {
        for (const core::ProjectElement* chapter : bookElements(*project)) {
            const core::ElementLink* link = card->linkTo(chapter->id);
            if (link == nullptr) {
                continue;
            }
            auto* item = new QTreeWidgetItem(m_list);
            item->setText(CHAPTER_COLUMN, chapter->title);
            item->setIcon(CHAPTER_COLUMN,
                          linkIcon(m_book, chapter->id, core::IconContext::TreeView));
            item->setText(ROLE_COLUMN, linkDetails(*link));
            item->setText(NOTE_COLUMN, link->note);
            item->setForeground(NOTE_COLUMN, palette().brush(QPalette::PlaceholderText));
            for (int column = 0; column <= NOTE_COLUMN; ++column) {
                item->setToolTip(column, link->note);
            }
            item->setData(CHAPTER_COLUMN, CHAPTER_ROLE, chapter->id);
        }
    }
    selectChapter(current);
    fitList();
    updateButtons();
}

QString CardChaptersBox::currentChapter() const {
    const QTreeWidgetItem* item = m_list->currentItem();
    return item != nullptr ? item->data(CHAPTER_COLUMN, CHAPTER_ROLE).toString() : QString();
}

void CardChaptersBox::selectChapter(const QString& chapterId) {
    if (chapterId.isEmpty()) {
        return;
    }
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        if (m_list->topLevelItem(i)->data(CHAPTER_COLUMN, CHAPTER_ROLE).toString() == chapterId) {
            m_list->setCurrentItem(m_list->topLevelItem(i));
            return;
        }
    }
}

void CardChaptersBox::editChapters() {
    if (m_cardId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const QString current = currentChapter();
    const std::optional<QList<core::ElementLink>> links =
        dialogs::chooseChapters(m_book, m_cardId, current, window());
    if (!links) {
        return;
    }
    if (!m_book.setLinks(m_cardId, *links)) {
        //: In Polish: Nie zapisano powiązań
        dialogs::MessageDialog::warning(this, tr("Links Not Saved"),
                                        tr("Failed to save changes."));
    }
    refresh();
    selectChapter(current);
    m_list->setFocus(Qt::OtherFocusReason);
}

void CardChaptersBox::removeChapter() {
    const QString chapter = currentChapter();
    const core::ProjectElement* card = m_book.findElement(m_cardId);
    if (chapter.isEmpty() || card == nullptr || m_book.isReadOnly()) {
        return;
    }
    const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
    QList<core::ElementLink> links = card->links;
    links.removeIf([&chapter](const core::ElementLink& each) { return each.to == chapter; });
    if (!m_book.setLinks(m_cardId, links)) {
        dialogs::MessageDialog::warning(this, tr("Links Not Saved"),
                                        tr("Failed to save changes."));
    }
    refresh();
    if (m_list->topLevelItemCount() > 0) {
        m_list->setCurrentItem(m_list->topLevelItem(std::min(row, m_list->topLevelItemCount() - 1)));
        m_list->setFocus(Qt::OtherFocusReason);
    }
}

void CardChaptersBox::fillMenu() {
    m_menu->clear();
    const bool row = !currentChapter().isEmpty();
    const bool writable = !m_book.isReadOnly() && !m_cardId.isEmpty();
    //: The menu of a chapter of a card. In Polish: Otwórz rozdział
    QAction* open = m_menu->addAction(tr("Open the Chapter"));
    open->setEnabled(row);
    connect(open, &QAction::triggered, this, [this]() {
        if (const QString chapter = currentChapter(); !chapter.isEmpty()) {
            emit requestOpenChapter(chapter);
        }
    });
    //: In Polish: Zmień powiązanie... (F2)
    QAction* change = m_menu->addAction(tr("Change the Link... (F2)"));
    change->setEnabled(row && writable);
    connect(change, &QAction::triggered, this, &CardChaptersBox::editChapters);
    QAction* remove = m_menu->addAction(tr("Remove from Chapter (Delete)"));
    remove->setEnabled(row && writable);
    connect(remove, &QAction::triggered, this, &CardChaptersBox::removeChapter);
    m_menu->addSeparator();
    QAction* add = m_menu->addAction(tr("Link with Chapters... (Insert)"));
    add->setEnabled(writable);
    connect(add, &QAction::triggered, this, &CardChaptersBox::editChapters);
}

void CardChaptersBox::fitList() {
    const int rows = m_list->topLevelItemCount();
    const int rowHeight =
        rows > 0 ? m_list->sizeHintForRow(0) : m_list->fontMetrics().height() + 6;
    m_list->setFixedHeight(card::listHeight(m_list, rows, rowHeight, 0));
    m_list->setVisible(rows > 0);
    m_emptyLabel->setVisible(rows == 0);
}

void CardChaptersBox::updateButtons() {
    const bool writable = !m_book.isReadOnly() && !m_cardId.isEmpty();
    m_addButton->setEnabled(writable);
    m_removeButton->setEnabled(writable && !currentChapter().isEmpty());
}

bool CardChaptersBox::eventFilter(QObject* watched, QEvent* event) {
    const bool key =
        event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride;
    if (watched != m_list || !key) {
        return QGroupBox::eventFilter(watched, event);
    }
    const ChapterKey action = keyOf(static_cast<const QKeyEvent*>(event));
    if (action == ChapterKey::None ||
        (action == ChapterKey::Open && currentChapter().isEmpty())) {
        return QGroupBox::eventFilter(watched, event);
    }
    // The same keys of the window's commands wait while the list has the keys
    event->accept();
    if (event->type() != QEvent::KeyPress) {
        return true;
    }
    switch (action) {
    case ChapterKey::Edit:
        editChapters();
        break;
    case ChapterKey::Remove:
        removeChapter();
        break;
    case ChapterKey::Open:
        emit requestOpenChapter(currentChapter());
        break;
    case ChapterKey::None:
        break;
    }
    return true;
}

}  // namespace kalahari::gui
