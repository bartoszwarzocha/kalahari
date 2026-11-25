// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file source_boxes.cpp
/// @brief The groups of the card of a material that is a source (how the bibliography shows it,
/// its bookmarks and its quotes) and of a quote; the bibliographic description is in
/// source_description_box.cpp

#include "kalahari/gui/panels/source_boxes.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/bibliography.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/source_description.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/pdf_files.h"
#include "kalahari/gui/source_names.h"

#include <QClipboard>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace kalahari::gui {

namespace {

using card::buttonRow;
using card::cardTable;
using card::editTriggers;
using card::fitRows;
using card::groupForm;
using card::groupLayout;
using card::hintLabel;
using card::isEditing;
using card::isListKey;
using card::isOpenKey;
using card::keepIcon;
using card::listButton;
using core::Bibliography;
using core::SourceDescription;

/// The columns of the bookmarks and of the quotes
constexpr int PAGE_COLUMN = 0;
constexpr int NOTE_COLUMN = 1;

/// The fields of a bookmark this version does not know, in the first cell
constexpr int EXTRA_ROLE = Qt::UserRole + 1;
/// The quote of a row of the quotes
constexpr int ID_ROLE = Qt::UserRole + 2;

/// The last page a bookmark can have
constexpr int MAXIMUM_PAGE = 99999;

/// The margin around the entry of a source
constexpr int ENTRY_MARGIN = 6;
/// The space between a field and its button
constexpr int ROW_SPACING = 6;
/// The letters the list of the sources of a quote has room for at least
constexpr int SOURCE_LETTERS = 16;

/// The page of a bookmark: from 1 to MAXIMUM_PAGE
class PageDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
            spin->setRange(1, MAXIMUM_PAGE);
        }
        return editor;
    }
};

/// A cell that shows @p text and is not edited
QTableWidgetItem* shownItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return item;
}

/// A cell of a page, at the right
QTableWidgetItem* pageItem(int page) {
    auto* item = new QTableWidgetItem();
    item->setData(Qt::EditRole, page);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

/// The title of @p element as a source: that of its description, else its own
QString sourceTitle(const core::ProjectElement& element) {
    const QString own = element.source.value(SourceDescription::TITLE);
    return own.isEmpty() ? element.title : own;
}

}  // namespace

// =============================================================================
// BibliographyEntryBox
// =============================================================================

BibliographyEntryBox::BibliographyEntryBox(core::ProjectManager& book, QWidget* parent,
                                           bool compact)
    : QGroupBox(parent),
      m_book(book),
      m_compact(compact),
      m_styleBox(new QComboBox(this)),
      m_entryLabel(new QLabel(this)),
      m_inclusionBox(new QComboBox(this)) {
    setObjectName(compact ? QStringLiteral("bibliographyEntryShort")
                          : QStringLiteral("bibliographyEntry"));
    //: The group with the entry of a source in the bibliography. In Polish: Tak wygląda w
    //: bibliografii
    setTitle(tr("As the Bibliography Shows It"));
    QVBoxLayout* layout = groupLayout(this);

    auto* styleForm = groupForm();
    m_styleBox->setObjectName(QStringLiteral("bibliographyStyleBox"));
    //: In Polish: Wybór pokazuje zapis w innej normie; normę książki zmienisz we
    //: Właściwościach książki.
    m_styleBox->setToolTip(tr("The choice shows the entry in another style; the style of the "
                              "book is in the Properties of the book."));
    //: The style of a bibliography: PN-ISO 690, APA... In Polish: Norma:
    styleForm->addRow(tr("Style:"), m_styleBox);
    layout->addLayout(styleForm);

    m_entryLabel->setObjectName(QStringLiteral("bibliographyEntryLabel"));
    m_entryLabel->setWordWrap(true);
    m_entryLabel->setTextFormat(Qt::RichText);
    m_entryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                          Qt::TextSelectableByKeyboard);
    m_entryLabel->setFrameShape(QFrame::StyledPanel);
    m_entryLabel->setMargin(ENTRY_MARGIN);
    m_entryLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    //: In Polish: Zapis w bibliografii
    m_entryLabel->setAccessibleName(tr("Entry in the bibliography"));
    layout->addWidget(m_entryLabel);

    auto* inclusionForm = groupForm();
    m_inclusionBox->setObjectName(QStringLiteral("bibliographyInclusionBox"));
    for (const char* inclusion : {SourceDescription::LINKED, SourceDescription::ALWAYS,
                                  SourceDescription::NEVER}) {
        m_inclusionBox->addItem(SourceNames::inclusionName(QString::fromLatin1(inclusion)),
                                QString::fromLatin1(inclusion));
    }
    //: Whether the bibliography of the book has a source. In Polish: W bibliografii:
    inclusionForm->addRow(tr("In the bibliography:"), m_inclusionBox);
    layout->addLayout(inclusionForm);

    //: In Polish: Kopiuj zapis
    m_copyButton = new QPushButton(tr("Copy Entry"), this);
    m_copyButton->setObjectName(QStringLiteral("bibliographyCopyButton"));
    keepIcon(m_copyButton, QStringLiteral("edit.copy"), core::IconContext::Button);
    //: Opens Settings > Materials. In Polish: Ustawienia źródeł
    m_settingsButton = listButton(this, "dock.settings", tr("Source Settings"),
                                  "bibliographySettingsButton");
    auto* copyRow = new QHBoxLayout();
    copyRow->addWidget(m_copyButton);
    copyRow->addStretch(1);
    copyRow->addWidget(m_settingsButton);
    layout->addLayout(copyRow);

    // The short card shows the entry in the style of the book alone
    if (compact) {
        m_styleBox->hide();
        styleForm->setRowVisible(m_styleBox, false);
        inclusionForm->setRowVisible(m_inclusionBox, false);
    }

    connect(m_styleBox, &QComboBox::activated, this, &BibliographyEntryBox::updateEntry);
    connect(m_copyButton, &QPushButton::clicked, this, &BibliographyEntryBox::copyEntry);
    connect(m_settingsButton, &QToolButton::clicked, this,
            &BibliographyEntryBox::requestSettings);
    connect(m_inclusionBox, &QComboBox::activated, this, &BibliographyEntryBox::saveInclusion);
    // The description or the style of the book changed
    connect(&m_book, &core::ProjectManager::sourcesChanged, this, [this]() {
        if (const core::ProjectElement* element = m_book.findElement(m_elementId)) {
            showMaterial(*element);
        }
    });
}

void BibliographyEntryBox::saveInclusion() {
    if (m_elementId.isEmpty() || m_book.isReadOnly() || m_source.isEmpty()) {
        return;
    }
    SourceDescription source = m_source;
    const QString inclusion = m_inclusionBox->currentData().toString();
    source.bibliography =
        inclusion == QLatin1String(SourceDescription::LINKED) ? QString() : inclusion;
    if (source == m_source) {
        return;
    }
    if (!m_book.setSource(m_elementId, source)) {
        dialogs::MessageDialog::warning(this, tr("Bibliographic Description Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    m_source = source;
    emit changed(m_elementId);
}

void BibliographyEntryBox::showMaterial(const core::ProjectElement& element) {
    const bool other = element.id != m_elementId;
    m_elementId = element.id;
    m_title = element.title;
    m_source = element.source;
    fillStyles(other || m_bookStyle != m_book.bibliographyStyle());
    {
        const QSignalBlocker blocker(m_inclusionBox);
        const int at = m_inclusionBox->findData(m_source.bibliography);
        m_inclusionBox->setCurrentIndex(std::max(at, 0));
    }
    m_inclusionBox->setEnabled(!m_book.isReadOnly() && !m_source.isEmpty());
    updateEntry();
}

QString BibliographyEntryBox::style() const {
    const QString chosen = m_styleBox->currentData().toString();
    return chosen.isEmpty() ? m_book.bibliographyStyle() : chosen;
}

QString BibliographyEntryBox::entryText() const {
    return Bibliography::entry(m_source, m_title, style(), m_book.bookLanguage(), false);
}

void BibliographyEntryBox::fillStyles(bool chooseBook) {
    const QString previous = m_styleBox->currentData().toString();
    m_bookStyle = m_book.bibliographyStyle();
    const QSignalBlocker blocker(m_styleBox);
    m_styleBox->clear();
    for (const QString& each : Bibliography::styles()) {
        const QString name = Bibliography::styleName(each);
        //: The style of the book among the styles of a bibliography: %1 its name. In Polish:
        //: %1 (domyślna książki)
        m_styleBox->addItem(each == m_bookStyle ? tr("%1 (the book's)").arg(name) : name, each);
    }
    const int at = m_styleBox->findData(chooseBook || previous.isEmpty() ? m_bookStyle : previous);
    m_styleBox->setCurrentIndex(std::max(at, 0));
    if (m_compact) {
        //: The short card: %1 the style of the book. In Polish: W bibliografii · %1
        setTitle(tr("In the Bibliography · %1").arg(Bibliography::styleName(m_bookStyle)));
    }
}

void BibliographyEntryBox::updateEntry() {
    const bool empty = m_source.isEmpty();
    if (empty) {
        m_entryLabel->setTextFormat(Qt::PlainText);
        //: In Polish: Uzupełnij opis bibliograficzny, a tu pojawi się zapis w bibliografii.
        m_entryLabel->setText(tr("Fill in the bibliographic description, and its entry in the "
                                 "bibliography shows here."));
        m_entryLabel->setForegroundRole(QPalette::PlaceholderText);
    } else {
        m_entryLabel->setTextFormat(Qt::RichText);
        m_entryLabel->setText(
            Bibliography::entry(m_source, m_title, style(), m_book.bookLanguage(), true));
        m_entryLabel->setForegroundRole(QPalette::WindowText);
    }
    m_copyButton->setEnabled(!empty);
}

void BibliographyEntryBox::copyEntry() {
    if (m_source.isEmpty()) {
        return;
    }
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        return;
    }
    auto* mime = new QMimeData();
    mime->setHtml(Bibliography::entry(m_source, m_title, style(), m_book.bookLanguage(), true));
    mime->setText(entryText());
    clipboard->setMimeData(mime);
    //: %1: the style. In Polish: Skopiowano zapis (%1)
    emit message(tr("Entry copied (%1)").arg(Bibliography::styleName(style())));
}

// =============================================================================
// BookmarksBox
// =============================================================================

BookmarksBox::BookmarksBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_book(book) {
    setObjectName(QStringLiteral("sourceBookmarks"));
    //: Places in the file of a material: a page and a note each. In Polish: Zakładki
    setTitle(tr("Bookmarks"));
    QVBoxLayout* layout = groupLayout(this);
    //: In Polish: Dodaj zakładkę (Insert)
    m_addButton = listButton(this, "common.add", tr("Add Bookmark (Insert)"), "bookmarksAdd");
    //: In Polish: Usuń zakładkę (Delete)
    m_removeButton = listButton(this, "common.remove", tr("Delete Bookmark (Delete)"),
                                "bookmarksRemove");
    //: In Polish: Pokaż stronę w podglądzie
    m_showButton = listButton(this, "common.preview", tr("Show the Page in the Preview"),
                              "bookmarksShow");
    QHBoxLayout* buttons = buttonRow(m_addButton, m_removeButton);
    buttons->insertWidget(1, m_showButton);
    layout->addLayout(buttons);
    m_table = cardTable(this, "bookmarksTable",
                        //: In Polish: Zakładki pliku
                        tr("Bookmarks of the file"),
                        //: The columns of the bookmarks. In Polish: Strona
                        {tr("Page"),
                         //: In Polish: Notatka
                         tr("Note")},
                        true);
    m_table->horizontalHeader()->setSectionResizeMode(PAGE_COLUMN, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(NOTE_COLUMN, QHeaderView::Stretch);
    m_table->setItemDelegate(new PageDelegate(m_table));
    m_table->installEventFilter(this);
    layout->addWidget(m_table);
    //: In Polish: Brak zakładek. „+” albo Insert dodaje zakładkę.
    m_emptyLabel = hintLabel(this, tr("No bookmarks. \"+\" or Insert adds a bookmark."),
                             "bookmarksEmpty");
    layout->addWidget(m_emptyLabel);
    connect(m_addButton, &QToolButton::clicked, this, &BookmarksBox::addBookmark);
    connect(m_removeButton, &QToolButton::clicked, this, &BookmarksBox::removeBookmark);
    connect(m_showButton, &QToolButton::clicked, this, &BookmarksBox::showPage);
    connect(m_table, &QTableWidget::itemChanged, this, [this]() {
        if (!m_updating) {
            save();
        }
    });
    connect(m_table, &QTableWidget::currentCellChanged, this, &BookmarksBox::updateButtons);
}

void BookmarksBox::showMaterial(const core::ProjectElement& element) {
    const bool other = element.id != m_elementId;
    m_elementId = element.id;
    m_lastPage = element.lastPage;
    m_preview = PdfFiles::previewAvailable() && PdfFiles::isPdf(m_book.filePathOf(element));
    if (other || (!isEditing(m_table) && element.bookmarks != m_bookmarks)) {
        m_bookmarks = element.bookmarks;
        m_updating = true;
        m_table->setRowCount(0);
        for (const core::PageBookmark& bookmark : m_bookmarks) {
            const int row = m_table->rowCount();
            m_table->insertRow(row);
            QTableWidgetItem* page = pageItem(bookmark.page);
            page->setData(EXTRA_ROLE, bookmark.extra);
            m_table->setItem(row, PAGE_COLUMN, page);
            m_table->setItem(row, NOTE_COLUMN, new QTableWidgetItem(bookmark.note));
        }
        m_updating = false;
    }
    m_table->setEditTriggers(editTriggers(m_book.isReadOnly()));
    fitRows(m_table);
    updateButtons();
}

void BookmarksBox::updateButtons() {
    const bool editable = !m_elementId.isEmpty() && !m_book.isReadOnly();
    m_addButton->setEnabled(editable);
    m_removeButton->setEnabled(editable && m_table->currentRow() >= 0 &&
                               m_table->rowCount() > 0);
    m_showButton->setVisible(m_preview);
    m_showButton->setEnabled(m_table->currentRow() >= 0 && m_table->rowCount() > 0);
    const bool none = m_table->rowCount() == 0;
    m_table->setVisible(!none);
    m_emptyLabel->setVisible(none);
}

void BookmarksBox::addBookmark() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    m_updating = true;
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, PAGE_COLUMN, pageItem(std::max(m_lastPage, 1)));
    m_table->setItem(row, NOTE_COLUMN, new QTableWidgetItem());
    m_updating = false;
    save();
    fitRows(m_table);
    updateButtons();
    m_table->setFocus(Qt::OtherFocusReason);
    m_table->setCurrentCell(row, PAGE_COLUMN);
    m_table->editItem(m_table->item(row, PAGE_COLUMN));
}

void BookmarksBox::removeBookmark() {
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount() || m_book.isReadOnly()) {
        return;
    }
    const QTableWidgetItem* page = m_table->item(row, PAGE_COLUMN);
    const QTableWidgetItem* note = m_table->item(row, NOTE_COLUMN);
    if (note != nullptr && !note->text().trimmed().isEmpty() &&
        //: In Polish: Usuń zakładkę
        !dialogs::MessageDialog::confirm(
            this, tr("Delete Bookmark"),
            //: %1: the page. In Polish: Usunąć zakładkę strony %1 razem z notatką?
            tr("Delete the bookmark of page %1 with its note?")
                .arg(page != nullptr ? page->text() : QString()),
            //: In Polish: &Usuń
            tr("&Delete"))) {
        return;
    }
    m_updating = true;
    m_table->removeRow(row);
    m_updating = false;
    save();
    fitRows(m_table);
    updateButtons();
    if (m_table->rowCount() > 0) {
        m_table->setCurrentCell(std::min(row, m_table->rowCount() - 1), PAGE_COLUMN);
        m_table->setFocus(Qt::OtherFocusReason);
    }
}

void BookmarksBox::showPage() {
    const QTableWidgetItem* page = m_table->item(m_table->currentRow(), PAGE_COLUMN);
    if (m_preview && page != nullptr) {
        emit requestShowPage(m_elementId, page->text());
    }
}

void BookmarksBox::save() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    QList<core::PageBookmark> bookmarks;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem* page = m_table->item(row, PAGE_COLUMN);
        const QTableWidgetItem* note = m_table->item(row, NOTE_COLUMN);
        core::PageBookmark bookmark;
        if (page != nullptr) {
            bookmark.page = std::clamp(page->data(Qt::EditRole).toInt(), 1, MAXIMUM_PAGE);
            bookmark.extra = page->data(EXTRA_ROLE).toJsonObject();
        }
        bookmark.note = note != nullptr ? note->text().trimmed() : QString();
        bookmarks << bookmark;
    }
    if (bookmarks == m_bookmarks) {
        return;
    }
    if (!m_book.setBookmarks(m_elementId, bookmarks)) {
        //: In Polish: Nie zapisano zakładek
        dialogs::MessageDialog::warning(this, tr("Bookmarks Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    m_bookmarks = bookmarks;
    emit changed(m_elementId);
}

bool BookmarksBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_table &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        const bool insert = isListKey(key, Qt::Key_Insert);
        const bool remove = isListKey(key, Qt::Key_Delete);
        if (insert || remove) {
            if (event->type() == QEvent::ShortcutOverride) {
                key->accept();
                return true;
            }
            insert ? addBookmark() : removeBookmark();
            return true;
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

// =============================================================================
// SourceQuotesBox
// =============================================================================

SourceQuotesBox::SourceQuotesBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_book(book) {
    setObjectName(QStringLiteral("sourceQuotes"));
    //: In Polish: Cytaty z tego źródła
    setTitle(tr("Quotes from This Source"));
    QVBoxLayout* layout = groupLayout(this);
    //: In Polish: Nowy cytat z tego źródła... (Insert)
    m_addButton = listButton(this, "common.add", tr("New Quote from This Source... (Insert)"),
                             "sourceQuotesAdd");
    layout->addLayout(buttonRow(m_addButton, nullptr));
    m_table = cardTable(this, "sourceQuotesTable",
                        //: In Polish: Cytaty z tego źródła
                        tr("Quotes from this source"),
                        //: The columns of the quotes of a source. In Polish: Strona
                        {tr("Page"),
                         //: In Polish: Cytat
                         tr("Quote")},
                        false);
    m_table->horizontalHeader()->setSectionResizeMode(PAGE_COLUMN, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(NOTE_COLUMN, QHeaderView::Stretch);
    //: In Polish: Dwuklik albo Enter otwiera cytat
    m_table->setToolTip(tr("A double-click or Enter opens the quote"));
    m_table->installEventFilter(this);
    layout->addWidget(m_table);
    //: In Polish: Brak cytatów. „+” albo Insert dodaje cytat z tego źródła.
    m_emptyLabel = hintLabel(this,
                             tr("No quotes. \"+\" or Insert adds a quote from this source."),
                             "sourceQuotesEmpty");
    layout->addWidget(m_emptyLabel);
    connect(m_addButton, &QToolButton::clicked, this, [this]() {
        if (!m_elementId.isEmpty() && !m_book.isReadOnly()) {
            emit requestNewQuote(m_elementId);
        }
    });
    connect(m_table, &QTableWidget::itemActivated, this,
            [this](const QTableWidgetItem* item) { openQuote(item->row()); });
    // A quote got this source or lost it
    connect(&m_book, &core::ProjectManager::sourcesChanged, this, [this]() {
        if (const core::ProjectElement* element = m_book.findElement(m_elementId)) {
            showMaterial(*element);
        }
    });
}

void SourceQuotesBox::showMaterial(const core::ProjectElement& element) {
    m_elementId = element.id;
    const QTableWidgetItem* currentItem =
        m_table->currentRow() >= 0 ? m_table->item(m_table->currentRow(), PAGE_COLUMN) : nullptr;
    const QString current = currentItem != nullptr ? currentItem->data(ID_ROLE).toString()
                                                   : QString();
    m_table->setRowCount(0);
    for (const core::ProjectElement* quote : m_book.quotesOf(m_elementId)) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        QTableWidgetItem* page = shownItem(quote->quote.page);
        page->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        page->setData(ID_ROLE, quote->id);
        m_table->setItem(row, PAGE_COLUMN, page);
        const QString words = quote->description.simplified();
        QTableWidgetItem* text = shownItem(words.isEmpty() ? quote->title : words);
        text->setToolTip(quote->description.isEmpty() ? quote->title : quote->description);
        m_table->setItem(row, NOTE_COLUMN, text);
        if (quote->id == current) {
            m_table->setCurrentCell(row, PAGE_COLUMN);
        }
    }
    fitRows(m_table);
    const bool none = m_table->rowCount() == 0;
    m_table->setVisible(!none);
    m_emptyLabel->setVisible(none);
    m_addButton->setEnabled(!m_book.isReadOnly());
}

void SourceQuotesBox::openQuote(int row) {
    if (const QTableWidgetItem* first = row >= 0 ? m_table->item(row, PAGE_COLUMN) : nullptr) {
        emit requestOpenQuote(first->data(ID_ROLE).toString());
    }
}

bool SourceQuotesBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_table &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (event->type() == QEvent::KeyPress && isOpenKey(key)) {
            openQuote(m_table->currentRow());
            return true;
        }
        if (isListKey(key, Qt::Key_Insert)) {
            if (event->type() == QEvent::ShortcutOverride) {
                key->accept();
                return true;
            }
            if (!m_book.isReadOnly()) {
                emit requestNewQuote(m_elementId);
            }
            return true;
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

// =============================================================================
// QuoteSourceBox
// =============================================================================

QuoteSourceBox::QuoteSourceBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent),
      m_book(book),
      m_form(groupForm()),
      m_pageEdit(new QLineEdit(this)),
      m_authorEdit(new QLineEdit(this)),
      m_workEdit(new QLineEdit(this)),
      m_referenceLabel(new QLabel(this)) {
    setObjectName(QStringLiteral("quoteSource"));
    //: Where a quote comes from. In Polish: Skąd
    setTitle(tr("Where It Comes From"));
    QVBoxLayout* layout = groupLayout(this);
    layout->addLayout(m_form);

    auto* sourceRow = new QWidget(this);
    auto* sourceLayout = new QHBoxLayout(sourceRow);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    sourceLayout->setSpacing(ROW_SPACING);
    m_sourceBox = new QComboBox(sourceRow);
    m_sourceBox->setObjectName(QStringLiteral("quoteSourceBox"));
    //: In Polish: Materiały z opisem bibliograficznym
    m_sourceBox->setToolTip(tr("The materials with a bibliographic description"));
    m_sourceBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_sourceBox->setMinimumContentsLength(SOURCE_LETTERS);
    //: In Polish: Otwórz źródło
    m_openSourceButton = new QPushButton(tr("Open Source"), sourceRow);
    m_openSourceButton->setObjectName(QStringLiteral("quoteOpenSourceButton"));
    keepIcon(m_openSourceButton, QStringLiteral("common.openExternal"), core::IconContext::Button);
    sourceLayout->addWidget(m_sourceBox, 1);
    sourceLayout->addWidget(m_openSourceButton);
    //: In Polish: Źródło:
    m_form->addRow(tr("Source:"), sourceRow);

    m_pageEdit->setObjectName(QStringLiteral("quotePageEdit"));
    //: The page of a quote, as the writer puts it. In Polish: 45, xii albo 41–42
    m_pageEdit->setPlaceholderText(tr("45, xii or 41–42"));
    auto* pageRow = new QWidget(this);
    auto* pageLayout = new QHBoxLayout(pageRow);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(ROW_SPACING);
    m_pageEdit->setParent(pageRow);
    //: In Polish: Pokaż stronę
    m_showPageButton = new QPushButton(tr("Show Page"), pageRow);
    m_showPageButton->setObjectName(QStringLiteral("quoteShowPageButton"));
    //: In Polish: Pokaż stronę w podglądzie pliku źródła
    m_showPageButton->setToolTip(tr("Show the page in the preview of the file of the source"));
    keepIcon(m_showPageButton, QStringLiteral("common.preview"), core::IconContext::Button);
    pageLayout->addWidget(m_pageEdit, 1);
    pageLayout->addWidget(m_showPageButton);
    //: In Polish: Strona:
    auto* pageLabel = new QLabel(tr("Page:"), this);
    pageLabel->setBuddy(m_pageEdit);
    m_form->addRow(pageLabel, pageRow);
    m_authorEdit->setObjectName(QStringLiteral("quoteAuthorEdit"));
    //: Who said a quote without a source. In Polish: Autor:
    m_form->addRow(tr("Author:"), m_authorEdit);
    m_workEdit->setObjectName(QStringLiteral("quoteWorkEdit"));
    //: Where a quote without a source was said. In Polish: Dzieło:
    m_form->addRow(tr("Work:"), m_workEdit);
    m_fromSourceLabel = hintLabel(this, QString(), "quoteFromSourceLabel");
    //: In Polish: Autor, dzieło:
    m_form->addRow(tr("Author, work:"), m_fromSourceLabel);
    m_referenceLabel->setObjectName(QStringLiteral("quoteReferenceLabel"));
    m_referenceLabel->setTextFormat(Qt::PlainText);
    m_referenceLabel->setWordWrap(true);
    m_referenceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                              Qt::TextSelectableByKeyboard);
    //: How the text refers to a quote: (Wiśniewska 2011, s. 45). In Polish: Odnośnik:
    m_form->addRow(tr("Reference:"), m_referenceLabel);

    connect(m_sourceBox, &QComboBox::activated, this, [this]() {
        updateShown();
        save();
    });
    connect(m_openSourceButton, &QPushButton::clicked, this, [this]() {
        const QString source = m_sourceBox->currentData().toString();
        if (m_book.findElement(source) != nullptr) {
            emit requestOpenSource(source, QString());
        }
    });
    connect(m_showPageButton, &QPushButton::clicked, this, [this]() {
        const QString source = m_sourceBox->currentData().toString();
        if (m_book.findElement(source) != nullptr) {
            emit requestOpenSource(source, m_pageEdit->text().trimmed());
        }
    });
    for (QLineEdit* edit : {m_pageEdit, m_authorEdit, m_workEdit}) {
        connect(edit, &QLineEdit::editingFinished, this, &QuoteSourceBox::save);
        connect(edit, &QLineEdit::textEdited, this, &QuoteSourceBox::updateShown);
    }
    // The sources changed: their descriptions, a new one, the style of the book
    connect(&m_book, &core::ProjectManager::sourcesChanged, this, [this]() {
        if (const core::ProjectElement* element = m_book.findElement(m_elementId)) {
            showMaterial(*element);
        }
    });
}

void QuoteSourceBox::showMaterial(const core::ProjectElement& element) {
    const bool other = element.id != m_elementId;
    m_elementId = element.id;
    m_quote = element.quote;
    m_updating = true;
    fillSources(m_quote.source);
    const std::initializer_list<std::pair<QLineEdit*, QString>> edits{
        {m_pageEdit, m_quote.page}, {m_authorEdit, m_quote.author}, {m_workEdit, m_quote.work}};
    for (const auto& [edit, text] : edits) {
        if (other || !edit->hasFocus()) {
            edit->setText(text);
        }
    }
    const bool readOnly = m_book.isReadOnly();
    m_sourceBox->setEnabled(!readOnly);
    for (QLineEdit* edit : {m_pageEdit, m_authorEdit, m_workEdit}) {
        edit->setReadOnly(readOnly);
    }
    m_updating = false;
    updateShown();
}

core::QuoteSource QuoteSourceBox::shownQuote() const {
    core::QuoteSource quote = m_quote;
    quote.source = m_sourceBox->currentData().toString();
    quote.page = m_pageEdit->text().trimmed();
    quote.author = m_authorEdit->text().trimmed();
    quote.work = m_workEdit->text().trimmed();
    return quote;
}

void QuoteSourceBox::save() {
    if (m_updating || m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const core::QuoteSource quote = shownQuote();
    if (quote == m_quote) {
        return;
    }
    if (!m_book.setQuoteSource(m_elementId, quote)) {
        //: In Polish: Nie zapisano źródła cytatu
        dialogs::MessageDialog::warning(this, tr("Source of the Quote Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    m_quote = quote;
    emit changed(m_elementId);
}

void QuoteSourceBox::fillSources(const QString& chosen) {
    QList<const core::ProjectElement*> sources = m_book.sources();
    sources.removeIf(
        [this](const core::ProjectElement* each) { return each->id == m_elementId; });
    std::ranges::sort(sources, [](const core::ProjectElement* left,
                                  const core::ProjectElement* right) {
        return QString::localeAwareCompare(sourceTitle(*left), sourceTitle(*right)) < 0;
    });
    const QSignalBlocker blocker(m_sourceBox);
    m_sourceBox->clear();
    //: The choice of a quote that comes from no material. In Polish: Bez źródła – autor i
    //: dzieło wpisane niżej
    m_sourceBox->addItem(tr("No source – the author and the work below"), QString());
    for (const core::ProjectElement* source : sources) {
        m_sourceBox->addItem(SourceNames::shortName(source->source, source->title), source->id);
    }
    if (!chosen.isEmpty() && m_sourceBox->findData(chosen) < 0) {
        m_sourceBox->addItem(lostSourceName(m_book.findElement(chosen)), chosen);
    }
    m_sourceBox->setCurrentIndex(std::max(m_sourceBox->findData(chosen), 0));
}

QString QuoteSourceBox::lostSourceName(const core::ProjectElement* material) {
    if (material == nullptr) {
        //: In Polish: Nie ma już tego źródła
        return tr("The source is gone");
    }
    //: A source of a quote whose description was emptied: %1 its title. In Polish: %1 (bez
    //: opisu bibliograficznego)
    return tr("%1 (no bibliographic description)").arg(material->title);
}

void QuoteSourceBox::updateShown() {
    const core::QuoteSource quote = shownQuote();
    const core::ProjectElement* source =
        quote.source.isEmpty() ? nullptr : m_book.findElement(quote.source);
    const bool none = quote.source.isEmpty();
    m_form->setRowVisible(m_authorEdit, none);
    m_form->setRowVisible(m_workEdit, none);
    m_form->setRowVisible(m_fromSourceLabel, !none);
    m_openSourceButton->setEnabled(source != nullptr);
    // The page in the preview of the PDF file of the source
    m_showPageButton->setVisible(source != nullptr && PdfFiles::previewAvailable() &&
                                 PdfFiles::isPdf(m_book.filePathOf(*source)));
    m_showPageButton->setEnabled(!m_pageEdit->text().trimmed().isEmpty());

    const QString language = m_book.bookLanguage();
    QString reference;
    if (source != nullptr) {
        QStringList authors;
        for (const core::SourcePerson& person :
             source->source.peopleOf(SourceDescription::AUTHOR)) {
            authors << person.name;
        }
        const QString title = sourceTitle(*source);
        if (authors.isEmpty()) {
            //: The author and the work of a quote from a source: %1 its title. In Polish: z
            //: karty źródła: „%1”
            m_fromSourceLabel->setText(tr("From the card of the source: “%1”").arg(title));
        } else {
            //: %1 the authors, %2 the title. In Polish: z karty źródła: %1, „%2”
            m_fromSourceLabel->setText(tr("From the card of the source: %1, “%2”")
                                           .arg(authors.join(QStringLiteral(", ")), title));
        }
        reference = SourceNames::referenceTo(m_book, quote.source, quote.page);
    } else if (none) {
        reference = Bibliography::reference(quote, language);
    } else {
        m_fromSourceLabel->setText(tr("The source is gone"));
    }
    //: In Polish: Wybierz źródło albo wpisz autora.
    m_referenceLabel->setText(reference.isEmpty() ? tr("Choose a source or type the author.")
                                                  : reference);
    m_referenceLabel->setForegroundRole(reference.isEmpty() ? QPalette::PlaceholderText
                                                            : QPalette::WindowText);
}

}  // namespace kalahari::gui
