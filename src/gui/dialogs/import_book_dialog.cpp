// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file import_book_dialog.cpp
/// @brief File > Import > DOCX, Markdown and Plain Text: the files, the place in the book, the
/// texts they become and how, and the progress and the result of the import in one window

#include "kalahari/gui/dialogs/import_book_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/extension_points.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/material_downloader.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLayoutItem>
#include <QShowEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <exception>
#include <map>
#include <memory>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int ROW_ROLE = Qt::UserRole;   ///< The index of a row of the list of the texts
constexpr int PATH_ROLE = Qt::UserRole;  ///< The path of a row of the list of the files
/// The id of a format of a plugin in the list of the formats (PluginImportFormat::id)
constexpr int PLUGIN_ROLE = Qt::UserRole + 1;
/// Rows the lists show at most (the others scroll in them): the window with the result of the
/// import fits 1366x768 at 150%
constexpr int FILE_ROWS = 3;
constexpr int TEXT_ROWS = 5;
constexpr int STYLE_ROWS = 5;
/// How much higher than its content asks for the window opens (within the screen): the lists
/// of the files, the place, the texts and the styles take the room
constexpr int EXTRA_HEIGHT = 100;
/// The width of the window in the letters of a text: the remarks in fewer lines
constexpr int WIDTH_IN_LETTERS = 140;
/// The widest the window starts: with its frame it fits a screen of 1366x768 at 150%
constexpr int MAX_START_WIDTH = 880;
/// The most letters of the title of the book in the place of the import
constexpr int TITLE_LETTERS = 40;
/// Lines of the preview of the start of a file
constexpr int PREVIEW_LINES = 2;
/// The fewest and the most empty lines that start a text of plain text
constexpr int MIN_EMPTY_LINES = 2;
constexpr int MAX_EMPTY_LINES = 6;

/// The id of the style of the dialogue in the packages of the program
const QString DIALOGUE_STYLE = QStringLiteral("dialogue");

/// The columns of the list of the texts
enum TextColumn { TITLE_COLUMN = 0, KIND_COLUMN = 1, WORDS_COLUMN = 2 };

/// The formats, as the settings and the list name them
const char* formatKey(ImportFormat format) {
    switch (format) {
        case ImportFormat::Docx:
            return "docx";
        case ImportFormat::Markdown:
            return "markdown";
        case ImportFormat::PlainText:
            return "text";
        case ImportFormat::Plugin:
            break;  // keeps no choices of its own
    }
    return "docx";
}

const char* formattingKey(editor::ImportFormatting formatting) {
    switch (formatting) {
        case editor::ImportFormatting::All:
            return "all";
        case editor::ImportFormatting::Emphasis:
            return "emphasis";
        case editor::ImportFormatting::StylesOnly:
            return "styles";
    }
    return "emphasis";
}

const char* emptyKey(editor::EmptyParagraphs empty) {
    switch (empty) {
        case editor::EmptyParagraphs::Remove:
            return "remove";
        case editor::EmptyParagraphs::Keep:
            return "keep";
        case editor::EmptyParagraphs::SceneBreak:
            return "sceneBreak";
    }
    return "remove";
}

const char* targetKey(ImportBookDialog::Target target) {
    switch (target) {
        case ImportBookDialog::Target::Book:
            return "book";
        case ImportBookDialog::Target::NewBook:
            return "newBook";
        case ImportBookDialog::Target::OtherFiles:
            return "otherFiles";
    }
    return "book";
}

const char* titlePatternKey(editor::TitlePattern pattern) {
    switch (pattern) {
        case editor::TitlePattern::None:
            return "none";
        case editor::TitlePattern::Polish:
            return "polish";
        case editor::TitlePattern::English:
            return "english";
        case editor::TitlePattern::Number:
            return "number";
        case editor::TitlePattern::Capitals:
            return "capitals";
        case editor::TitlePattern::Prefix:
            return "prefix";
    }
    return "polish";
}

const char* partPatternKey(editor::PartPattern pattern) {
    switch (pattern) {
        case editor::PartPattern::None:
            return "none";
        case editor::PartPattern::Polish:
            return "polish";
        case editor::PartPattern::English:
            return "english";
    }
    return "none";
}

/// The index of the item of @p box with data @p data in @p role; the first item when none has
/// it
void chooseData(QComboBox* box, const QVariant& data, int role = Qt::UserRole) {
    const int index = box->findData(data, role);
    box->setCurrentIndex(index >= 0 ? index : 0);
}

/// The name of an encoding in the list
QString encodingName(editor::TextEncoding encoding) {
    switch (encoding) {
        case editor::TextEncoding::Utf8:
            return QStringLiteral("UTF-8");
        case editor::TextEncoding::Utf16:
            return QStringLiteral("UTF-16");
        case editor::TextEncoding::Windows1250:
            //: An encoding of plain text. In Polish: Windows-1250 (środkowoeuropejskie)
            return ImportBookDialog::tr("Windows-1250 (Central European)");
        case editor::TextEncoding::Iso8859_2:
            return QStringLiteral("ISO 8859-2 (Latin-2)");
    }
    return QStringLiteral("UTF-8");
}

/// A row of a field and the button(s) at its right, without margins
QWidget* newLine(QWidget* parent, const QList<QWidget*>& widgets, bool stretch = true) {
    auto* line = new QWidget(parent);
    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(0, 0, 0, 0);
    for (QWidget* widget : widgets) {
        widget->setParent(line);
        layout->addWidget(widget);
    }
    if (stretch) {
        layout->addStretch(1);
    }
    return line;
}

/// The languages a new book can have, with their names in the program's language
QList<std::pair<QString, QString>> languages() {
    return {{QStringLiteral("en"), ImportBookDialog::tr("English")},
            {QStringLiteral("pl"), ImportBookDialog::tr("Polish")},
            {QStringLiteral("de"), ImportBookDialog::tr("German")},
            {QStringLiteral("fr"), ImportBookDialog::tr("French")},
            {QStringLiteral("es"), ImportBookDialog::tr("Spanish")},
            {QStringLiteral("it"), ImportBookDialog::tr("Italian")},
            {QStringLiteral("pt"), ImportBookDialog::tr("Portuguese")},
            {QStringLiteral("ru"), ImportBookDialog::tr("Russian")},
            {QStringLiteral("cs"), ImportBookDialog::tr("Czech")},
            {QStringLiteral("uk"), ImportBookDialog::tr("Ukrainian")}};
}

/// The autocorrect the cleanup of the typography follows: the dashes of Settings > Editor >
/// Auto-correct, and what the writer turned on there
editor::AutoCorrectOptions autoCorrectOfSettings() {
    const auto& settings = core::SettingsManager::getInstance();
    editor::AutoCorrectOptions options;
    options.quotes = settings.get<bool>("editor.autoCorrect.quotes");
    options.apostrophe = settings.get<bool>("editor.autoCorrect.apostrophe");
    options.dialogueDash = settings.get<bool>("editor.autoCorrect.dialogueDash");
    options.dash = settings.get<std::string>("editor.autoCorrect.dialogueDashType") == "enDash"
                       ? editor::DialogueDash::En
                       : editor::DialogueDash::Em;
    options.ellipsis = settings.get<bool>("editor.autoCorrect.ellipsis");
    options.dashBetweenWords = settings.get<bool>("editor.autoCorrect.dashBetweenWords");
    options.wordDash = settings.get<std::string>("editor.autoCorrect.dashBetweenWordsType") ==
                               "emDash"
                           ? editor::DialogueDash::Em
                           : editor::DialogueDash::En;
    options.dialogueDashInDialogue = settings.get<bool>("editor.autoCorrect.dashInDialogue");
    options.doubleHyphen = settings.get<bool>("editor.autoCorrect.doubleHyphen");
    return options;
}

/// The name of a heading level in the lists of the division: "Heading 2 (6)"
QString levelName(int level, int count) {
    //: A level of the headings of the file, %1: its number, %2: how many headings it has. In
    //: Polish: Nagłówek %1 (%2)
    return ImportBookDialog::tr("Heading %1 (%2)").arg(level).arg(count);
}

/// A tab of @p tabs titled @p title: a grid of the labels and the fields
QGridLayout* newTab(QTabWidget* tabs, const QString& title) {
    auto* page = new QWidget(tabs);
    auto* grid = new QGridLayout(page);
    grid->setColumnStretch(1, 1);
    tabs->addTab(page, title);
    return grid;
}

/// The rows of a tab start at its top
void endTab(QGridLayout* grid) {
    grid->setRowStretch(grid->rowCount(), 1);
}

/// The row @p row of a tab takes the height the window has beyond what its rows ask for: the
/// list in it gets as high as the tab; the label of the row stays level with its top
void stretchRow(QGridLayout* grid, int row) {
    grid->setRowStretch(row, 1);
    // A label of its own, not a field across the columns, which would then keep its height
    QLayoutItem* label = grid->itemAtPosition(row, 0);
    if (label != nullptr && label != grid->itemAtPosition(row, 1)) {
        label->setAlignment((label->alignment() & Qt::AlignHorizontal_Mask) | Qt::AlignTop);
    }
}

/// A button of a window that Enter does not press
QPushButton* newButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setAutoDefault(false);
    return button;
}

/// The label of a field
QLabel* newLabel(const QString& text, QWidget* buddy, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setBuddy(buddy);
    return label;
}

/// The share of the progress of an import that the download of the images of Internet
/// addresses takes, in percent; the texts take the rest
constexpr int DOWNLOAD_SHARE = 30;

/// The images of Internet addresses of @p files: each address once, with the file and the
/// image of each of its places
struct WebImages {
    QList<QUrl> addresses;
    std::vector<std::vector<std::pair<size_t, size_t>>> places;
};

WebImages webImagesOf(const std::vector<ImportFile>& files) {
    WebImages web;
    for (size_t file = 0; file < files.size(); ++file) {
        if (!files[file].isRead()) {
            continue;
        }
        const std::vector<editor::ImportImage>& images = files[file].document->images;
        for (size_t image = 0; image < images.size(); ++image) {
            if (!images[image].web) {
                continue;
            }
            const QUrl url(images[image].address);
            qsizetype index = web.addresses.indexOf(url);
            if (index < 0) {
                index = web.addresses.size();
                web.addresses << url;
                web.places.emplace_back();
            }
            web.places[static_cast<size_t>(index)].emplace_back(file, image);
        }
    }
    return web;
}

/// Download the images of Internet addresses of @p files into copies of their documents, each
/// address once and all of them at a time, with @p progress from 0 to 100
/// @return The addresses that did not come, each with why, as the result of the import lists
///         them
QStringList downloadWebImages(std::vector<ImportFile>& files,
                              const ArchiveDialog::Progress& progress) {
    const WebImages web = webImagesOf(files);
    const auto total = static_cast<int>(web.addresses.size());
    if (total == 0) {
        return {};
    }
    std::vector<std::shared_ptr<editor::ImportDocument>> copies(files.size());
    std::vector<QString> failures(web.addresses.size());
    MaterialDownloader downloader;
    QEventLoop loop;
    int done = 0;
    QObject::connect(
        &downloader, &MaterialDownloader::finished, &loop,
        [&files, &web, &copies, &failures, &done, &loop, &progress,
         total](const MaterialDownloader::Result& result) {
            const auto index = static_cast<size_t>(result.elementId.toInt());
            if (result.error.isEmpty()) {
                for (const auto& [file, image] : web.places[index]) {
                    if (!copies[file]) {
                        copies[file] =
                            std::make_shared<editor::ImportDocument>(*files[file].document);
                    }
                    editor::ImportImage& downloaded = copies[file]->images[image];
                    downloaded.data = result.data;
                    downloaded.fileName = result.fileName;
                    downloaded.web = false;
                }
            } else {
                failures[index] = result.error;
            }
            ++done;
            if (done == total) {
                loop.quit();
            }
            progress(done * 100 / total);
        });
    for (int index = 0; index < total; ++index) {
        downloader.fetchPicture(QString::number(index), web.addresses[index]);
    }
    // The window waits, as for the progress of the texts: reject() waits for the work
    if (done < total) {
        loop.exec();
    }
    for (size_t file = 0; file < files.size(); ++file) {
        if (copies[file]) {
            files[file].document = copies[file];
        }
    }
    QStringList lines;
    for (size_t index = 0; index < failures.size(); ++index) {
        if (!failures[index].isEmpty()) {
            //: A line of the images that could not be downloaded; %1: the address, %2: why.
            //: In Polish: – %1: %2
            const QUrl& address = web.addresses[static_cast<qsizetype>(index)];
            lines << ImportBookDialog::tr("– %1: %2").arg(address.toString(), failures[index]);
        }
    }
    return lines;
}

}  // namespace

ImportBookDialog::ImportBookDialog(const Book& book, const core::BookTypeRegistry& registry,
                                   ImportFormat format, const QStringList& files, Actions actions,
                                   QWidget* parent)
    : ImportBookDialog(book, registry, format, QString(), files, std::move(actions), parent) {}

ImportBookDialog::ImportBookDialog(const Book& book, const core::BookTypeRegistry& registry,
                                   const QString& plugin, const QStringList& files,
                                   Actions actions, QWidget* parent)
    : ImportBookDialog(book, registry, ImportFormat::Plugin, plugin, files, std::move(actions),
                       parent) {}

ImportBookDialog::ImportBookDialog(const Book& book, const core::BookTypeRegistry& registry,
                                   ImportFormat format, const QString& plugin,
                                   const QStringList& files, Actions actions, QWidget* parent)
    : ArchiveDialog(parent),
      m_book(book),
      m_registry(registry),
      m_actions(std::move(actions)),
      m_types(bookTypes(registry)) {
    m_filling = true;
    //: The title of the window of File > Import > DOCX, Markdown and Plain Text. In Polish:
    //: Importuj tekst
    setHeading(tr("Import Text"));
    setCompactHeading(true);
    setMinimumWidth(
        std::max(minimumWidth(),
                 std::min(widthOfLetters(fontMetrics(), WIDTH_IN_LETTERS), MAX_START_WIDTH)));
    //: The tooltip of the gear of the window of the import. In Polish: Ustawienia importu
    addSettingsButton(QString::fromLatin1(SettingsDialog::IMPORT_PAGE), tr("Import Settings"));

    m_tabs = new FittingTabs(this);
    fieldsLayout()->addWidget(m_tabs);
    buildFileTab();
    buildPlaceTab();
    buildDivisionTab();
    buildStylesTab();
    buildCleanupTab();
    buildExtrasTab();

    // The result: the texts in the Navigator, the first one open, and Undo Import
    //: A link under the result of the import. In Polish: Pokaż w Nawigatorze
    m_showInNavigatorLink = addResultLink(tr("Show in Navigator"),
                                          tr("Show the imported texts in the Navigator"));
    //: A link under the result of the import. In Polish: Otwórz pierwszy tekst
    m_openTextLink = addResultLink(tr("Open First Text"), tr("Open the first imported text"));
    //: A link under the result of the import. In Polish: Cofnij import
    m_undoLink = addResultLink(
        tr("Undo Import"),
        //: The tooltip of Undo Import. In Polish: Usuń z książki
        //: wszystko, co import dodał, także nowe style
        tr("Take out of the book everything the import added, also the new styles"));

    //: In Polish: Postęp i wynik pokażą się tutaj.
    setHint(tr("The progress and the result will show here."));
    //: The button that imports the texts. In Polish: Importuj
    setAcceptText(tr("Import"));

    // The book: the open one, else a new one
    const bool canChangeBook = !m_book.title.isEmpty() && !m_book.readOnly;
    m_bookButton->setVisible(canChangeBook);
    m_otherFilesButton->setVisible(canChangeBook);
    m_newBookButton->setVisible(canChangeBook);
    m_noBookNote->setVisible(!canChangeBook);
    if (m_book.readOnly && !m_book.title.isEmpty()) {
        //: In the place of the import into a book open read-only. In Polish: Otwarta książka
        //: jest tylko do odczytu: import tworzy nową książkę.
        m_noBookNote->setText(tr("The open book is read-only: the import makes a new book."));
    }
    m_picker->setBook(m_book.sections, m_registry);
    const auto& settings = core::SettingsManager::getInstance();
    m_authorField->setText(
        QString::fromStdString(settings.get<std::string>("project.defaultAuthor")));
    chooseData(m_typeBox, QStringLiteral("kalahari.novel"));
    m_newBookFolder = ProgramFolders::startFolder(ProgramFolders::Operation::NewBook);
    showPath(m_folderField, m_newBookFolder);
    Target target = Target::Book;
    const std::string lastTarget = settings.get<std::string>("import.target");
    if (lastTarget == targetKey(Target::NewBook)) {
        target = Target::NewBook;
    } else if (lastTarget == targetKey(Target::OtherFiles)) {
        target = Target::OtherFiles;
    }
    if (!canChangeBook) {
        target = Target::NewBook;
    }
    (target == Target::Book      ? m_bookButton
     : target == Target::NewBook ? m_newBookButton
                                 : m_otherFilesButton)
        ->setChecked(true);
    chooseData(m_formatBox, static_cast<int>(format));
    if (format == ImportFormat::Plugin) {
        chooseData(m_formatBox, plugin, PLUGIN_ROLE);
    }
    m_filling = false;

    connectFields();
    m_filling = true;
    loadChoices();
    showTarget();
    m_filling = false;
    showPluginPage();  // before the plugin reads the files with its options
    setFiles(files);
    showFormat();
    m_tabs->setCurrentIndex(0);
    // Without files the window opens at Add...: Enter chooses them (test of 24.11)
    if (m_files.empty()) {
        m_addButton->setFocus();
    }
}

void ImportBookDialog::buildFileTab() {
    // The files: their format, the list, the encoding and what is in them
    //: A tab of the window of the import: the format and the files. In Polish: &Plik
    QGridLayout* file = newTab(m_tabs, tr("&File"));
    QWidget* filePage = file->parentWidget();
    m_formatBox = new QComboBox(filePage);
    //: A format of the window of the import. In Polish: Dokument Worda (.docx)
    m_formatBox->addItem(tr("Word document (.docx)"), static_cast<int>(ImportFormat::Docx));
    //: A format of the window of the import. In Polish: Markdown (.md)
    m_formatBox->addItem(tr("Markdown (.md)"), static_cast<int>(ImportFormat::Markdown));
    //: A format of the window of the import. In Polish: Zwykły tekst (.txt)
    m_formatBox->addItem(tr("Plain text (.txt)"), static_cast<int>(ImportFormat::PlainText));
    for (const PluginImportFormat& each : pluginImportFormats()) {
        //: A format of a plugin in the list of the formats; %1: its name, %2: the extensions of
        //: its files. In Polish: %1 (%2) – wtyczka
        m_formatBox->addItem(tr("%1 (%2) – plugin")
                                 .arg(each.name, each.extensions.join(QStringLiteral(", "))),
                             static_cast<int>(ImportFormat::Plugin));
        m_formatBox->setItemData(m_formatBox->count() - 1, each.id, PLUGIN_ROLE);
    }
    m_formatBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In Polish: F&ormat:
    addRow(file, tr("F&ormat:"), newLine(filePage, {m_formatBox}))->setBuddy(m_formatBox);

    auto* files = new QWidget(filePage);
    auto* filesLayout = new QGridLayout(files);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->setColumnStretch(0, 1);
    m_fileList = new RowList(files);
    m_fileList->setColumnCount(2);
    m_fileList->setHeaderHidden(true);
    m_fileList->setRootIsDecorated(false);
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileList->header()->setStretchLastSection(false);
    m_fileList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    //: The name of the list of the files for screen readers. In Polish: Pliki importu
    m_fileList->setAccessibleName(tr("Files of the import"));
    m_fileList->setRows(FILE_ROWS);
    filesLayout->addWidget(m_fileList, 0, 0, 3, 1);
    filesLayout->setRowStretch(2, 1);
    auto* fileButtons = new QGridLayout();
    //: Adds files to the list of the import. In Polish: Dod&aj…
    m_addButton = newButton(tr("&Add..."), files);
    //: Moves the file up in the list. In Polish: W &górę
    m_upButton = newButton(tr("Move &Up"), files);
    //: Moves the file down in the list. In Polish: &W dół
    m_downButton = newButton(tr("Move Do&wn"), files);
    //: Takes the file out of the list of the import. In Polish: &Usuń z listy
    m_removeButton = newButton(tr("&Remove from List"), files);
    fileButtons->addWidget(m_addButton, 0, 0);
    fileButtons->addWidget(m_removeButton, 0, 1);
    fileButtons->addWidget(m_upButton, 1, 0);
    fileButtons->addWidget(m_downButton, 1, 1);
    filesLayout->addLayout(fileButtons, 0, 1);
    filesLayout->addWidget(
        newRemark(files,
                  //: Beside the list of the files. In Polish: Kolejność plików to kolejność
                  //: tekstów w książce, z liczbami po kolei („Rozdział 2” przed „Rozdział 10”).
                  tr("The order of the files is the order of the texts in the book, the numbers in "
                     "order (\"Chapter 2\" before \"Chapter 10\").")),
        1, 1);
    //: In Polish: Pl&iki:
    addRow(file, tr("F&iles:"), files)->setBuddy(m_fileList);
    const int filesRow = file->rowCount() - 1;
    m_filesNote = addNote(file);

    m_encodingBox = new QComboBox(filePage);
    m_encodingBox->addItem(QString(), -1);  // detected: its text follows the file
    for (const editor::TextEncoding encoding :
         {editor::TextEncoding::Utf8, editor::TextEncoding::Utf16,
          editor::TextEncoding::Windows1250, editor::TextEncoding::Iso8859_2}) {
        m_encodingBox->addItem(encodingName(encoding), static_cast<int>(encoding));
    }
    m_encodingBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In Polish: &Kodowanie:
    addRow(file, tr("E&ncoding:"), newLine(filePage, {m_encodingBox}))->setBuddy(m_encodingBox);
    m_previewLabel = newRemark(filePage, QString());
    m_previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    file->addWidget(m_previewLabel, file->rowCount(), 1);
    m_contentsLabel = new QLabel(filePage);
    m_contentsLabel->setTextFormat(Qt::PlainText);
    m_contentsLabel->setWordWrap(true);
    m_contentsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    //: In Polish: W pliku:
    addRow(file, tr("In the file:"), m_contentsLabel);
    stretchRow(file, filesRow);
    connect(m_addButton, &QPushButton::clicked, this, &ImportBookDialog::browseFiles);
}

void ImportBookDialog::buildPlaceTab() {
    // The place: the open book, a new book, or Other Files
    //: A tab of the window of the import: where the texts go. In Polish: &Miejsce
    QGridLayout* place = newTab(m_tabs, tr("&Place"));
    QWidget* placePage = place->parentWidget();
    // A long title in its first letters, so the window fits a small screen
    const QString title = fontMetrics().elidedText(m_book.title, Qt::ElideRight,
                                                   widthOfLetters(fontMetrics(), TITLE_LETTERS));
    //: %1: the title of the open book. In Polish: Do &książki „%1”
    m_bookButton = new QRadioButton(tr("To the &book \"%1\"").arg(literal(title)), placePage);
    if (title != m_book.title) {
        m_bookButton->setToolTip(m_book.title);
    }
    //: In Polish: Do &nowej książki
    m_newBookButton = new QRadioButton(tr("To a &new book"), placePage);
    //: In Polish: Do grupy „&Inne pliki”
    m_otherFilesButton = new QRadioButton(tr("To the group \"&Other Files\""), placePage);
    auto* targets = new QHBoxLayout();
    targets->setSpacing(fontMetrics().averageCharWidth() * 3);
    targets->addWidget(m_bookButton);
    targets->addWidget(m_newBookButton);
    targets->addWidget(m_otherFilesButton);
    targets->addStretch(1);
    place->addLayout(targets, 0, 0, 1, 2);
    m_noBookNote = newRemark(placePage,
                             //: In the place of the import without a book. In Polish: Żadna
                             //: książka nie jest otwarta: import tworzy nową książkę.
                             tr("No book is open: the import makes a new book."));
    place->addWidget(m_noBookNote, 1, 0, 1, 2);

    m_targetPages = new QStackedWidget(placePage);
    m_picker = new ElementPlacePicker(m_targetPages);
    m_picker->setLabel(QString());
    m_picker->setListRows(ElementPlacePicker::MINIMUM_LIST_ROWS);
    //: Under the list of the place. In Polish: Teksty pójdą tu jeden po drugim; te o rodzajach
    //: z własnym miejscem pójdą tam same: prolog na początek tekstu głównego, podziękowania do
    //: stron końcowych.
    m_picker->setNote(
        tr("The texts go here one after another; those of kinds with places of their own go "
           "there: a prologue to the start of the main text, acknowledgments to the back "
           "matter."));
    m_targetPages->addWidget(m_picker);

    m_newBookPage = new QWidget(m_targetPages);
    auto* newBook = new QGridLayout(m_newBookPage);
    newBook->setContentsMargins(0, 0, 0, 0);
    newBook->setColumnStretch(1, 1);
    newBook->setColumnStretch(3, 1);
    m_titleField = new QLineEdit(m_newBookPage);
    m_authorField = new QLineEdit(m_newBookPage);
    m_languageBox = new QComboBox(m_newBookPage);
    for (const auto& [code, name] : languages()) {
        m_languageBox->addItem(name, code);
    }
    m_typeBox = new QComboBox(m_newBookPage);
    for (const BookType& type : std::as_const(m_types)) {
        m_typeBox->addItem(type.name, type.id);
    }
    QPushButton* folderButton = nullptr;
    QWidget* folderField = newPathField(&m_folderField, &folderButton);
    folderField->setParent(m_newBookPage);
    folderButton->setToolTip(tr("Choose the folder in the system window"));
    //: In Polish: &Tytuł:
    newBook->addWidget(newLabel(tr("&Title:"), m_titleField, m_newBookPage), 0, 0);
    newBook->addWidget(m_titleField, 0, 1);
    //: In Polish: A&utor:
    newBook->addWidget(newLabel(tr("A&uthor:"), m_authorField, m_newBookPage), 0, 2);
    newBook->addWidget(m_authorField, 0, 3);
    //: In Polish: &Język:
    newBook->addWidget(newLabel(tr("&Language:"), m_languageBox, m_newBookPage), 1, 0);
    newBook->addWidget(m_languageBox, 1, 1);
    //: In Polish: T&yp książki:
    newBook->addWidget(newLabel(tr("Book t&ype:"), m_typeBox, m_newBookPage), 1, 2);
    newBook->addWidget(m_typeBox, 1, 3);
    //: In Polish: Folder:
    newBook->addWidget(newLabel(tr("Folder:"), m_folderField, m_newBookPage), 2, 0);
    newBook->addWidget(folderField, 2, 1, 1, 3);
    m_newBookNote.widget = newNote(m_newBookPage, &m_newBookNote.icon, &m_newBookNote.text);
    m_newBookNote.widget->hide();
    newBook->addWidget(m_newBookNote.widget, 3, 1, 1, 3);
    newBook->setRowStretch(4, 1);
    m_targetPages->addWidget(m_newBookPage);

    m_otherFilesNote = newRemark(
        m_targetPages,
        //: The place Other Files. In Polish: Każdy tekst z pliku stanie
        //: się osobnym plikiem w grupie „Inne pliki” w Nawigatorze: jest
        //: w książce, ale poza jej treścią, tak jak notatki albo tekst
        //: wklejony skądinąd. Tytuły części nie przechodzą.
        tr("Each text of the file becomes a file of its own in the group \"Other "
           "Files\" of the Navigator: in the book, but outside its text, as notes or "
           "a text pasted from elsewhere. The titles of the parts do not go."));
    m_otherFilesNote->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_targetPages->addWidget(m_otherFilesNote);
    place->addWidget(m_targetPages, 2, 0, 1, 2);
    stretchRow(place, 2);
    connect(folderButton, &QPushButton::clicked, this, &ImportBookDialog::browseFolder);
}

void ImportBookDialog::buildDivisionTab() {
    // The division: where the texts start, and the list of the texts
    //: A tab of the window of the import: the texts the file becomes. In Polish: Po&dział
    QGridLayout* split = newTab(m_tabs, tr("&Division"));
    QWidget* splitPage = split->parentWidget();
    m_headingsButton = new QRadioButton(splitPage);
    m_pagesButton = new QRadioButton(splitPage);
    //: In Polish: Cały plik jako &jeden tekst
    m_wholeButton = new QRadioButton(tr("The whole file as &one text"), splitPage);
    auto* modes = new QHBoxLayout();
    modes->setSpacing(fontMetrics().averageCharWidth() * 3);
    modes->addWidget(m_headingsButton);
    modes->addWidget(m_pagesButton);
    modes->addWidget(m_wholeButton);
    modes->addStretch(1);
    split->addLayout(modes, 0, 0, 1, 2);
    // A format of a plugin: in the place of the division
    m_pluginNote = newRemark(
        splitPage,
        //: The division of a format of a plugin. In Polish: Teksty i ich tytuły daje wtyczka;
        //: rodzaj każdego tekstu zmienisz na liście.
        tr("The plugin gives the texts and their titles; change the kind of each text in the "
           "list."));
    m_pluginNote->hide();
    split->addWidget(m_pluginNote, 0, 0, 1, 2);

    // The levels of the headings (DOCX, Markdown)
    m_levels = new QWidget(splitPage);
    auto* levels = new QHBoxLayout(m_levels);
    levels->setContentsMargins(0, 0, 0, 0);
    m_partLevelBox = new QComboBox(m_levels);
    m_textLevelBox = new QComboBox(m_levels);
    //: In Polish: &Część zaczyna:
    levels->addWidget(newLabel(tr("A pa&rt starts at:"), m_partLevelBox, m_levels));
    levels->addWidget(m_partLevelBox);
    levels->addSpacing(fontMetrics().averageCharWidth() * 3);
    //: In Polish: &Tekst zaczyna:
    levels->addWidget(newLabel(tr("A te&xt starts at:"), m_textLevelBox, m_levels));
    levels->addWidget(m_textLevelBox);
    levels->addStretch(1);
    split->addWidget(m_levels, 1, 0, 1, 2);

    // The titles of plain text
    m_patterns = new QWidget(splitPage);
    auto* patterns = new QHBoxLayout(m_patterns);
    patterns->setContentsMargins(0, 0, 0, 0);
    m_titlePatternBox = new QComboBox(m_patterns);
    //: A pattern of the titles of plain text. In Polish: Rozdział 1, I, pierwszy
    m_titlePatternBox->addItem(tr("Rozdział 1, I, pierwszy"),
                               static_cast<int>(editor::TitlePattern::Polish));
    //: A pattern of the titles of plain text. In Polish: Chapter 1, I, One
    m_titlePatternBox->addItem(tr("Chapter 1, I, One"),
                               static_cast<int>(editor::TitlePattern::English));
    //: A pattern of the titles of plain text. In Polish: Sam numer: 1, 1., I, I.
    m_titlePatternBox->addItem(tr("A number alone: 1, 1., I, I."),
                               static_cast<int>(editor::TitlePattern::Number));
    //: A pattern of the titles of plain text. In Polish: Wiersz wielkimi literami
    m_titlePatternBox->addItem(tr("A line in capitals"),
                               static_cast<int>(editor::TitlePattern::Capitals));
    //: A pattern of the titles of plain text. In Polish: Wiersz zaczynający się od…
    m_titlePatternBox->addItem(tr("A line that starts with..."),
                               static_cast<int>(editor::TitlePattern::Prefix));
    m_prefixField = new QLineEdit(m_patterns);
    m_prefixField->setMaximumWidth(widthOfLetters(fontMetrics(), 16));
    //: The start of the lines of the titles. In Polish: Początek wiersza tytułu
    m_prefixField->setAccessibleName(tr("Start of the title lines"));
    m_partPatternBox = new QComboBox(m_patterns);
    //: A pattern of the titles of the parts of plain text. In Polish: Część pierwsza, I
    m_partPatternBox->addItem(tr("Część pierwsza, I"),
                              static_cast<int>(editor::PartPattern::Polish));
    //: A pattern of the titles of the parts of plain text. In Polish: Part One, I
    m_partPatternBox->addItem(tr("Part One, I"), static_cast<int>(editor::PartPattern::English));
    //: No parts in plain text. In Polish: – bez części –
    m_partPatternBox->addItem(tr("– no parts –"), static_cast<int>(editor::PartPattern::None));
    //: In Polish: Tytuł te&kstu:
    patterns->addWidget(newLabel(tr("Text t&itle:"), m_titlePatternBox, m_patterns));
    patterns->addWidget(m_titlePatternBox);
    patterns->addWidget(m_prefixField);
    patterns->addSpacing(fontMetrics().averageCharWidth() * 3);
    //: In Polish: &Część:
    patterns->addWidget(newLabel(tr("Pa&rt:"), m_partPatternBox, m_patterns));
    patterns->addWidget(m_partPatternBox);
    patterns->addStretch(1);
    split->addWidget(m_patterns, 2, 0, 1, 2);

    m_emptyLines = new QWidget(splitPage);
    auto* emptyLines = new QHBoxLayout(m_emptyLines);
    emptyLines->setContentsMargins(0, 0, 0, 0);
    m_emptyLinesBox = new QSpinBox(m_emptyLines);
    m_emptyLinesBox->setRange(MIN_EMPTY_LINES, MAX_EMPTY_LINES);
    //: Before the number of empty lines. In Polish: Nowy tekst po co najmn&iej
    emptyLines->addWidget(newLabel(tr("A ne&w text after at least"), m_emptyLinesBox, m_emptyLines));
    emptyLines->addWidget(m_emptyLinesBox);
    //: After the number of empty lines. In Polish: pustych wierszach
    emptyLines->addWidget(new QLabel(tr("empty lines"), m_emptyLines));
    emptyLines->addStretch(1);
    split->addWidget(m_emptyLines, 3, 0, 1, 2);

    //: In Polish: Każdy p&lik jako jeden tekst
    m_wholeFilesBox = new QCheckBox(tr("Each fi&le as one text"), splitPage);
    //: In Polish: N&agłówek staje się tytułem w Nawigatorze i nie zostaje w tekście
    m_headingAsTitleBox = new QCheckBox(
        tr("The heading beco&mes the title in the Navigator and leaves the text"), splitPage);
    auto* boxes = new QHBoxLayout();
    boxes->setSpacing(fontMetrics().averageCharWidth() * 3);
    boxes->addWidget(m_wholeFilesBox);
    boxes->addWidget(m_headingAsTitleBox);
    boxes->addStretch(1);
    split->addLayout(boxes, 4, 0, 1, 2);

    auto* texts = new QWidget(splitPage);
    auto* textsLayout = new QGridLayout(texts);
    textsLayout->setContentsMargins(0, 0, 0, 0);
    textsLayout->setColumnStretch(0, 1);
    m_textList = new RowList(texts);
    m_textList->setColumnCount(3);
    //: The columns of the list of the texts. In Polish: Tytuł
    m_textList->setHeaderLabels({tr("Title"),
                                 //: In Polish: Rodzaj
                                 tr("Kind"),
                                 //: In Polish: Słowa
                                 tr("Words")});
    m_textList->header()->setStretchLastSection(false);
    m_textList->header()->setSectionResizeMode(TITLE_COLUMN, QHeaderView::Stretch);
    m_textList->header()->setSectionResizeMode(KIND_COLUMN, QHeaderView::ResizeToContents);
    m_textList->header()->setSectionResizeMode(WORDS_COLUMN, QHeaderView::ResizeToContents);
    m_textList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_textList->setRootIsDecorated(true);
    m_textList->setItemsExpandable(true);
    m_textList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //: The name of the list of the texts for screen readers. In Polish: Teksty importu
    m_textList->setAccessibleName(tr("Texts of the import"));
    m_textList->setRows(TEXT_ROWS);
    m_textList->installEventFilter(this);
    textsLayout->addWidget(m_textList, 0, 0);
    textsLayout->setRowStretch(0, 1);
    auto* textButtons = new QVBoxLayout();
    //: Checks every text of the list. In Polish: Zaznacz wszystki&e
    QPushButton* allButton = newButton(tr("Check &All"), texts);
    //: Unchecks every text of the list. In Polish: &Odznacz wszystkie
    QPushButton* noneButton = newButton(tr("&Uncheck All"), texts);
    //: Changes the title of the text chosen in the list (F2). In Polish: Zmień tyt&uł
    m_renameButton = newButton(tr("Re&name"), texts);
    m_kindBox = new QComboBox(texts);
    //: The kind of the text chosen in the list (F4). In Polish: Rodzaj tekstu
    m_kindBox->setAccessibleName(tr("Kind of the text"));
    m_kindBox->setToolTip(tr("The kind of the text chosen in the list (F4)"));
    for (QWidget* widget :
         {static_cast<QWidget*>(allButton), static_cast<QWidget*>(noneButton),
          static_cast<QWidget*>(m_renameButton), static_cast<QWidget*>(m_kindBox)}) {
        textButtons->addWidget(widget);
    }
    textButtons->addStretch(1);
    textsLayout->addLayout(textButtons, 0, 1);
    m_textsNote = newRemark(texts, QString());
    textsLayout->addWidget(m_textsNote, 1, 0, 1, 2);
    m_fullNote.widget = newNote(texts, &m_fullNote.icon, &m_fullNote.text);
    m_fullNote.widget->hide();
    textsLayout->addWidget(m_fullNote.widget, 2, 0, 1, 2);
    split->addWidget(texts, 5, 0, 1, 2);
    stretchRow(split, 5);
    connect(allButton, &QPushButton::clicked, this, &ImportBookDialog::checkAll);
    connect(noneButton, &QPushButton::clicked, this, &ImportBookDialog::checkNone);
}

void ImportBookDialog::buildStylesTab() {
    // The styles: the formatting of the file and the book's style of each of its styles
    //: A tab of the window of the import: the styles. In Polish: &Style
    QGridLayout* styles = newTab(m_tabs, tr("&Styles"));
    QWidget* stylesPage = styles->parentWidget();
    m_stylesTab = stylesPage;
    m_fileStyles = new QWidget(stylesPage);
    auto* fileStyles = new QGridLayout(m_fileStyles);
    fileStyles->setContentsMargins(0, 0, 0, 0);
    fileStyles->setColumnStretch(1, 1);
    m_formattingBox = new QComboBox(m_fileStyles);
    //: How much of the formatting of the file the texts keep. In Polish: Całe formatowanie
    m_formattingBox->addItem(tr("All the formatting"),
                             static_cast<int>(editor::ImportFormatting::All));
    //: In Polish: Wyróżnienia i wyrównanie
    m_formattingBox->addItem(tr("Emphasis and alignment"),
                             static_cast<int>(editor::ImportFormatting::Emphasis));
    //: In Polish: Tylko style
    m_formattingBox->addItem(tr("Only the styles"),
                             static_cast<int>(editor::ImportFormatting::StylesOnly));
    m_formattingBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In Polish: &Formatowanie z pliku:
    fileStyles->addWidget(newLine(m_fileStyles, {newLabel(tr("Fo&rmatting of the file:"),
                                                          m_formattingBox, m_fileStyles),
                                                 m_formattingBox}),
                          0, 0, 1, 2);
    m_styleList = new RowList(m_fileStyles);
    m_styleList->setColumnCount(2);
    //: The columns of the list of the styles. In Polish: Styl w pliku
    m_styleList->setHeaderLabels({tr("Style of the file"),
                                  //: In Polish: Styl w książce
                                  tr("Style of the book")});
    m_styleList->setRootIsDecorated(false);
    m_styleList->setSelectionMode(QAbstractItemView::NoSelection);
    m_styleList->setFocusPolicy(Qt::NoFocus);
    m_styleList->header()->setStretchLastSection(true);
    m_styleList->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    //: The name of the list of the styles for screen readers. In Polish: Style pliku
    m_styleList->setAccessibleName(tr("Styles of the file"));
    m_styleList->setRows(STYLE_ROWS);
    fileStyles->addWidget(m_styleList, 1, 0, 1, 2);
    fileStyles->setRowStretch(1, 1);
    styles->addWidget(m_fileStyles, 0, 0, 1, 2);
    m_stylesLayout = styles;

    m_textParagraphs = new QWidget(stylesPage);
    auto* textParagraphs = new QGridLayout(m_textParagraphs);
    textParagraphs->setContentsMargins(0, 0, 0, 0);
    textParagraphs->setColumnStretch(1, 1);
    //: In Polish: Każdy &wiersz to akapit
    m_linesButton = new QRadioButton(tr("Each &line is a paragraph"), m_textParagraphs);
    //: In Polish: Akapity &oddziela pusty wiersz
    m_emptyLineButton = new QRadioButton(tr("An &empty line separates the paragraphs"),
                                         m_textParagraphs);
    auto* paragraphs = new QHBoxLayout();
    paragraphs->setSpacing(fontMetrics().averageCharWidth() * 3);
    paragraphs->addWidget(m_linesButton);
    paragraphs->addWidget(m_emptyLineButton);
    paragraphs->addStretch(1);
    textParagraphs->addLayout(paragraphs, 0, 0, 1, 2);
    m_paragraphsNote = newRemark(m_textParagraphs, QString());
    textParagraphs->addWidget(m_paragraphsNote, 1, 0, 1, 2);
    m_textStyleBox = new QComboBox(m_textParagraphs);
    //: In Polish: St&yl akapitów:
    textParagraphs->addWidget(newLabel(tr("Paragraph st&yle:"), m_textStyleBox, m_textParagraphs),
                              2, 0);
    textParagraphs->addWidget(newLine(m_textParagraphs, {m_textStyleBox}), 2, 1);
    styles->addWidget(m_textParagraphs, 1, 0, 1, 2);
    //: In Polish: Akapity zaczynające się od pauzy albo półpauzy w stylu Dia&log
    m_dialogueBox = new QCheckBox(tr("Paragraphs that start &with a dash in the style Dialogue"),
                                  stylesPage);
    styles->addWidget(m_dialogueBox, 2, 0, 1, 2);
    endTab(styles);
    stretchStyles();
}

void ImportBookDialog::stretchStyles() {
    // The list of the styles takes the height of the tab; plain text, without it, has its
    // paragraphs at the top
    const bool list = !m_fileStyles->isHidden();
    m_stylesLayout->setRowStretch(0, list ? 1 : 0);
    m_stylesLayout->setRowStretch(m_stylesLayout->rowCount() - 1, list ? 0 : 1);
}

void ImportBookDialog::showEvent(QShowEvent* event) {
    ArchiveDialog::showEvent(event);
    if (event->spontaneous() || m_grown) {
        return;
    }
    // Higher than the content asks for, within the screen: the lists show more rows
    m_grown = true;
    resizeOnScreen(QSize(width(), height() + EXTRA_HEIGHT));
}

void ImportBookDialog::buildCleanupTab() {
    // The cleanups of the text
    //: A tab of the window of the import: the cleanups of the text. In Polish: Por&ządki
    QGridLayout* cleanup = newTab(m_tabs, tr("&Cleanup"));
    QWidget* cleanupPage = cleanup->parentWidget();
    m_cleanupTab = cleanupPage;
    //: In Polish: Cudzysłowy, apostrofy, półpauzy i wielokropki jak w &autokorekcie
    m_typographyBox = new QCheckBox(
        tr("Quotation marks, apostrophes, dashes and ellipses as in &auto-correct"), cleanupPage);
    cleanup->addWidget(
        withRemark(m_typographyBox,
                   newRemark(cleanupPage,
                             //: Under the typography. In Polish: Według języka
                             //: książki i rodzaju pauzy dialogowej z Ustawień ›
                             //: Edytor › Autokorekta.
                             tr("By the language of the book and the dialogue dash of Settings › "
                                "Editor › Auto-correct."))),
        0, 0, 1, 2);
    //: In Polish: Podwójne spacje jako &jedna
    m_doubleSpacesBox = new QCheckBox(tr("Double spaces as &one"), cleanupPage);
    cleanup->addWidget(m_doubleSpacesBox, 1, 0, 1, 2);
    //: In Polish: Bez spacji i tabulatorów na początku a&kapitu
    m_leadingSpacesBox = new QCheckBox(tr("No spaces and tabs at the s&tart of a paragraph"),
                                       cleanupPage);
    cleanup->addWidget(
        withRemark(m_leadingSpacesBox,
                   newRemark(cleanupPage,
                             //: Under the spaces at the start. In Polish: Wcięcie
                             //: pierwszego wiersza daje styl akapitu.
                             tr("The style of the paragraph gives the indent of its first line."))),
        2, 0, 1, 2);
    //: In Polish: Akapity „* * *”, „***”, „#”, „⁂” i „❦” jako podział sce&ny
    m_sceneMarkersBox = new QCheckBox(
        tr("Paragraphs \"* * *\", \"***\", \"#\", \"⁂\" and \"❦\" as scene brea&ks"), cleanupPage);
    cleanup->addWidget(m_sceneMarkersBox, 3, 0, 1, 2);
    m_emptyBox = new QComboBox(cleanupPage);
    //: What the empty paragraphs become. In Polish: usuń
    m_emptyBox->addItem(tr("remove"), static_cast<int>(editor::EmptyParagraphs::Remove));
    //: In Polish: zostaw
    m_emptyBox->addItem(tr("keep"), static_cast<int>(editor::EmptyParagraphs::Keep));
    //: In Polish: jako podział sceny
    m_emptyBox->addItem(tr("as a scene break"),
                        static_cast<int>(editor::EmptyParagraphs::SceneBreak));
    m_emptyBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    //: In Polish: P&uste akapity:
    addRow(cleanup, tr("E&mpty paragraphs:"), newLine(cleanupPage, {m_emptyBox}))
        ->setBuddy(m_emptyBox);
    cleanup->addWidget(
        newRemark(cleanupPage,
                  //: Under the cleanups. In Polish: Porządki nie zmieniają akapitów
                  //: w stylu Kod, tak jak autokorekta.
                  tr("The cleanups do not change the paragraphs in the style Code, as "
                     "auto-correct does not.")),
        cleanup->rowCount(), 0, 1, 2);
    endTab(cleanup);
}

void ImportBookDialog::buildExtrasTab() {
    // The annotations and the images
    //: A tab of the window of the import: the annotations and the images. In Polish: Uwagi
    //: i o&brazy
    QGridLayout* extras = newTab(m_tabs, tr("Annotations and Ima&ges"));
    m_extrasTab = extras->parentWidget();
    //: A group of the window of the import. In Polish: Uwagi
    m_notesGroup = new QGroupBox(tr("Annotations"), m_extrasTab);
    auto* notes = new QGridLayout(m_notesGroup);
    m_commentsBox = new QCheckBox(m_notesGroup);
    m_commentsOption = withRemark(
        m_commentsBox,
        newRemark(m_notesGroup,
                  //: Under the comments as annotations. In Polish: Z autorem, datą
                  //: i rozwiązaniem. Komentarz zaczynający się od „Do zrobienia:” albo
                  //: „Notatka:” (tak zapisuje je eksport Kalahari) staje się zadaniem
                  //: albo notatką. Odpowiedzi trafiają do treści uwagi.
                  tr("With the author, the date and whether it is resolved. A comment that "
                     "starts with \"To do:\" or \"Note:\" (as the export of Kalahari writes "
                     "them) becomes a to-do or a note. The replies go to the text of the "
                     "annotation.")));
    notes->addWidget(m_commentsOption, 0, 0, 1, 2);
    m_noCommentsNote = newRemark(
        m_notesGroup,
        //: In the group of the annotations, for Markdown. In Polish:
        //: Markdown nie ma komentarzy; komentarze HTML (<!-- -->)
        //: program pomija.
        tr("Markdown has no comments; the program leaves out the comments of "
           "HTML (<!-- -->)."));
    notes->addWidget(m_noCommentsNote, 1, 0, 1, 2);
    m_changes = new QWidget(m_notesGroup);
    auto* changes = new QHBoxLayout(m_changes);
    changes->setContentsMargins(0, 0, 0, 0);
    m_changesLabel = new QLabel(m_changes);
    //: In Polish: Przyjmij &wszystkie
    m_acceptChangesButton = new QRadioButton(tr("Accept &all"), m_changes);
    //: In Polish: &Odrzuć wszystkie
    m_rejectButton = new QRadioButton(tr("&Reject all"), m_changes);
    changes->addWidget(m_changesLabel);
    changes->addWidget(m_acceptChangesButton);
    changes->addWidget(m_rejectButton);
    changes->addStretch(1);
    notes->addWidget(m_changes, 2, 0, 1, 2);
    extras->addWidget(m_notesGroup, 0, 0, 1, 2);
    //: A group of the window of the import. In Polish: Obrazy i przypisy
    auto* imagesGroup = new QGroupBox(tr("Images and Notes"), m_extrasTab);
    auto* images = new QVBoxLayout(imagesGroup);
    m_imagesLabel = new QLabel(imagesGroup);
    m_imagesLabel->setTextFormat(Qt::PlainText);
    m_imagesLabel->setWordWrap(true);
    images->addWidget(m_imagesLabel);
    // The program connects to the Internet only on the writer's wish
    m_webImagesBox = new QCheckBox(imagesGroup);
    m_webImagesOption = withRemark(
        m_webImagesBox,
        newRemark(imagesGroup,
                  //: Under the download of the images of Internet addresses; %1: the most of an
                  //: image, e.g. 50 MB. In Polish: Gdy import się zacznie, program połączy się
                  //: z adresami podanymi w plikach i pobierze obrazy (każdy do %1). Bez tej
                  //: opcji, a także gdy obrazu nie uda się pobrać, w jego miejscu zostanie opis.
                  tr("When the import starts, the program connects to the addresses in the files "
                     "and downloads the images (each up to %1). Without it, or when an image "
                     "cannot be downloaded, its description stays in its place.")
                      .arg(QLocale().formattedDataSize(MaterialDownloader::MAXIMUM_PICTURE, 0))));
    images->addWidget(m_webImagesOption);
    m_webImagesOption->hide();
    extras->addWidget(imagesGroup, 1, 0, 1, 2);
    endTab(extras);
}

void ImportBookDialog::connectFields() {
    connect(m_formatBox, &QComboBox::currentIndexChanged, this, [this]() {
        if (!m_filling) {
            formatChanged();
        }
    });
    // The options of a plugin read the files again when their tab goes to the back
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int tab) {
        const QWidget* left = m_tabs->widget(m_tab);
        m_tab = tab;
        if (left != nullptr && !m_filling && format() == ImportFormat::Plugin &&
            left == m_pluginPages.value(pluginFormat())) {
            dropFailure();
            readFiles();
        }
    });
    connect(m_upButton, &QPushButton::clicked, this, [this]() { moveFile(-1); });
    connect(m_downButton, &QPushButton::clicked, this, [this]() { moveFile(1); });
    connect(m_removeButton, &QPushButton::clicked, this, &ImportBookDialog::removeFile);
    connect(m_fileList, &QTreeWidget::currentItemChanged, this, &ImportBookDialog::updateFields);
    const auto readAgain = [this]() {
        if (!m_filling) {
            dropFailure();
            readFiles();
        }
    };
    const auto splitAgain = [this]() {
        if (!m_filling) {
            dropFailure();
            splitFiles();
        }
    };
    connect(m_encodingBox, &QComboBox::currentIndexChanged, this, readAgain);
    for (QRadioButton* button : {m_bookButton, m_newBookButton, m_otherFilesButton}) {
        connect(button, &QRadioButton::toggled, this, [this](bool on) {
            if (on && !m_filling) {
                dropFailure();
                showTarget();
            }
        });
    }
    connect(m_typeBox, &QComboBox::currentIndexChanged, this, [this]() {
        if (!m_filling && m_newBookButton->isChecked()) {
            showTarget();
        }
    });
    connect(m_titleField, &QLineEdit::textEdited, this, [this]() { m_newBookTyped = true; });
    connect(m_titleField, &QLineEdit::textChanged, this, &ImportBookDialog::updateFields);
    connect(m_languageBox, &QComboBox::currentIndexChanged, this, &ImportBookDialog::updateFields);
    connect(m_picker, &ElementPlacePicker::placeChanged, this, &ImportBookDialog::dropFailure);
    for (QRadioButton* button : {m_headingsButton, m_pagesButton, m_wholeButton}) {
        connect(button, &QRadioButton::toggled, this, [this, readAgain, splitAgain](bool on) {
            if (!on) {
                return;
            }
            // The plain text is read by its division; the other files only divided again
            if (format() == ImportFormat::PlainText) {
                readAgain();
            } else {
                splitAgain();
            }
        });
    }
    connect(m_partLevelBox, &QComboBox::currentIndexChanged, this, splitAgain);
    connect(m_textLevelBox, &QComboBox::currentIndexChanged, this, splitAgain);
    connect(m_titlePatternBox, &QComboBox::currentIndexChanged, this, readAgain);
    connect(m_partPatternBox, &QComboBox::currentIndexChanged, this, readAgain);
    connect(m_prefixField, &QLineEdit::editingFinished, this, readAgain);
    connect(m_emptyLinesBox, &QSpinBox::valueChanged, this, readAgain);
    connect(m_wholeFilesBox, &QCheckBox::toggled, this, splitAgain);
    connect(m_headingAsTitleBox, &QCheckBox::toggled, this, splitAgain);
    connect(m_linesButton, &QRadioButton::toggled, this, [readAgain](bool on) {
        if (on) {
            readAgain();
        }
    });
    connect(m_emptyLineButton, &QRadioButton::toggled, this, [readAgain](bool on) {
        if (on) {
            readAgain();
        }
    });
    connect(m_textList, &QTreeWidget::itemChanged, this, &ImportBookDialog::rowChanged);
    connect(m_textList, &QTreeWidget::currentItemChanged, this,
            &ImportBookDialog::showKindOfCurrent);
    connect(m_textList, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        if (item != nullptr && item->flags().testFlag(Qt::ItemIsEditable)) {
            m_textList->editItem(item, TITLE_COLUMN);
        }
    });
    connect(m_renameButton, &QPushButton::clicked, this, [this]() {
        QTreeWidgetItem* item = m_textList->currentItem();
        if (item != nullptr && item->flags().testFlag(Qt::ItemIsEditable)) {
            m_textList->setFocus();
            m_textList->editItem(item, TITLE_COLUMN);
        }
    });
    connect(m_kindBox, &QComboBox::activated, this, &ImportBookDialog::kindChosen);
    connect(m_rejectButton, &QRadioButton::toggled, this, [this]() {
        if (m_filling) {
            return;
        }
        dropFailure();
        for (ImportFile& each : m_files) {
            resolveChanges(each, m_rejectButton->isChecked());
        }
        splitFiles();
        showStyles();
        showExtras();
    });
    for (QCheckBox* box : {m_dialogueBox, m_typographyBox, m_doubleSpacesBox, m_leadingSpacesBox,
                           m_sceneMarkersBox, m_commentsBox}) {
        connect(box, &QCheckBox::toggled, this, &ImportBookDialog::dropFailure);
    }
    connect(m_formattingBox, &QComboBox::currentIndexChanged, this, &ImportBookDialog::dropFailure);
    connect(m_emptyBox, &QComboBox::currentIndexChanged, this, &ImportBookDialog::dropFailure);
    connect(m_textStyleBox, &QComboBox::currentIndexChanged, this, &ImportBookDialog::dropFailure);
    connect(m_showInNavigatorLink, &QLabel::linkActivated, this, [this]() {
        if (m_actions.showInNavigator) {
            m_actions.showInNavigator(m_outcome);
        }
        ArchiveDialog::accept();  // done: the window closes
    });
    connect(m_openTextLink, &QLabel::linkActivated, this, [this]() {
        if (m_actions.openFirstText) {
            m_actions.openFirstText(m_outcome);
        }
        ArchiveDialog::accept();  // done: the window closes
    });
    connect(m_undoLink, &QLabel::linkActivated, this, &ImportBookDialog::undoImport);
}

ImportBookDialog::~ImportBookDialog() {
    if (m_settingsListener >= 0) {
        core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    }
}

QList<ImportBookDialog::BookType> ImportBookDialog::bookTypes(
    const core::BookTypeRegistry& registry) {
    QList<BookType> types;
    const QString language = programLanguage();
    for (const core::BookTypePackage* type : registry.bookTypes()) {
        types.append({type->id, type->name.text(language)});
    }
    return types;
}

// =============================================================================
// The choices
// =============================================================================

ImportFormat ImportBookDialog::format() const {
    return static_cast<ImportFormat>(m_formatBox->currentData().toInt());
}

QString ImportBookDialog::pluginFormat() const {
    return format() == ImportFormat::Plugin ? m_formatBox->currentData(PLUGIN_ROLE).toString()
                                            : QString();
}

std::string ImportBookDialog::settingsPrefix() const {
    return std::string("import.") + formatKey(format()) + ".";
}

editor::SplitOptions ImportBookDialog::splitOptions() const {
    editor::SplitOptions options;
    if (format() == ImportFormat::Plugin) {
        return options;  // the plugin's texts: at the headings of their titles, each a title
    }
    options.headingAsTitle = m_headingAsTitleBox->isChecked();
    if (m_wholeButton->isChecked()) {
        options.mode = editor::SplitMode::Whole;
        return options;
    }
    if (format() == ImportFormat::PlainText) {
        if (m_pagesButton->isChecked()) {
            options.mode = editor::SplitMode::Marks;  // the empty lines mark the texts
            return options;
        }
        // At the titles: the parts are headings of level 1, the texts of level 2, of level 1
        // when the text has no parts
        for (const ImportFile& file : m_files) {
            if (file.isRead()) {
                editor::SplitOptions suggested = editor::suggestedSplit(*file.document);
                suggested.headingAsTitle = options.headingAsTitle;
                if (suggested.mode == editor::SplitMode::Headings) {
                    return suggested;
                }
            }
        }
        options.mode = editor::SplitMode::Headings;
        return options;
    }
    if (m_pagesButton->isChecked()) {
        options.mode = editor::SplitMode::PageBreaks;
        return options;
    }
    options.mode = editor::SplitMode::Headings;
    options.partLevel = m_partLevelBox->currentData().toInt();
    options.textLevel = std::max(1, m_textLevelBox->currentData().toInt());
    return options;
}

ImportReading ImportBookDialog::reading() const {
    ImportReading reading;
    reading.format = format();
    reading.plugin = pluginFormat();
    const int encoding = m_encodingBox->currentData().toInt();
    if (encoding >= 0) {
        reading.encoding = static_cast<editor::TextEncoding>(encoding);
    }
    reading.rejectChanges = m_rejectButton->isChecked();
    if (reading.format == ImportFormat::PlainText) {
        editor::TextReadOptions& text = reading.text;
        text.emptyLineParagraphs = m_emptyLineButton->isChecked();
        if (m_headingsButton->isChecked()) {
            text.titles = static_cast<editor::TitlePattern>(
                m_titlePatternBox->currentData().toInt());
            text.prefix = m_prefixField->text().trimmed();
            text.parts = static_cast<editor::PartPattern>(m_partPatternBox->currentData().toInt());
            for (const editor::KindTitle& kind : m_kinds.titles) {
                text.kindTitles += kind.titles;
            }
        } else if (m_pagesButton->isChecked()) {
            text.emptyLinesForText = m_emptyLinesBox->value();
        }
    }
    return reading;
}

ImportBookDialog::Choice ImportBookDialog::choice() const {
    Choice choice;
    choice.format = format();
    for (const ImportFile& file : m_files) {
        choice.files << file.path;
    }
    choice.reading = reading();
    choice.target = m_newBookButton->isChecked()      ? Target::NewBook
                    : m_otherFilesButton->isChecked() ? Target::OtherFiles
                                                      : Target::Book;
    choice.place = m_picker->place();
    choice.newBook.title = m_titleField->text().trimmed();
    choice.newBook.author = m_authorField->text().trimmed();
    choice.newBook.language = m_languageBox->currentData().toString();
    choice.newBook.typeId = m_typeBox->currentData().toString();
    choice.newBook.folder = m_newBookFolder;
    const bool plugin = choice.format == ImportFormat::Plugin;
    choice.wholeFiles = !plugin && m_files.size() > 1 && m_wholeFilesBox->isChecked();
    choice.split = splitOptions();

    editor::ImportKmlOptions& kml = choice.kml;
    kml.formatting = choice.format == ImportFormat::Docx
                         ? static_cast<editor::ImportFormatting>(
                               m_formattingBox->currentData().toInt())
                         : editor::ImportFormatting::Emphasis;
    kml.bookStyles = m_styles;
    kml.dialogueStyle = m_dialogueBox->isChecked() && m_styles.contains(DIALOGUE_STYLE);
    kml.cleanup.typography = m_typographyBox->isChecked();
    kml.cleanup.autoCorrect = autoCorrectOfSettings();
    const QString language = choice.target == Target::NewBook ? choice.newBook.language
                                                              : m_book.language;
    kml.cleanup.quoteMarks = editor::QuoteMarks::forLanguage(language);
    kml.cleanup.doubleSpaces = m_doubleSpacesBox->isChecked();
    kml.cleanup.leadingSpaces = m_leadingSpacesBox->isChecked();
    kml.cleanup.sceneMarkers = m_sceneMarkersBox->isChecked();
    kml.cleanup.empty = static_cast<editor::EmptyParagraphs>(m_emptyBox->currentData().toInt());
    if (plugin) {
        // The plugin's texts as it wrote them
        kml.dialogueStyle = false;
        kml.cleanup = editor::ImportCleanup();
        kml.cleanup.empty = editor::EmptyParagraphs::Keep;
    }
    kml.annotations = choice.format == ImportFormat::Docx && m_commentsBox->isChecked();
    //: The word a comment of Word starts with that makes it a to-do. In Polish: Do zrobienia
    kml.annotationLabels.emplace_back(editor::AnnotationKind::Todo, tr("To do"));
    //: The word a comment of Word starts with that makes it a note. In Polish: Notatka
    kml.annotationLabels.emplace_back(editor::AnnotationKind::Note, tr("Note"));
    if (tr("To do") != QLatin1String("To do")) {
        kml.annotationLabels.emplace_back(editor::AnnotationKind::Todo, QStringLiteral("To do"));
        kml.annotationLabels.emplace_back(editor::AnnotationKind::Note, QStringLiteral("Note"));
    }

    // The styles: the plain text's paragraphs in the style chosen, the others as their list
    if (choice.format == ImportFormat::PlainText) {
        choice.styles.insert(QStringLiteral("normal"), m_textStyleBox->currentData().toString());
    } else {
        for (auto it = m_styleChoices.cbegin(); it != m_styleChoices.cend(); ++it) {
            choice.styles.insert(it.key(), it.value()->currentData().toString());
        }
    }
    return choice;
}

void ImportBookDialog::loadChoices() {
    const auto& settings = core::SettingsManager::getInstance();
    m_webImagesBox->setChecked(settings.get<bool>("import.webImages"));
    if (format() == ImportFormat::Plugin) {
        m_acceptChangesButton->setChecked(true);
        return;  // the plugin's texts: no choices of the program's formats
    }
    const std::string prefix = settingsPrefix();
    const auto flag = [&settings, &prefix](const char* key) {
        return settings.get<bool>(prefix + key);
    };
    const auto text = [&settings, &prefix](const char* key) {
        return settings.get<std::string>(prefix + key);
    };
    m_headingAsTitleBox->setChecked(flag("headingAsTitle"));
    m_dialogueBox->setChecked(flag("dialogueStyle"));
    m_typographyBox->setChecked(flag("typography"));
    m_doubleSpacesBox->setChecked(flag("doubleSpaces"));
    m_leadingSpacesBox->setChecked(flag("leadingSpaces"));
    m_sceneMarkersBox->setChecked(flag("sceneMarkers"));
    const std::string empty = text("emptyParagraphs");
    for (const editor::EmptyParagraphs each :
         {editor::EmptyParagraphs::Remove, editor::EmptyParagraphs::Keep,
          editor::EmptyParagraphs::SceneBreak}) {
        if (empty == emptyKey(each)) {
            chooseData(m_emptyBox, static_cast<int>(each));
        }
    }
    m_wholeFilesBox->setChecked(settings.get<bool>("import.wholeFiles"));
    switch (format()) {
        case ImportFormat::Docx: {
            const std::string formatting = text("formatting");
            for (const editor::ImportFormatting each :
                 {editor::ImportFormatting::All, editor::ImportFormatting::Emphasis,
                  editor::ImportFormatting::StylesOnly}) {
                if (formatting == formattingKey(each)) {
                    chooseData(m_formattingBox, static_cast<int>(each));
                }
            }
            m_commentsBox->setChecked(flag("comments"));
            (flag("rejectChanges") ? m_rejectButton : m_acceptChangesButton)->setChecked(true);
            break;
        }
        case ImportFormat::Markdown:
        case ImportFormat::Plugin:
            m_acceptChangesButton->setChecked(true);
            break;
        case ImportFormat::PlainText: {
            const std::string split = text("split");
            (split == "emptyLines" ? m_pagesButton
             : split == "whole"    ? m_wholeButton
                                   : m_headingsButton)
                ->setChecked(true);
            // The patterns of the language of the book at first
            const bool polish = m_book.language.startsWith(QLatin1String("pl")) ||
                                (m_book.language.isEmpty() &&
                                 programLanguage().startsWith(QLatin1String("pl")));
            editor::TitlePattern titles = polish ? editor::TitlePattern::Polish
                                                 : editor::TitlePattern::English;
            const std::string titleKey = text("titles");
            for (const editor::TitlePattern each :
                 {editor::TitlePattern::Polish, editor::TitlePattern::English,
                  editor::TitlePattern::Number, editor::TitlePattern::Capitals,
                  editor::TitlePattern::Prefix}) {
                if (titleKey == titlePatternKey(each)) {
                    titles = each;
                }
            }
            chooseData(m_titlePatternBox, static_cast<int>(titles));
            m_prefixField->setText(QString::fromStdString(text("prefix")));
            editor::PartPattern parts = polish ? editor::PartPattern::Polish
                                               : editor::PartPattern::English;
            const std::string partKey = text("parts");
            for (const editor::PartPattern each :
                 {editor::PartPattern::Polish, editor::PartPattern::English,
                  editor::PartPattern::None}) {
                if (partKey == partPatternKey(each)) {
                    parts = each;
                }
            }
            chooseData(m_partPatternBox, static_cast<int>(parts));
            m_emptyLinesBox->setValue(settings.get<int>(prefix + "emptyLines"));
            m_acceptChangesButton->setChecked(true);
            break;
        }
    }
}

void ImportBookDialog::rememberChoices() const {
    auto& settings = core::SettingsManager::getInstance();
    const Choice now = choice();
    const std::string prefix = settingsPrefix();
    settings.set<std::string>("import.target", targetKey(now.target));
    if (m_webImages > 0) {
        settings.set<bool>("import.webImages", m_webImagesBox->isChecked());
    }
    if (now.format == ImportFormat::Plugin) {
        return;  // keeps no choices of its own
    }
    if (m_files.size() > 1) {
        settings.set<bool>("import.wholeFiles", m_wholeFilesBox->isChecked());
    }
    settings.set<bool>(prefix + "headingAsTitle", now.split.headingAsTitle);
    settings.set<bool>(prefix + "dialogueStyle", m_dialogueBox->isChecked());
    settings.set<bool>(prefix + "typography", now.kml.cleanup.typography);
    settings.set<bool>(prefix + "doubleSpaces", now.kml.cleanup.doubleSpaces);
    settings.set<bool>(prefix + "leadingSpaces", now.kml.cleanup.leadingSpaces);
    settings.set<bool>(prefix + "sceneMarkers", now.kml.cleanup.sceneMarkers);
    settings.set<std::string>(prefix + "emptyParagraphs", emptyKey(now.kml.cleanup.empty));
    switch (now.format) {
        case ImportFormat::Docx:
            settings.set<std::string>(prefix + "formatting", formattingKey(now.kml.formatting));
            settings.set<bool>(prefix + "comments", m_commentsBox->isChecked());
            settings.set<bool>(prefix + "rejectChanges", now.reading.rejectChanges);
            break;
        case ImportFormat::Markdown:
        case ImportFormat::Plugin:
            break;
        case ImportFormat::PlainText:
            settings.set<std::string>(prefix + "split", m_wholeButton->isChecked()   ? "whole"
                                                        : m_pagesButton->isChecked() ? "emptyLines"
                                                                                     : "titles");
            settings.set<std::string>(prefix + "titles",
                                      titlePatternKey(static_cast<editor::TitlePattern>(
                                          m_titlePatternBox->currentData().toInt())));
            settings.set<std::string>(prefix + "prefix",
                                      m_prefixField->text().trimmed().toStdString());
            settings.set<std::string>(prefix + "parts",
                                      partPatternKey(static_cast<editor::PartPattern>(
                                          m_partPatternBox->currentData().toInt())));
            settings.set<int>(prefix + "emptyLines", m_emptyLinesBox->value());
            settings.set<std::string>(prefix + "style",
                                      m_textStyleBox->currentData().toString().toStdString());
            break;
    }
}

// =============================================================================
// The files
// =============================================================================

void ImportBookDialog::setFormat(ImportFormat format) {
    if (this->format() != format) {
        m_filling = true;
        chooseData(m_formatBox, static_cast<int>(format));
        m_filling = false;
    }
    formatChanged();
}

void ImportBookDialog::setPluginFormat(const QString& id) {
    m_filling = true;
    chooseData(m_formatBox, id, PLUGIN_ROLE);
    m_filling = false;
    formatChanged();
}

void ImportBookDialog::formatChanged() {
    m_filling = true;
    loadChoices();
    m_filling = false;
    m_suggestSplit = true;
    dropFailure();
    showFormat();
    readFiles();
}

void ImportBookDialog::setFiles(const QStringList& paths) {
    m_suggestSplit = true;
    m_files.clear();
    for (const QString& path : sortedByName(paths)) {
        ImportFile file;
        file.path = QFileInfo(path).absoluteFilePath();
        if (std::none_of(m_files.cbegin(), m_files.cend(),
                         [&file](const ImportFile& each) { return each.path == file.path; })) {
            m_files.push_back(file);
        }
    }
    readFiles();
}

void ImportBookDialog::addFiles(const QStringList& paths) {
    QStringList all;
    for (const ImportFile& file : m_files) {
        all << file.path;
    }
    m_suggestSplit = true;
    // The new files in the order of the names, after those of the list in their order
    for (const QString& path : sortedByName(paths)) {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        if (!all.contains(absolute)) {
            ImportFile file;
            file.path = absolute;
            m_files.push_back(file);
            all << absolute;
        }
    }
    dropFailure();
    readFiles();
}

void ImportBookDialog::moveFile(int step) {
    const int row = m_fileList->indexOfTopLevelItem(m_fileList->currentItem());
    const int to = row + step;
    if (row < 0 || to < 0 || to >= static_cast<int>(m_files.size())) {
        return;
    }
    std::swap(m_files[static_cast<size_t>(row)], m_files[static_cast<size_t>(to)]);
    dropFailure();
    showFiles();
    m_fileList->setCurrentItem(m_fileList->topLevelItem(to));
    splitFiles();
}

void ImportBookDialog::removeFile() {
    const int row = m_fileList->indexOfTopLevelItem(m_fileList->currentItem());
    if (row < 0 || row >= static_cast<int>(m_files.size())) {
        return;
    }
    m_files.erase(m_files.begin() + row);
    dropFailure();
    showFiles();
    if (m_fileList->topLevelItemCount() > 0) {
        m_fileList->setCurrentItem(
            m_fileList->topLevelItem(std::min(row, m_fileList->topLevelItemCount() - 1)));
    }
    splitFiles();
    showStyles();
    showExtras();
}

void ImportBookDialog::readFiles() {
    const ImportReading how = reading();
    for (ImportFile& file : m_files) {
        file = readImportFile(file.path, how);
    }

    // The encoding found in the first file, in the first item of the list
    const ImportFile* first = m_files.empty() || !m_files.front().isRead() ? nullptr
                                                                           : &m_files.front();
    const bool ownText = how.format == ImportFormat::Markdown ||
                         how.format == ImportFormat::PlainText;
    if (first != nullptr && ownText) {
        QFile source(first->path);
        const editor::TextEncoding detected = source.open(QIODevice::ReadOnly)
                                                  ? editor::detectTextEncoding(source.readAll())
                                                  : first->encoding;
        //: The first encoding of the list; %1: the encoding found in the file. In Polish: Wykryj
        //: samo (wykryto: %1)
        m_encodingBox->setItemText(0, tr("Detect (found: %1)").arg(encodingName(detected)));
    } else {
        //: The first encoding of the list. In Polish: Wykryj samo
        m_encodingBox->setItemText(0, tr("Detect"));
    }

    // The paragraphs of plain text: as the file looks, until the writer chooses
    if (how.format == ImportFormat::PlainText && first != nullptr) {
        QFile source(first->path);
        if (source.open(QIODevice::ReadOnly)) {
            const bool byEmptyLines = editor::paragraphsByEmptyLines(
                editor::decodeText(source.readAll(), first->encoding));
            m_paragraphsNote->setText(
                byEmptyLines
                    //: Under the paragraphs of plain text. In Polish: Wykryto: puste wiersze
                    //: oddzielają akapity, więc wiersze jednego akapitu się łączą.
                    ? tr("Found: empty lines separate the paragraphs, so the lines of a paragraph "
                         "are joined.")
                    //: Under the paragraphs of plain text. In Polish: Wykryto: nie ma pustych
                    //: wierszy między akapitami, więc każdy wiersz to akapit. Drugi wariant łączy
                    //: wiersze złamane w pliku ręcznie.
                    : tr("Found: no empty lines between the paragraphs, so each line is a "
                         "paragraph. The "
                         "other choice joins the lines broken by hand in the file."));
        }
    }
    showFiles();
    splitFiles();
    showStyles();
    showExtras();
}

void ImportBookDialog::showFiles() {
    m_fileList->clear();
    QStringList problems;
    for (const ImportFile& file : m_files) {
        auto* item = new QTreeWidgetItem(m_fileList);
        item->setText(0, QFileInfo(file.path).fileName());
        item->setToolTip(0, QDir::toNativeSeparators(file.path));
        item->setData(0, PATH_ROLE, file.path);
        if (!file.isRead()) {
            item->setIcon(0, core::ArtProvider::getInstance().getIcon(
                                 QStringLiteral("common.warning"), core::IconContext::TreeView));
            //: Beside a file that could not be read. In Polish: nie można odczytać
            item->setText(1, tr("cannot be read"));
            switch (file.problem.kind) {
                case editor::ImportProblem::Kind::NotDocx:
                    //: %1: the name of the file. In Polish: Plik „%1” nie jest dokumentem Worda
                    //: (DOCX).
                    problems << tr("The file \"%1\" is not a Word document (DOCX).")
                                    .arg(QFileInfo(file.path).fileName());
                    break;
                case editor::ImportProblem::Kind::Damaged:
                    //: %1: the name of the file. In Polish: Części pliku „%1” nie można
                    //: odczytać: plik jest uszkodzony.
                    problems << tr("A part of the file \"%1\" cannot be read: the file is damaged.")
                                    .arg(QFileInfo(file.path).fileName());
                    break;
                case editor::ImportProblem::Kind::Plugin:
                    //: %1: the name of the file; what the plugin said follows under Details.
                    //: In Polish: Wtyczka nie odczytała pliku „%1”.
                    problems << tr("The plugin could not read the file \"%1\".")
                                    .arg(QFileInfo(file.path).fileName());
                    break;
                case editor::ImportProblem::Kind::None:
                case editor::ImportProblem::Kind::CannotOpen:
                    //: %1: the name of the file. In Polish: Nie można otworzyć pliku „%1”.
                    problems << tr("The file \"%1\" cannot be opened.")
                                    .arg(QFileInfo(file.path).fileName());
                    break;
            }
        } else {
            item->setForeground(1, m_fileList->palette().brush(QPalette::PlaceholderText));
            if (format() == ImportFormat::Markdown || format() == ImportFormat::PlainText) {
                item->setText(1, encodingName(file.encoding));
            }
        }
    }
    if (m_fileList->currentItem() == nullptr && m_fileList->topLevelItemCount() > 0) {
        m_fileList->setCurrentItem(m_fileList->topLevelItem(0));
    }
    showNote(m_filesNote, problems.join(QLatin1Char('\n')), QStringLiteral("common.warning"));
    QStringList details;
    for (const ImportFile& file : m_files) {
        if (!file.isRead() && !file.problem.detail.isEmpty()) {
            details << file.problem.detail;
        }
    }
    setFieldProblem(problems.join(QLatin1Char('\n')), details);

    // The start of the first file, to see whether its letters are right
    const ImportFile* first = m_files.empty() || !m_files.front().isRead() ? nullptr
                                                                           : &m_files.front();
    const bool ownText = format() == ImportFormat::Markdown || format() == ImportFormat::PlainText;
    m_previewLabel->setText(first != nullptr && ownText
                                ? textPreview(first->path, first->encoding, PREVIEW_LINES)
                                : QString());
    m_previewLabel->setVisible(ownText && !m_previewLabel->text().isEmpty());

    // What is in the files: the headings, the images, the notes and the comments, and the
    // properties of the first one
    int paragraphs = 0;
    std::map<int, int> headings;
    int images = 0;
    int notes = 0;
    int comments = 0;
    int changes = 0;
    QString title;
    QString author;
    for (const ImportFile& file : m_files) {
        if (!file.isRead()) {
            continue;
        }
        const editor::ImportDocument& document = *file.document;
        // The paragraphs of the text, without the headings
        paragraphs += static_cast<int>(
            std::count_if(document.blocks.begin(), document.blocks.end(),
                          [&document](const editor::ImportBlock& block) {
                              return block.kind == editor::ImportBlock::Kind::Paragraph &&
                                     document.headingLevelOf(block) == 0;
                          }));
        for (const auto& [level, count] : editor::headingCounts(document)) {
            headings[level] += count;
        }
        images += static_cast<int>(document.images.size());
        notes += static_cast<int>(document.notes.size());
        comments += static_cast<int>(document.annotations.size());
        changes += file.read->trackedChanges;
        if (title.isEmpty()) {
            title = document.title;
            author = document.author;
        }
    }
    QStringList parts;
    //: What is in the files. In Polish: %n akapit / akapity / akapitów
    parts << tr("%n paragraphs", nullptr, paragraphs);
    for (const auto& [level, count] : headings) {
        //: What is in the files; %1: the level. In Polish: %n nagłówek %1 / nagłówki %1 /
        //: nagłówków %1
        parts << tr("%n headings %1", nullptr, count).arg(level);
    }
    if (images > 0) {
        //: In Polish: %n obraz / obrazy / obrazów
        parts << tr("%n images", nullptr, images);
    }
    if (notes > 0) {
        //: In Polish: %n przypis / przypisy / przypisów
        parts << tr("%n notes", nullptr, notes);
    }
    if (comments > 0) {
        //: In Polish: %n komentarz / komentarze / komentarzy
        parts << tr("%n comments", nullptr, comments);
    }
    if (changes > 0) {
        //: In Polish: %n zmiana śledzona / zmiany śledzone / zmian śledzonych
        parts << tr("%n tracked changes", nullptr, changes);
    }
    QString contents = parts.join(QStringLiteral(", ")) + QLatin1Char('.');
    if (!title.isEmpty() && !author.isEmpty()) {
        //: After what is in the file; %1: its title, %2: its author. In Polish: Z właściwości
        //: pliku: tytuł „%1”, autor %2.
        contents += QLatin1Char(' ') +
                    tr("From its properties: the title \"%1\", the author %2.").arg(title, author);
    } else if (!title.isEmpty()) {
        //: After what is in the file; %1: its title. In Polish: Z właściwości pliku: tytuł „%1”.
        contents += QLatin1Char(' ') + tr("From its properties: the title \"%1\".").arg(title);
    }
    //: Under the empty list of the files. In Polish: Brak plików – wybierz je przyciskiem Dodaj…
    m_contentsLabel->setText(m_files.empty() ? tr("No files: choose them with Add...") : contents);

    // A new book takes its title, author and language from the file, until the writer types
    if (!m_newBookTyped) {
        const ImportFile* source = first;
        QString bookTitle = source != nullptr ? source->document->title : QString();
        if (bookTitle.isEmpty() && !m_files.empty()) {
            bookTitle = QFileInfo(m_files.front().path).completeBaseName();
        }
        m_titleField->setText(bookTitle);
        if (source != nullptr && !source->document->author.isEmpty()) {
            m_authorField->setText(source->document->author);
        }
        QString language = source != nullptr ? source->document->language.left(2).toLower()
                                             : QString();
        if (m_languageBox->findData(language) < 0) {
            language = QString::fromStdString(
                core::SettingsManager::getInstance().get<std::string>("project.defaultLanguage"));
        }
        if (m_languageBox->findData(language) < 0) {
            language = programLanguage();
        }
        chooseData(m_languageBox, language);
    }
    updateFields();
}

void ImportBookDialog::browseFiles() {
    QString start = m_files.empty()
                        ? ProgramFolders::startFolder(ProgramFolders::Operation::ImportText)
                        : QFileInfo(m_files.back().path).absolutePath();
    QStringList patterns;
    for (const QString& extension : importExtensions(format(), pluginFormat())) {
        patterns << QLatin1Char('*') + extension;
    }
    //: The title of the system window that chooses the files of the import. In Polish: Pliki
    //: do importu
    const QStringList chosen = QFileDialog::getOpenFileNames(
        this, tr("Files to Import"), start,
        QStringLiteral("%1 (%2)").arg(m_formatBox->currentText(), patterns.join(QLatin1Char(' '))));
    if (!chosen.isEmpty()) {
        ProgramFolders::remember(ProgramFolders::Operation::ImportText,
                                 QFileInfo(chosen.front()).absolutePath());
        addFiles(chosen);
    }
}

void ImportBookDialog::browseFolder() {
    QString start = ProgramFolders::existingFolderAt(m_newBookFolder);
    if (start.isEmpty()) {
        start = ProgramFolders::documentsFolder();
    }
    //: The title of the system window that chooses the folder of the new book. In Polish:
    //: Folder nowej książki
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Folder of the New Book"),
                                                             start,
                                                             QFileDialog::ShowDirsOnly |
                                                                 QFileDialog::DontResolveSymlinks);
    if (!chosen.isEmpty()) {
        setNewBookFolder(chosen);
    }
}

void ImportBookDialog::setNewBookFolder(const QString& folder) {
    m_newBookFolder = ProgramFolders::cleanPath(folder);
    showPath(m_folderField, m_newBookFolder);
    dropFailure();
    updateFields();
}

QTreeWidget* ImportBookDialog::fileList() const {
    return m_fileList;
}

QString ImportBookDialog::previewText() const {
    return m_previewLabel->isHidden() ? QString() : m_previewLabel->text();
}

QString ImportBookDialog::contentsText() const {
    return m_contentsLabel->text();
}

// =============================================================================
// The format and the place
// =============================================================================

void ImportBookDialog::showFormat() {
    const ImportFormat now = format();
    const bool text = now == ImportFormat::PlainText;
    const bool word = now == ImportFormat::Docx;
    const bool plugin = now == ImportFormat::Plugin;
    const bool ownText = now == ImportFormat::Markdown || text;
    setHeadingIcon(text                            ? QStringLiteral("file.import.text")
                   : now == ImportFormat::Markdown ? QStringLiteral("file.import.markdown")
                   : plugin                        ? QStringLiteral("file.import.plugin")
                                                   : QStringLiteral("file.import.docx"));
    setRowVisible(m_encodingBox, ownText);
    m_encodingBox->parentWidget()->setVisible(ownText);
    m_previewLabel->setVisible(ownText && !m_previewLabel->text().isEmpty());
    // A format of a plugin: the plugin gives the texts, as it wrote them
    for (QWidget* mode : {static_cast<QWidget*>(m_headingsButton),
                          static_cast<QWidget*>(m_pagesButton),
                          static_cast<QWidget*>(m_wholeButton)}) {
        mode->setVisible(!plugin);
    }
    m_pluginNote->setVisible(plugin);
    m_headingAsTitleBox->setVisible(!plugin);
    m_tabs->setTabVisible(m_tabs->indexOf(m_stylesTab), !plugin);
    m_tabs->setTabVisible(m_tabs->indexOf(m_cleanupTab), !plugin);
    showPluginPage();
    if (text) {
        //: The division of plain text. In Polish: Przy t&ytułach
        m_headingsButton->setText(tr("At the &titles"));
        //: The division of plain text. In Polish: Po pustych &wierszach
        m_pagesButton->setText(tr("After &empty lines"));
    } else {
        //: The division of a file. In Polish: Przy &nagłówkach
        m_headingsButton->setText(tr("At the &headings"));
        //: The division of a file. In Polish: Przy podziałach st&rony
        m_pagesButton->setText(tr("At the page &breaks"));
    }
    m_pagesButton->setVisible(now != ImportFormat::Markdown && !plugin);
    if (now == ImportFormat::Markdown && m_pagesButton->isChecked()) {
        m_headingsButton->setChecked(true);
    }
    m_fileStyles->setVisible(!text);
    m_formattingBox->parentWidget()->setVisible(word);
    m_textParagraphs->setVisible(text);
    stretchStyles();
    m_tabs->setTabVisible(m_tabs->indexOf(m_extrasTab), !text && !plugin);
    m_commentsOption->setVisible(word);
    m_noCommentsNote->setVisible(!word);
    updateFields();
}

void ImportBookDialog::showPluginPage() {
    const QString id = pluginFormat();
    if (!id.isEmpty() && !m_pluginPages.contains(id)) {
        // Made once, when the format is first chosen; none when the plugin has no options
        QWidget* page = nullptr;
        if (const std::shared_ptr<core::IImporter> importer = importerOf(id)) {
            try {
                page = importer->createOptionsPage(id.toStdString(), m_tabs);
            } catch (const std::exception& error) {
                core::Logger::getInstance().error("Import: the options of the format {}: {}",
                                                  id.toStdString(), error.what());
            }
        }
        if (page != nullptr) {
            QString title;
            for (const PluginImportFormat& each : pluginImportFormats()) {
                if (each.id == id) {
                    title = each.name;
                }
            }
            const bool filling = m_filling;
            m_filling = true;  // the new tab does not read the files again
            m_tabs->addTab(page, title);
            m_filling = filling;
        }
        m_pluginPages.insert(id, page);
    }
    for (auto it = m_pluginPages.cbegin(); it != m_pluginPages.cend(); ++it) {
        if (it.value() != nullptr) {
            m_tabs->setTabVisible(m_tabs->indexOf(it.value()), it.key() == id);
        }
    }
}

void ImportBookDialog::setTarget(Target target) {
    (target == Target::Book      ? m_bookButton
     : target == Target::NewBook ? m_newBookButton
                                 : m_otherFilesButton)
        ->setChecked(true);
}

void ImportBookDialog::showTarget() {
    const bool newBook = m_newBookButton->isChecked();
    const bool otherFiles = m_otherFilesButton->isChecked();
    m_targetPages->setCurrentWidget(newBook      ? m_newBookPage
                                    : otherFiles ? static_cast<QWidget*>(m_otherFilesNote)
                                                 : static_cast<QWidget*>(m_picker));
    // The kinds and the styles of the book the texts go to; Other Files have no kinds
    if (newBook) {
        const QString typeId = m_typeBox->currentData().toString();
        m_kinds = importKindsOf(m_registry, typeId);
        m_styles = newBookParagraphStyles(typeId);
    } else if (otherFiles) {
        m_kinds = ImportKinds();
        m_styles = m_book.styles;
    } else {
        m_kinds = m_book.kinds;
        m_styles = m_book.styles;
    }
    m_textList->setColumnHidden(KIND_COLUMN, otherFiles);
    m_kindBox->setVisible(!otherFiles);
    m_kindBox->clear();
    for (const ImportKind& kind : std::as_const(m_kinds.kinds)) {
        m_kindBox->addItem(core::ArtProvider::getInstance().getIcon(kind.iconId,
                                                                    core::IconContext::ComboBox),
                           kind.name, kind.kind.reference());
    }
    if (!newBook && !otherFiles && m_kinds.main) {
        m_picker->setElement(m_kinds.main, m_rows.empty() ? QString() : m_rows.front().title);
        m_picker->setPlace(m_book.place);
    }
    // The texts get the kinds of this book; the plain text reads the names of its kinds
    if (format() == ImportFormat::PlainText && !m_filling) {
        readFiles();
    } else if (!m_filling) {
        splitFiles();
        showStyles();
    }
}

// =============================================================================
// The division and the texts
// =============================================================================

void ImportBookDialog::splitFiles() {
    // The levels of the headings the files have, with how many there are
    std::map<int, int> counts;
    for (const ImportFile& file : m_files) {
        if (file.isRead()) {
            for (const auto& [level, count] : editor::headingCounts(*file.document)) {
                counts[level] += count;
            }
        }
    }
    const ImportFile* first = nullptr;
    for (const ImportFile& file : m_files) {
        if (file.isRead()) {
            first = &file;
            break;
        }
    }
    if (format() != ImportFormat::PlainText) {
        const bool filling = m_filling;
        m_filling = true;
        const int part = m_partLevelBox->count() > 0 ? m_partLevelBox->currentData().toInt() : -1;
        const int text = m_textLevelBox->count() > 0 ? m_textLevelBox->currentData().toInt() : -1;
        m_partLevelBox->clear();
        m_textLevelBox->clear();
        //: No parts in the file. In Polish: – bez części –
        m_partLevelBox->addItem(tr("– no parts –"), 0);
        for (const auto& [level, count] : counts) {
            m_partLevelBox->addItem(levelName(level, count), level);
            m_textLevelBox->addItem(levelName(level, count), level);
        }
        if (m_textLevelBox->count() == 0) {
            m_textLevelBox->addItem(levelName(1, 0), 1);
        }
        // The division the file suggests, else the writer's choice of the same files
        const editor::SplitOptions suggested = first != nullptr
                                                   ? editor::suggestedSplit(*first->document)
                                                   : editor::SplitOptions();
        const bool suggest = m_suggestSplit || part < 0;
        chooseData(m_partLevelBox, suggest ? suggested.partLevel : part);
        chooseData(m_textLevelBox, suggest ? suggested.textLevel : text);
        if (m_suggestSplit && first != nullptr) {
            m_suggestSplit = false;
            switch (suggested.mode) {
                case editor::SplitMode::Headings:
                case editor::SplitMode::Marks:
                    m_headingsButton->setChecked(true);
                    break;
                case editor::SplitMode::PageBreaks:
                    m_pagesButton->setChecked(true);
                    break;
                case editor::SplitMode::Whole:
                    m_wholeButton->setChecked(true);
                    break;
            }
        }
        if (!m_headingsButton->isChecked() && !m_pagesButton->isChecked() &&
            !m_wholeButton->isChecked()) {
            m_headingsButton->setChecked(true);
        }
        m_filling = filling;
    }
    const bool text = format() == ImportFormat::PlainText;
    const bool plugin = format() == ImportFormat::Plugin;
    m_levels->setVisible(!text && !plugin);
    m_levels->setEnabled(m_headingsButton->isChecked());
    m_patterns->setVisible(text);
    m_patterns->setEnabled(m_headingsButton->isChecked());
    m_prefixField->setVisible(m_titlePatternBox->currentData().toInt() ==
                              static_cast<int>(editor::TitlePattern::Prefix));
    m_emptyLines->setVisible(text);
    m_emptyLines->setEnabled(m_pagesButton->isChecked());
    m_wholeFilesBox->setVisible(m_files.size() > 1 && !plugin);

    const bool wholeFiles = !plugin && m_files.size() > 1 && m_wholeFilesBox->isChecked();
    m_rows = importRows(m_files, splitOptions(), wholeFiles, m_kinds);
    showRows();
}

void ImportBookDialog::showRows() {
    const bool filling = m_filling;
    m_filling = true;
    m_textList->clear();
    auto& art = core::ArtProvider::getInstance();
    QTreeWidgetItem* part = nullptr;
    for (size_t i = 0; i < m_rows.size(); ++i) {
        const ImportRow& row = m_rows[i];
        if (!row.piece.inPart) {
            part = nullptr;
        }
        auto* item = part != nullptr ? new QTreeWidgetItem(part) : new QTreeWidgetItem(m_textList);
        item->setData(TITLE_COLUMN, ROW_ROLE, static_cast<int>(i));
        item->setText(TITLE_COLUMN, row.title);
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
        if (row.piece.part) {
            item->setText(KIND_COLUMN, m_kinds.part ? m_kinds.partName : QString());
            item->setIcon(TITLE_COLUMN, art.getIcon(QStringLiteral("structure.part"),
                                                    core::IconContext::TreeView));
            if (m_kinds.part) {
                flags |= Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate;
            }
            part = item;
        } else {
            const ImportKind* kind = m_kinds.find(row.kind);
            item->setText(KIND_COLUMN, kind != nullptr ? kind->name : QString());
            if (kind != nullptr) {
                item->setIcon(TITLE_COLUMN, art.getIcon(kind->iconId, core::IconContext::TreeView));
            }
            item->setText(WORDS_COLUMN, QLocale().toString(row.piece.words));
            item->setTextAlignment(WORDS_COLUMN, Qt::AlignRight | Qt::AlignVCenter);
            flags |= Qt::ItemIsUserCheckable;
        }
        item->setFlags(flags);
        if (!row.piece.part) {
            item->setCheckState(TITLE_COLUMN, row.checked ? Qt::Checked : Qt::Unchecked);
        }
    }
    m_textList->expandAll();
    if (m_textList->topLevelItemCount() > 0) {
        m_textList->setCurrentItem(m_textList->topLevelItem(0));
    }
    m_filling = filling;
    if (!m_rows.empty()) {
        m_picker->setTitle(m_rows.front().title);
    }
    showKindOfCurrent();
    updateFields();
}

void ImportBookDialog::rowChanged(QTreeWidgetItem* item, int column) {
    if (m_filling || item == nullptr) {
        return;
    }
    const int index = item->data(TITLE_COLUMN, ROW_ROLE).toInt();
    if (index < 0 || index >= static_cast<int>(m_rows.size()) || column != TITLE_COLUMN) {
        return;
    }
    ImportRow& row = m_rows[static_cast<size_t>(index)];
    row.title = item->text(TITLE_COLUMN).trimmed();
    if (!row.piece.part) {
        row.checked = item->checkState(TITLE_COLUMN) == Qt::Checked;
        if (row.checked) {
            row.full = false;
        }
    }
    dropFailure();
    updateFields();
}

void ImportBookDialog::showKindOfCurrent() {
    const QTreeWidgetItem* item = m_textList->currentItem();
    const int index = item != nullptr ? item->data(TITLE_COLUMN, ROW_ROLE).toInt() : -1;
    const bool text = index >= 0 && index < static_cast<int>(m_rows.size()) &&
                      !m_rows[static_cast<size_t>(index)].piece.part;
    m_kindBox->setEnabled(text && m_kindBox->count() > 0);
    m_renameButton->setEnabled(item != nullptr);
    if (text) {
        const bool filling = m_filling;
        m_filling = true;
        chooseData(m_kindBox, m_rows[static_cast<size_t>(index)].kind);
        m_filling = filling;
    }
}

void ImportBookDialog::kindChosen() {
    QTreeWidgetItem* item = m_textList->currentItem();
    const int index = item != nullptr ? item->data(TITLE_COLUMN, ROW_ROLE).toInt() : -1;
    if (index < 0 || index >= static_cast<int>(m_rows.size()) ||
        m_rows[static_cast<size_t>(index)].piece.part) {
        return;
    }
    ImportRow& row = m_rows[static_cast<size_t>(index)];
    row.kind = m_kindBox->currentData().toString();
    const ImportKind* kind = m_kinds.find(row.kind);
    const bool filling = m_filling;
    m_filling = true;
    item->setText(KIND_COLUMN, kind != nullptr ? kind->name : QString());
    if (kind != nullptr) {
        item->setIcon(TITLE_COLUMN, core::ArtProvider::getInstance().getIcon(
                                        kind->iconId, core::IconContext::TreeView));
    }
    m_filling = filling;
    dropFailure();
    updateFields();
}

void ImportBookDialog::checkAll() {
    for (QTreeWidgetItemIterator it(m_textList); *it != nullptr; ++it) {
        if ((*it)->flags().testFlag(Qt::ItemIsUserCheckable) &&
            !(*it)->flags().testFlag(Qt::ItemIsAutoTristate)) {
            (*it)->setCheckState(TITLE_COLUMN, Qt::Checked);
        }
    }
}

void ImportBookDialog::checkNone() {
    for (QTreeWidgetItemIterator it(m_textList); *it != nullptr; ++it) {
        if ((*it)->flags().testFlag(Qt::ItemIsUserCheckable) &&
            !(*it)->flags().testFlag(Qt::ItemIsAutoTristate)) {
            (*it)->setCheckState(TITLE_COLUMN, Qt::Unchecked);
        }
    }
}

QTreeWidget* ImportBookDialog::textList() const {
    return m_textList;
}

QString ImportBookDialog::textsNote() const {
    QStringList lines{m_textsNote->text(), noteText(m_fullNote)};
    lines.removeAll(QString());
    return lines.join(QLatin1Char('\n'));
}

// =============================================================================
// The styles and the annotations
// =============================================================================

void ImportBookDialog::showStyles() {
    const bool filling = m_filling;
    m_filling = true;
    // The choices the writer made stay for the styles of the same names
    QHash<QString, QString> chosen;
    for (auto it = m_styleChoices.cbegin(); it != m_styleChoices.cend(); ++it) {
        chosen.insert(it.key(), it.value()->currentData().toString());
    }
    m_styleList->clear();
    m_styleChoices.clear();
    const auto fillBookStyles = [this](QComboBox* box) {
        for (const editor::ParagraphStyle& style : m_styles.styles()) {
            box->addItem(style.name.isEmpty() ? style.id : style.name, style.id);
        }
    };
    for (const ImportFileStyle& style : importFileStyles(m_files)) {
        auto* item = new QTreeWidgetItem(m_styleList);
        //: A style of the file in the list of the styles; %1: its name, %2: its paragraphs. In
        //: Polish: %1 (%2)
        item->setText(0, tr("%1 (%2)").arg(style.name).arg(style.paragraphs));
        auto* box = new QComboBox(m_styleList);
        //: The name of the choice of the book's style for screen readers; %1: the file's style.
        //: In Polish: Styl w książce dla „%1”
        box->setAccessibleName(tr("Style of the book for \"%1\"").arg(style.name));
        fillBookStyles(box);
        box->insertSeparator(box->count());
        //: A choice of the list of the styles: a new style of the book; %1: the style of the file.
        //: In Polish: Nowy styl książki „%1”
        box->addItem(tr("New book style \"%1\"").arg(style.name),
                     QLatin1String(NEW_STYLE_PREFIX) + style.name);
        const editor::ImportDocument* document = nullptr;
        for (const ImportFile& file : m_files) {
            if (file.isRead() && file.document->style(style.key) == style.style) {
                document = file.document.get();
                break;
            }
        }
        const QString suggested = document != nullptr
                                      ? editor::suggestedBookStyle(*document, style.key, m_styles)
                                      : QString();
        const QString wanted = chosen.value(style.name, suggested);
        chooseData(box, box->findData(wanted) >= 0 ? wanted : suggested);
        connect(box, &QComboBox::currentIndexChanged, this, &ImportBookDialog::dropFailure);
        m_styleList->setItemWidget(item, 1, box);
        m_styleChoices.insert(style.name, box);
    }
    m_styleList->resizeColumnToContents(0);

    // Plain text: its paragraphs in one style
    const QString textStyle = m_textStyleBox->count() > 0
                                  ? m_textStyleBox->currentData().toString()
                                  : QString::fromStdString(
                                        core::SettingsManager::getInstance().get<std::string>(
                                            "import.text.style"));
    m_textStyleBox->clear();
    fillBookStyles(m_textStyleBox);
    chooseData(m_textStyleBox, m_styles.contains(textStyle) ? textStyle : QStringLiteral("normal"));
    m_dialogueBox->setVisible(m_styles.contains(DIALOGUE_STYLE));
    m_filling = filling;
}

QTreeWidget* ImportBookDialog::styleList() const {
    return m_styleList;
}

QComboBox* ImportBookDialog::styleChoice(const QString& name) const {
    return m_styleChoices.value(name, nullptr);
}

void ImportBookDialog::showExtras() {
    int comments = 0;
    int changes = 0;
    int images = 0;
    int missing = 0;
    int web = 0;
    int footnotes = 0;
    int endnotes = 0;
    QStringList missingNames;
    for (const ImportFile& file : m_files) {
        if (!file.isRead()) {
            continue;
        }
        comments += static_cast<int>(file.read->annotations.size());
        changes += file.read->trackedChanges;
        for (const editor::ImportImage& image : file.document->images) {
            if (image.web) {
                ++web;
            } else if (image.missing || image.data.isEmpty()) {
                ++missing;
                missingNames << image.address;
            } else {
                ++images;
            }
        }
        for (const editor::ImportNote& note : file.document->notes) {
            ++(note.endnote ? endnotes : footnotes);
        }
    }
    //: %n: the comments of the files. In Polish: &Komentarze Worda jako uwagi (%n)
    m_commentsBox->setText(tr("&Word comments as annotations (%n)", nullptr, comments));
    m_commentsBox->setEnabled(comments > 0);
    //: %n: the tracked changes of the files. In Polish: Zmiany śledzone (%n):
    m_changesLabel->setText(tr("Tracked changes (%n):", nullptr, changes));
    m_changes->setVisible(format() == ImportFormat::Docx && changes > 0);

    QStringList sentences;
    if (images > 0 && format() == ImportFormat::Docx) {
        //: The images of a Word document. In Polish: Obrazy z pliku (%n) trafią do obrazów
        //: książki, w rozmiarze z Worda, z przycięciem i tekstem alternatywnym.
        sentences << tr("The images of the file (%n) go to the images of the book, in their size "
                        "of Word, with their crop and their alternative text.",
                        nullptr, images);
    } else if (images > 0) {
        //: The images next to a Markdown file. In Polish: Obrazy z plików obok pliku Markdown
        //: (%n) trafią do obrazów książki.
        sentences << tr(
            "The images of the files next to the Markdown file (%n) go to the images of the book.",
            nullptr, images);
    } else if (missing + web == 0) {
        //: In Polish: Plik nie ma obrazów.
        sentences << tr("The file has no images.");
    }
    if (missing > 0) {
        //: %1: the names of the missing files. In Polish: Brakuje plików obrazów (%n): %1. W ich
        //: miejscu zostanie ich opis.
        sentences << tr("The files of %n images are missing: %1. Their descriptions stay in their "
                        "places.",
                        nullptr, missing)
                         .arg(missingNames.join(QStringLiteral(", ")));
    }
    if (footnotes + endnotes > 0) {
        //: In Polish: Przypisy dolne (%1) i końcowe (%2) zostają przypisami, a numery nadaje
        //: im Kalahari.
        sentences
            << tr("The footnotes (%1) and the endnotes (%2) stay notes, and Kalahari numbers them.")
                   .arg(footnotes)
                   .arg(endnotes);
    }
    m_imagesLabel->setText(sentences.join(QLatin1Char(' ')));
    m_webImages = web;
    //: %n: the images of Internet addresses of the files. In Polish: Pobierz obrazy z adresów
    //: &internetowych (%n)
    m_webImagesBox->setText(tr("Download the &images of Internet addresses (%n)", nullptr, web));
    m_webImagesOption->setVisible(web > 0);
}

QString ImportBookDialog::imagesText() const {
    return m_imagesLabel->text();
}

// =============================================================================
// The fields and the work
// =============================================================================

void ImportBookDialog::updateFields() {
    if (m_filling) {
        return;
    }
    m_upButton->setEnabled(m_fileList->indexOfTopLevelItem(m_fileList->currentItem()) > 0);
    const int current = m_fileList->indexOfTopLevelItem(m_fileList->currentItem());
    m_downButton->setEnabled(current >= 0 && current + 1 < m_fileList->topLevelItemCount());
    m_removeButton->setEnabled(current >= 0);

    // The texts that go, and the kinds the book cannot have more of
    int texts = 0;
    int all = 0;
    for (const ImportRow& row : m_rows) {
        if (!row.piece.part) {
            ++all;
            texts += row.checked ? 1 : 0;
        }
    }
    //: Under the list of the texts; %1: how many are checked, %n: how many there are, %2: their
    //: words. In Polish: Do książki trafią teksty: %1 z %n (słowa: %2).
    m_textsNote->setText(m_rows.empty()
                             ? QString()
                             : tr("%1 of %n texts go to the book (%2 words).", nullptr, all)
                                   .arg(texts)
                                   .arg(QLocale().toString(checkedWords(m_rows))));
    QStringList full;
    for (const QString& reference : fullKinds(m_rows, m_kinds)) {
        if (const ImportKind* kind = m_kinds.find(reference)) {
            //: A kind the book cannot have more of; %1: its name. In Polish: Książka nie może
            //: mieć więcej tekstów rodzaju „%1”: odznacz je albo zmień ich rodzaj.
            full << tr("The book cannot have more texts of the kind \"%1\": uncheck them or change "
                       "their kind.")
                        .arg(kind->name);
        }
    }
    const bool unchecked = std::any_of(m_rows.cbegin(), m_rows.cend(), [](const ImportRow& row) {
        return row.full && !row.checked;
    });
    if (full.isEmpty() && unchecked) {
        //: Under the list of the texts. In Polish: Teksty rodzaju, którego książka nie może mieć
        //: więcej, są odznaczone; zmień ich rodzaj, żeby je dodać.
        full << tr("The texts of a kind the book cannot have more of are unchecked; change their "
                   "kind to add "
                   "them.");
    }
    showNote(m_fullNote, full.join(QLatin1Char('\n')), QStringLiteral("common.warning"));

    // A new book: its title and its folder
    bool placeUsable = true;
    if (m_newBookButton->isChecked()) {
        const QString title = m_titleField->text().trimmed();
        const QString folderProblem = ProgramFolders::problemOf(m_newBookFolder);
        const QString bookFolder = core::ProjectManager::newProjectFolder(m_newBookFolder, title,
                                                                          true);
        if (title.isEmpty()) {
            placeUsable = false;
            //: Under the folder of a new book without a title. In Polish: Wpisz tytuł książki.
            showNote(m_newBookNote, tr("Type the title of the book."),
                     QStringLiteral("common.warning"));
        } else if (!folderProblem.isEmpty()) {
            placeUsable = false;
            showNote(m_newBookNote, folderProblem, QStringLiteral("common.warning"));
        } else if (bookFolder.isEmpty() || !core::ProjectManager::canHoldNewProject(bookFolder)) {
            placeUsable = false;
            //: %1: the folder of the new book. In Polish: Folder %1 już istnieje i nie jest
            //: pusty. Zmień tytuł albo wybierz inny folder.
            showNote(m_newBookNote,
                     tr("The folder %1 exists and is not empty. Change the title or choose another "
                        "folder.")
                         .arg(QDir::toNativeSeparators(bookFolder)),
                     QStringLiteral("common.warning"));
        } else {
            //: %1: the folder of the new book. In Polish: Książka powstanie w folderze %1.
            showNote(
                m_newBookNote,
                tr("The book is made in the folder %1.").arg(QDir::toNativeSeparators(bookFolder)));
        }
    } else if (m_bookButton->isChecked()) {
        placeUsable = !m_picker->places().isEmpty();
    }
    const bool readable = std::any_of(m_files.cbegin(), m_files.cend(),
                                      [](const ImportFile& file) { return file.isRead(); });
    const bool anyText = std::any_of(m_rows.cbegin(), m_rows.cend(), [](const ImportRow& row) {
        return !row.piece.part && row.checked;
    });
    acceptButton()->setEnabled(readable && anyText && placeUsable &&
                               fullKinds(m_rows, m_kinds).isEmpty());
}

bool ImportBookDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_textList && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_F2 && key->modifiers() == Qt::NoModifier) {
            m_renameButton->click();
            return true;
        }
        if (key->key() == Qt::Key_F4 && key->modifiers() == Qt::NoModifier &&
            m_kindBox->isEnabled()) {
            m_kindBox->setFocus();
            m_kindBox->showPopup();
            return true;
        }
    }
    return ArchiveDialog::eventFilter(watched, event);
}

void ImportBookDialog::run() {
    if (!acceptButton()->isEnabled() || !m_actions.import) {
        return;
    }
    rememberChoices();
    startWork();
    Outcome outcome;
    QString problem;
    QStringList details;
    const Choice now = choice();
    // The images of Internet addresses first, on the writer's wish: the texts get the copies of
    // the documents with them, and the files of the window stay as they were read
    std::vector<ImportFile> files = m_files;
    QStringList notDownloaded;
    Progress importing = progress();
    if (m_webImages > 0 && m_webImagesBox->isChecked()) {
        const Progress whole = progress();
        notDownloaded = downloadWebImages(
            files, [whole](int percent) { whole(percent * DOWNLOAD_SHARE / 100); });
        importing = [whole](int percent) {
            whole(DOWNLOAD_SHARE + percent * (100 - DOWNLOAD_SHARE) / 100);
        };
    }
    const bool done = m_actions.import(now, files, m_rows, m_kinds, importing, &outcome, &problem,
                                       &details);
    if (!done) {
        //: In Polish: Nie udało się zaimportować tekstów.
        failWork(tr("The texts could not be imported."), problem, details);
        updateFields();
        return;
    }
    m_outcome = outcome;

    // What went to the book
    QString text;
    const QString words = QLocale().toString(checkedWords(m_rows));
    switch (now.target) {
        case Target::Book:
            text = outcome.parts > 0
                       //: %n: the texts, %1: the parts, %2: the title of the book, %3: the words.
                       //: In Polish: Do książki „%2” dodano %n tekst / teksty / tekstów
                       //: w częściach: %1 (słowa: %3).
                       ? tr("Added %n texts in %1 parts to the book \"%2\" (%3 words).", nullptr,
                            outcome.texts)
                             .arg(outcome.parts)
                             .arg(m_book.title, words)
                       //: %n: the texts, %1: the title of the book, %2: the words. In Polish: Do
                       //: książki „%1” dodano %n tekst / teksty / tekstów (słowa: %2).
                       : tr("Added %n texts to the book \"%1\" (%2 words).", nullptr, outcome.texts)
                             .arg(m_book.title, words);
            break;
        case Target::OtherFiles:
            //: %n: the files. In Polish: Do grupy „Inne pliki” dodano %n plik / pliki / plików.
            text = tr("Added %n files to the group \"Other Files\".", nullptr, outcome.texts);
            break;
        case Target::NewBook:
            //: %1: the title of the book, %n: its texts, %2: its folder. In Polish: Utworzono
            //: książkę „%1” z tekstami (%n) w folderze %2. Otworzy się po zamknięciu tego okna.
            text = tr("Made the book \"%1\" of %n texts in the folder %2. It opens when this "
                      "window closes.",
                      nullptr, outcome.texts)
                       .arg(now.newBook.title,
                            QDir::toNativeSeparators(QFileInfo(outcome.bookFile).absolutePath()));
            break;
    }
    if (outcome.images > 0) {
        //: After the result of the import. In Polish: Obrazy (%n) są w obrazach książki.
        text += QLatin1Char(' ') +
                tr("The images (%n) are in the images of the book.", nullptr, outcome.images);
    }
    if (outcome.styles > 0) {
        //: After the result of the import. In Polish: Książka ma nowe style (%n).
        text += QLatin1Char(' ') + tr("The book has new styles (%n).", nullptr, outcome.styles);
    }
    const bool newBook = now.target == Target::NewBook;
    finishWork(text, newBook ? QFileInfo(outcome.bookFile).absolutePath() : QString());
    progressBar()->hide();
    const bool inWindow = !newBook || outcome.bookOpened;
    m_showInNavigatorLink->setVisible(inWindow && m_actions.showInNavigator);
    m_openTextLink->setVisible(inWindow && !outcome.firstText.isEmpty() && m_actions.openFirstText);
    m_undoLink->setVisible(!newBook && m_actions.undo && !outcome.added.isEmpty());

    // What the files had that the import left out
    std::map<editor::ImportOmission, int> omissions;
    for (const ImportFile& file : m_files) {
        if (file.isRead()) {
            for (const auto& [omission, count] : file.document->omissions) {
                omissions[omission] += count;
            }
        }
    }
    QStringList notes;
    for (const auto& [omission, count] : omissions) {
        switch (omission) {
            case editor::ImportOmission::TextBox:
                //: What the import left out. In Polish: – pola tekstowe (%n): ich tekst jest za
                //: akapitem, w którym były
                notes << tr("– text boxes (%n): their text is after their paragraph", nullptr,
                            count);
                break;
            case editor::ImportOmission::Shape:
                //: In Polish: – kształty (%n)
                notes << tr("– shapes (%n)", nullptr, count);
                break;
            case editor::ImportOmission::Chart:
                //: In Polish: – wykresy i diagramy (%n)
                notes << tr("– charts and diagrams (%n)", nullptr, count);
                break;
            case editor::ImportOmission::Equation:
                //: In Polish: – równania (%n)
                notes << tr("– equations (%n)", nullptr, count);
                break;
            case editor::ImportOmission::Object:
                //: In Polish: – osadzone obiekty (%n)
                notes << tr("– embedded objects (%n)", nullptr, count);
                break;
            case editor::ImportOmission::HeaderFooter:
                //: In Polish: – nagłówki i stopki stron
                notes << tr("– the headers and footers of the pages");
                break;
            case editor::ImportOmission::TableOfContents:
                //: In Polish: – spis treści Worda: książka ma własny
                notes << tr("– the table of contents of Word: the book has its own");
                break;
            case editor::ImportOmission::Image:
                //: In Polish: – obrazy w formatach, których program nie odczytuje (EMF, WMF,
                //: TIFF) (%n)
                notes << tr("– images in formats the program does not read (EMF, WMF, TIFF) (%n)",
                            nullptr, count);
                break;
            case editor::ImportOmission::Html:
                //: In Polish: – znaczniki HTML (%n)
                notes << tr("– HTML tags (%n)", nullptr, count);
                break;
        }
    }
    if (!notes.isEmpty()) {
        //: Before what the import left out. In Polish: Import pominął:
        notes.prepend(tr("The import left out:"));
    }
    if (!notDownloaded.isEmpty()) {
        //: Before the images of Internet addresses that did not come. In Polish: Nie udało się
        //: pobrać tych obrazów; w ich miejscu jest opis:
        notes << tr("These images could not be downloaded; their descriptions are in their "
                    "places:")
              << notDownloaded;
    }
    setResultNotes(notes);
}

void ImportBookDialog::undoImport() {
    if (!m_actions.undo || m_outcome.added.isEmpty()) {
        return;
    }
    const bool undone = m_actions.undo(m_outcome);
    m_outcome = Outcome();
    finishWork(undone
                   //: After Undo Import. In Polish: Cofnięto import: książka jest taka jak przed
                   //: nim.
                   ? tr("The import is undone: the book is as it was before it.")
                   //: After Undo Import that could not delete everything. In Polish: Cofnięto
                   //: import, ale nie wszystkie pliki udało się usunąć.
                   : tr("The import is undone, but not every file could be deleted."),
               QString());
    m_showInNavigatorLink->hide();
    m_openTextLink->hide();
    m_undoLink->hide();
    setResultNotes({});
}

}  // namespace dialogs
}  // namespace gui
}  // namespace kalahari
