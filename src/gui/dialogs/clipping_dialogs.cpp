// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file clipping_dialogs.cpp
/// @brief The windows of the clippings: Cut to Clippings and Insert > Clipping

#include "kalahari/gui/dialogs/clipping_dialogs.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/quick_insert.h"
#include "kalahari/gui/clipping_texts.h"
#include "kalahari/gui/clippings_coordinator.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/version_texts.h"
#include "kalahari/gui/widgets/quick_character_palette.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

/// @brief Characters of a title at most
constexpr int MAX_TITLE_LENGTH = 200;

/// @brief Lines of the field of the note
constexpr int NOTE_LINES = 3;

/// @brief The window of Insert > Clipping at first, in average characters
constexpr int WINDOW_WIDTH_CHARACTERS = 110;
constexpr int WINDOW_HEIGHT_LINES = 30;

/// @brief The list at least this many characters wide, the preview this many
constexpr int LIST_WIDTH_CHARACTERS = 34;
constexpr int PREVIEW_WIDTH_CHARACTERS = 40;

/// @brief The part of the screen the window takes at most at first
constexpr double SCREEN_PART = 0.9;

/// @brief The list at the cursor, in average characters and in rows
constexpr int POPUP_WIDTH_CHARACTERS = 56;
constexpr int POPUP_ROWS = 8;
constexpr int POPUP_MARGIN = 4;

/// @brief The role of the id of a clipping in its row
constexpr int ID_ROLE = Qt::UserRole;

/// @brief Where a clipping comes from and when, in its row and over the preview:
///        "Rozdział 2 · today, 7:42 · 46 words"
QString detailsOf(const ClippingEntry& clipping) {
    QStringList parts;
    if (!clipping.from.isEmpty()) {
        parts << clipping.from;
    }
    if (clipping.cut.isValid()) {
        parts << versionTime(clipping.cut);
    }
    parts << wordCountText(clipping.words);
    if (clipping.inserted) {
        //: A clipping inserted into a text already, in its row. In Polish: wstawiony
        parts << InsertClippingDialog::tr("inserted");
    }
    return parts.join(QStringLiteral(" · "));
}

/// @brief Whether @p clipping has all the words of @p query (folded for the search)
bool matches(const ClippingEntry& clipping, const QString& query) {
    const QStringList words = query.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return std::ranges::all_of(words, [&clipping](const QString& word) {
        return clipping.search.contains(word);
    });
}

/// @brief Add the row of @p clipping to @p list
QListWidgetItem* addRow(QListWidget* list, const ClippingEntry& clipping) {
    auto* item =
        new QListWidgetItem(clipping.title + QLatin1Char('\n') + detailsOf(clipping), list);
    item->setData(ID_ROLE, clipping.id);
    item->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("template.clipping"),
                                                           core::IconContext::Menu));
    return item;
}

/// @brief The keys that choose, from the search to @p list: Up, Down, Page Up, Page Down
/// @return false for another key
bool passedToList(QListWidget* list, const QKeyEvent& key) {
    switch (key.key()) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        if (list->count() > 0) {
            QKeyEvent copy(key.type(), key.key(), key.modifiers(), key.text());
            QCoreApplication::sendEvent(list, &copy);
        }
        return true;
    default:
        return false;
    }
}

/// @brief Choose the first row of @p list when none is chosen
void chooseFirst(QListWidget* list) {
    if (list->currentRow() < 0 && list->count() > 0) {
        list->setCurrentRow(0);
    }
}

/// @brief What Restore to the Place It Came From does with @p clipping, or why it cannot
QString restoreToolTip(const ClippingEntry& clipping) {
    return ClippingTexts::restoreToolTip(
        {.title = clipping.fromId.isEmpty() ? QString() : clipping.from,
         .live = !clipping.fromId.isEmpty() && !clipping.originGone});
}

}  // namespace

// =============================================================================
// Cut to Clippings
// =============================================================================

CutClippingDialog::CutClippingDialog(const QString& title, int words, const QString& from,
                                     QWidget* parent)
    : KalahariDialog(parent)
    , m_title(new QLineEdit(title, this))
    , m_note(new QPlainTextEdit(this))
{
    //: The small window of Edit > Cut to Clippings. In Polish: Wytnij do wycinków
    const QString heading = tr("Cut to Clippings");
    //: %1: "46 words", %2: the title of the text. In Polish: Fragment (%1) przejdzie z „%2” do
    //: wycinków w Warsztacie. Można do niego wrócić: Wstaw > Wycinek.
    setHeading(heading, tr("The fragment (%1) goes from \"%2\" to the clippings in the "
                           "Workshop. It can come back later: Insert > Clipping.")
                            .arg(wordCountText(words), from));
    setHeadingIcon(QString::fromLatin1(ClippingsCoordinator::CUT_COMMAND));

    m_title->setObjectName(QStringLiteral("clippingTitle"));
    m_title->setMaxLength(MAX_TITLE_LENGTH);
    m_title->selectAll();
    //: In Polish: &Tytuł
    addField(tr("&Title"), m_title);

    m_note->setObjectName(QStringLiteral("clippingNote"));
    m_note->setTabChangesFocus(true);
    //: In Polish: Np. dlaczego wycięty albo gdzie może wrócić
    m_note->setPlaceholderText(tr("E.g. why it was cut, or where it may come back"));
    const auto margin = static_cast<int>(m_note->document()->documentMargin() * 2);
    m_note->setFixedHeight((m_note->fontMetrics().lineSpacing() * NOTE_LINES) +
                           (m_note->frameWidth() * 2) + margin);
    //: In Polish: &Notatka (nieobowiązkowa)
    addField(tr("&Note (optional)"), m_note);

    //: In Polish: Następnym razem wycinaj od &razu
    m_atOnce = new QCheckBox(tr("Next time cut at &once"), this);
    m_atOnce->setObjectName(QStringLiteral("clippingCutAtOnce"));
    //: In Polish: Bez tego okna; tytuł to pierwsze słowa fragmentu. Wybór jest też w Ustawieniach
    //: (Edytor > Wycinki).
    m_atOnce->setToolTip(tr("Without this window; the title is the first words of the "
                            "fragment. The choice is also in the Settings (Editor > Clippings)."));
    contentLayout()->addWidget(m_atOnce);

    //: The gear of the window. In Polish: Ustawienia wycinków
    addSettingsButton(QString::fromLatin1(ClippingsCoordinator::SETTINGS_PAGE),
                      tr("Clipping Settings"));
    //: The button that cuts. In Polish: Wytnij
    setAcceptText(tr("Cut"));
    m_title->setFocus();
}

QString CutClippingDialog::title() const
{
    return m_title->text().trimmed();
}

QString CutClippingDialog::note() const
{
    return m_note->toPlainText().trimmed();
}

bool CutClippingDialog::cutAtOnceNextTime() const
{
    return m_atOnce->isChecked();
}

// =============================================================================
// Insert > Clipping
// =============================================================================

InsertClippingDialog::InsertClippingDialog(QList<ClippingEntry> clippings, TextOf textOf,
                                           const editor::BookEditor* text,
                                           const QString& textTitle, const QString& textId,
                                           bool keepsInserted, QWidget* parent)
    : KalahariDialog(parent)
    , m_clippings(std::move(clippings))
    , m_textOf(std::move(textOf))
    , m_textId(textId)
    , m_search(new QLineEdit(this))
    , m_from(new QComboBox(this))
{
    //: The window of Insert > Clipping. In Polish: Wstaw wycinek
    const QString heading = tr("Insert Clipping");
    //: %1: the title of the text. In Polish: do „%1”, w miejscu kursora
    setHeading(heading, tr("into \"%1\", at the cursor").arg(textTitle));
    setHeadingIcon(QString::fromLatin1(ClippingsCoordinator::INSERT_COMMAND));
    setCompactHeading(true);

    // The search, where they come from, and the inserted ones
    auto* filters = new QHBoxLayout();
    m_search->setObjectName(QStringLiteral("clippingSearch"));
    m_search->setClearButtonEnabled(true);
    //: In Polish: Szukaj w wycinkach
    m_search->setPlaceholderText(tr("Search the clippings"));
    //: In Polish: Szukaj w tytułach, tekstach i notatkach wycinków
    m_search->setAccessibleName(tr("Search in the titles, texts and notes of the clippings"));
    filters->addWidget(m_search, 1);

    m_from->setObjectName(QStringLiteral("clippingFrom"));
    //: Where from: all the clippings. In Polish: wszystkie
    m_from->addItem(tr("all"), QString());
    //: Where from: the clippings of the text the window inserts into. In Polish: ten tekst
    m_from->addItem(tr("this text"), textId);
    QStringList fromIds;
    for (const ClippingEntry& clipping : std::as_const(m_clippings)) {
        if (!clipping.fromId.isEmpty() && clipping.fromId != textId &&
            !fromIds.contains(clipping.fromId)) {
            fromIds << clipping.fromId;
            m_from->addItem(clipping.from, clipping.fromId);
        }
    }
    //: In Polish: &Skąd:
    auto* fromLabel = new QLabel(tr("&From:"), this);
    fromLabel->setBuddy(m_from);
    filters->addWidget(fromLabel);
    filters->addWidget(m_from);

    if (keepsInserted) {
        //: In Polish: Pokaż &także wstawione
        m_inserted = new QCheckBox(tr("Show &inserted too"), this);
        m_inserted->setObjectName(QStringLiteral("clippingShowInserted"));
        filters->addWidget(m_inserted);
    }
    contentLayout()->addLayout(filters);

    // The list, and the preview of the one chosen
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("clippingList"));
    //: In Polish: Wycinki
    m_list->setAccessibleName(tr("Clippings"));
    m_list->setMinimumWidth(fontMetrics().averageCharWidth() * LIST_WIDTH_CHARACTERS);
    m_list->setWordWrap(true);
    m_list->setUniformItemSizes(false);

    auto* previewColumn = new QWidget(this);
    auto* previewLayout = new QVBoxLayout(previewColumn);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    m_details = new QLabel(previewColumn);
    m_details->setObjectName(QStringLiteral("clippingDetails"));
    m_details->setTextFormat(Qt::PlainText);
    m_details->setWordWrap(true);
    previewLayout->addWidget(m_details);
    m_preview = new editor::BookEditor(previewColumn);
    m_preview->setObjectName(QStringLiteral("clippingPreview"));
    //: In Polish: Podgląd wycinka, tylko do czytania
    m_preview->setAccessibleName(tr("Preview of the clipping, read-only"));
    m_preview->setMinimumWidth(fontMetrics().averageCharWidth() * PREVIEW_WIDTH_CHARACTERS);
    editor::EditorAppearance appearance =
        text != nullptr ? text->appearance() : m_preview->appearance();
    appearance.typewriter.enabled = false;
    appearance.focusMode.enabled = false;
    m_preview->setAppearance(appearance);
    if (text != nullptr) {
        m_preview->setParagraphStyles(text->paragraphStyles());
        m_preview->setPaperScale(text->paperScale());
        m_preview->setImagesFolder(text->imagesFolder());
    }
    m_preview->setWordCountRules(core::wordCountRules());
    m_preview->setReadOnly(true);
    m_preview->setKeepsPageWidth(true);
    //: In Polish: Podgląd wycinka jest tylko do czytania
    m_preview->setReadOnlyReason(tr("The preview of a clipping can only be read"));
    previewLayout->addWidget(m_preview, 1);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(m_list);
    splitter->addWidget(previewColumn);
    splitter->setStretchFactor(1, 1);
    contentLayout()->addWidget(splitter, 1);

    //: The keys of the window, under the list. In Polish: Pisz, żeby szukać · ↑ ↓ wybór · Enter –
    //: wstaw przy kursorze · Alt+P – na pierwotne miejsce · Esc – zamknij
    auto* keys = new QLabel(tr("Type to search · ↑ ↓ choose · Enter – "
                               "insert at the cursor · Alt+P – the place it came "
                               "from · Esc – close"),
                            this);
    keys->setObjectName(QStringLiteral("clippingKeys"));
    keys->setWordWrap(true);
    keys->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(keys);

    // The buttons: Open on the side; Restore, Insert and Close
    //: In Polish: &Otwórz
    m_open = addSideButton(tr("&Open"));
    m_open->setObjectName(QStringLiteral("clippingOpen"));
    //: In Polish: Otwórz wycinek w edytorze
    m_open->setToolTip(tr("Open the clipping in the editor"));
    connect(m_open, &QPushButton::clicked, this, [this]() { finish(Choice::Open); });
    //: In Polish: &Przywróć na pierwotne miejsce
    m_restore = new QPushButton(tr("Restore to the &Place It Came From"), this);
    m_restore->setObjectName(QStringLiteral("clippingRestore"));
    m_restore->setAutoDefault(false);
    buttonBox()->addButton(m_restore, QDialogButtonBox::ActionRole);
    connect(m_restore, &QPushButton::clicked, this, [this]() { finish(Choice::Restore); });
    //: The gear of the window. In Polish: Ustawienia wycinków
    addSettingsButton(QString::fromLatin1(ClippingsCoordinator::SETTINGS_PAGE),
                      tr("Clipping Settings"));
    //: In Polish: &Wstaw
    setAcceptText(tr("&Insert"));
    //: In Polish: Zamknij
    cancelButton()->setText(tr("Close"));
    // Enter and Insert insert the one chosen, as its own choice
    disconnect(buttonBox(), &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox(), &QDialogButtonBox::accepted, this, [this]() { finish(Choice::Insert); });

    connect(m_search, &QLineEdit::textChanged, this, &InsertClippingDialog::filter);
    connect(m_from, &QComboBox::currentIndexChanged, this, &InsertClippingDialog::filter);
    if (m_inserted != nullptr) {
        connect(m_inserted, &QCheckBox::toggled, this, &InsertClippingDialog::filter);
    }
    connect(m_list, &QListWidget::currentRowChanged, this, &InsertClippingDialog::showChosen);
    connect(m_list, &QListWidget::itemActivated, this, [this]() { finish(Choice::Insert); });
    m_search->installEventFilter(this);

    filter();
    setTabOrder(m_search, m_from);
    m_search->setFocus();

    const QFontMetrics metrics = fontMetrics();
    QSize size(metrics.averageCharWidth() * WINDOW_WIDTH_CHARACTERS,
               metrics.lineSpacing() * WINDOW_HEIGHT_LINES);
    if (const QScreen* screen = this->screen()) {
        size = size.boundedTo(screen->availableGeometry().size() * SCREEN_PART);
    }
    resize(size);
}

QString InsertClippingDialog::chosenId() const
{
    const QListWidgetItem* item = m_list->currentItem();
    return item != nullptr ? item->data(ID_ROLE).toString() : QString();
}

QStringList InsertClippingDialog::shownIds() const
{
    QStringList ids;
    for (int row = 0; row < m_list->count(); ++row) {
        ids << m_list->item(row)->data(ID_ROLE).toString();
    }
    return ids;
}

bool InsertClippingDialog::eventFilter(QObject* watched, QEvent* event)
{
    // The keys that choose go from the search to the list
    if (watched == m_search && event->type() == QEvent::KeyPress &&
        passedToList(m_list, *static_cast<QKeyEvent*>(event))) {
        return true;
    }
    return KalahariDialog::eventFilter(watched, event);
}

void InsertClippingDialog::filter()
{
    const QString chosen = chosenId();
    const QString query = editor::foldForSearch(m_search->text().simplified());
    m_list->clear();
    for (const ClippingEntry& clipping : std::as_const(m_clippings)) {
        if (!letsThrough(clipping) || !matches(clipping, query)) {
            continue;
        }
        if (QListWidgetItem* item = addRow(m_list, clipping); clipping.id == chosen) {
            m_list->setCurrentItem(item);
        }
    }
    chooseFirst(m_list);
    showChosen();
}

bool InsertClippingDialog::letsThrough(const ClippingEntry& clipping) const
{
    const bool withInserted = m_inserted != nullptr && m_inserted->isChecked();
    const QString from = m_from->currentData().toString();
    return (withInserted || !clipping.inserted) && (from.isEmpty() || clipping.fromId == from);
}

void InsertClippingDialog::showChosen()
{
    const auto clipping = std::ranges::find(m_clippings, chosenId(), &ClippingEntry::id);
    const bool any = clipping != m_clippings.cend();
    acceptButton()->setEnabled(any);
    m_open->setEnabled(any);
    m_restore->setEnabled(any && !clipping->fromId.isEmpty() && !clipping->originGone);
    if (!any) {
        m_details->setText(m_clippings.isEmpty()
                               //: In Polish: Książka nie ma jeszcze wycinków.
                               ? tr("The book has no clippings yet.")
                               //: In Polish: Żaden wycinek nie pasuje.
                               : tr("No clipping matches."));
        m_preview->fromKml(QString());
        return;
    }
    QString details = clipping->title + QStringLiteral(" · ") + detailsOf(*clipping);
    if (!clipping->note.isEmpty()) {
        details += QLatin1Char('\n') + clipping->note;
    }
    m_details->setText(details);
    m_restore->setToolTip(restoreToolTip(*clipping));
    m_preview->fromKml(m_textOf ? m_textOf(clipping->id) : QString());
}

void InsertClippingDialog::finish(Choice choice)
{
    if (chosenId().isEmpty()) {
        return;
    }
    if (choice == Choice::Restore && !m_restore->isEnabled()) {
        return;
    }
    m_choice = choice;
    accept();
}

// =============================================================================
// The list at the cursor
// =============================================================================

ClippingListPopup::ClippingListPopup(QList<ClippingEntry> clippings, QWidget* parent)
    : QFrame(parent, Qt::Popup)
    , m_clippings(std::move(clippings))
    , m_search(new QLineEdit(this))
    , m_list(new QListWidget(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setObjectName(QStringLiteral("clippingListPopup"));
    //: The list of Insert > Clipping at the cursor. In Polish: Wstaw wycinek
    setAccessibleName(InsertClippingDialog::tr("Insert Clipping"));
    // Those not inserted yet first
    std::ranges::stable_partition(m_clippings,
                                  [](const ClippingEntry& each) { return !each.inserted; });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(POPUP_MARGIN, POPUP_MARGIN, POPUP_MARGIN, POPUP_MARGIN);
    m_search->setObjectName(QStringLiteral("clippingSearch"));
    m_search->setClearButtonEnabled(true);
    m_search->setPlaceholderText(InsertClippingDialog::tr("Search the clippings"));
    m_search->setAccessibleName(
        InsertClippingDialog::tr("Search in the titles, texts and notes of the clippings"));
    m_search->installEventFilter(this);
    layout->addWidget(m_search);

    m_list->setObjectName(QStringLiteral("clippingList"));
    m_list->setAccessibleName(InsertClippingDialog::tr("Clippings"));
    m_list->setFocusPolicy(Qt::NoFocus);  // the keys stay in the search
    m_list->setWordWrap(true);
    m_list->setUniformItemSizes(false);
    layout->addWidget(m_list, 1);

    //: The keys of the list at the cursor. In Polish: Enter – wstaw · Alt+P – na pierwotne
    //: miejsce · Alt+O – otwórz · Esc – zamknij
    auto* keys = new QLabel(tr("Enter – insert · Alt+P – the place it came from · Alt+O – "
                               "open · Esc – close"),
                            this);
    keys->setObjectName(QStringLiteral("clippingKeys"));
    keys->setWordWrap(true);
    keys->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(keys);

    connect(m_search, &QLineEdit::textChanged, this, &ClippingListPopup::filter);
    connect(m_list, &QListWidget::itemClicked, this,
            [this]() { finish(ClippingChoice::Insert); });
    filter();

    const QFontMetrics metrics = fontMetrics();
    resize(metrics.averageCharWidth() * POPUP_WIDTH_CHARACTERS,
           m_search->sizeHint().height() + keys->sizeHint().height() +
               (metrics.lineSpacing() * 2 * POPUP_ROWS) + (POPUP_MARGIN * 4));
}

void ClippingListPopup::showAt(const QRect& cursor)
{
    const QScreen* screen = QGuiApplication::screenAt(cursor.center());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect available = screen != nullptr ? screen->availableGeometry() : QRect();
    if (available.isValid()) {
        resize(size().boundedTo(available.size()));
        move(QuickCharacterPalette::placeFor(cursor, size(), available).topLeft());
    } else {
        move(cursor.bottomLeft());
    }
    show();
}

QString ClippingListPopup::chosenId() const
{
    const QListWidgetItem* item = m_list->currentItem();
    return item != nullptr ? item->data(ID_ROLE).toString() : QString();
}

QStringList ClippingListPopup::shownIds() const
{
    QStringList ids;
    for (int row = 0; row < m_list->count(); ++row) {
        ids << m_list->item(row)->data(ID_ROLE).toString();
    }
    return ids;
}

void ClippingListPopup::setSearchText(const QString& text)
{
    m_search->setText(text);
}

void ClippingListPopup::finish(ClippingChoice choice)
{
    const QString clippingId = chosenId();
    if (clippingId.isEmpty()) {
        return;
    }
    if (choice == ClippingChoice::Restore) {
        const auto clipping = std::ranges::find(m_clippings, clippingId, &ClippingEntry::id);
        if (clipping == m_clippings.cend() || clipping->fromId.isEmpty() ||
            clipping->originGone) {
            m_search->setToolTip(clipping != m_clippings.cend() ? restoreToolTip(*clipping)
                                                                : QString());
            return;
        }
    }
    close();
    emit chosen(choice, clippingId);
}

void ClippingListPopup::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    m_search->setFocus(Qt::PopupFocusReason);
}

bool ClippingListPopup::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_search && event->type() == QEvent::KeyPress &&
        keyPressed(*static_cast<QKeyEvent*>(event))) {
        return true;
    }
    return QFrame::eventFilter(watched, event);
}

bool ClippingListPopup::keyPressed(const QKeyEvent& key)
{
    if (passedToList(m_list, key)) {
        return true;
    }
    switch (key.key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        finish(ClippingChoice::Insert);
        return true;
    case Qt::Key_Escape:
        close();
        return true;
    default:
        return altKeyPressed(key);
    }
}

bool ClippingListPopup::altKeyPressed(const QKeyEvent& key)
{
    if (!key.modifiers().testFlag(Qt::AltModifier)) {
        return false;
    }
    if (key.key() == Qt::Key_P) {
        finish(ClippingChoice::Restore);
        return true;
    }
    if (key.key() == Qt::Key_O) {
        finish(ClippingChoice::Open);
        return true;
    }
    return false;
}

void ClippingListPopup::filter()
{
    const QString chosen = chosenId();
    const QString query = editor::foldForSearch(m_search->text().simplified());
    m_list->clear();
    for (const ClippingEntry& clipping : std::as_const(m_clippings)) {
        if (!matches(clipping, query)) {
            continue;
        }
        QListWidgetItem* item = addRow(m_list, clipping);
        item->setToolTip(clipping.note);
        if (clipping.inserted) {
            item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
        }
        if (clipping.id == chosen) {
            m_list->setCurrentItem(item);
        }
    }
    chooseFirst(m_list);
}

}  // namespace kalahari::gui::dialogs
