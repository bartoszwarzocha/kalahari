// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_output_dialog.cpp
/// @brief The base of the windows that put the texts of the book on pages, the export and the
///        print: the tabs of the texts and of the layout they share

#include "kalahari/gui/dialogs/book_output_dialog.h"
#include "book_output_parts.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

using book_output::nameOfValue;
using book_output::valueOfName;

constexpr int ID_ROLE = Qt::UserRole;          ///< The element of a row of the list
constexpr int STATUS_ROLE = Qt::UserRole + 1;  ///< The status of a text of the list
/// Rows the list of the texts shows at most (the others scroll in it): the window with the
/// result of the export fits 1366x768 at 150%
constexpr int LIST_ROWS = 5;
/// Rows the list of the texts shows in the small window of Choose Texts...
constexpr int WINDOW_LIST_ROWS = 12;
/// The width of the list of the fonts, in letters of their names
constexpr int FONT_NAME_LETTERS = 22;
/// The width of the list Volume, in letters of the names of the volumes (longer ones are cut
/// in it, whole in its list)
constexpr int VOLUME_NAME_LETTERS = 32;
/// The width of the window in the letters of a text: the remarks in fewer lines
constexpr int WIDTH_IN_LETTERS = 140;
/// The widest the window starts, a wide font or not: with its frame it fits a screen of
/// 1366x768 at 150% (911 pixels)
constexpr int MAX_START_WIDTH = 880;

/// The fonts a Word document is often written in, offered first after the editor's
const QStringList& commonFonts() {
    static const QStringList fonts = {QStringLiteral("Times New Roman"), QStringLiteral("Georgia"),
                                      QStringLiteral("Garamond"), QStringLiteral("Calibri")};
    return fonts;
}

/// The running heads as the settings keep them
const std::array<std::pair<editor::RunningHead, const char*>, 4>& runningHeadNames() {
    static const std::array<std::pair<editor::RunningHead, const char*>, 4> names = {{
        {editor::RunningHead::None, "none"},
        {editor::RunningHead::AuthorAndTitle, "authorAndTitle"},
        {editor::RunningHead::TitleAndChapter, "titleAndChapter"},
        {editor::RunningHead::Chapter, "chapter"},
    }};
    return names;
}

/// The contents as the settings keep them
const std::array<std::pair<BookOutputDialog::Content, const char*>, 3>& contentNames() {
    static const std::array<std::pair<BookOutputDialog::Content, const char*>, 3> names = {{
        {BookOutputDialog::Content::Book, "book"},
        {BookOutputDialog::Content::Chosen, "chosen"},
        {BookOutputDialog::Content::Front, "front"},
    }};
    return names;
}

} // anonymous namespace

BookOutputDialog::BookOutputDialog(Book book, QWidget* parent, bool withTabs)
    : ArchiveDialog(parent)
    , m_book(std::move(book))
{
    setCompactHeading(true);
    setMinimumWidth(std::max(minimumWidth(),
                             std::min(widthOfLetters(fontMetrics(), WIDTH_IN_LETTERS),
                                      MAX_START_WIDTH)));
    //: The tooltip of the gear of the windows of the export and of the print. In Polish:
    //: Ustawienia stron i marginesów
    addSettingsButton(QString::fromLatin1(SettingsDialog::PAGES_PAGE),
                      tr("Page and Margin Settings"));
    if (withTabs) {
        m_tabs = new FittingTabs(this);
    }

    // The page follows the settings the gear opens
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key.starts_with("editor.page") || key.starts_with("editor.margins") ||
                key == "ui.lengthUnit") {
                QMetaObject::invokeMethod(this, "showPage", Qt::QueuedConnection);
            }
        });
}

BookOutputDialog::~BookOutputDialog()
{
    if (m_settingsListener >= 0) {
        core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    }
}

QGridLayout* BookOutputDialog::newTab(const QString& title)
{
    if (m_tabs == nullptr && m_partMaker) {
        return m_partMaker(title);
    }
    auto* page = new QWidget(m_tabs != nullptr ? static_cast<QWidget*>(m_tabs) : this);
    auto* grid = new QGridLayout(page);
    grid->setColumnStretch(1, 1);
    if (m_tabs != nullptr) {
        m_tabs->addTab(page, title);
    }
    return grid;
}

void BookOutputDialog::endTab(QGridLayout* grid)
{
    grid->setRowStretch(grid->rowCount(), 1);
}

void BookOutputDialog::addTextsTab(const QString& title, const QString& note, bool compact)
{
    QGridLayout* content = newTab(title);
    QWidget* contentBox = content->parentWidget();
    m_bookButton = new QRadioButton(contentBox);
    //: In Polish: &Wybrane teksty
    m_chosenButton = new QRadioButton(tr("&Chosen texts"), contentBox);
    //: %1: the title of the text of the tab in front. In Polish: Tekst na wie&rzchu: „%1”
    m_frontButton = new QRadioButton(
        tr("&Text in front: \"%1\"").arg(literal(m_book.frontTitle)), contentBox);
    m_setButton = new QRadioButton(contentBox);
    showBookCounts();
    int row = 0;
    const bool series = m_book.volumes.size() > 1;
    if (compact) {
        // One under another, the text in front first; the chosen texts with the button of
        // their list and how many are chosen
        content->addWidget(m_frontButton, row++, 0, 1, 2);
        content->addWidget(m_bookButton, row++, 0, 1, 2);
        auto* chosenLine = new QHBoxLayout();
        chosenLine->setSpacing(fontMetrics().averageCharWidth() * 2);
        chosenLine->addWidget(m_chosenButton);
        //: Beside the chosen texts in the window of the print: opens their list. In Polish:
        //: W&ybierz teksty...
        m_chooseTextsButton = new QPushButton(tr("Choose Te&xts..."), contentBox);
        m_chooseTextsButton->setAutoDefault(false);
        chosenLine->addWidget(m_chooseTextsButton);
        m_chosenCount = newRemark(contentBox, QString());
        chosenLine->addWidget(m_chosenCount);
        chosenLine->addStretch();
        content->addLayout(chosenLine, row++, 0, 1, 2);
        content->addWidget(m_setButton, row++, 0, 1, 2);
        if (series) {
            content->addWidget(newVolumeRow(contentBox), row++, 0, 1, 2);
        }
    } else {
        // A series: the volume whose texts go, over them
        if (series) {
            content->addWidget(newVolumeRow(contentBox), row++, 0, 1, 2);
        }
        // The book and the chosen texts side by side; the text in front, whose title may be
        // long, under them
        auto* bookOrChosen = new QHBoxLayout();
        bookOrChosen->setSpacing(fontMetrics().averageCharWidth() * 3);
        bookOrChosen->addWidget(m_bookButton);
        bookOrChosen->addWidget(m_chosenButton);
        bookOrChosen->addWidget(m_setButton);
        bookOrChosen->addStretch();
        content->addLayout(bookOrChosen, row++, 0, 1, 2);
        content->addWidget(m_frontButton, row++, 0, 1, 2);
    }

    // The list of the texts, with their sections and groups, and its buttons at its side. On
    // a tab it stays in its place and works with Chosen texts; in a section it is in the small
    // window of Choose Texts...
    if (compact) {
        m_chosenWindow = new KalahariDialog(this);
        m_chosenWindow->setCompactHeading(true);
        //: The button of the small window of the texts to print. In Polish: Wybierz
        m_chosenWindow->setAcceptText(tr("Choose"));
    }
    m_chosenBox = new QWidget(compact ? static_cast<QWidget*>(m_chosenWindow) : contentBox);
    auto* chosenLayout = new QGridLayout(m_chosenBox);
    chosenLayout->setContentsMargins(0, 0, 0, 0);
    chosenLayout->setColumnStretch(0, 1);
    //: The name of the list of the texts for screen readers. In Polish: Teksty książki
    m_textList = newRowList(m_chosenBox, tr("Texts of the book"));
    // A tree as the Navigator: the sections and the parts open and close, all open at first
    m_textList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_textList->setRootIsDecorated(true);
    m_textList->setItemsExpandable(true);
    fillTextList();
    m_textList->expandAll();
    // As high for each volume of a series
    const int rows = compact ? WINDOW_LIST_ROWS : LIST_ROWS;
    m_textList->setRows(series ? rows : std::min(static_cast<int>(m_book.rows.size()), rows));
    chosenLayout->addWidget(m_textList, 0, 0);
    auto* buttons = new QVBoxLayout();
    buttons->setContentsMargins(0, 0, 0, 0);
    //: Checks every text of the list. In Polish: &Zaznacz wszystkie
    auto* allButton = new QPushButton(tr("Check &All"), m_chosenBox);
    //: Unchecks every text of the list. In Polish: &Odznacz wszystkie
    auto* noneButton = new QPushButton(tr("&Uncheck All"), m_chosenBox);
    //: Checks only the final texts of the list. In Polish: Tylko &gotowe
    auto* finalButton = new QPushButton(tr("Only &Final"), m_chosenBox);
    for (QPushButton* button : {allButton, noneButton, finalButton}) {
        button->setAutoDefault(false);
        buttons->addWidget(button);
    }
    // How many texts are chosen, under the buttons
    m_chosenNote = newRemark(m_chosenBox, QString());
    buttons->addWidget(m_chosenNote);
    buttons->addStretch();
    chosenLayout->addLayout(buttons, 0, 1);
    if (compact) {
        m_chosenWindow->contentLayout()->addWidget(m_chosenBox, 1);
        connect(m_chooseTextsButton, &QPushButton::clicked, this, &BookOutputDialog::chooseTexts);
    } else {
        content->addWidget(m_chosenBox, row++, 0, 1, 2);
    }
    content->addWidget(newRemark(contentBox, note), row++, 0, 1, 2);
    endTab(content);
    connect(allButton, &QPushButton::clicked, this, &BookOutputDialog::checkAll);
    connect(noneButton, &QPushButton::clicked, this, &BookOutputDialog::checkNone);
    connect(finalButton, &QPushButton::clicked, this, &BookOutputDialog::checkFinal);
    connect(m_textList, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem*, int column) {
        if (column == 0) {
            dropFailure();
            updateFields();
        }
    });
    for (QRadioButton* button : {m_bookButton, m_chosenButton, m_frontButton, m_setButton}) {
        connect(button, &QRadioButton::toggled, this, [this](bool on) {
            if (on) {
                followContentChoice();
            }
        });
    }

    // Without a book only the text in front; without a text in front, only the book
    m_bookButton->setVisible(!m_book.title.isEmpty());
    m_chosenButton->setVisible(!m_book.title.isEmpty());
    m_chosenBox->setVisible(!m_book.title.isEmpty());
    if (compact) {
        m_chooseTextsButton->setVisible(!m_book.title.isEmpty());
    }
    m_frontButton->setVisible(!m_book.frontTitle.isEmpty());
}

void BookOutputDialog::chooseTexts()
{
    if (m_chosenWindow == nullptr) {
        return;
    }
    // The checks as they are, for Cancel
    const QList<QTreeWidgetItem*> texts = leaves();
    QList<Qt::CheckState> before;
    before.reserve(texts.size());
    for (const QTreeWidgetItem* item : texts) {
        before << item->checkState(0);
    }
    //: The title of the small window of the texts to print. In Polish: Wybierz teksty
    m_chosenWindow->setHeading(tr("Choose Texts"),
                               //: Under the title of the small window of the texts to print;
                               //: %1: the title of the book or of the volume. In Polish:
                               //: Zaznacz teksty książki „%1” do wydruku.
                               tr("Check the texts of the book \"%1\" to print.")
                                   .arg(m_book.title));
    if (m_chosenWindow->exec() == QDialog::Accepted) {
        m_chosenButton->setChecked(true);
        return;
    }
    {
        const QSignalBlocker blocker(m_textList);
        for (qsizetype i = 0; i < texts.size() && i < before.size(); ++i) {
            texts.at(i)->setCheckState(0, before.at(i));
        }
    }
    m_textList->viewport()->update();
    updateFields();
}

void BookOutputDialog::followContentChoice()
{
    // The list stays in its place, so the tab keeps its look; it works with Chosen texts. In
    // its small window it is for choosing. The text in front is of no volume.
    m_chosenBox->setEnabled(m_chosenWindow != nullptr || m_chosenButton->isChecked());
    if (m_volumeRow != nullptr) {
        m_volumeRow->setEnabled(!m_frontButton->isChecked());
    }
    // The parts of the set follow one another on the pages
    if (m_newPageBox != nullptr) {
        m_newPageBox->setEnabled(!m_setButton->isChecked());
    }
    dropFailure();
    contentChanged();
}

QWidget* BookOutputDialog::newVolumeRow(QWidget* parent)
{
    m_volumeRow = new QWidget(parent);
    auto* layout = new QHBoxLayout(m_volumeRow);
    layout->setContentsMargins(0, 0, 0, 0);
    //: The list of the volumes of a series in the windows of the export and of the print. In
    //: Polish: To&m:
    auto* label = new QLabel(tr("&Volume:"), m_volumeRow);
    m_volumeBox = new QComboBox(m_volumeRow);
    m_volumeBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_volumeBox->setMinimumContentsLength(VOLUME_NAME_LETTERS);
    for (const Volume& volume : std::as_const(m_book.volumes)) {
        m_volumeBox->addItem(volume.name, volume.id);
        m_volumeBox->setItemData(m_volumeBox->count() - 1, volume.name, Qt::ToolTipRole);
    }
    m_volumeBox->setCurrentIndex(std::max(0, m_volumeBox->findData(m_book.volume)));
    label->setBuddy(m_volumeBox);
    layout->addWidget(label);
    layout->addWidget(m_volumeBox);
    layout->addStretch();
    connect(m_volumeBox, &QComboBox::currentIndexChanged, this, &BookOutputDialog::showVolume);
    return m_volumeRow;
}

void BookOutputDialog::showBookCounts()
{
    int leafCount = 0;
    for (const Row& row : std::as_const(m_book.rows)) {
        leafCount += row.leaf ? 1 : 0;
    }
    //: %n: how many texts the book has. In Polish: &Cała książka (%n tekst / teksty / tekstów)
    m_bookButton->setText(tr("Whole &book (%n texts)", nullptr, leafCount));
    //: %n: the parts of the set for a publisher that have a text. In Polish: Zes&taw dla
    //: wydawnictwa (%n część / części / części)
    m_setButton->setText(tr("Set for the &publisher (%n parts)", nullptr, m_book.setParts));
    m_setButton->setVisible(!m_book.title.isEmpty() && !m_book.setTitle.isEmpty());
}

void BookOutputDialog::showVolume(int index)
{
    const QString id = m_volumeBox->itemData(index).toString();
    if (!m_volumeBook || id.isEmpty() || id == m_book.volume) {
        return;
    }
    Book shown = m_volumeBook(id);
    // The text in front stays, whichever volume is chosen
    shown.frontTitle = m_book.frontTitle;
    shown.frontId = m_book.frontId;
    shown.volumes = m_book.volumes;
    shown.volume = id;
    m_book = std::move(shown);
    {
        const QSignalBlocker blocker(m_textList);
        m_textList->clear();
        fillTextList();
        m_textList->expandAll();
    }
    showBookCounts();
    // A volume without a set: its whole book instead
    if (m_setButton->isChecked() && m_setButton->isHidden()) {
        m_bookButton->setChecked(true);
    }
    dropFailure();
    bookChanged();
    contentChanged();
}

void BookOutputDialog::setVolumeBook(VolumeBook volumeBook)
{
    m_volumeBook = std::move(volumeBook);
}

void BookOutputDialog::bookChanged()
{
    // The base shows nothing of the book but its texts
}

void BookOutputDialog::fillTextList()
{
    auto& artProvider = core::ArtProvider::getInstance();
    QList<QTreeWidgetItem*> parents;
    for (const Row& each : std::as_const(m_book.rows)) {
        while (!parents.isEmpty() && parents.size() > each.level) {
            parents.removeLast();
        }
        auto* item = parents.isEmpty() ? new QTreeWidgetItem(m_textList)
                                       : new QTreeWidgetItem(parents.last());
        item->setText(0, each.title);
        if (!each.iconId.isEmpty()) {
            item->setIcon(0, artProvider.getIcon(each.iconId, core::IconContext::TreeView));
        }
        item->setData(0, ID_ROLE, each.id);
        item->setData(0, STATUS_ROLE, each.status);
        if (!each.status.isEmpty()) {
            item->setText(1, AddToBookDialog::statusName(each.status));
            item->setForeground(1, m_textList->palette().brush(QPalette::PlaceholderText));
        }
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
        if (!each.leaf) {
            flags |= Qt::ItemIsAutoTristate;
        }
        item->setFlags(flags);
        item->setCheckState(0, Qt::Checked);
        if (!each.leaf) {
            while (parents.size() < each.level + 1) {
                parents.append(item);
            }
        }
    }
}

QGridLayout* BookOutputDialog::addLayoutTab(const QString& title, bool narrow)
{
    const auto& settings = core::SettingsManager::getInstance();
    QGridLayout* look = newTab(title);
    QWidget* lookPage = look->parentWidget();
    int row = 0;
    //: In Polish: &Tytuły części i rozdziałów z Nawigatora jako nagłówki
    m_titlesBox =
        new QCheckBox(tr("Navigator titles of parts and chapters as &headings"), lookPage);
    look->addWidget(
        withRemark(m_titlesBox,
                   newRemark(lookPage,
                             //: Under the titles as headings. In Polish: Tekst, który zaczyna się
                             //: od nagłówka, zostaje z własnym. Strona tytułowa, strona
                             //: redakcyjna, dedykacja i motto nie dostają tytułu.
                             tr("A text that starts with a heading keeps its own. The title page, "
                                "the copyright page, the dedication and the motto get no title."))),
        row++, 0, 1, 2);
    m_pagesLayout = new QWidget(lookPage);
    auto* pagesLayout = new QGridLayout(m_pagesLayout);
    pagesLayout->setContentsMargins(0, 0, 0, 0);
    pagesLayout->setColumnStretch(1, 1);
    //: In Polish: Każd&y tekst od nowej strony
    m_newPageBox = new QCheckBox(tr("Each text from a new &page"), m_pagesLayout);
    //: In Polish: Numery &stron na dole strony
    m_pageNumbersBox = new QCheckBox(tr("Page numbers at the b&ottom of the page"), m_pagesLayout);
    // The page numbers beside a new page: the tab as high as the Texts tab
    pagesLayout->addWidget(
        withRemark(m_newPageBox,
                   newRemark(m_pagesLayout,
                             //: Under each text from a new page. In Polish: Przy marginesach
                             //: lustrzanych tekst zaczyna się na stronie, którą podają typ
                             //: książki, Ustawienia i jego Właściwości: prawej, lewej lub
                             //: następnej.
                             tr("With mirror margins a text starts on the page the book type, "
                                "the Settings and its properties say: a right one, a left one "
                                "or the next one.")),
                   m_pageNumbersBox),
        0, 0, 1, 2);
    look->addWidget(m_pagesLayout, row++, 0, 1, 2);

    m_fontBox = new QComboBox(lookPage);
    const QString editorFont =
        QString::fromStdString(settings.get<std::string>("editor.fontFamily"));
    //: The first font of the export: the font of the editor; %1: its name. In Polish: %1
    //: (czcionka edytora)
    m_fontBox->addItem(tr("%1 (editor font)").arg(editorFont), QString());
    for (const QString& font : commonFonts()) {
        m_fontBox->addItem(font, font);
    }
    m_fontBox->insertSeparator(m_fontBox->count());
    for (const QString& font : QFontDatabase::families()) {
        if (!commonFonts().contains(font) && !QFontDatabase::isPrivateFamily(font)) {
            m_fontBox->addItem(font, font);
        }
    }
    // The font and the running head in one row, the tab as high as the others; in a narrow
    // tab each in a row of its own
    m_fontRow = new QWidget(lookPage);
    auto* fontRowLayout = new QHBoxLayout(m_fontRow);
    fontRowLayout->setContentsMargins(0, 0, 0, 0);
    fontRowLayout->setSpacing(fontMetrics().averageCharWidth() * 2);
    m_fontBox->setParent(m_fontRow);
    // As wide as a long name of a font, whichever fonts the system has
    m_fontBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_fontBox->setMinimumContentsLength(FONT_NAME_LETTERS);
    fontRowLayout->addWidget(m_fontBox);
    m_runningHeadBox = new QComboBox(m_fontRow);
    //: A running head of the export. In Polish: Bez żywej paginy
    m_runningHeadBox->addItem(tr("No running head"), static_cast<int>(editor::RunningHead::None));
    // The running heads in short words: the list beside the font in a small window
    //: A running head of the export. In Polish: Autor na lewej, tytuł książki na prawej
    m_runningHeadBox->addItem(tr("Author on the left, book title on the right"),
                              static_cast<int>(editor::RunningHead::AuthorAndTitle));
    //: A running head of the export. In Polish: Tytuł książki na lewej, rozdziału na prawej
    m_runningHeadBox->addItem(tr("Book title on the left, chapter on the right"),
                              static_cast<int>(editor::RunningHead::TitleAndChapter));
    //: A running head of the export. In Polish: Tytuł rozdziału na obu stronach
    m_runningHeadBox->addItem(tr("Chapter title on both pages"),
                              static_cast<int>(editor::RunningHead::Chapter));
    m_runningHeadBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: The line at the top of the pages of a book. In Polish: Żywa pa&gina:
    const QString runningHeadText = tr("&Running head:");
    if (narrow) {
        fontRowLayout->addStretch();
        //: In Polish: Czcionk&a tekstu:
        addRow(look, tr("Font of the te&xt:"), m_fontRow)->setBuddy(m_fontBox);
        m_runningHeadRow = new QWidget(lookPage);
        auto* runningHeadRowLayout = new QHBoxLayout(m_runningHeadRow);
        runningHeadRowLayout->setContentsMargins(0, 0, 0, 0);
        m_runningHeadBox->setParent(m_runningHeadRow);
        runningHeadRowLayout->addWidget(m_runningHeadBox);
        runningHeadRowLayout->addStretch();
        addRow(look, runningHeadText, m_runningHeadRow)->setBuddy(m_runningHeadBox);
    } else {
        auto* runningHeadLabel = new QLabel(runningHeadText, m_fontRow);
        runningHeadLabel->setBuddy(m_runningHeadBox);
        auto* runningHeadLayout = new QHBoxLayout();
        runningHeadLayout->setSpacing(
            style()->layoutSpacing(QSizePolicy::Label, QSizePolicy::ComboBox, Qt::Horizontal));
        runningHeadLayout->addWidget(runningHeadLabel);
        runningHeadLayout->addWidget(m_runningHeadBox);
        fontRowLayout->addLayout(runningHeadLayout);
        fontRowLayout->addStretch();
        //: In Polish: Czcionk&a tekstu:
        addRow(look, tr("Font of the te&xt:"), m_fontRow)->setBuddy(m_fontBox);
    }
    // Where the running head goes: in its tip, not in a line of its own, so the window fits a
    // small screen
    //: The tip of the running head. In Polish: U góry stron, bez stron tytułowych i pierwszych
    //: stron rozdziałów.
    const QString runningHeadPlace =
        tr("At the top of the pages, not on title pages or first pages of chapters.");
    m_runningHeadBox->setToolTip(runningHeadPlace);
    m_runningHeadBox->setAccessibleDescription(runningHeadPlace);
    // The page of the settings; the print sets it in a section of its own
    if (!narrow) {
        m_pageLabel = new QLabel(lookPage);
        m_pageLabel->setTextFormat(Qt::PlainText);
        m_pageLabel->setWordWrap(true);
        //: In Polish: Strona:
        addRow(look, tr("Page:"), m_pageLabel);
        m_pageNote = addNote(look);
        //: Under the page of the export. In Polish: Rozmiar strony i marginesy pochodzą z
        //: Ustawień › Edytor › Strony i marginesy (zębatka w nagłówku okna). Rozmiary liter,
        //: odstępy i wcięcia – ze stylów książki.
        showNote(m_pageNote,
                 tr("The size of the page and the margins come from Settings › Editor › Pages "
                    "and Margins (the gear in the heading of the window). The sizes of the "
                    "letters, the spacing and the indents come from the styles of the book."));
        m_pageLabel->setText(pageText());
    }

    for (QCheckBox* box : {m_titlesBox, m_newPageBox, m_pageNumbersBox}) {
        connect(box, &QCheckBox::toggled, this, [this]() {
            dropFailure();
            layoutChanged();
        });
    }
    for (QComboBox* box : {m_fontBox, m_runningHeadBox}) {
        connect(box, &QComboBox::currentIndexChanged, this, [this]() {
            dropFailure();
            layoutChanged();
        });
    }
    return look;
}

void BookOutputDialog::setPagesVisible(bool visible)
{
    m_pagesLayout->setVisible(visible);
    m_fontRow->setVisible(visible);
    setRowVisible(m_fontBox, visible);
    if (m_runningHeadRow != nullptr) {
        setRowVisible(m_runningHeadRow, visible);
    }
    if (m_pageLabel != nullptr) {
        setRowVisible(m_pageLabel, visible);
        m_pageNote.widget->setVisible(visible);
    }
}

void BookOutputDialog::readLayout(const std::string& contentKey)
{
    const auto& settings = core::SettingsManager::getInstance();
    m_titlesBox->setChecked(settings.get<bool>("export.titles"));
    m_newPageBox->setChecked(settings.get<bool>("export.newPage"));
    m_pageNumbersBox->setChecked(settings.get<bool>("export.pageNumbers"));
    const editor::RunningHead runningHead =
        valueOfName(runningHeadNames(), settings.get<std::string>("export.runningHead"),
                    editor::RunningHead::AuthorAndTitle);
    m_runningHeadBox->setCurrentIndex(
        std::max(0, m_runningHeadBox->findData(static_cast<int>(runningHead))));
    const QString font = QString::fromStdString(settings.get<std::string>("export.font"));
    int fontIndex = m_fontBox->findData(font);
    if (fontIndex < 0) {
        m_fontBox->addItem(font, font);  // a font this system does not have
        fontIndex = m_fontBox->count() - 1;
    }
    m_fontBox->setCurrentIndex(fontIndex);
    Content start =
        valueOfName(contentNames(), settings.get<std::string>(contentKey), Content::Book);
    if (m_book.title.isEmpty()) {
        start = Content::Front;
    } else if (start == Content::Front && m_book.frontTitle.isEmpty()) {
        start = Content::Book;
    }
    setContent(start);
    m_chosenBox->setEnabled(m_chosenWindow != nullptr || m_chosenButton->isChecked());
}

void BookOutputDialog::rememberLayout(const std::string& contentKey) const
{
    auto& settings = core::SettingsManager::getInstance();
    const Layout now = layoutChoice();
    // The set for a publisher is chosen each time: the window opens with the book again
    if (now.content != Content::Set) {
        settings.set<std::string>(contentKey, nameOfValue(contentNames(), now.content));
        settings.set<bool>("export.newPage", now.newPage);
    }
    settings.set<bool>("export.titles", now.titles);
    settings.set<bool>("export.pageNumbers", now.pageNumbers);
    settings.set<std::string>("export.font", now.fontFamily.toStdString());
    settings.set<std::string>("export.runningHead",
                              nameOfValue(runningHeadNames(), now.runningHead));
}

BookOutputDialog::Layout BookOutputDialog::layoutChoice() const
{
    Layout layout;
    layout.content = Content::Book;
    if (m_frontButton->isChecked()) {
        layout.content = Content::Front;
    } else if (m_chosenButton->isChecked()) {
        layout.content = Content::Chosen;
    } else if (m_setButton->isChecked()) {
        layout.content = Content::Set;
    }
    for (const QTreeWidgetItem* item : leaves()) {
        if (item->checkState(0) == Qt::Checked) {
            layout.chosen << item->data(0, ID_ROLE).toString();
        }
    }
    layout.titles = m_titlesBox->isChecked();
    layout.newPage = m_newPageBox->isChecked() && layout.content != Content::Set;
    layout.pageNumbers = m_pageNumbersBox->isChecked();
    layout.fontFamily = m_fontBox->currentData().toString();
    layout.runningHead =
        static_cast<editor::RunningHead>(m_runningHeadBox->currentData().toInt());
    layout.volume = m_book.volume;
    return layout;
}

bool BookOutputDialog::updateChosen()
{
    int chosen = 0;
    const QList<QTreeWidgetItem*> texts = leaves();
    for (const QTreeWidgetItem* item : texts) {
        chosen += item->checkState(0) == Qt::Checked ? 1 : 0;
    }
    //: Under the list of the texts; %1: how many are checked, %n: how many there are. In
    //: Polish: Wybrano %1 z %n tekstu / tekstów.
    m_chosenNote->setText(
        tr("%1 of %n texts chosen.", nullptr, static_cast<int>(texts.size())).arg(chosen));
    if (m_chosenCount != nullptr) {
        //: Beside Choose Texts... of the window of the print; %1: how many texts are checked,
        //: %n: how many there are. In Polish: %1 z %n
        m_chosenCount->setText(
            tr("%1 of %n", nullptr, static_cast<int>(texts.size())).arg(chosen));
        m_chosenCount->setVisible(m_chosenButton->isChecked());
    }
    return !m_chosenButton->isChecked() || chosen > 0;
}

void BookOutputDialog::layoutChanged()
{
    // The window has nothing to show of the layout
}

void BookOutputDialog::setContent(Content content)
{
    switch (content) {
        case Content::Book:
            m_bookButton->setChecked(true);
            break;
        case Content::Chosen:
            m_chosenButton->setChecked(true);
            break;
        case Content::Front:
            m_frontButton->setChecked(true);
            break;
        case Content::Set:
            m_setButton->setChecked(true);
            break;
    }
}

QString BookOutputDialog::contentTitle() const
{
    if (m_frontButton->isChecked()) {
        return m_book.frontTitle;
    }
    return m_setButton->isChecked() ? m_book.setTitle : m_book.title;
}

bool BookOutputDialog::oneText() const
{
    return m_frontButton->isChecked() || m_setButton->isChecked();
}

QTreeWidget* BookOutputDialog::textList() const
{
    return m_textList;
}

QString BookOutputDialog::chosenNote() const
{
    return m_chosenButton->isChecked() && !m_chosenBox->isHidden() ? m_chosenNote->text()
                                                                    : QString();
}

void BookOutputDialog::checkAll()
{
    for (QTreeWidgetItem* item : leaves()) {
        item->setCheckState(0, Qt::Checked);
    }
}

void BookOutputDialog::checkNone()
{
    for (QTreeWidgetItem* item : leaves()) {
        item->setCheckState(0, Qt::Unchecked);
    }
}

void BookOutputDialog::checkFinal()
{
    // A text that is not final goes; a table of contents, which has no status, stays
    for (QTreeWidgetItem* item : leaves()) {
        const QString status = item->data(0, STATUS_ROLE).toString();
        item->setCheckState(0, status.isEmpty() || status == QLatin1String("final")
                                   ? Qt::Checked
                                   : Qt::Unchecked);
    }
}

QList<QTreeWidgetItem*> BookOutputDialog::leaves() const
{
    QList<QTreeWidgetItem*> items;
    QList<QTreeWidgetItem*> pending;
    for (int i = m_textList->topLevelItemCount() - 1; i >= 0; --i) {
        pending.append(m_textList->topLevelItem(i));
    }
    // Depth first, in the order of the list
    while (!pending.isEmpty()) {
        QTreeWidgetItem* item = pending.takeLast();
        if (!item->flags().testFlag(Qt::ItemIsAutoTristate)) {
            items.append(item);
        }
        for (int i = item->childCount() - 1; i >= 0; --i) {
            pending.append(item->child(i));
        }
    }
    return items;
}

void BookOutputDialog::showPage()
{
    if (m_pageLabel != nullptr) {
        m_pageLabel->setText(pageText());
    }
    layoutChanged();
}

QString BookOutputDialog::pageSizeName()
{
    const auto& settings = core::SettingsManager::getInstance();
    editor::PageLayout page;
    page.pageSize = editor::PageLayout::pageSizeFromId(
        QString::fromStdString(settings.get<std::string>("editor.page.size")));
    page.customWidth = settings.get<double>("editor.page.customWidth");
    page.customHeight = settings.get<double>("editor.page.customHeight");
    const auto length = [](double millimeters) {
        return lengthText(millimeters, LengthUnit::Millimeters);
    };
    QString size;
    switch (page.pageSize) {
        case editor::PageLayout::PageSize::A4:
            size = QStringLiteral("A4");
            break;
        case editor::PageLayout::PageSize::A5:
            size = QStringLiteral("A5");
            break;
        case editor::PageLayout::PageSize::B5:
            size = QStringLiteral("B5");
            break;
        case editor::PageLayout::PageSize::Trade6x9:
            //: A size of the page. In Polish: 6 x 9 cali
            size = tr("6 x 9 in");
            break;
        case editor::PageLayout::PageSize::Letter:
            //: A size of the page (8.5 x 11 in). In Polish: Letter
            size = tr("Letter");
            break;
        case editor::PageLayout::PageSize::Legal:
            //: A size of the page (8.5 x 14 in). In Polish: Legal
            size = tr("Legal");
            break;
        case editor::PageLayout::PageSize::Custom: {
            const QSizeF millimeters = page.pageSizeMm();
            size = QStringLiteral("%1 x %2").arg(length(millimeters.width()),
                                                 length(millimeters.height()));
            break;
        }
    }
    return size;
}

QString BookOutputDialog::pageText()
{
    const auto& settings = core::SettingsManager::getInstance();
    const auto length = [](double millimeters) {
        return lengthText(millimeters, LengthUnit::Millimeters);
    };
    const QString size = pageSizeName();
    if (settings.get<bool>("editor.margins.mirrorEnabled")) {
        //: The page of the export; %1: its size, %2 and %3: the margins. In Polish: %1,
        //: marginesy lustrzane: wewnętrzny %2, zewnętrzny %3
        return tr("%1, mirror margins: inner %2, outer %3")
            .arg(size, length(settings.get<double>("editor.margins.pageInner")),
                 length(settings.get<double>("editor.margins.pageOuter")));
    }
    //: The page of the export; %1: its size, %2-%5: the margins. In Polish: %1, marginesy: lewy
    //: %2, prawy %3, górny %4, dolny %5
    return tr("%1, margins: left %2, right %3, top %4, bottom %5")
        .arg(size, length(settings.get<double>("editor.margins.pageLeft")),
             length(settings.get<double>("editor.margins.pageRight")),
             length(settings.get<double>("editor.margins.pageTop")),
             length(settings.get<double>("editor.margins.pageBottom")));
}

QString BookOutputDialog::shownPageText() const
{
    return m_pageLabel != nullptr ? m_pageLabel->text() : QString();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
