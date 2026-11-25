// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file export_book_dialog.cpp
/// @brief File > Export > DOCX, PDF, Markdown and EPUB: the file, the texts of the book and how
/// they look in it, and the progress and the result of the export in one window

#include "kalahari/gui/dialogs/export_book_dialog.h"
#include "book_output_parts.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/extension_points.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/text_images.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
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
#include <QSignalBlocker>
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

namespace kalahari::gui::dialogs {

namespace {

using book_output::nameOfValue;
using book_output::valueOfName;

/// The height of the picture of the cover of an EPUB, in lines of the window's text; its width
/// is two thirds of it, as a page of a book
constexpr int COVER_LINES = 3;
/// The width of the description of the book on the tab of an EPUB, in letters: one line
constexpr int DESCRIPTION_LETTERS = 70;
/// The width of the field of the ISBN, in letters: the 17 characters of an ISBN with its
/// hyphens (digits are wider than letters) and the frame of the field
constexpr int ISBN_LETTERS = 24;

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

/// The purposes of a PDF as the settings keep them
const std::array<std::pair<editor::PdfPurpose, const char*>, 3>& purposeNames() {
    static const std::array<std::pair<editor::PdfPurpose, const char*>, 3> names = {{
        {editor::PdfPurpose::Reading, "reading"},
        {editor::PdfPurpose::PrintShop, "printShop"},
        {editor::PdfPurpose::Archive, "archive"},
    }};
    return names;
}

} // anonymous namespace

ExportBookDialog::ExportBookDialog(const Book& book, editor::ExportFormat format,
                                   const QString& folder, Exporter exporter, QWidget* parent)
    : BookOutputDialog(book, parent)
    , m_exporter(std::move(exporter))
{
    const auto& settings = core::SettingsManager::getInstance();
    //: The title of the window of File > Export > DOCX and Markdown. In Polish: Eksportuj
    //: książkę
    setHeading(tr("Export Book"));

    // The tabs: the file, the texts, the layout, and the annotations and the images. The
    // tabs are as high as the highest, so the window keeps its size and nothing scrolls; the
    // progress and the result show under them, whichever tab is in front.
    fieldsLayout()->addWidget(tabs());

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
    addTextsTab(
        //: A tab of the window of the export: what goes to the file. In Polish: T&eksty
        tr("Text&s"),
        book.title.isEmpty()
            //: Under the content of the export without a book. In Polish: Tekst trafi do
            //: pliku także z niezapisanymi zmianami.
            ? tr("The text goes to the file also with its unsaved changes.")
            //: Under the content of the export. In Polish: Teksty trafią do pliku w kolejności
            //: Nawigatora, także z niezapisanymi zmianami.
            : tr("The texts go to the file in the order of the Navigator, also with their "
                 "unsaved changes."));

    // The layout: the titles as headings, the pages of a Word document and of a PDF
    //: A tab of the window of the export: how the texts look in the file. In Polish: U&kład
    QGridLayout* look = addLayoutTab(tr("La&yout"));
    QWidget* lookPage = look->parentWidget();

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
    auto* purposeLayout = new QVBoxLayout();
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
    purposeGroup->setLayout(purposeLayout);
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
                             tr("The fonts are in the file, so it looks the same everywhere. "
                                "Title, author and language come from the book's properties.")),
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
    //: The cover of an EPUB. In Polish: Bez okładki
    m_coverBox->addItem(tr("No cover"), static_cast<int>(Cover::None));
    showCoverDesign();
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
    auto* dataGrid = new QGridLayout(dataGroup);
    dataGrid->setColumnStretch(1, 1);
    const auto newValue = [dataGroup](const QString& text) {
        auto* label = new QLabel(text, dataGroup);
        label->setTextFormat(Qt::PlainText);
        return label;
    };
    m_titleAndAuthor = newValue(QString());
    //: In Polish: Tytuł i autor:
    addRow(dataGrid, tr("Title and author:"), m_titleAndAuthor);
    m_languageLabel = newValue(QString());
    //: In Polish: Język:
    addRow(dataGrid, tr("Language:"), m_languageLabel);
    m_descriptionLabel = newValue(QString());
    //: In Polish: Opis:
    addRow(dataGrid, tr("Description:"), m_descriptionLabel);
    // The ISBN and the publisher in one row: the tab as high as the others
    auto* publication = new QWidget(dataGroup);
    auto* publicationLayout = new QHBoxLayout(publication);
    publicationLayout->setContentsMargins(0, 0, 0, 0);
    publicationLayout->setSpacing(fontMetrics().averageCharWidth() * 3);
    m_isbnField = new QLineEdit(publication);
    //: The example of an ISBN in its empty field. In Polish: 978-83-…
    m_isbnField->setPlaceholderText(tr("978-..."));
    // As wide as an ISBN with its hyphens
    m_isbnField->setFixedWidth(widthOfLetters(m_isbnField->fontMetrics(), ISBN_LETTERS));
    publicationLayout->addWidget(m_isbnField);
    m_publisherField = new QLineEdit(publication);
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
    addRow(dataGrid, tr("&ISBN:"), publication)->setBuddy(m_isbnField);
    // The sentence under the data as wide as the group: in fewer lines
    m_bookDataNote = addNote(dataGrid);
    int noteRow = 0;
    int noteColumn = 0;
    int noteRows = 0;
    int noteColumns = 0;
    dataGrid->getItemPosition(dataGrid->indexOf(m_bookDataNote.widget), &noteRow, &noteColumn,
                              &noteRows, &noteColumns);
    dataGrid->removeWidget(m_bookDataNote.widget);
    dataGrid->addWidget(m_bookDataNote.widget, noteRow, 0, 1, 2);
    epub->addWidget(dataGroup, 1, 0, 1, 2);
    endTab(epub);
    showBookData();

    // The options of the formats of the plugins: a tab of each, shown while it is chosen
    for (const Format& each : std::as_const(m_formats)) {
        const std::shared_ptr<core::IExporter> plugin = exporterOf(each.id);
        if (!plugin) {
            continue;
        }
        QWidget* page = nullptr;
        try {
            page = plugin->createOptionsPage(each.id.toStdString(), tabs());
        } catch (const std::exception& error) {
            core::Logger::getInstance().error("Export: the options of the format {}: {}",
                                              each.id.toStdString(), error.what());
        }
        if (page != nullptr) {
            const QString title = each.name.left(each.name.lastIndexOf(QLatin1String(" ("))).trimmed();
            tabs()->setTabVisible(tabs()->addTab(page, title.isEmpty() ? each.name : title),
                                  false);
            m_formatPages.insert(each.id, page);
        }
    }

    //: In Polish: Postęp i wynik pokażą się tutaj.
    setHint(tr("The progress and the result will show here."));
    //: The button that writes the file. In Polish: Eksportuj
    setAcceptText(tr("Export"));

    // The writer's last choices
    m_commentsBox->setChecked(settings.get<bool>("export.comments"));
    m_copyImagesBox->setChecked(settings.get<bool>("export.copyImages"));
    m_pdfCommentsBox->setChecked(settings.get<bool>("export.pdf.comments"));
    editor::PdfPurpose purpose =
        valueOfName(purposeNames(), settings.get<std::string>("export.pdf.purpose"),
                    editor::PdfPurpose::Reading);
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
    readLayout("export.content");
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
    for (QCheckBox* box : {m_commentsBox, m_copyImagesBox, m_pdfCommentsBox, m_bookmarksBox,
                           m_facingPagesBox, m_grayscaleBox, m_embedFontBox}) {
        connect(box, &QCheckBox::toggled, this, &ExportBookDialog::dropFailure);
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

    setFolder(folder);
    showFormat();
    showPurpose();
    showCover();
    ExportBookDialog::contentChanged();

    // The name is typed over, without its extension
    m_nameField->setFocus();
    m_nameField->setSelection(0, static_cast<int>(baseName().size()));
}

ExportBookDialog::~ExportBookDialog() = default;

ExportBookDialog::Choice ExportBookDialog::choice() const
{
    Choice choice;
    static_cast<Layout&>(choice) = layoutChoice();
    choice.formatId = m_formatBox->currentData().toString();
    choice.format = currentFormat();
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
    choice.coverImage = choice.cover == Cover::Design ? book().coverFile
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

void ExportBookDialog::setFolder(const QString& folder)
{
    m_folder = ProgramFolders::cleanPath(folder);
    showPath(m_folderField, m_folder);
    dropFailure();
    ExportBookDialog::updateFields();
}

QString ExportBookDialog::filePath() const
{
    if (!ProgramFolders::problemOf(m_folder).isEmpty() || !nameProblem().isEmpty()) {
        return QString();
    }
    const QString path = QDir(m_folder).filePath(fileName());
    return QFileInfo(path).isDir() ? QString() : path;
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

void ExportBookDialog::showBookData()
{
    const Book& shown = book();
    m_titleAndAuthor->setText(shown.author.trimmed().isEmpty()
                                  ? shown.title
                                  //: The title and the author of an EPUB; %1: the title, %2:
                                  //: the author. In Polish: %1 – %2
                                  : tr("%1 – %2").arg(shown.title, shown.author));
    //: The language of a book that has none. In Polish: Nie ustawiono
    m_languageLabel->setText(shown.language.isEmpty() ? tr("Not set") : shown.language);
    const QString description = shown.description.simplified();
    m_descriptionLabel->setText(
        description.isEmpty()
            //: The description of a book that has none. In Polish: Brak
            ? tr("None")
            : fontMetrics().elidedText(
                  //: A description in quotes; %1: its text. In Polish: „%1”
                  tr("\"%1\"").arg(description), Qt::ElideMiddle,
                  widthOfLetters(fontMetrics(), DESCRIPTION_LETTERS)));
    m_descriptionLabel->setToolTip(description);
    m_isbnField->setText(shown.isbn);
    m_publisherField->setText(shown.publisher);
}

void ExportBookDialog::showCoverDesign()
{
    // The design of the book first in the list, chosen when the one before was chosen
    const auto designData = static_cast<int>(Cover::Design);
    const int before = m_coverBox->findData(designData);
    const bool chosen = before >= 0 && m_coverBox->currentIndex() == before;
    const QSignalBlocker blocker(m_coverBox);
    if (before >= 0) {
        m_coverBox->removeItem(before);
    }
    if (book().coverFile.isEmpty()) {
        return;
    }
    //: The cover of an EPUB: the design chosen in the materials; %1: its title. In Polish:
    //: %1 (z materiałów)
    m_coverBox->insertItem(0, tr("%1 (from the materials)").arg(book().coverTitle), designData);
    if (chosen) {
        m_coverBox->setCurrentIndex(0);
    }
}

void ExportBookDialog::bookChanged()
{
    showBookData();
    showCoverDesign();
    showCover();
}

void ExportBookDialog::showCover()
{
    const auto cover = static_cast<Cover>(m_coverBox->currentData().toInt());
    const QString path = cover == Cover::Design ? book().coverFile
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
                     book().coverFile.isEmpty()
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
    setPagesVisible(paged);
    m_epubLayout->setVisible(epub);
    // The groups of the annotations and of the images stay for the program's formats: each
    // says what its format does. A plugin's format has its own tab instead.
    m_commentsOption->setVisible(word);
    m_pdfCommentsOption->setVisible(pdf);
    m_noCommentsNote->setVisible(markdown || epub);
    if (epub) {
        //: In the group of the annotations, for an EPUB. In Polish: EPUB nie ma komentarzy: uwagi
        //: zostają tylko w książce.
        m_noCommentsNote->setText(
            tr("EPUB has no comments: the annotations stay only in the book."));
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
    QTabWidget* tabs = this->tabs();
    tabs->setTabVisible(tabs->indexOf(m_extrasTab), word || pdf || markdown || epub);
    tabs->setTabVisible(tabs->indexOf(m_pdfTab), pdf);
    tabs->setTabVisible(tabs->indexOf(m_epubTab), epub);
    for (auto page = m_formatPages.cbegin(); page != m_formatPages.cend(); ++page) {
        tabs->setTabVisible(tabs->indexOf(page.value()), page.key() == id);
    }
    ExportBookDialog::contentChanged();
}

void ExportBookDialog::contentChanged()
{
    // The set for a publisher goes as one text with its title
    const bool front = oneText();
    const QString id = m_formatBox->currentData().toString();
    const QString title = contentTitle();
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
    ExportBookDialog::updateFields();
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
    const bool contentUsable = updateChosen();

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

void ExportBookDialog::rememberChoices() const
{
    auto& settings = core::SettingsManager::getInstance();
    const Choice now = choice();
    rememberLayout("export.content");
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
    const QString name = safeName(contentTitle());
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

} // namespace kalahari::gui::dialogs
