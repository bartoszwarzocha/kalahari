/// @file export_book_dialog.cpp
/// @brief File > Export > DOCX and Markdown: the file, the texts of the book and how they look
/// in it, and the progress and the result of the export in one window

#include "kalahari/gui/dialogs/export_book_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/utils/program_folders.h"
#include "kalahari/gui/widgets/length_spin_box.h"

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
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int ID_ROLE = Qt::UserRole;          ///< The element of a row of the list
constexpr int STATUS_ROLE = Qt::UserRole + 1;  ///< The status of a text of the list

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

/// The extension of a format, with its dot
QString extensionOf(editor::ExportFormat format) {
    return format == editor::ExportFormat::Docx ? QStringLiteral(".docx") : QStringLiteral(".md");
}

/// A grey sentence under an option or a list
QLabel* newRemark(QWidget* parent, const QString& text) {
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
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
    //: The tooltip of the gear of the window of the export. In Polish: Ustawienia stron
    //: i marginesów
    addSettingsButton(QString::fromLatin1(SettingsDialog::PAGES_PAGE),
                      tr("Page and Margin Settings"));

    // The file: its format, its folder and its name
    //: A group of the window: the format, the folder and the name of the file. In Polish: Plik
    QGridLayout* file = addGroup(tr("File"));
    auto* formats = new QWidget(file->parentWidget());
    auto* formatsLayout = new QHBoxLayout(formats);
    formatsLayout->setContentsMargins(0, 0, 0, 0);
    //: In Polish: &Dokument Worda (DOCX)
    m_docxButton = new QRadioButton(tr("&Word document (DOCX)"), formats);
    //: In Polish: &Markdown
    m_markdownButton = new QRadioButton(tr("&Markdown"), formats);
    formatsLayout->addWidget(m_docxButton);
    formatsLayout->addWidget(m_markdownButton);
    formatsLayout->addStretch();
    //: In Polish: Format:
    addRow(file, tr("Format:"), formats);
    QPushButton* browseButton = nullptr;
    //: In Polish: Folder:
    addRow(file, tr("Folder:"), newPathField(&m_folderField, &browseButton));
    browseButton->setToolTip(tr("Choose the folder in the system window"));
    m_folderNote = addNote(file);
    m_nameField = new QLineEdit(file->parentWidget());
    //: In Polish: &Nazwa:
    addRow(file, tr("&Name:"), m_nameField);
    m_nameNote = addNote(file);

    // The content: the whole book, the texts chosen in the list, or the text in front
    //: A group of the window: what goes to the file. In Polish: Zawartość
    QGridLayout* content = addGroup(tr("Content"));
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
    int row = 0;
    content->addWidget(m_bookButton, row++, 0, 1, 2);
    content->addWidget(m_chosenButton, row++, 0, 1, 2);
    content->addWidget(m_frontButton, row++, 0, 1, 2);

    // The list of the texts, with their sections and groups
    m_chosenBox = new QWidget(contentBox);
    auto* chosenLayout = new QVBoxLayout(m_chosenBox);
    chosenLayout->setContentsMargins(0, 0, 0, 0);
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
    m_textList->setRows(static_cast<int>(m_book.rows.size()));
    chosenLayout->addWidget(m_textList, 1);
    auto* buttons = new QHBoxLayout();
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
    buttons->addStretch();
    chosenLayout->addLayout(buttons);
    m_chosenNote = newRemark(m_chosenBox, QString());
    chosenLayout->addWidget(m_chosenNote);
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
    m_frontButton->setVisible(!m_book.frontTitle.isEmpty());

    // The layout: the titles as headings, and the pages of a Word document
    //: A group of the window: how the texts look in the file. In Polish: Układ
    QGridLayout* look = addGroup(tr("Layout"));
    m_layoutGroup = qobject_cast<QGroupBox*>(look->parentWidget());
    row = 0;
    //: In Polish: &Tytuły części i rozdziałów z Nawigatora jako nagłówki
    m_titlesBox = new QCheckBox(tr("Navigator titles of parts and chapters as &headings"),
                                m_layoutGroup);
    look->addWidget(m_titlesBox, row++, 0, 1, 2);
    look->addWidget(
        newRemark(m_layoutGroup,
                  //: Under the titles as headings. In Polish: Tekst, który zaczyna się od
                  //: nagłówka, zostaje z własnym. Strona tytułowa, strona redakcyjna, dedykacja
                  //: i motto nie dostają tytułu.
                  tr("A text that starts with a heading keeps its own. The title page, the "
                     "copyright page, the dedication and the motto get no title.")),
        row++, 0, 1, 2);
    m_wordLayout = new QWidget(m_layoutGroup);
    auto* wordLayout = new QGridLayout(m_wordLayout);
    wordLayout->setContentsMargins(0, 0, 0, 0);
    wordLayout->setColumnStretch(1, 1);
    int wordRow = 0;
    //: In Polish: Każd&y tekst od nowej strony
    m_newPageBox = new QCheckBox(tr("Each text from a new &page"), m_wordLayout);
    wordLayout->addWidget(m_newPageBox, wordRow++, 0, 1, 2);
    wordLayout->addWidget(
        newRemark(m_wordLayout,
                  //: Under each text from a new page. In Polish: Przy marginesach lustrzanych
                  //: tekst zaczyna się na prawej stronie, chyba że w jego właściwościach
                  //: ustawiono lewą.
                  tr("With mirror margins a text starts on a right page, unless its "
                     "properties say a left one.")),
        wordRow++, 0, 1, 2);
    //: In Polish: Numery &stron na dole strony
    m_pageNumbersBox = new QCheckBox(tr("Page numbers at the b&ottom of the page"), m_wordLayout);
    wordLayout->addWidget(m_pageNumbersBox, wordRow++, 0, 1, 2);
    look->addWidget(m_wordLayout, row++, 0, 1, 2);

    m_fontBox = new QComboBox(m_layoutGroup);
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
    //: In Polish: Czcionk&a tekstu:
    addRow(look, tr("Font of the te&xt:"), m_fontBox);
    m_pageLabel = new QLabel(m_layoutGroup);
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

    // The annotations of a Word document
    //: A group of the window: the annotations of the text. In Polish: Uwagi
    QGridLayout* notes = addGroup(tr("Annotations"));
    m_notesGroup = qobject_cast<QGroupBox*>(notes->parentWidget());
    //: In Polish: &Uwagi jako komentarze Worda
    m_commentsBox = new QCheckBox(tr("Annotations as Word comm&ents"), m_notesGroup);
    notes->addWidget(m_commentsBox, 0, 0, 1, 2);
    notes->addWidget(
        newRemark(m_notesGroup,
                  //: Under the annotations as comments. In Polish: Komentarze, zadania
                  //: i notatki z tekstu, z autorem i datą. Bez tej opcji plik nie zawiera uwag.
                  tr("The comments, to-dos and notes of the text, with their authors and dates. "
                     "Without this option the file has no annotations.")),
        1, 0, 1, 2);

    // The images of a Markdown file
    //: A group of the window: the images of a Markdown file. In Polish: Obrazy
    QGridLayout* images = addGroup(tr("Images"));
    m_imagesGroup = qobject_cast<QGroupBox*>(images->parentWidget());
    m_copyImagesBox = new QCheckBox(m_imagesGroup);
    images->addWidget(m_copyImagesBox, 0, 0, 1, 2);
    images->addWidget(
        newRemark(m_imagesGroup,
                  //: Under the copy of the images. In Polish: Bez tej opcji odnośniki do obrazów
                  //: wskazują pliki w folderze książki.
                  tr("Without this option the links to the images name the files in the folder "
                     "of the book.")),
        1, 0, 1, 2);
    images->addWidget(
        newRemark(m_imagesGroup,
                  //: In the group of the images of a Markdown file. In Polish: Markdown nie ma
                  //: komentarzy: uwagi zostają tylko w książce.
                  tr("Markdown has no comments: the annotations stay only in the book.")),
        2, 0, 1, 2);
    fieldsLayout()->addStretch();

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
    (format == editor::ExportFormat::Docx ? m_docxButton : m_markdownButton)->setChecked(true);
    m_nameField->setText(suggestedName() + extensionOf(format));

    connect(browseButton, &QPushButton::clicked, this, &ExportBookDialog::browse);
    connect(m_nameField, &QLineEdit::textEdited, this, [this]() { m_nameTyped = true; });
    connect(m_nameField, &QLineEdit::textChanged, this, &ExportBookDialog::dropFailure);
    connect(m_nameField, &QLineEdit::textChanged, this, &ExportBookDialog::updateFields);
    for (QRadioButton* button : {m_docxButton, m_markdownButton}) {
        connect(button, &QRadioButton::toggled, this, [this](bool on) {
            if (on) {
                dropFailure();
                showFormat();
            }
        });
    }
    for (QRadioButton* button : {m_bookButton, m_chosenButton, m_frontButton}) {
        connect(button, &QRadioButton::toggled, this, [this](bool on) {
            if (on) {
                dropFailure();
                showContent();
            }
        });
    }
    for (QCheckBox* box :
         {m_titlesBox, m_newPageBox, m_pageNumbersBox, m_commentsBox, m_copyImagesBox}) {
        connect(box, &QCheckBox::toggled, this, &ExportBookDialog::dropFailure);
    }

    // The page follows the settings the gear opens
    m_pageLabel->setText(pageText());
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key.rfind("editor.page", 0) == 0 || key.rfind("editor.margins", 0) == 0 ||
                key == "ui.lengthUnit") {
                QMetaObject::invokeMethod(this, [this]() { m_pageLabel->setText(pageText()); },
                                          Qt::QueuedConnection);
            }
        });

    setFolder(folder);
    showFormat();
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
    choice.format = m_markdownButton->isChecked() ? editor::ExportFormat::Markdown
                                                  : editor::ExportFormat::Docx;
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
    choice.comments = m_commentsBox->isChecked();
    choice.copyImages = m_copyImagesBox->isChecked();
    choice.imagesFolder = imagesFolderName();
    return choice;
}

void ExportBookDialog::setFormat(editor::ExportFormat format)
{
    (format == editor::ExportFormat::Docx ? m_docxButton : m_markdownButton)->setChecked(true);
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
    return m_chosenBox->isVisibleTo(this) ? m_chosenNote->text() : QString();
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
        finishWork(text, path, true);
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

void ExportBookDialog::showFormat()
{
    const bool word = m_docxButton->isChecked();
    setHeadingIcon(word ? QStringLiteral("file.export.docx") : QStringLiteral("file.export.markdown"));
    // The name keeps what the writer typed, with the extension of the format
    const QString name = m_nameField->text().trimmed();
    const QString base = baseName();
    if (!name.isEmpty() && name != base + extension()) {
        m_nameField->setText(base + extension());
    }
    m_wordLayout->setVisible(word);
    setRowVisible(m_fontBox, word);
    setRowVisible(m_pageLabel, word);
    m_pageNote.widget->setVisible(word);
    m_notesGroup->setVisible(word);
    m_imagesGroup->setVisible(!word);
    showContent();
}

void ExportBookDialog::showContent()
{
    const bool front = m_frontButton->isChecked();
    m_chosenBox->setVisible(m_chosenButton->isChecked());
    const bool word = m_docxButton->isChecked();
    const QString title = front ? m_book.frontTitle : m_book.title;
    if (front) {
        setHeading(tr("Export Book"),
                   word
                       //: %1: the title of the text. In Polish: Zapisuje tekst „%1” jako dokument
                       //: Worda.
                       ? tr("Saves the text \"%1\" as a Word document.").arg(title)
                       //: %1: the title of the text. In Polish: Zapisuje tekst „%1” jako plik
                       //: Markdown.
                       : tr("Saves the text \"%1\" as a Markdown file.").arg(title));
    } else {
        setHeading(tr("Export Book"),
                   word
                       //: %1: the title of the book. In Polish: Zapisuje książkę „%1” jako
                       //: dokument Worda.
                       ? tr("Saves the book \"%1\" as a Word document.").arg(title)
                       //: %1: the title of the book. In Polish: Zapisuje książkę „%1” jako plik
                       //: Markdown.
                       : tr("Saves the book \"%1\" as a Markdown file.").arg(title));
    }
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

    acceptButton()->setEnabled(folderProblem.isEmpty() && nameUsable && contentUsable);
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
    settings.set<bool>("export.comments", now.comments);
    settings.set<bool>("export.copyImages", now.copyImages);
}

QString ExportBookDialog::fileName() const
{
    const QString base = baseName();
    return base.isEmpty() ? QString() : base + extension();
}

QString ExportBookDialog::extension() const
{
    return extensionOf(m_markdownButton->isChecked() ? editor::ExportFormat::Markdown
                                                     : editor::ExportFormat::Docx);
}

QString ExportBookDialog::baseName() const
{
    QString name = m_nameField->text().trimmed();
    for (const editor::ExportFormat format :
         {editor::ExportFormat::Docx, editor::ExportFormat::Markdown}) {
        const QString each = extensionOf(format);
        if (name.endsWith(each, Qt::CaseInsensitive)) {
            name.chop(each.size());
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
