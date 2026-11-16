// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file export_book_dialog.cpp
/// @brief File > Export > DOCX, PDF, Markdown and EPUB: the file, the texts of the book and how
/// they look in it, and the progress and the result of the export in one window

#include "kalahari/gui/dialogs/export_book_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/extension_points.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/text_images.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/utils/program_folders.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int ID_ROLE = Qt::UserRole;          ///< The element of a row of the list
constexpr int STATUS_ROLE = Qt::UserRole + 1;  ///< The status of a text of the list
/// Rows the list of the texts shows at most (the others scroll in it): the window with the
/// result of the export fits 1366x768 at 150%
constexpr int LIST_ROWS = 5;
/// The width of the window in the letters of a text: the remarks in fewer lines
constexpr int WIDTH_IN_LETTERS = 140;
/// The widest the window starts, a wide font or not: with its frame it fits a screen of
/// 1366x768 at 150% (911 pixels)
constexpr int MAX_START_WIDTH = 880;
/// The height of the picture of the cover of an EPUB, in lines of the window's text; its width
/// is two thirds of it, as a page of a book
constexpr int COVER_LINES = 3;
/// The width of the description of the book on the tab of an EPUB, in letters: one line
constexpr int DESCRIPTION_LETTERS = 70;
/// The width of the list of the fonts, in letters of their names
constexpr int FONT_NAME_LETTERS = 24;
/// The width of the field of the ISBN, in letters: the 17 characters of an ISBN with its
/// hyphens (digits are wider than letters) and the frame of the field
constexpr int ISBN_LETTERS = 24;

/// The width of @p letters letters of a text in the font of @p metrics. Not
/// averageCharWidth(): fonts give it differently (DejaVu Sans 6 pixels, Inter 8, for text
/// that takes 5.8 and 5.5), and the window would wrap its sentences into more lines in one
/// font than in another
int widthOfLetters(const QFontMetrics& metrics, int letters) {
    const QString text = QStringLiteral("the quick brown fox jumps over the lazy dog ");
    return qRound(metrics.horizontalAdvance(text) * static_cast<qreal>(letters) /
                  static_cast<qreal>(text.size()));
}

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

/// Characters that file systems do not allow in a name
const QString& forbiddenCharacters() {
    static const QString characters = QStringLiteral("\\/:*?\"<>|");
    return characters;
}

/// @p title as a name of a file: the characters file systems do not allow become "_"
QString safeName(const QString& title) {
    static const QRegularExpression invalid(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]"));
    QString name = title.simplified();
    name.replace(invalid, QStringLiteral("_"));
    return name;
}

/// @p text in the label of a button: an '&' shows as itself, not as an access key
QString literal(QString text) {
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

/// A grey sentence under an option or a list
QLabel* newRemark(QWidget* parent, const QString& text) {
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

/// Check box @p box with @p remark right under it, in line with its text; @p beside, when
/// given, on the line of the box after it (an option of its own, which saves a line)
QWidget* withRemark(QCheckBox* box, QLabel* remark, QWidget* beside = nullptr) {
    auto* option = new QWidget(box->parentWidget());
    auto* layout = new QVBoxLayout(option);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    box->setParent(option);
    remark->setParent(option);
    const QStyle* style = box->style();
    remark->setContentsMargins(style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, box) +
                                   style->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, box),
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

/// The purposes of a PDF as the settings keep them
const std::array<std::pair<editor::PdfPurpose, const char*>, 3>& purposeNames() {
    static const std::array<std::pair<editor::PdfPurpose, const char*>, 3> names = {{
        {editor::PdfPurpose::Reading, "reading"},
        {editor::PdfPurpose::PrintShop, "printShop"},
        {editor::PdfPurpose::Archive, "archive"},
    }};
    return names;
}

/// The value of @p name in @p names; @p fallback for a name the list does not have
template <typename Value, std::size_t Size>
Value valueOfName(const std::array<std::pair<Value, const char*>, Size>& names,
                  const std::string& name, Value fallback) {
    for (const auto& [value, each] : names) {
        if (name == each) {
            return value;
        }
    }
    return fallback;
}

/// The name of @p value in @p names
template <typename Value, std::size_t Size>
std::string nameOfValue(const std::array<std::pair<Value, const char*>, Size>& names,
                        Value value) {
    for (const auto& [each, name] : names) {
        if (each == value) {
            return name;
        }
    }
    return names.front().second;
}

/// The content as the settings keep it
QString contentName(ExportBookDialog::Content content) {
    switch (content) {
        case ExportBookDialog::Content::Book:
            return QStringLiteral("book");
        case ExportBookDialog::Content::Chosen:
            return QStringLiteral("chosen");
        case ExportBookDialog::Content::Front:
            return QStringLiteral("front");
    }
    return QStringLiteral("book");
}

} // anonymous namespace

ExportBookDialog::ExportBookDialog(const Book& book, editor::ExportFormat format,
                                   const QString& folder, Exporter exporter, QWidget* parent)
    : ArchiveDialog(parent)
    , m_book(book)
    , m_exporter(std::move(exporter))
{
    const auto& settings = core::SettingsManager::getInstance();
    //: The title of the window of File > Export > DOCX and Markdown. In Polish: Eksportuj
    //: książkę
    setHeading(tr("Export Book"));
    setCompactHeading(true);
    setMinimumWidth(std::max(
        minimumWidth(), std::min(widthOfLetters(fontMetrics(), WIDTH_IN_LETTERS), MAX_START_WIDTH)));
    //: The tooltip of the gear of the window of the export. In Polish: Ustawienia stron
    //: i marginesów
    addSettingsButton(QString::fromLatin1(SettingsDialog::PAGES_PAGE),
                      tr("Page and Margin Settings"));

    // The tabs: the file, the texts, the layout, and the annotations and the images. The
    // tabs are as high as the highest, so the window keeps its size and nothing scrolls; the
    // progress and the result show under them, whichever tab is in front.
    m_tabs = new FittingTabs(this);
    fieldsLayout()->addWidget(m_tabs);
    const auto newTab = [this](const QString& title) {
        auto* page = new QWidget(m_tabs);
        auto* grid = new QGridLayout(page);
        grid->setColumnStretch(1, 1);
        m_tabs->addTab(page, title);
        return grid;
    };
    // The rows of a tab start at its top
    const auto endTab = [](QGridLayout* grid) { grid->setRowStretch(grid->rowCount(), 1); };

    // The file: its format, its folder and its name
    //: A tab of the window of the export: the format, the folder and the name of the file. In
    //: Polish: &Plik
    QGridLayout* file = newTab(tr("Fi&le"));
    // The list of the formats: the program's own and those of the plugins, as many as they
    // come
    m_formats = formats();
    m_formatBox = new QComboBox(file->parentWidget());
    for (const Format& each : std::as_const(m_formats)) {
        m_formatBox->addItem(each.name, each.id);
    }
    m_formatBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    auto* formatRow = new QWidget(file->parentWidget());
    auto* formatLayout = new QHBoxLayout(formatRow);
    formatLayout->setContentsMargins(0, 0, 0, 0);
    formatLayout->addWidget(m_formatBox);
    formatLayout->addStretch();
    //: In Polish: &Format:
    addRow(file, tr("&Format:"), formatRow)->setBuddy(m_formatBox);
    QPushButton* browseButton = nullptr;
    //: In Polish: Folder:
    addRow(file, tr("Folder:"), newPathField(&m_folderField, &browseButton));
    browseButton->setToolTip(tr("Choose the folder in the system window"));
    m_folderNote = addNote(file);
    m_nameField = new QLineEdit(file->parentWidget());
    //: In Polish: &Nazwa:
    addRow(file, tr("&Name:"), m_nameField);
    m_nameNote = addNote(file);
    endTab(file);

    // The texts: the whole book, the texts chosen in the list, or the text in front
    //: A tab of the window of the export: what goes to the file. In Polish: T&eksty
    QGridLayout* content = newTab(tr("Text&s"));
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
    m_frontButton =
        new QRadioButton(tr("&Text in front: \"%1\"").arg(literal(m_book.frontTitle)), contentBox);
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
    QLabel* orderNote = newRemark(
        contentBox,
        m_book.title.isEmpty()
            //: Under the content of the export without a book. In Polish: Tekst trafi do
            //: pliku także z niezapisanymi zmianami.
            ? tr("The text goes to the file also with its unsaved changes.")
            //: Under the content of the export. In Polish: Teksty trafią do pliku w kolejności
            //: Nawigatora, także z niezapisanymi zmianami.
            : tr("The texts go to the file in the order of the Navigator, also with their "
                 "unsaved changes."));
    content->addWidget(orderNote, row++, 0, 1, 2);
    endTab(content);
    connect(allButton, &QPushButton::clicked, this, &ExportBookDialog::checkAll);
    connect(noneButton, &QPushButton::clicked, this, &ExportBookDialog::checkNone);
    connect(finalButton, &QPushButton::clicked, this, &ExportBookDialog::checkFinal);
    connect(m_textList, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item, int column) {
                if (column == 0) {
                    textChanged(item);
                }
            });

    // Without a book only the text in front; without a text in front, only the book
    m_bookButton->setVisible(!m_book.title.isEmpty());
    m_chosenButton->setVisible(!m_book.title.isEmpty());
    m_chosenBox->setVisible(!m_book.title.isEmpty());
    m_frontButton->setVisible(!m_book.frontTitle.isEmpty());

    // The layout: the titles as headings, and the pages of a Word document
    //: A tab of the window of the export: how the texts look in the file. In Polish: U&kład
    QGridLayout* look = newTab(tr("La&yout"));
    QWidget* lookPage = look->parentWidget();
    row = 0;
    //: In Polish: &Tytuły części i rozdziałów z Nawigatora jako nagłówki
    m_titlesBox = new QCheckBox(tr("Navigator titles of parts and chapters as &headings"), lookPage);
    look->addWidget(
        withRemark(m_titlesBox,
                   newRemark(lookPage,
                             //: Under the titles as headings. In Polish: Tekst, który zaczyna się
                             //: od nagłówka, zostaje z własnym. Strona tytułowa, strona
                             //: redakcyjna, dedykacja i motto nie dostają tytułu.
                             tr("A text that starts with a heading keeps its own. The title page, "
                                "the copyright page, the dedication and the motto get no title."))),
        row++, 0, 1, 2);
    m_wordLayout = new QWidget(lookPage);
    auto* wordLayout = new QGridLayout(m_wordLayout);
    wordLayout->setContentsMargins(0, 0, 0, 0);
    wordLayout->setColumnStretch(1, 1);
    int wordRow = 0;
    //: In Polish: Każd&y tekst od nowej strony
    m_newPageBox = new QCheckBox(tr("Each text from a new &page"), m_wordLayout);
    //: In Polish: Numery &stron na dole strony
    m_pageNumbersBox = new QCheckBox(tr("Page numbers at the b&ottom of the page"), m_wordLayout);
    // The page numbers beside a new page: the tab as high as the Texts tab
    wordLayout->addWidget(
        withRemark(m_newPageBox,
                   newRemark(m_wordLayout,
                             //: Under each text from a new page. In Polish: Przy marginesach
                             //: lustrzanych tekst zaczyna się na prawej stronie, chyba że w jego
                             //: właściwościach ustawiono lewą.
                             tr("With mirror margins a text starts on a right page, unless its "
                                "properties say a left one.")),
                   m_pageNumbersBox),
        wordRow++, 0, 1, 2);
    look->addWidget(m_wordLayout, row++, 0, 1, 2);

    m_fontBox = new QComboBox(lookPage);
    const QString editorFont = QString::fromStdString(settings.get<std::string>("editor.fontFamily"));
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
    // The font and the running head of a Word document and of a PDF, in one row: the tab as
    // high as the others
    m_fontRow = new QWidget(lookPage);
    auto* fontRowLayout = new QHBoxLayout(m_fontRow);
    fontRowLayout->setContentsMargins(0, 0, 0, 0);
    fontRowLayout->setSpacing(fontMetrics().averageCharWidth() * 3);
    m_fontBox->setParent(m_fontRow);
    // As wide as a long name of a font, whichever fonts the system has
    m_fontBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_fontBox->setMinimumContentsLength(FONT_NAME_LETTERS);
    fontRowLayout->addWidget(m_fontBox);
    m_runningHeadBox = new QComboBox(m_fontRow);
    //: A running head of the export. In Polish: Bez żywej paginy
    m_runningHeadBox->addItem(tr("No running head"), static_cast<int>(editor::RunningHead::None));
    //: A running head of the export. In Polish: Autor na lewej stronie, tytuł książki na prawej
    m_runningHeadBox->addItem(tr("Author on the left page, book title on the right one"),
                              static_cast<int>(editor::RunningHead::AuthorAndTitle));
    //: A running head of the export. In Polish: Tytuł książki na lewej, tytuł rozdziału na
    //: prawej
    m_runningHeadBox->addItem(tr("Book title on the left, chapter title on the right"),
                              static_cast<int>(editor::RunningHead::TitleAndChapter));
    //: A running head of the export. In Polish: Tytuł rozdziału na obu stronach
    m_runningHeadBox->addItem(tr("Chapter title on both pages"),
                              static_cast<int>(editor::RunningHead::Chapter));
    m_runningHeadBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: The line at the top of the pages of a book. In Polish: Żywa pa&gina:
    auto* runningHeadLabel = new QLabel(tr("&Running head:"), m_fontRow);
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
    m_runningHeadNote = addNote(look);
    //: Under the running head. In Polish: U góry stron tekstu, bez stron tytułowych i bez
    //: pierwszych stron rozdziałów.
    showNote(m_runningHeadNote, tr("At the top of the pages of the text, without the title pages "
                                   "and without the first pages of the chapters."));
    m_pageLabel = new QLabel(lookPage);
    m_pageLabel->setTextFormat(Qt::PlainText);
    m_pageLabel->setWordWrap(true);
    //: In Polish: Strona:
    addRow(look, tr("Page:"), m_pageLabel);
    m_pageNote = addNote(look);
    //: Under the page of the export. In Polish: Rozmiar strony i marginesy pochodzą z Ustawień ›
    //: Edytor › Strony i marginesy (zębatka w nagłówku okna). Rozmiary liter, odstępy i wcięcia
    //: – ze stylów książki.
    showNote(m_pageNote, tr("The size of the page and the margins come from Settings › Editor › "
                          "Pages and Margins (the gear in the heading of the window). The sizes of "
                          "the letters, the spacing and the indents come from the styles of the "
                          "book."));

    // The look of an EPUB in the reader, and its font
    m_epubLayout = new QWidget(lookPage);
    auto* epubLayout = new QVBoxLayout(m_epubLayout);
    epubLayout->setContentsMargins(0, 0, 0, 0);
    //: A group of the layout of an EPUB: how the reader shows it. In Polish: Wygląd w czytniku
    auto* readerLook = new QGroupBox(tr("Look in the Reader"), m_epubLayout);
    auto* readerLookLayout = new QVBoxLayout(readerLook);
    //: In Polish: Ze &stylów książki: czcionka, rozmiary liter, wcięcia i odstępy
    m_bookStylesButton = new QRadioButton(
        tr("From the &book's styles: the font, the sizes of the letters, the indents and the "
           "spacing"),
        readerLook);
    //: In Polish: Prost&y: czcionkę i rozmiar liter dobiera czytnik, zostają wcięcia i odstępy
    m_simpleLookButton = new QRadioButton(
        tr("Si&mple: the reader chooses the font and the size of the letters, the indents and "
           "the spacing stay"),
        readerLook);
    readerLookLayout->addWidget(m_bookStylesButton);
    readerLookLayout->addWidget(m_simpleLookButton);
    epubLayout->addWidget(readerLook);
    //: In Polish: Osadź &czcionkę tekstu w pliku
    m_embedFontBox = new QCheckBox(tr("&Embed the font of the text in the file"), m_epubLayout);
    epubLayout->addWidget(withRemark(
        m_embedFontBox,
        newRemark(m_epubLayout,
                  //: Under the embedding of the font. In Polish: Tylko czcionkę, której licencja
                  //: na to pozwala. Czytelnik i tak może ją zmienić w czytniku.
                  tr("Only a font whose licence allows it. The reader can still change it in the "
                     "e-book reader."))));
    epubLayout->addWidget(newRemark(
        m_epubLayout,
        //: Under the layout of an EPUB. In Polish: Każdy tekst zaczyna się na nowej stronie
        //: czytnika. Strony dzieli czytnik, więc nie ma tu rozmiaru strony ani numerów stron.
        tr("Each text starts on a new page of the e-book reader. The reader divides the pages, "
           "so there is no size of the page and there are no page numbers here.")));
    look->addWidget(m_epubLayout, look->rowCount(), 0, 1, 2);
    endTab(look);

    // The annotations and the images: each group says what its format does
    //: A tab of the window of the export: the annotations and the images. In Polish: Uwagi
    //: i o&brazy
    QGridLayout* extras = newTab(tr("Annotations and Ima&ges"));
    QWidget* extrasPage = extras->parentWidget();
    m_extrasTab = extrasPage;
    //: A group of the window of the export: the annotations of the text. In Polish: Uwagi
    m_notesGroup = new QGroupBox(tr("Annotations"), extrasPage);
    auto* notes = new QGridLayout(m_notesGroup);
    //: In Polish: &Uwagi jako komentarze Worda
    m_commentsBox = new QCheckBox(tr("Annotations as Word comm&ents"), m_notesGroup);
    m_commentsOption = withRemark(
        m_commentsBox,
        newRemark(m_notesGroup,
                  //: Under the annotations as comments. In Polish: Komentarze, zadania
                  //: i notatki z tekstu, z autorem i datą. Bez tej opcji plik nie zawiera uwag.
                  tr("The comments, to-dos and notes of the text, with their authors and dates. "
                     "Without this option the file has no annotations.")));
    notes->addWidget(m_commentsOption, 0, 0);
    //: In Polish: &Uwagi jako komentarze PDF
    m_pdfCommentsBox = new QCheckBox(tr("Annotations as PDF comm&ents"), m_notesGroup);
    m_pdfCommentsRemark = newRemark(m_notesGroup, QString());
    m_pdfCommentsOption = withRemark(m_pdfCommentsBox, m_pdfCommentsRemark);
    notes->addWidget(m_pdfCommentsOption, 1, 0);
    // Markdown and EPUB: what they do with the annotations (showFormat())
    m_noCommentsNote = newRemark(m_notesGroup, QString());
    notes->addWidget(m_noCommentsNote, 2, 0);
    extras->addWidget(m_notesGroup, 0, 0, 1, 2);

    //: A group of the window of the export: the images of the text. In Polish: Obrazy
    m_imagesGroup = new QGroupBox(tr("Images"), extrasPage);
    auto* images = new QGridLayout(m_imagesGroup);
    m_copyImagesBox = new QCheckBox(m_imagesGroup);
    m_copyImagesOption = withRemark(
        m_copyImagesBox,
        newRemark(m_imagesGroup,
                  //: Under the copy of the images. In Polish: Bez tej opcji odnośniki do obrazów
                  //: wskazują pliki w folderze książki.
                  tr("Without this option the links to the images name the files in the folder "
                     "of the book.")));
    images->addWidget(m_copyImagesOption, 0, 0);
    // A Word document, a PDF and an EPUB: what they do with the images (showFormat())
    m_imagesInFileNote = newRemark(m_imagesGroup, QString());
    images->addWidget(m_imagesInFileNote, 1, 0);
    extras->addWidget(m_imagesGroup, 1, 0, 1, 2);
    endTab(extras);

    // The options of a PDF: its purpose, its bookmarks, its pages in pairs, its shades of gray
    //: A tab of the window of the export: the options of a PDF. In Polish: P&DF
    QGridLayout* pdf = newTab(tr("P&DF"));
    m_pdfTab = pdf->parentWidget();
    //: A group of the tab of a PDF: what the file is for. In Polish: Przeznaczenie pliku
    auto* purposeGroup = new QGroupBox(tr("Purpose of the File"), m_pdfTab);
    auto* purposeLayout = new QVBoxLayout(purposeGroup);
    m_purposeButtons = new QButtonGroup(this);
    const std::array<std::pair<editor::PdfPurpose, QString>, 3> purposes = {{
        //: A purpose of a PDF. In Polish: Do &czytania i wysłania: klikalne łącza, spis treści
        //: i zakładki
        {editor::PdfPurpose::Reading,
         tr("For &reading and sending: clickable links, contents and bookmarks")},
        //: A purpose of a PDF. In Polish: Do druku w drukar&ni (PDF/X-4): bez łączy, z profilem
        //: kolorów
        {editor::PdfPurpose::PrintShop,
         tr("For a print s&hop (PDF/X-4): without links, with a color profile")},
        //: A purpose of a PDF. In Polish: Do archi&wum (PDF/A-1b): otworzy się tak samo za wiele
        //: lat
        {editor::PdfPurpose::Archive,
         tr("For the &archive (PDF/A-1b): it opens the same in many years")},
    }};
    for (const auto& [purpose, text] : purposes) {
        auto* button = new QRadioButton(text, purposeGroup);
        m_purposeButtons->addButton(button, static_cast<int>(purpose));
        purposeLayout->addWidget(button);
        if (!editor::pdfPurposeSupported(purpose)) {
            button->setEnabled(false);
            //: The tooltip of a purpose of a PDF this build cannot write. In Polish: Ta wersja
            //: programu nie zapisuje takiego pliku: potrzebuje Qt 6.8 lub nowszego.
            button->setToolTip(tr("This version of the program cannot write such a file: it "
                                  "needs Qt 6.8 or newer."));
        }
    }
    pdf->addWidget(purposeGroup, 0, 0, 1, 2);
    //: In Polish: &Zakładki z tytułami części, rozdziałów i nagłówków
    m_bookmarksBox = new QCheckBox(
        tr("Boo&kmarks with the titles of the parts, chapters and headings"), m_pdfTab);
    pdf->addWidget(
        withRemark(m_bookmarksBox,
                   newRemark(m_pdfTab,
                             //: Under the bookmarks. In Polish: Czytnik PDF pokazuje je w panelu
                             //: zakładek, jak spis treści obok stron.
                             tr("A PDF reader shows them in its panel of bookmarks, as contents "
                                "beside the pages."))),
        1, 0, 1, 2);
    //: In Polish: Strony pa&rami, jak w otwartej książce
    m_facingPagesBox = new QCheckBox(tr("Pages in pa&irs, as in an open book"), m_pdfTab);
    //: In Polish: W &odcieniach szarości – do druku czarno-białego
    m_grayscaleBox =
        new QCheckBox(tr("In shades of gray, for &black and white printing"), m_pdfTab);
    if (!editor::pdfGrayscaleSupported()) {
        m_grayscaleBox->setEnabled(false);
        m_grayscaleBox->setToolTip(tr("This version of the program cannot write such a file: it "
                                      "needs Qt 6.8 or newer."));
    }
    // The shades of gray beside the pages in pairs: the tab as high as the others
    pdf->addWidget(
        withRemark(m_facingPagesBox,
                   newRemark(m_pdfTab,
                             //: Under the pages in pairs. In Polish: Czytnik PDF pokaże pierwszą
                             //: stronę osobno, a dalej lewą i prawą obok siebie.
                             tr("A PDF reader shows the first page alone, then the left and the "
                                "right page side by side.")),
                   m_grayscaleBox),
        2, 0, 1, 2);
    pdf->addWidget(newRemark(m_pdfTab,
                             //: At the end of the tab of a PDF. In Polish: Czcionki są w pliku,
                             //: więc wygląda on wszędzie tak samo. Tytuł, autor i język pliku –
                             //: z właściwości książki.
                             tr("The fonts are in the file, so it looks the same everywhere. The "
                                "title, the author and the language of the file come from the "
                                "properties of the book.")),
                   3, 0, 1, 2);
    endTab(pdf);

    // The cover and the data of an EPUB
    //: A tab of the window of the export: the cover and the data of an EPUB. In Polish:
    //: Okła&dka i dane
    QGridLayout* epub = newTab(tr("Cover an&d Data"));
    m_epubTab = epub->parentWidget();
    auto* coverRow = new QWidget(m_epubTab);
    auto* coverLayout = new QHBoxLayout(coverRow);
    coverLayout->setContentsMargins(0, 0, 0, 0);
    m_coverPicture = new QLabel(coverRow);
    const int pictureHeight = fontMetrics().lineSpacing() * COVER_LINES;
    m_coverPicture->setFixedSize(pictureHeight * 2 / 3, pictureHeight);
    m_coverPicture->setAlignment(Qt::AlignCenter);
    m_coverPicture->setWordWrap(true);
    m_coverPicture->setFrameShape(QFrame::StyledPanel);
    m_coverPicture->setForegroundRole(QPalette::PlaceholderText);
    coverLayout->addWidget(m_coverPicture, 0, Qt::AlignTop);
    auto* coverFields = new QWidget(coverRow);
    auto* coverGrid = new QGridLayout(coverFields);
    coverGrid->setContentsMargins(0, 0, 0, 0);
    coverGrid->setColumnStretch(1, 1);
    m_coverBox = new QComboBox(coverFields);
    if (!m_book.coverFile.isEmpty()) {
        //: The cover of an EPUB: the design chosen in the materials; %1: its title. In Polish:
        //: %1 (z materiałów)
        m_coverBox->addItem(tr("%1 (from the materials)").arg(m_book.coverTitle),
                            static_cast<int>(Cover::Design));
    }
    //: The cover of an EPUB. In Polish: Bez okładki
    m_coverBox->addItem(tr("No cover"), static_cast<int>(Cover::None));
    //: Chooses another picture of the cover of an EPUB. In Polish: Inny &obraz…
    auto* otherPictureButton = new QPushButton(tr("Other &Picture..."), coverFields);
    otherPictureButton->setAutoDefault(false);
    auto* coverChoice = new QWidget(coverFields);
    auto* coverChoiceLayout = new QHBoxLayout(coverChoice);
    coverChoiceLayout->setContentsMargins(0, 0, 0, 0);
    coverChoiceLayout->addWidget(m_coverBox, 1);
    coverChoiceLayout->addWidget(otherPictureButton);
    //: In Polish: Okładk&a:
    addRow(coverGrid, tr("Co&ver:"), coverChoice)->setBuddy(m_coverBox);
    m_coverNote = addNote(coverGrid);
    coverGrid->setRowStretch(coverGrid->rowCount(), 1);
    coverLayout->addWidget(coverFields, 1);
    epub->addWidget(coverRow, 0, 0, 1, 2);

    //: A group of the tab of an EPUB: the data of the book. In Polish: Dane książki
    auto* dataGroup = new QGroupBox(tr("Book Data"), m_epubTab);
    auto* data = new QGridLayout(dataGroup);
    data->setColumnStretch(1, 1);
    const auto newValue = [dataGroup](const QString& text) {
        auto* label = new QLabel(text, dataGroup);
        label->setTextFormat(Qt::PlainText);
        return label;
    };
    m_titleAndAuthor = newValue(m_book.author.trimmed().isEmpty()
                                    ? m_book.title
                                    //: The title and the author of an EPUB; %1: the title, %2:
                                    //: the author. In Polish: %1 – %2
                                    : tr("%1 – %2").arg(m_book.title, m_book.author));
    //: In Polish: Tytuł i autor:
    addRow(data, tr("Title and author:"), m_titleAndAuthor);
    //: The language of a book that has none. In Polish: Nie ustawiono
    m_languageLabel = newValue(m_book.language.isEmpty() ? tr("Not set") : m_book.language);
    //: In Polish: Język:
    addRow(data, tr("Language:"), m_languageLabel);
    const QString description = m_book.description.simplified();
    m_descriptionLabel = newValue(
        description.isEmpty()
            //: The description of a book that has none. In Polish: Brak
            ? tr("None")
            : fontMetrics().elidedText(
                  //: A description in quotes; %1: its text. In Polish: „%1”
                  tr("\"%1\"").arg(description), Qt::ElideMiddle,
                  widthOfLetters(fontMetrics(), DESCRIPTION_LETTERS)));
    m_descriptionLabel->setToolTip(description);
    //: In Polish: Opis:
    addRow(data, tr("Description:"), m_descriptionLabel);
    // The ISBN and the publisher in one row: the tab as high as the others
    auto* publication = new QWidget(dataGroup);
    auto* publicationLayout = new QHBoxLayout(publication);
    publicationLayout->setContentsMargins(0, 0, 0, 0);
    publicationLayout->setSpacing(fontMetrics().averageCharWidth() * 3);
    m_isbnField = new QLineEdit(m_book.isbn, publication);
    //: The example of an ISBN in its empty field. In Polish: 978-83-…
    m_isbnField->setPlaceholderText(tr("978-..."));
    // As wide as an ISBN with its hyphens
    m_isbnField->setFixedWidth(widthOfLetters(m_isbnField->fontMetrics(), ISBN_LETTERS));
    publicationLayout->addWidget(m_isbnField);
    m_publisherField = new QLineEdit(m_book.publisher, publication);
    //: The example of a publisher in its empty field. In Polish: np. Wydawnictwo Wydmy albo
    //: puste
    m_publisherField->setPlaceholderText(tr("e.g. Dunes Publishing, or empty"));
    //: In Polish: W&ydawca:
    auto* publisherLabel = new QLabel(tr("P&ublisher:"), publication);
    publisherLabel->setBuddy(m_publisherField);
    auto* publisherLayout = new QHBoxLayout();
    publisherLayout->setSpacing(
        style()->layoutSpacing(QSizePolicy::Label, QSizePolicy::LineEdit, Qt::Horizontal));
    publisherLayout->addWidget(publisherLabel);
    publisherLayout->addWidget(m_publisherField, 1);
    publicationLayout->addLayout(publisherLayout, 1);
    //: In Polish: &ISBN:
    addRow(data, tr("&ISBN:"), publication)->setBuddy(m_isbnField);
    // The sentence under the data as wide as the group: in fewer lines
    m_bookDataNote = addNote(data);
    int noteRow = 0;
    int noteColumn = 0;
    int noteRows = 0;
    int noteColumns = 0;
    data->getItemPosition(data->indexOf(m_bookDataNote.widget), &noteRow, &noteColumn, &noteRows,
                          &noteColumns);
    data->removeWidget(m_bookDataNote.widget);
    data->addWidget(m_bookDataNote.widget, noteRow, 0, 1, 2);
    epub->addWidget(dataGroup, 1, 0, 1, 2);
    endTab(epub);

    // The options of the formats of the plugins: a tab of each, shown while it is chosen
    for (const Format& each : std::as_const(m_formats)) {
        const std::shared_ptr<core::IExporter> plugin = exporterOf(each.id);
        if (!plugin) {
            continue;
        }
        QWidget* page = nullptr;
        try {
            page = plugin->createOptionsPage(each.id.toStdString(), m_tabs);
        } catch (const std::exception& error) {
            core::Logger::getInstance().error("Export: the options of the format {}: {}",
                                              each.id.toStdString(), error.what());
        }
        if (page != nullptr) {
            const QString title = each.name.left(each.name.lastIndexOf(QLatin1String(" ("))).trimmed();
            m_tabs->setTabVisible(m_tabs->addTab(page, title.isEmpty() ? each.name : title), false);
            m_formatPages.insert(each.id, page);
        }
    }

    //: In Polish: Postęp i wynik pokażą się tutaj.
    setHint(tr("The progress and the result will show here."));
    //: The button that writes the file. In Polish: Eksportuj
    setAcceptText(tr("Export"));

    // The writer's last choices
    m_titlesBox->setChecked(settings.get<bool>("export.titles"));
    m_newPageBox->setChecked(settings.get<bool>("export.newPage"));
    m_pageNumbersBox->setChecked(settings.get<bool>("export.pageNumbers"));
    m_commentsBox->setChecked(settings.get<bool>("export.comments"));
    m_copyImagesBox->setChecked(settings.get<bool>("export.copyImages"));
    const editor::RunningHead runningHead =
        valueOfName(runningHeadNames(), settings.get<std::string>("export.runningHead"),
                    editor::RunningHead::AuthorAndTitle);
    m_runningHeadBox->setCurrentIndex(
        std::max(0, m_runningHeadBox->findData(static_cast<int>(runningHead))));
    m_pdfCommentsBox->setChecked(settings.get<bool>("export.pdf.comments"));
    editor::PdfPurpose purpose = valueOfName(
        purposeNames(), settings.get<std::string>("export.pdf.purpose"), editor::PdfPurpose::Reading);
    if (!editor::pdfPurposeSupported(purpose)) {
        purpose = editor::PdfPurpose::Reading;
    }
    m_purposeButtons->button(static_cast<int>(purpose))->setChecked(true);
    m_bookmarksBox->setChecked(settings.get<bool>("export.pdf.bookmarks"));
    m_facingPagesBox->setChecked(settings.get<bool>("export.pdf.facingPages"));
    m_grayscaleBox->setChecked(editor::pdfGrayscaleSupported() &&
                               settings.get<bool>("export.pdf.grayscale"));
    (settings.get<std::string>("export.epub.look") == "simple" ? m_simpleLookButton
                                                                : m_bookStylesButton)
        ->setChecked(true);
    m_embedFontBox->setChecked(settings.get<bool>("export.epub.embedFont"));
    const bool noCover = settings.get<std::string>("export.epub.cover") == "none";
    m_coverBox->setCurrentIndex(
        std::max(0, m_coverBox->findData(static_cast<int>(noCover ? Cover::None : Cover::Design))));
    const QString font = QString::fromStdString(settings.get<std::string>("export.font"));
    int fontIndex = m_fontBox->findData(font);
    if (fontIndex < 0) {
        m_fontBox->addItem(font, font);  // a font this system does not have
        fontIndex = m_fontBox->count() - 1;
    }
    m_fontBox->setCurrentIndex(fontIndex);
    const QString lastContent = QString::fromStdString(settings.get<std::string>("export.content"));
    Content start = lastContent == contentName(Content::Chosen) ? Content::Chosen
                    : lastContent == contentName(Content::Front) ? Content::Front
                                                                  : Content::Book;
    if (m_book.title.isEmpty()) {
        start = Content::Front;
    } else if (start == Content::Front && m_book.frontTitle.isEmpty()) {
        start = Content::Book;
    }
    (start == Content::Book ? m_bookButton : start == Content::Chosen ? m_chosenButton
                                                                       : m_frontButton)
        ->setChecked(true);
    setFormat(format);
    m_nameField->setText(suggestedName() + extension());

    connect(browseButton, &QPushButton::clicked, this, &ExportBookDialog::browse);
    connect(m_nameField, &QLineEdit::textEdited, this, [this]() { m_nameTyped = true; });
    connect(m_nameField, &QLineEdit::textChanged, this, &ExportBookDialog::dropFailure);
    connect(m_nameField, &QLineEdit::textChanged, this, &ExportBookDialog::updateFields);
    connect(m_formatBox, &QComboBox::currentIndexChanged, this, [this]() {
        dropFailure();
        showFormat();
    });
    for (QRadioButton* button : {m_bookButton, m_chosenButton, m_frontButton}) {
        connect(button, &QRadioButton::toggled, this, [this](bool on) {
            if (on) {
                dropFailure();
                showContent();
            }
        });
    }
    for (QCheckBox* box :
         {m_titlesBox, m_newPageBox, m_pageNumbersBox, m_commentsBox, m_copyImagesBox,
          m_pdfCommentsBox, m_bookmarksBox, m_facingPagesBox, m_grayscaleBox, m_embedFontBox}) {
        connect(box, &QCheckBox::toggled, this, &ExportBookDialog::dropFailure);
    }
    for (QComboBox* box : {m_fontBox, m_runningHeadBox}) {
        connect(box, &QComboBox::currentIndexChanged, this, &ExportBookDialog::dropFailure);
    }
    connect(m_bookStylesButton, &QRadioButton::toggled, this, &ExportBookDialog::dropFailure);
    connect(m_purposeButtons, &QButtonGroup::idToggled, this, [this](int, bool on) {
        if (on) {
            dropFailure();
            showPurpose();
        }
    });
    connect(m_coverBox, &QComboBox::currentIndexChanged, this, [this]() {
        dropFailure();
        showCover();
    });
    connect(otherPictureButton, &QPushButton::clicked, this, &ExportBookDialog::browseCover);
    for (QLineEdit* field : {m_isbnField, m_publisherField}) {
        connect(field, &QLineEdit::textChanged, this, &ExportBookDialog::dropFailure);
        connect(field, &QLineEdit::textChanged, this, &ExportBookDialog::updateFields);
    }

    // The page follows the settings the gear opens
    showPage();
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key.rfind("editor.page", 0) == 0 || key.rfind("editor.margins", 0) == 0 ||
                key == "ui.lengthUnit") {
                QMetaObject::invokeMethod(this, "showPage", Qt::QueuedConnection);
            }
        });

    setFolder(folder);
    showFormat();
    showPurpose();
    showCover();
    showContent();

    // The name is typed over, without its extension
    m_nameField->setFocus();
    m_nameField->setSelection(0, static_cast<int>(baseName().size()));
}

ExportBookDialog::~ExportBookDialog()
{
    if (m_settingsListener >= 0) {
        core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    }
}

ExportBookDialog::Choice ExportBookDialog::choice() const
{
    Choice choice;
    choice.formatId = m_formatBox->currentData().toString();
    choice.format = currentFormat();
    choice.content = m_frontButton->isChecked()    ? Content::Front
                     : m_chosenButton->isChecked() ? Content::Chosen
                                                   : Content::Book;
    for (const QTreeWidgetItem* item : leaves()) {
        if (item->checkState(0) == Qt::Checked) {
            choice.chosen << item->data(0, ID_ROLE).toString();
        }
    }
    choice.titles = m_titlesBox->isChecked();
    choice.newPage = m_newPageBox->isChecked();
    choice.pageNumbers = m_pageNumbersBox->isChecked();
    choice.fontFamily = m_fontBox->currentData().toString();
    choice.runningHead = static_cast<editor::RunningHead>(m_runningHeadBox->currentData().toInt());
    // The comments of the format: Word's, or those of a PDF that may have them
    if (choice.formatId == QLatin1String(DOCX_ID)) {
        choice.comments = m_commentsBox->isChecked();
    } else if (choice.formatId == QLatin1String(PDF_ID)) {
        choice.comments = m_pdfCommentsBox->isEnabled() && m_pdfCommentsBox->isChecked();
    } else {
        choice.comments = false;
    }
    choice.copyImages = m_copyImagesBox->isChecked();
    choice.imagesFolder = imagesFolderName();
    choice.pdf.purpose = static_cast<editor::PdfPurpose>(m_purposeButtons->checkedId());
    choice.pdf.bookmarks = m_bookmarksBox->isChecked();
    choice.pdf.facingPages = m_facingPagesBox->isChecked();
    choice.pdf.grayscale = m_grayscaleBox->isEnabled() && m_grayscaleBox->isChecked();
    choice.bookStyles = m_bookStylesButton->isChecked();
    choice.embedFont = m_embedFontBox->isChecked();
    choice.cover = static_cast<Cover>(m_coverBox->currentData().toInt());
    choice.coverImage = choice.cover == Cover::Design ? m_book.coverFile
                        : choice.cover == Cover::File ? m_coverFile
                                                      : QString();
    choice.isbn = m_isbnField->text().simplified();
    choice.publisher = m_publisherField->text().simplified();
    return choice;
}

QList<ExportBookDialog::Format> ExportBookDialog::formats()
{
    QList<Format> list = {
        //: A format of the list of the window of the export. In Polish: Dokument Worda (.docx)
        {QString::fromLatin1(DOCX_ID), tr("Word document (.docx)"), QStringLiteral(".docx")},
        //: A format of the list of the window of the export. In Polish: Dokument PDF (.pdf)
        {QString::fromLatin1(PDF_ID), tr("PDF document (.pdf)"), QStringLiteral(".pdf")},
        //: A format of the list of the window of the export. In Polish: Markdown (.md)
        {QString::fromLatin1(MARKDOWN_ID), tr("Markdown (.md)"), QStringLiteral(".md")},
        //: A format of the list of the window of the export. In Polish: Książka elektroniczna
        //: EPUB (.epub)
        {QString::fromLatin1(EPUB_ID), tr("EPUB e-book (.epub)"), QStringLiteral(".epub")}};
    for (const auto& exporter :
         core::ExtensionPointRegistry::getInstance().getPluginsOfType<core::IExporter>()) {
        std::vector<core::ExportFormatInfo> infos;
        try {
            infos = exporter->getFormats();
        } catch (const std::exception& error) {
            core::Logger::getInstance().error("Export: the formats of the plugin {}: {}",
                                              exporter->getPluginId(), error.what());
        }
        for (const core::ExportFormatInfo& info : infos) {
            const QString id = QString::fromStdString(info.id);
            const QString name = QString::fromStdString(info.name).trimmed();
            const QString extension = QLatin1Char('.') + QString::fromStdString(info.extension);
            // A format needs its id, name and extension; an id the list has already stays
            // the first one's
            const bool listed = std::any_of(list.cbegin(), list.cend(),
                                            [&id](const Format& each) { return each.id == id; });
            if (id.isEmpty() || name.isEmpty() || info.extension.empty() || listed) {
                continue;
            }
            list.append({id, QStringLiteral("%1 (%2)").arg(name, extension), extension});
        }
    }
    return list;
}

std::shared_ptr<core::IExporter> ExportBookDialog::exporterOf(const QString& id)
{
    if (id == QLatin1String(DOCX_ID) || id == QLatin1String(PDF_ID) ||
        id == QLatin1String(MARKDOWN_ID) || id == QLatin1String(EPUB_ID)) {
        return nullptr;
    }
    for (const auto& exporter :
         core::ExtensionPointRegistry::getInstance().getPluginsOfType<core::IExporter>()) {
        try {
            for (const core::ExportFormatInfo& info : exporter->getFormats()) {
                if (QString::fromStdString(info.id) == id) {
                    return exporter;
                }
            }
        } catch (const std::exception&) {
            continue;  // formats() logged it
        }
    }
    return nullptr;
}

void ExportBookDialog::setFormat(editor::ExportFormat format)
{
    const char* id = DOCX_ID;
    switch (format) {
        case editor::ExportFormat::Docx:
            break;
        case editor::ExportFormat::Pdf:
            id = PDF_ID;
            break;
        case editor::ExportFormat::Markdown:
            id = MARKDOWN_ID;
            break;
        case editor::ExportFormat::Epub:
            id = EPUB_ID;
            break;
    }
    setFormatId(QString::fromLatin1(id));
}

editor::ExportFormat ExportBookDialog::currentFormat() const
{
    // A plugin's format: DOCX, which the window does not write then (Choice::formatId)
    const QString id = m_formatBox->currentData().toString();
    if (id == QLatin1String(PDF_ID)) {
        return editor::ExportFormat::Pdf;
    }
    if (id == QLatin1String(MARKDOWN_ID)) {
        return editor::ExportFormat::Markdown;
    }
    if (id == QLatin1String(EPUB_ID)) {
        return editor::ExportFormat::Epub;
    }
    return editor::ExportFormat::Docx;
}

QRadioButton* ExportBookDialog::purposeButton(editor::PdfPurpose purpose) const
{
    return qobject_cast<QRadioButton*>(m_purposeButtons->button(static_cast<int>(purpose)));
}

QString ExportBookDialog::pdfCommentsNote() const
{
    return m_pdfCommentsOption->isVisibleTo(m_notesGroup) ? m_pdfCommentsRemark->text()
                                                          : QString();
}

QString ExportBookDialog::notesNote() const
{
    return m_noCommentsNote->isVisibleTo(m_notesGroup) ? m_noCommentsNote->text() : QString();
}

QString ExportBookDialog::imagesNote() const
{
    return m_imagesInFileNote->isVisibleTo(m_imagesGroup) ? m_imagesInFileNote->text()
                                                          : QString();
}

QString ExportBookDialog::bookDataNote() const
{
    return noteText(m_bookDataNote);
}

void ExportBookDialog::setCoverFile(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    m_coverFile = path;
    int index = m_coverBox->findData(static_cast<int>(Cover::File));
    if (index < 0) {
        m_coverBox->addItem(QString(), static_cast<int>(Cover::File));
        index = m_coverBox->count() - 1;
    }
    m_coverBox->setItemText(index, QFileInfo(path).fileName());
    m_coverBox->setItemData(index, QDir::toNativeSeparators(path), Qt::ToolTipRole);
    if (m_coverBox->currentIndex() == index) {
        showCover();
    } else {
        m_coverBox->setCurrentIndex(index);
    }
}


void ExportBookDialog::setFormatId(const QString& id)
{
    const int index = m_formatBox->findData(id);
    if (index >= 0) {
        m_formatBox->setCurrentIndex(index);
    }
}

void ExportBookDialog::setContent(Content content)
{
    (content == Content::Book ? m_bookButton : content == Content::Chosen ? m_chosenButton
                                                                           : m_frontButton)
        ->setChecked(true);
}

void ExportBookDialog::setFolder(const QString& folder)
{
    m_folder = ProgramFolders::cleanPath(folder);
    showPath(m_folderField, m_folder);
    dropFailure();
    updateFields();
}

QString ExportBookDialog::filePath() const
{
    if (!ProgramFolders::problemOf(m_folder).isEmpty() || !nameProblem().isEmpty()) {
        return QString();
    }
    const QString path = QDir(m_folder).filePath(fileName());
    return QFileInfo(path).isDir() ? QString() : path;
}

QTreeWidget* ExportBookDialog::textList() const
{
    return m_textList;
}

QString ExportBookDialog::chosenNote() const
{
    return m_chosenButton->isChecked() && !m_chosenBox->isHidden() ? m_chosenNote->text()
                                                                    : QString();
}

void ExportBookDialog::checkAll()
{
    for (QTreeWidgetItem* item : leaves()) {
        item->setCheckState(0, Qt::Checked);
    }
}

void ExportBookDialog::checkNone()
{
    for (QTreeWidgetItem* item : leaves()) {
        item->setCheckState(0, Qt::Unchecked);
    }
}

void ExportBookDialog::checkFinal()
{
    // A text that is not final goes; a table of contents, which has no status, stays
    for (QTreeWidgetItem* item : leaves()) {
        const QString status = item->data(0, STATUS_ROLE).toString();
        item->setCheckState(0, status.isEmpty() || status == QLatin1String("final")
                                   ? Qt::Checked
                                   : Qt::Unchecked);
    }
}

void ExportBookDialog::showPage()
{
    m_pageLabel->setText(pageText());
}

QString ExportBookDialog::pageText()
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

QString ExportBookDialog::shownPageText() const
{
    return m_pageLabel->text();
}

QString ExportBookDialog::imagesFolderName() const
{
    //: The last word of the name of the folder of the images of a Markdown file ("Book –
    //: images"). In Polish: obrazy
    return editor::markdownImagesFolderName(baseName(), tr("images"));
}

void ExportBookDialog::run()
{
    const QString path = filePath();
    if (path.isEmpty()) {
        return;
    }
    rememberChoices();
    ProgramFolders::remember(ProgramFolders::Operation::ExportBook, m_folder);
    startWork();
    QString reason;
    editor::ExportResult result;
    editor::ExportProblem problem;
    bool written = false;
    if (!QDir().mkpath(m_folder)) {
        //: %1: the folder of the file. In Polish: Kalahari nie może utworzyć folderu %1.
        reason =
            tr("Kalahari cannot create the folder %1.").arg(QDir::toNativeSeparators(m_folder));
    } else {
        written = m_exporter(path, choice(), progress(), &result, &problem);
        reason = reasonOf(problem);
    }
    if (written) {
        //: %1: the path of the file. In Polish: Zapisano plik %1.
        QString text = tr("Saved the file %1.").arg(QDir::toNativeSeparators(path));
        if (result.images > 0 && !result.imagesFolder.isEmpty()) {
            //: After the file of a Markdown export; %1: the name of the folder. In Polish:
            //: Obrazy (%n) są w folderze „%1”.
            text += QLatin1Char(' ') + tr("The images (%n) are in the folder \"%1\".", nullptr,
                                          result.images)
                                           .arg(QFileInfo(result.imagesFolder).fileName());
        }
        if (result.missingImages > 0) {
            //: After the file of an export. In Polish: Brak plików %n obrazów – nie ma ich
            //: w wyeksportowanym pliku.
            text += QLatin1Char(' ') + tr("The files of %n images were not found: the file does "
                                          "not have them.",
                                          nullptr, result.missingImages);
        }
        if (result.coverMissing) {
            //: After the file of an EPUB. In Polish: Nie udało się odczytać obrazu okładki –
            //: e-book jest bez okładki.
            text += QLatin1Char(' ') + tr("The picture of the cover could not be read: the e-book "
                                          "has no cover.");
        }
        if (result.fontNotEmbedded) {
            //: After the file of an EPUB. In Polish: Czcionki tekstu nie ma w pliku: jej
            //: licencja na to nie pozwala albo nie ma jej na komputerze.
            text += QLatin1Char(' ') + tr("The font of the text is not in the file: its licence "
                                          "does not allow it, or the computer does not have it.");
        }
        finishWork(text, path, true);
        // The result in the place of the bar, which said only that the work went on: the
        // window with the result keeps the height of the screen of 1366x768 at 150%
        progressBar()->hide();
    } else {
        //: In Polish: Nie udało się zapisać pliku.
        failWork(tr("The file could not be saved."), reason, problem.lines);
        updateFields();
    }
}

void ExportBookDialog::browse()
{
    // A folder that does not exist yet opens the window in the nearest one that does
    QString start = ProgramFolders::existingFolderAt(m_folder);
    if (start.isEmpty()) {
        start = ProgramFolders::documentsFolder();
    }
    //: The title of the system window that chooses the folder of the exported file. In
    //: Polish: Folder eksportu
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Folder of the Export"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!chosen.isEmpty()) {
        setFolder(chosen);
    }
}

void ExportBookDialog::showPurpose()
{
    // A file for a print shop (PDF/X-4) cannot have comments
    const bool printShop =
        m_purposeButtons->checkedId() == static_cast<int>(editor::PdfPurpose::PrintShop);
    m_pdfCommentsBox->setEnabled(!printShop);
    if (printShop) {
        m_pdfCommentsBox->setChecked(false);
    }
    m_pdfCommentsRemark->setText(
        printShop
            //: Under the annotations as comments of a PDF for a print shop. In Polish: Plik do
            //: drukarni (PDF/X-4) nie może mieć komentarzy.
            ? tr("A file for a print shop (PDF/X-4) cannot have comments.")
            //: Under the annotations as comments of a PDF. In Polish: Komentarze, zadania
            //: i notatki z tekstu, z autorem i datą, jako notatki przy tekście w czytniku PDF.
            //: Bez tej opcji plik nie zawiera uwag.
            : tr("The comments, to-dos and notes of the text, with their authors and dates, as "
                 "notes beside the text in a PDF reader. Without this option the file has no "
                 "annotations."));
}

void ExportBookDialog::showCover()
{
    const auto cover = static_cast<Cover>(m_coverBox->currentData().toInt());
    const QString path = cover == Cover::Design ? m_book.coverFile
                         : cover == Cover::File ? m_coverFile
                                                : QString();
    // The picture as the reader shows it, as high as the frame
    QPixmap picture;
    if (!path.isEmpty()) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        const QSize frame = m_coverPicture->contentsRect().size() * devicePixelRatio();
        if (size.isValid() && !frame.isEmpty()) {
            reader.setScaledSize(size.scaled(frame, Qt::KeepAspectRatio));
        }
        picture = QPixmap::fromImageReader(&reader);
        picture.setDevicePixelRatio(devicePixelRatio());
    }
    if (picture.isNull()) {
        m_coverPicture->setPixmap(QPixmap());
        m_coverPicture->setText(path.isEmpty()
                                    //: In the place of the picture of the cover of an EPUB
                                    //: without one. In Polish: bez okładki
                                    ? tr("no cover")
                                    //: In the place of a picture of the cover that cannot be
                                    //: read. In Polish: nie można odczytać obrazu
                                    : tr("the picture cannot be read"));
    } else {
        m_coverPicture->setPixmap(picture);
    }
    switch (cover) {
        case Cover::Design:
            //: Under the cover of an EPUB from the materials. In Polish: Projekt przodu okładki
            //: oznaczony jako wybrany w materiałach „Okładka”.
            showNote(m_coverNote, tr("The design of the front of the cover chosen in the "
                                     "materials \"Cover\"."));
            break;
        case Cover::None:
            showNote(m_coverNote,
                     m_book.coverFile.isEmpty()
                         //: Under the cover of an EPUB, when the materials have none. In
                         //: Polish: W materiałach „Okładka” nie wybrano projektu przodu okładki.
                         ? tr("The materials \"Cover\" have no chosen design of the front of the "
                              "cover.")
                         //: Under no cover of an EPUB. In Polish: E-book będzie bez okładki.
                         : tr("The e-book has no cover."));
            break;
        case Cover::File:
            //: Under another picture of the cover of an EPUB; %1: its file. In Polish: Obraz
            //: %1.
            showNote(m_coverNote, tr("The picture %1.").arg(QDir::toNativeSeparators(path)));
            break;
    }
}

void ExportBookDialog::browseCover()
{
    using Operation = ProgramFolders::Operation;
    //: The title of the system window that chooses the picture of the cover of an EPUB. In
    //: Polish: Obraz okładki
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Picture of the Cover"), ProgramFolders::windowFolder(Operation::InsertImage),
        editor::imageFileFilter(tr("Images")) + QStringLiteral(";;") +
            //: In Polish: Wszystkie pliki (*)
            tr("All Files (*)"));
    if (!path.isEmpty()) {
        ProgramFolders::remember(Operation::InsertImage, QFileInfo(path).absolutePath());
        setCoverFile(path);
    }
}

void ExportBookDialog::showFormat()
{
    const QString id = m_formatBox->currentData().toString();
    const bool word = id == QLatin1String(DOCX_ID);
    const bool pdf = id == QLatin1String(PDF_ID);
    const bool markdown = id == QLatin1String(MARKDOWN_ID);
    const bool epub = id == QLatin1String(EPUB_ID);
    const bool paged = word || pdf;  // the pages of the settings
    setHeadingIcon(markdown ? QStringLiteral("file.export.markdown")
                   : pdf    ? QStringLiteral("file.export.pdf")
                   : epub   ? QStringLiteral("file.export.epub")
                            : QStringLiteral("file.export.docx"));
    // The name keeps what the writer typed, with the extension of the format
    const QString name = m_nameField->text().trimmed();
    const QString base = baseName();
    if (!name.isEmpty() && name != base + extension()) {
        m_nameField->setText(base + extension());
    }
    m_wordLayout->setVisible(paged);
    m_fontRow->setVisible(paged);
    setRowVisible(m_fontBox, paged);
    m_runningHeadNote.widget->setVisible(paged);
    setRowVisible(m_pageLabel, paged);
    m_pageNote.widget->setVisible(paged);
    m_epubLayout->setVisible(epub);
    // The groups of the annotations and of the images stay for the program's formats: each
    // says what its format does. A plugin's format has its own tab instead.
    m_commentsOption->setVisible(word);
    m_pdfCommentsOption->setVisible(pdf);
    m_noCommentsNote->setVisible(markdown || epub);
    if (epub) {
        //: In the group of the annotations, for an EPUB. In Polish: EPUB nie ma komentarzy: uwagi
        //: zostają tylko w książce.
        m_noCommentsNote->setText(tr("EPUB has no comments: the annotations stay only in the book."));
    } else {
        //: In the group of the annotations, for a Markdown file. In Polish: Markdown nie ma
        //: komentarzy: uwagi zostają tylko w książce.
        m_noCommentsNote->setText(
            tr("Markdown has no comments: the annotations stay only in the book."));
    }
    m_copyImagesOption->setVisible(markdown);
    m_imagesInFileNote->setVisible(word || pdf || epub);
    if (pdf) {
        //: In the group of the images, for a PDF. In Polish: Dokument PDF zawiera obrazy w samym
        //: pliku, z ich przycięciem, obrotem, filtrem, ramką i cieniem.
        m_imagesInFileNote->setText(tr("A PDF document has the images in the file itself, with "
                                       "their cropping, rotation, filter, border and shadow."));
    } else if (epub) {
        //: In the group of the images, for an EPUB. In Polish: Książka EPUB zawiera obrazy
        //: w samym pliku, z tekstem alternatywnym dla czytników ekranu.
        m_imagesInFileNote->setText(tr("An EPUB e-book has the images in the file itself, with "
                                       "their alternative text for screen readers."));
    } else {
        //: In the group of the images, for a Word document. In Polish: Dokument Worda zawiera
        //: obrazy w samym pliku.
        m_imagesInFileNote->setText(tr("A Word document has the images in the file itself."));
    }
    m_tabs->setTabVisible(m_tabs->indexOf(m_extrasTab), word || pdf || markdown || epub);
    m_tabs->setTabVisible(m_tabs->indexOf(m_pdfTab), pdf);
    m_tabs->setTabVisible(m_tabs->indexOf(m_epubTab), epub);
    for (auto page = m_formatPages.cbegin(); page != m_formatPages.cend(); ++page) {
        m_tabs->setTabVisible(m_tabs->indexOf(page.value()), page.key() == id);
    }
    showContent();
}

void ExportBookDialog::showContent()
{
    const bool front = m_frontButton->isChecked();
    // The list stays in its place, so the tab keeps its look; it works with Chosen texts
    m_chosenBox->setEnabled(m_chosenButton->isChecked());
    const QString id = m_formatBox->currentData().toString();
    const QString title = front ? m_book.frontTitle : m_book.title;
    QString description;
    if (id == QLatin1String(DOCX_ID)) {
        description = front
                          //: %1: the title of the text. In Polish: Zapisuje tekst „%1” jako
                          //: dokument Worda.
                          ? tr("Saves the text \"%1\" as a Word document.").arg(title)
                          //: %1: the title of the book. In Polish: Zapisuje książkę „%1” jako
                          //: dokument Worda.
                          : tr("Saves the book \"%1\" as a Word document.").arg(title);
    } else if (id == QLatin1String(PDF_ID)) {
        description = front
                          //: %1: the title of the text. In Polish: Zapisuje tekst „%1” jako
                          //: dokument PDF.
                          ? tr("Saves the text \"%1\" as a PDF document.").arg(title)
                          //: %1: the title of the book. In Polish: Zapisuje książkę „%1” jako
                          //: dokument PDF.
                          : tr("Saves the book \"%1\" as a PDF document.").arg(title);
    } else if (id == QLatin1String(EPUB_ID)) {
        description = front
                          //: %1: the title of the text. In Polish: Zapisuje tekst „%1” jako
                          //: e-book w formacie EPUB.
                          ? tr("Saves the text \"%1\" as an e-book in the EPUB format.").arg(title)
                          //: %1: the title of the book. In Polish: Zapisuje książkę „%1” jako
                          //: e-book w formacie EPUB.
                          : tr("Saves the book \"%1\" as an e-book in the EPUB format.").arg(title);
    } else if (id == QLatin1String(MARKDOWN_ID)) {
        description = front
                          //: %1: the title of the text. In Polish: Zapisuje tekst „%1” jako plik
                          //: Markdown.
                          ? tr("Saves the text \"%1\" as a Markdown file.").arg(title)
                          //: %1: the title of the book. In Polish: Zapisuje książkę „%1” jako
                          //: plik Markdown.
                          : tr("Saves the book \"%1\" as a Markdown file.").arg(title);
    } else {
        const QString format = m_formatBox->currentText();
        description = front
                          //: The format of a plugin; %1: the title of the text, %2: the format
                          //: with its extension. In Polish: Zapisuje tekst „%1” w formacie %2.
                          ? tr("Saves the text \"%1\" in the format %2.").arg(title, format)
                          //: The format of a plugin; %1: the title of the book, %2: the format
                          //: with its extension. In Polish: Zapisuje książkę „%1” w formacie %2.
                          : tr("Saves the book \"%1\" in the format %2.").arg(title, format);
    }
    setHeading(tr("Export Book"), description);
    // The name follows the content until the writer types it
    if (!m_nameTyped) {
        const QString wanted = suggestedName() + extension();
        if (m_nameField->text() != wanted) {
            m_nameField->setText(wanted);
        }
    }
    updateFields();
}

void ExportBookDialog::updateFields()
{
    // The folder: why the program cannot use it, or that it is made for the file
    const QString folderProblem = ProgramFolders::problemOf(m_folder);
    if (!folderProblem.isEmpty()) {
        showNote(m_folderNote, folderProblem, QStringLiteral("common.warning"));
    } else if (!QFileInfo(m_folder).isDir()) {
        //: Under a folder that does not exist yet. In Polish: Kalahari utworzy ten folder
        //: podczas eksportu.
        showNote(m_folderNote, tr("Kalahari creates this folder during the export."));
    } else {
        showNote(m_folderNote, QString());
    }

    // The name: why it cannot be the name of the file, the name the file gets, or the file it
    // replaces
    bool nameUsable = true;
    const QString problem = nameProblem();
    if (!problem.isEmpty()) {
        nameUsable = false;
        showNote(m_nameNote, problem, QStringLiteral("common.warning"));
    } else if (!folderProblem.isEmpty()) {
        showNote(m_nameNote, QString());
    } else {
        const QFileInfo target(QDir(m_folder).filePath(fileName()));
        if (target.isDir()) {
            nameUsable = false;
            //: %1: the name of the file. In Polish: W tym folderze jest folder „%1”. Wybierz
            //: inną nazwę.
            showNote(m_nameNote,
                     tr("The folder has a folder named \"%1\". Choose another name.")
                         .arg(fileName()),
                     QStringLiteral("common.warning"));
        } else if (target.exists()) {
            //: %1: the name of the file. In Polish: W tym folderze jest już plik „%1”. Eksport
            //: go zastąpi.
            showNote(m_nameNote,
                     tr("The folder already has the file \"%1\". The export replaces it.")
                         .arg(fileName()),
                     QStringLiteral("common.warning"));
        } else if (fileName() != m_nameField->text().trimmed()) {
            //: %1: the name with its extension. In Polish: Plik dostanie nazwę „%1”.
            showNote(m_nameNote, tr("The file gets the name \"%1\".").arg(fileName()));
        } else {
            showNote(m_nameNote, QString());
        }
    }

    // The images of a Markdown file go to a folder named after it
    //: %1: the name of the folder of the images. In Polish: Skopiuj obrazy do &folderu „%1”
    //: obok pliku
    m_copyImagesBox->setText(tr("Copy the &images to the folder \"%1\" next to the file")
                                 .arg(literal(imagesFolderName())));

    // The texts chosen in the list
    int chosen = 0;
    const QList<QTreeWidgetItem*> texts = leaves();
    for (const QTreeWidgetItem* item : texts) {
        chosen += item->checkState(0) == Qt::Checked ? 1 : 0;
    }
    //: Under the list of the texts; %1: how many are checked, %n: how many there are. In
    //: Polish: Wybrano %1 z %n tekstu / tekstów.
    m_chosenNote->setText(
        tr("%1 of %n texts chosen.", nullptr, static_cast<int>(texts.size())).arg(chosen));
    const bool contentUsable = !m_chosenButton->isChecked() || chosen > 0;

    // The ISBN of an EPUB: none, or an ISBN
    const QString isbn = m_isbnField->text().simplified();
    const bool isbnUsable = isbn.isEmpty() || editor::isIsbn(isbn) ||
                            m_formatBox->currentData().toString() != QLatin1String(EPUB_ID);
    if (isbn.isEmpty() || editor::isIsbn(isbn)) {
        showNote(m_bookDataNote,
                 //: Under the data of an EPUB. In Polish: Tytuł, autora i język zmienisz we
                 //: właściwościach książki, a opis – w materiałach „Okładka”. ISBN i wydawcę
                 //: program zapamięta w książce. Bez ISBN plik dostaje stały identyfikator
                 //: książki.
                 tr("Change the title, the author and the language in the properties of the "
                    "book, and the description in the materials \"Cover\". Kalahari keeps the "
                    "ISBN and the publisher in the book. Without an ISBN the file gets a lasting "
                    "identifier of the book."));
    } else {
        //: Under the data of an EPUB whose ISBN is not one. In Polish: To nie jest ISBN: ma on
        //: 13 cyfr (albo 10) i właściwą cyfrę kontrolną, np. 978-3-16-148410-0.
        showNote(m_bookDataNote,
                 tr("This is not an ISBN: an ISBN has 13 digits (or 10) and the right check "
                    "digit, e.g. 978-3-16-148410-0."),
                 QStringLiteral("common.warning"));
    }

    acceptButton()->setEnabled(folderProblem.isEmpty() && nameUsable && contentUsable &&
                               isbnUsable);
}

void ExportBookDialog::textChanged(QTreeWidgetItem* item)
{
    Q_UNUSED(item);
    dropFailure();
    updateFields();
}

QList<QTreeWidgetItem*> ExportBookDialog::leaves() const
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

void ExportBookDialog::rememberChoices() const
{
    auto& settings = core::SettingsManager::getInstance();
    const Choice now = choice();
    settings.set<std::string>("export.content", contentName(now.content).toStdString());
    settings.set<bool>("export.titles", now.titles);
    settings.set<bool>("export.newPage", now.newPage);
    settings.set<bool>("export.pageNumbers", now.pageNumbers);
    settings.set<std::string>("export.font", now.fontFamily.toStdString());
    settings.set<std::string>("export.runningHead", nameOfValue(runningHeadNames(), now.runningHead));
    // The comments of each format: Word's, and those of a PDF
    settings.set<bool>("export.comments", m_commentsBox->isChecked());
    settings.set<bool>("export.copyImages", now.copyImages);
    settings.set<bool>("export.pdf.comments", m_pdfCommentsBox->isChecked());
    settings.set<std::string>("export.pdf.purpose", nameOfValue(purposeNames(), now.pdf.purpose));
    settings.set<bool>("export.pdf.bookmarks", now.pdf.bookmarks);
    settings.set<bool>("export.pdf.facingPages", now.pdf.facingPages);
    settings.set<bool>("export.pdf.grayscale", m_grayscaleBox->isChecked());
    settings.set<std::string>("export.epub.look", now.bookStyles ? "book" : "simple");
    settings.set<bool>("export.epub.embedFont", now.embedFont);
    // Another picture is not kept: it is not the book's
    settings.set<std::string>("export.epub.cover", now.cover == Cover::None ? "none" : "design");
}

QString ExportBookDialog::fileName() const
{
    const QString base = baseName();
    return base.isEmpty() ? QString() : base + extension();
}

QString ExportBookDialog::extension() const
{
    const int index = m_formatBox->currentIndex();
    return index >= 0 && index < m_formats.size() ? m_formats.at(index).extension
                                                  : QStringLiteral(".docx");
}

QString ExportBookDialog::baseName() const
{
    QString name = m_nameField->text().trimmed();
    for (const Format& format : std::as_const(m_formats)) {
        if (name.endsWith(format.extension, Qt::CaseInsensitive)) {
            name.chop(format.extension.size());
            break;
        }
    }
    return name.trimmed();
}

QString ExportBookDialog::suggestedName() const
{
    const QString title = m_frontButton->isChecked() ? m_book.frontTitle : m_book.title;
    const QString name = safeName(title);
    //: The name of the exported file of a book without a title. In Polish: Książka
    return name.isEmpty() ? tr("Book") : name;
}

QString ExportBookDialog::nameProblem() const
{
    const QString name = baseName();
    if (name.isEmpty()) {
        //: Under an empty name of the file. In Polish: Wpisz nazwę pliku.
        return tr("Type the name of the file.");
    }
    const bool forbidden = std::any_of(name.cbegin(), name.cend(), [](QChar character) {
        return forbiddenCharacters().contains(character) || character.unicode() < 32;
    });
    if (forbidden) {
        QStringList characters;
        for (const QChar character : forbiddenCharacters()) {
            characters << QString(character);
        }
        //: %1: the characters, with spaces between them. In Polish: Nazwa nie może zawierać
        //: znaków %1.
        return tr("A name cannot have the characters %1.")
            .arg(characters.join(QLatin1Char(' ')));
    }
    return QString();
}

QString ExportBookDialog::reasonOf(const editor::ExportProblem& problem) const
{
    using Kind = editor::ExportProblem::Kind;
    switch (problem.kind) {
        case Kind::None:
        case Kind::Other:
            break;
        case Kind::CannotWrite:
            //: %1: the folder of the file. In Polish: Kalahari nie może zapisać pliku
            //: w folderze %1. Jeśli plik jest otwarty w innym programie, zamknij go albo wybierz
            //: inny folder.
            return tr("Kalahari cannot write the file in the folder %1. If the file is open in "
                      "another program, close it, or choose another folder.")
                .arg(QDir::toNativeSeparators(m_folder));
        case Kind::DiskFull:
            //: In Polish: Na dysku zabrakło miejsca. Zwolnij miejsce albo wybierz inny folder.
            return tr("There is no room left on the disk. Free some space or choose another "
                      "folder.");
    }
    return QString();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
