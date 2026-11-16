// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

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
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace book_output {

int widthOfLetters(const QFontMetrics& metrics, int letters) {
    const QString text = QStringLiteral("the quick brown fox jumps over the lazy dog ");
    return qRound(metrics.horizontalAdvance(text) * static_cast<qreal>(letters) /
                  static_cast<qreal>(text.size()));
}

QLabel* newRemark(QWidget* parent, const QString& text) {
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

QWidget* withRemark(QCheckBox* box, QLabel* remark, QWidget* beside) {
    auto* option = new QWidget(box->parentWidget());
    auto* layout = new QVBoxLayout(option);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    box->setParent(option);
    remark->setParent(option);
    const QStyle* style = box->style();
    remark->setContentsMargins(style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, box) +
                                   style->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr,
                                                      box),
                               0, 0, 0);
    if (beside != nullptr) {
        beside->setParent(option);
        auto* line = new QHBoxLayout();
        line->setSpacing(box->fontMetrics().averageCharWidth() * 3);
        line->addWidget(box);
        line->addWidget(beside);
        line->addStretch(1);
        layout->addLayout(line);
    } else {
        layout->addWidget(box);
    }
    layout->addWidget(remark);
    return option;
}

QString literal(QString text) {
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

}  // namespace book_output

namespace {

using book_output::nameOfValue;
using book_output::newRemark;
using book_output::valueOfName;
using book_output::withRemark;

constexpr int ID_ROLE = Qt::UserRole;          ///< The element of a row of the list
constexpr int STATUS_ROLE = Qt::UserRole + 1;  ///< The status of a text of the list
/// Rows the list of the texts shows at most (the others scroll in it): the window with the
/// result of the export fits 1366x768 at 150%
constexpr int LIST_ROWS = 5;
/// The width of the list of the fonts, in letters of their names
constexpr int FONT_NAME_LETTERS = 22;
/// The width of the window in the letters of a text: the remarks in fewer lines
constexpr int WIDTH_IN_LETTERS = 140;
/// The widest the window starts, a wide font or not: with its frame it fits a screen of
/// 1366x768 at 150% (911 pixels)
constexpr int MAX_START_WIDTH = 880;

/// Tabs as high as their highest page at the width they have: a page of wrapped sentences
/// asks for the lines its sentences take there, whichever page is in front. A plain
/// QTabWidget asks so only for the page in front, and for the others takes their own hints,
/// which wrap the sentences at a narrower width into more lines.
class FittingTabs : public QTabWidget {
public:
    using QTabWidget::QTabWidget;

    [[nodiscard]] bool hasHeightForWidth() const override { return true; }

    [[nodiscard]] int heightForWidth(int width) const override {
        // The frame around the pages, as QTabWidget measures it
        QStyleOption option(0);
        option.initFrom(this);
        option.state = QStyle::State_None;
        const QSize frame =
            style()->sizeFromContents(QStyle::CT_TabWidget, &option, QSize(0, 0), this);
        const int pageWidth = width - frame.width();
        int pages = 0;
        for (int i = 0; i < count(); ++i) {
            if (!isTabVisible(i)) {
                continue;  // the options of a format that is not chosen
            }
            const QWidget* page = widget(i);
            const int height = page->hasHeightForWidth() ? page->heightForWidth(pageWidth) : -1;
            pages = std::max(pages, height >= 0 ? height : page->sizeHint().height());
        }
        return frame.height() + tabBar()->sizeHint().height() + pages;
    }

    [[nodiscard]] QSize sizeHint() const override {
        const QSize plain = QTabWidget::sizeHint();
        return {plain.width(), heightForWidth(plain.width())};
    }
};

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

BookOutputDialog::BookOutputDialog(Book book, QWidget* parent)
    : ArchiveDialog(parent)
    , m_book(std::move(book))
{
    setCompactHeading(true);
    setMinimumWidth(std::max(minimumWidth(),
                             std::min(book_output::widthOfLetters(fontMetrics(), WIDTH_IN_LETTERS),
                                      MAX_START_WIDTH)));
    //: The tooltip of the gear of the windows of the export and of the print. In Polish:
    //: Ustawienia stron i marginesów
    addSettingsButton(QString::fromLatin1(SettingsDialog::PAGES_PAGE),
                      tr("Page and Margin Settings"));
    m_tabs = new FittingTabs(this);

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
    auto* page = new QWidget(m_tabs);
    auto* grid = new QGridLayout(page);
    grid->setColumnStretch(1, 1);
    m_tabs->addTab(page, title);
    return grid;
}

void BookOutputDialog::endTab(QGridLayout* grid)
{
    grid->setRowStretch(grid->rowCount(), 1);
}

void BookOutputDialog::addTextsTab(const QString& title, const QString& note)
{
    QGridLayout* content = newTab(title);
    QWidget* contentBox = content->parentWidget();
    int leafCount = 0;
    for (const Row& row : std::as_const(m_book.rows)) {
        leafCount += row.leaf ? 1 : 0;
    }
    //: %n: how many texts the book has. In Polish: &Cała książka (%n tekst / teksty / tekstów)
    m_bookButton = new QRadioButton(tr("Whole &book (%n texts)", nullptr, leafCount), contentBox);
    //: In Polish: &Wybrane teksty
    m_chosenButton = new QRadioButton(tr("&Chosen texts"), contentBox);
    //: %1: the title of the text of the tab in front. In Polish: Tekst na wie&rzchu: „%1”
    m_frontButton = new QRadioButton(
        tr("&Text in front: \"%1\"").arg(book_output::literal(m_book.frontTitle)), contentBox);
    // The book and the chosen texts side by side; the text in front, whose title may be long,
    // under them
    int row = 0;
    auto* bookOrChosen = new QHBoxLayout();
    bookOrChosen->setSpacing(fontMetrics().averageCharWidth() * 3);
    bookOrChosen->addWidget(m_bookButton);
    bookOrChosen->addWidget(m_chosenButton);
    bookOrChosen->addStretch();
    content->addLayout(bookOrChosen, row++, 0, 1, 2);
    content->addWidget(m_frontButton, row++, 0, 1, 2);

    // The list of the texts, with their sections and groups, and its buttons at its side. It
    // stays in its place and works with Chosen texts.
    m_chosenBox = new QWidget(contentBox);
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
    m_textList->setRows(std::min(static_cast<int>(m_book.rows.size()), LIST_ROWS));
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
    content->addWidget(m_chosenBox, row++, 0, 1, 2);
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
    for (QRadioButton* button : {m_bookButton, m_chosenButton, m_frontButton}) {
        connect(button, &QRadioButton::toggled, this, [this](bool on) {
            if (on) {
                // The list stays in its place, so the tab keeps its look; it works with
                // Chosen texts
                m_chosenBox->setEnabled(m_chosenButton->isChecked());
                dropFailure();
                contentChanged();
            }
        });
    }

    // Without a book only the text in front; without a text in front, only the book
    m_bookButton->setVisible(!m_book.title.isEmpty());
    m_chosenButton->setVisible(!m_book.title.isEmpty());
    m_chosenBox->setVisible(!m_book.title.isEmpty());
    m_frontButton->setVisible(!m_book.frontTitle.isEmpty());
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
                             //: lustrzanych tekst zaczyna się na prawej stronie, chyba że
                             //: ustawiono lewą.
                             tr("With mirror margins a text starts on a right page, unless its "
                                "properties say a left one.")),
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
    m_pageLabel = new QLabel(lookPage);
    m_pageLabel->setTextFormat(Qt::PlainText);
    m_pageLabel->setWordWrap(true);
    //: In Polish: Strona:
    addRow(look, tr("Page:"), m_pageLabel);
    m_pageNote = addNote(look);
    if (narrow) {
        // One line in a narrow tab: the window whole on a small screen
        //: Under the page of the print. In Polish: Rozmiar i marginesy: Ustawienia › Edytor ›
        //: Strony i marginesy.
        showNote(m_pageNote,
                 tr("The size and the margins: Settings › Editor › Pages and Margins."));
    } else {
        //: Under the page of the export. In Polish: Rozmiar strony i marginesy pochodzą z
        //: Ustawień › Edytor › Strony i marginesy (zębatka w nagłówku okna). Rozmiary liter,
        //: odstępy i wcięcia – ze stylów książki.
        showNote(m_pageNote,
                 tr("The size of the page and the margins come from Settings › Editor › Pages "
                    "and Margins (the gear in the heading of the window). The sizes of the "
                    "letters, the spacing and the indents come from the styles of the book."));
    }
    m_pageLabel->setText(pageText());

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
    setRowVisible(m_pageLabel, visible);
    m_pageNote.widget->setVisible(visible);
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
    m_chosenBox->setEnabled(m_chosenButton->isChecked());
}

void BookOutputDialog::rememberLayout(const std::string& contentKey) const
{
    auto& settings = core::SettingsManager::getInstance();
    const Layout now = layoutChoice();
    settings.set<std::string>(contentKey, nameOfValue(contentNames(), now.content));
    settings.set<bool>("export.titles", now.titles);
    settings.set<bool>("export.newPage", now.newPage);
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
    }
    for (const QTreeWidgetItem* item : leaves()) {
        if (item->checkState(0) == Qt::Checked) {
            layout.chosen << item->data(0, ID_ROLE).toString();
        }
    }
    layout.titles = m_titlesBox->isChecked();
    layout.newPage = m_newPageBox->isChecked();
    layout.pageNumbers = m_pageNumbersBox->isChecked();
    layout.fontFamily = m_fontBox->currentData().toString();
    layout.runningHead =
        static_cast<editor::RunningHead>(m_runningHeadBox->currentData().toInt());
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
    }
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
        layoutChanged();
    }
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
    return m_pageLabel->text();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
