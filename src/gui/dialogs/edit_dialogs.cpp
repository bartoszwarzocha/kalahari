/// @file edit_dialogs.cpp
/// @brief The windows of the Edit menu: Paste Special... and Go To...

#include "kalahari/gui/dialogs/edit_dialogs.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/settings_dialog.h"

#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPalette>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// The width of the list of the forms, in average characters
constexpr int FORMS_WIDTH_CHARS = 42;

/// The width of the list of the kinds of places, in average characters
constexpr int KINDS_WIDTH_CHARS = 20;

/// The width of the column of the number, in average characters
constexpr int NUMBER_WIDTH_CHARS = 34;

/// The lines the hints keep room for (the windows keep their height)
constexpr int HINT_LINES = 4;

/// The forms of Paste Special, in the order of its list
constexpr std::array<editor::PasteForm, 5> PASTE_FORMS = {
    editor::PasteForm::Kalahari, editor::PasteForm::Source, editor::PasteForm::Merge,
    editor::PasteForm::Text, editor::PasteForm::Image};

/// A list as high as its lines
class LinesList : public QListWidget {
public:
    LinesList(int lines, int widthChars, QWidget* parent)
        : QListWidget(parent),
          m_lines(lines),
          m_widthChars(widthChars)
    {
        setMinimumHeight(linesHeight());
    }

    QSize sizeHint() const override
    {
        return {fontMetrics().averageCharWidth() * m_widthChars + 2 * frameWidth(),
                linesHeight()};
    }

private:
    [[nodiscard]] int linesHeight() const
    {
        const int row = sizeHintForRow(0) > 0 ? sizeHintForRow(0) : fontMetrics().lineSpacing();
        return row * m_lines + 2 * frameWidth();
    }

    int m_lines;
    int m_widthChars;
};

/// A sentence under the fields, in the color of hints, with room for its lines
QLabel* hintLabel(QWidget* parent, int lines)
{
    auto* hint = new QLabel(parent);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    hint->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    hint->setMinimumHeight(hint->fontMetrics().lineSpacing() * lines);
    return hint;
}

}  // namespace

// =============================================================================
// Edit > Paste Special...
// =============================================================================

PasteSpecialDialog::PasteSpecialDialog(Data shown, QWidget* parent)
    : KalahariDialog(parent),
      m_data(std::move(shown))
{
    //: In Polish: Wklej specjalnie / Postać, w jakiej zawartość schowka trafi do tekstu.
    setHeading(tr("Paste Special"),
               tr("The form the content of the clipboard goes into the text in."));
    setHeadingIcon(QStringLiteral("edit.pasteSpecial"));
    //: In Polish: Wklej
    setAcceptText(tr("Paste"));
    //: The tooltip of the gear of Paste Special: the Paste Command chooses the form Paste
    //: gives the text of other programs. In Polish: Ustawienia wklejania
    addSettingsButton(QString::fromLatin1(SettingsDialog::PASTE_PAGE), tr("Paste Settings"));

    //: Where the content of the clipboard comes from. %1: "Microsoft Word". In Polish:
    //: Źródło: %1
    auto* source = new QLabel(
        tr("Source: %1")
            .arg(QStringLiteral("<b>%1</b>").arg(sourceName(m_data.source).toHtmlEscaped())),
        this);
    source->setTextFormat(Qt::RichText);
    contentLayout()->addWidget(source);

    m_forms = new LinesList(static_cast<int>(PASTE_FORMS.size()), FORMS_WIDTH_CHARS, this);
    for (const editor::PasteForm form : PASTE_FORMS) {
        QString name;
        QString whyNot;
        switch (form) {
            case editor::PasteForm::Kalahari:
                //: In Polish: Format Kalahari (z uwagami)
                name = tr("Kalahari Format (with annotations)");
                //: In Polish: Format Kalahari jest dostępny, gdy tekst skopiowano w Kalahari.
                whyNot = tr("The Kalahari format is there when the text was copied in Kalahari.");
                break;
            case editor::PasteForm::Source:
                //: In Polish: Zachowaj formatowanie źródła
                name = tr("Keep Source Formatting");
                //: In Polish: Schowek nie ma tekstu z formatowaniem.
                whyNot = tr("The clipboard has no formatted text.");
                break;
            case editor::PasteForm::Merge:
                //: In Polish: Scal formatowanie
                name = tr("Merge Formatting");
                whyNot = tr("The clipboard has no formatted text.");
                break;
            case editor::PasteForm::Text:
                //: In Polish: Tylko tekst
                name = tr("Text Only");
                //: In Polish: Schowek nie ma tekstu.
                whyNot = tr("The clipboard has no text.");
                break;
            case editor::PasteForm::Image:
                //: In Polish: Obraz
                name = tr("Image");
                //: In Polish: Schowek nie ma obrazu.
                whyNot = m_data.picture && !m_data.whyNoImage.isEmpty()
                             ? m_data.whyNoImage
                             : tr("The clipboard has no picture.");
                break;
        }
        auto* item = new QListWidgetItem(name, m_forms);
        item->setData(Qt::UserRole, static_cast<int>(form));
        if (std::find(m_data.forms.begin(), m_data.forms.end(), form) == m_data.forms.end()) {
            item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            item->setToolTip(whyNot);
        }
    }
    const auto chosen = std::find(PASTE_FORMS.begin(), PASTE_FORMS.end(), m_data.chosen);
    m_forms->setCurrentRow(static_cast<int>(chosen - PASTE_FORMS.begin()));
    //: The list of the forms. In Polish: &Wklej jako
    addField(tr("Paste &as"), m_forms);

    m_hint = hintLabel(this, HINT_LINES);
    contentLayout()->addWidget(m_hint);
    showHint();

    acceptButton()->setEnabled(!m_data.forms.empty());
    connect(m_forms, &QListWidget::currentRowChanged, this, &PasteSpecialDialog::showHint);
    connect(m_forms, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if ((item->flags() & Qt::ItemIsEnabled) != 0) {
            accept();
        }
    });
    m_forms->setFocus();
}

editor::PasteForm PasteSpecialDialog::form() const
{
    const QListWidgetItem* item = m_forms->currentItem();
    return item != nullptr ? static_cast<editor::PasteForm>(item->data(Qt::UserRole).toInt())
                           : m_data.chosen;
}

void PasteSpecialDialog::showHint()
{
    QStringList hint;
    switch (form()) {
        case editor::PasteForm::Kalahari:
            //: In Polish: Wszystko, co tekst miał w Kalahari: style, uwagi, przypisy, obrazy
            //: i pola.
            hint << tr("Everything the text had in Kalahari: its styles, annotations, notes, "
                       "images and fields.");
            break;
        case editor::PasteForm::Source:
            //: In Polish: Zostają nagłówki, cytaty, akapity, wyróżnienia, listy i tabele,
            //: a także czcionka, rozmiar, kolor i wyrównanie źródła.
            hint << tr("Headings, quotes, paragraphs, emphasis, lists and tables stay, and so do "
                       "the font, size, color and alignment of the source.");
            break;
        case editor::PasteForm::Merge:
            //: In Polish: Zostają nagłówki, cytaty, akapity, wyróżnienia, listy i tabele;
            //: czcionka, rozmiar i kolor są takie jak w książce.
            hint << tr("Headings, quotes, paragraphs, emphasis, lists and tables stay; the font, "
                       "size and color are those of the book.");
            break;
        case editor::PasteForm::Text:
            //: In Polish: Sam tekst z akapitami, w wyglądzie miejsca, do którego trafia.
            hint << tr("Only the text, with its paragraphs, in the look of the place it goes "
                       "to.");
            break;
        case editor::PasteForm::Image:
            hint << (m_data.source == editor::ClipboardSource::ImageFiles
                         //: In Polish: Pliki obrazów jako obrazy w tekście.
                         ? tr("The files of pictures as images of the text.")
                         //: In Polish: Obraz ze schowka (np. zrzut ekranu) jako obraz w tekście.
                         : tr("The picture of the clipboard (a screenshot, say) as an image of "
                              "the text."));
            break;
    }
    const bool fromKalahari = m_data.source == editor::ClipboardSource::Kalahari;
    if (fromKalahari &&
        (form() == editor::PasteForm::Source || form() == editor::PasteForm::Merge)) {
        //: In Polish: Tekst skopiowany w Kalahari przychodzi bez uwag i obrazów, tak jak
        //: z innego programu.
        hint << tr("Text copied in Kalahari comes without its annotations and images, as from "
                   "another program.");
    }
    if (!fromKalahari && !m_data.forms.empty()) {
        hint << tr("The Kalahari format is there when the text was copied in Kalahari.");
    }
    m_hint->setText(hint.join(QLatin1Char(' ')));
}

PasteSpecialDialog::Data PasteSpecialDialog::dataFor(const editor::BookEditor& text,
                                                     const QMimeData* data)
{
    Data shown;
    shown.source = editor::ClipboardHandler::sourceOf(data);
    shown.forms = text.pasteFormsOf(data);
    shown.picture =
        data != nullptr &&
        (data->hasImage() || !editor::ClipboardHandler::pictureFilesOf(*data).isEmpty());
    if (shown.picture && !text.isReadOnly() && text.imagesFolder().isEmpty()) {
        //: Why Image is greyed out in a new file. In Polish: Najpierw zapisz plik: jego obrazy
        //: są przechowywane w folderze obok niego
        shown.whyNoImage =
            tr("Save the file first: its images are kept in a folder next to it");
    }
    // As Paste gives it: text copied in Kalahari as it was, else in the mode of the setting
    const auto has = [&shown](editor::PasteForm form) {
        return std::find(shown.forms.begin(), shown.forms.end(), form) != shown.forms.end();
    };
    const editor::PasteMode mode = editor::ClipboardHandler::pasteModeFromId(
        QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
            editor::PASTE_MODE_KEY, std::string(editor::PASTE_MODE_MERGE))));
    const editor::PasteForm ofMode = mode == editor::PasteMode::Source ? editor::PasteForm::Source
                                     : mode == editor::PasteMode::Text ? editor::PasteForm::Text
                                                                       : editor::PasteForm::Merge;
    if (has(editor::PasteForm::Kalahari)) {
        shown.chosen = editor::PasteForm::Kalahari;
    } else if (has(ofMode)) {
        shown.chosen = ofMode;
    } else if (!shown.forms.empty()) {
        shown.chosen = shown.forms.front();
    }
    return shown;
}

bool PasteSpecialDialog::pasteInto(editor::BookEditor& text, QWidget* parent)
{
    const QMimeData* clipboard = QGuiApplication::clipboard()->mimeData();
    Data shown = dataFor(text, clipboard);
    if (shown.forms.empty()) {
        return false;
    }
    PasteSpecialDialog dialog(std::move(shown), parent);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    text.pasteInForm(dialog.form());
    return true;
}

QString PasteSpecialDialog::sourceName(editor::ClipboardSource source)
{
    switch (source) {
        case editor::ClipboardSource::Nothing:
            //: The clipboard is empty. In Polish: pusty schowek
            return tr("an empty clipboard");
        case editor::ClipboardSource::Kalahari:
            return QStringLiteral("Kalahari");
        case editor::ClipboardSource::Word:
            return QStringLiteral("Microsoft Word");
        case editor::ClipboardSource::LibreOffice:
            return QStringLiteral("LibreOffice");
        case editor::ClipboardSource::GoogleDocs:
            //: In Polish: Dokumenty Google
            return tr("Google Docs");
        case editor::ClipboardSource::Html:
            //: In Polish: strona internetowa albo inny program
            return tr("a web page or another program");
        case editor::ClipboardSource::Text:
            //: In Polish: tekst bez formatowania
            return tr("text without formatting");
        case editor::ClipboardSource::Image:
            //: In Polish: obraz
            return tr("a picture");
        case editor::ClipboardSource::ImageFiles:
            //: In Polish: pliki obrazów
            return tr("files of pictures");
    }
    return QString();
}

// =============================================================================
// Edit > Go To...
// =============================================================================

namespace {

/// The name of a kind in the list
QString kindName(editor::PlaceKind kind)
{
    switch (kind) {
        case editor::PlaceKind::Page:
            //: Edit > Go To..., the kinds of places. In Polish: Strona
            return GoToDialog::tr("Page");
        case editor::PlaceKind::Heading:
            //: In Polish: Nagłówek
            return GoToDialog::tr("Heading");
        case editor::PlaceKind::Note:
            //: In Polish: Przypis
            return GoToDialog::tr("Footnote or Endnote");
        case editor::PlaceKind::Image:
            //: In Polish: Obraz
            return GoToDialog::tr("Image");
        case editor::PlaceKind::Table:
            //: In Polish: Tabela
            return GoToDialog::tr("Table");
        case editor::PlaceKind::Annotation:
            //: In Polish: Uwaga
            return GoToDialog::tr("Annotation");
    }
    return QString();
}

/// The label of the number of a kind
QString numberLabel(editor::PlaceKind kind)
{
    switch (kind) {
        case editor::PlaceKind::Page:
            //: In Polish: Nu&mer strony
            return GoToDialog::tr("Page n&umber");
        case editor::PlaceKind::Heading:
            //: In Polish: Nu&mer nagłówka
            return GoToDialog::tr("Heading n&umber");
        case editor::PlaceKind::Note:
            //: In Polish: Nu&mer przypisu
            return GoToDialog::tr("Footnote or endnote n&umber");
        case editor::PlaceKind::Image:
            //: In Polish: Nu&mer obrazu
            return GoToDialog::tr("Image n&umber");
        case editor::PlaceKind::Table:
            //: In Polish: Nu&mer tabeli
            return GoToDialog::tr("Table n&umber");
        case editor::PlaceKind::Annotation:
            //: In Polish: Nu&mer uwagi
            return GoToDialog::tr("Annotation n&umber");
    }
    return QString();
}

/// What the numbers of a kind go to
QString kindHint(editor::PlaceKind kind)
{
    switch (kind) {
        case editor::PlaceKind::Page:
            //: In Polish: „3” – trzecia strona tekstu, „+2” – dwie strony dalej, „-1” – stronę
            //: wcześniej. „Następny” i „Poprzedni” przechodzą do sąsiedniej strony.
            return GoToDialog::tr("“3”: the third page of the text, “+2”: two pages on, “-1”: a "
                                  "page back. Next and Previous go to the page after or before.");
        case editor::PlaceKind::Heading:
            //: In Polish: „3” – trzeci nagłówek tekstu, „+2” – dwa nagłówki dalej, „-1” –
            //: nagłówek wcześniej. „Następny” i „Poprzedni” przechodzą do sąsiedniego nagłówka.
            return GoToDialog::tr("“3”: the third heading of the text, “+2”: two headings on, "
                                  "“-1”: a heading back. Next and Previous go to the heading "
                                  "after or before.");
        case editor::PlaceKind::Note:
            //: In Polish: „3” – przypis o numerze 3, „+2” – dwa przypisy dalej, „-1” – przypis
            //: wcześniej. „Następny” i „Poprzedni” przechodzą do sąsiedniego przypisu.
            return GoToDialog::tr("“3”: the note numbered 3, “+2”: two notes on, “-1”: a note "
                                  "back. Next and Previous go to the note after or before.");
        case editor::PlaceKind::Image:
            //: In Polish: „3” – trzeci obraz tekstu, „+2” – dwa obrazy dalej, „-1” – obraz
            //: wcześniej. „Następny” i „Poprzedni” przechodzą do sąsiedniego obrazu.
            return GoToDialog::tr("“3”: the third image of the text, “+2”: two images on, “-1”: an "
                                  "image back. Next and Previous go to the image after or before.");
        case editor::PlaceKind::Table:
            //: In Polish: „3” – trzecia tabela tekstu, „+2” – dwie tabele dalej, „-1” – tabelę
            //: wcześniej. „Następny” i „Poprzedni” przechodzą do sąsiedniej tabeli.
            return GoToDialog::tr("“3”: the third table of the text, “+2”: two tables on, “-1”: a "
                                  "table back. Next and Previous go to the table after or before.");
        case editor::PlaceKind::Annotation:
            //: In Polish: „3” – trzecia uwaga tekstu, „+2” – dwie uwagi dalej, „-1” – uwagę
            //: wcześniej. „Następny” i „Poprzedni” przechodzą do sąsiedniej uwagi.
            return GoToDialog::tr("“3”: the third annotation of the text, “+2”: two annotations "
                                  "on, “-1”: an annotation back. Next and Previous go to the "
                                  "annotation after or before.");
    }
    return QString();
}

/// Why Page is greyed out in the views without pages
QString noPagesText()
{
    //: In Polish: Strony są widoczne w widoku „Układ strony” (Widok > Tryb widoku).
    return GoToDialog::tr("The pages show in the Page Layout view (View > View Mode).");
}

}  // namespace

GoToDialog::GoToDialog(TextOf textOf, QWidget* parent)
    : KalahariDialog(parent),
      m_textOf(std::move(textOf))
{
    //: In Polish: Przejdź do / Miejsce w rozdziale według numeru albo następne i poprzednie.
    setHeading(tr("Go To"), tr("A place of the text by its number, or the next and the previous "
                               "one."));
    setHeadingIcon(QStringLiteral("edit.goTo"));
    //: In Polish: Prze&jdź
    setAcceptText(tr("&Go"));
    //: In Polish: Zamknij
    cancelButton()->setText(tr("Close"));
    //: In Polish: &Poprzedni
    m_previous = addSideButton(tr("&Previous"));
    //: In Polish: &Następny
    m_next = addSideButton(tr("&Next"));

    // The kinds at the left, the number with its hint at the right
    auto* row = new QHBoxLayout();
    auto* kinds = new QVBoxLayout();
    kinds->setSpacing(fontMetrics().lineSpacing() / 4);
    m_kinds = new LinesList(static_cast<int>(editor::PLACE_KINDS.size()), KINDS_WIDTH_CHARS,
                            this);
    for (const editor::PlaceKind kind : editor::PLACE_KINDS) {
        auto* item = new QListWidgetItem(kindName(kind), m_kinds);
        item->setData(Qt::UserRole, static_cast<int>(kind));
    }
    //: The list of the kinds of places. In Polish: Przejdź &do
    auto* kindsLabel = new QLabel(tr("Go &to"), this);
    kindsLabel->setBuddy(m_kinds);
    kinds->addWidget(kindsLabel);
    kinds->addWidget(m_kinds);
    kinds->addStretch(1);
    row->addLayout(kinds);

    auto* number = new QVBoxLayout();
    number->setSpacing(fontMetrics().lineSpacing() / 4);
    m_numberLabel = new QLabel(this);
    m_number = new QLineEdit(this);
    m_number->setMinimumWidth(fontMetrics().averageCharWidth() * NUMBER_WIDTH_CHARS);
    m_number->setClearButtonEnabled(true);
    m_numberLabel->setBuddy(m_number);
    m_hint = hintLabel(this, HINT_LINES);
    m_result = new QLabel(this);
    m_result->setWordWrap(true);
    m_result->setMinimumHeight(m_result->fontMetrics().lineSpacing() * 2);
    m_result->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_result->setAccessibleName(tr("What Go To did"));
    number->addWidget(m_numberLabel);
    number->addWidget(m_number);
    number->addWidget(m_hint);
    number->addWidget(m_result);
    number->addStretch(1);
    row->addLayout(number, 1);
    contentLayout()->addLayout(row);

    m_kinds->setCurrentRow(static_cast<int>(editor::PlaceKind::Heading));
    showKind();
    connect(m_kinds, &QListWidget::currentRowChanged, this, [this]() {
        showKind();
        m_result->clear();
    });
    connect(m_previous, &QPushButton::clicked, this, [this]() { goAdjacent(false); });
    connect(m_next, &QPushButton::clicked, this, [this]() { goAdjacent(true); });
    m_number->setFocus();
}

editor::PlaceKind GoToDialog::kind() const
{
    const QListWidgetItem* item = m_kinds->currentItem();
    return item != nullptr ? static_cast<editor::PlaceKind>(item->data(Qt::UserRole).toInt())
                           : editor::PlaceKind::Heading;
}

void GoToDialog::setKind(editor::PlaceKind kind)
{
    const int row = static_cast<int>(kind);
    if (QListWidgetItem* item = m_kinds->item(row);
        item != nullptr && (item->flags() & Qt::ItemIsEnabled) != 0) {
        m_kinds->setCurrentRow(row);
    }
}

QString GoToDialog::result() const
{
    return m_result->text();
}

void GoToDialog::updateTarget()
{
    const editor::BookEditor* text = m_textOf ? m_textOf() : nullptr;
    const bool pages = text != nullptr && text->viewMode() == editor::ViewMode::Page;
    QListWidgetItem* page = m_kinds->item(static_cast<int>(editor::PlaceKind::Page));
    if (pages) {
        page->setFlags(page->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        page->setToolTip(QString());
    } else {
        page->setFlags(page->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        page->setToolTip(noPagesText());
        if (kind() == editor::PlaceKind::Page) {
            m_kinds->setCurrentRow(static_cast<int>(editor::PlaceKind::Heading));
        }
    }
    for (QPushButton* button : {acceptButton(), m_previous, m_next}) {
        button->setEnabled(text != nullptr);
    }
    if (text == nullptr) {
        //: In Polish: Żaden rozdział ani plik nie jest otwarty
        m_result->setText(tr("No chapter or file is open"));
    }
}

void GoToDialog::accept()
{
    updateTarget();
    editor::BookEditor* text = m_textOf ? m_textOf() : nullptr;
    if (text == nullptr) {
        return;
    }
    if (m_number->text().trimmed().isEmpty()) {
        goAdjacent(true);
        return;
    }
    m_result->setText(outcomeText(kind(), text->goToPlace(kind(), m_number->text())));
}

void GoToDialog::goAdjacent(bool next)
{
    updateTarget();
    editor::BookEditor* text = m_textOf ? m_textOf() : nullptr;
    if (text == nullptr) {
        return;
    }
    m_result->setText(outcomeText(kind(), text->goToAdjacentPlace(kind(), next)));
}

void GoToDialog::showKind()
{
    m_numberLabel->setText(numberLabel(kind()));
    m_hint->setText(kindHint(kind()));
}

void GoToDialog::changeEvent(QEvent* event)
{
    KalahariDialog::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        updateTarget();
    }
}

void GoToDialog::showEvent(QShowEvent* event)
{
    KalahariDialog::showEvent(event);
    updateTarget();
}

QString GoToDialog::outcomeText(editor::PlaceKind kind, const editor::GoToOutcome& outcome)
{
    using editor::GoToResult;
    using editor::PlaceKind;
    if (outcome.result == GoToResult::Invalid) {
        //: In Polish: Wpisz numer, np. 3, +2 albo -1.
        return kind == PlaceKind::Note ? tr("Type the note's number, as 3, +2 or -1.")
                                       : tr("Type a number, as 3, +2 or -1.");
    }
    switch (kind) {
        case PlaceKind::Page:
            switch (outcome.result) {
                case GoToResult::Done:
                    //: Where Go To went. In Polish: Strona %1 z %2
                    return tr("Page %1 of %2").arg(outcome.ordinal).arg(outcome.count);
                case GoToResult::NoPlaces:
                    return noPagesText();
                case GoToResult::NoFurther:
                    //: In Polish: To ostatnia strona tekstu.
                    return tr("This is the last page of the text.");
                case GoToResult::NoEarlier:
                    //: In Polish: To pierwsza strona tekstu.
                    return tr("This is the first page of the text.");
                default:
                    //: In Polish: Tekst ma %n stronę / strony / stron.
                    return tr("The text has %n page(s).", nullptr, outcome.count);
            }
        case PlaceKind::Heading:
            switch (outcome.result) {
                case GoToResult::Done:
                    //: In Polish: Nagłówek %1 z %2
                    return tr("Heading %1 of %2").arg(outcome.ordinal).arg(outcome.count);
                case GoToResult::NoPlaces:
                    //: In Polish: Tekst nie ma nagłówków.
                    return tr("The text has no headings.");
                case GoToResult::NoFurther:
                    //: In Polish: Za kursorem nie ma już nagłówka.
                    return tr("There is no heading after the cursor.");
                case GoToResult::NoEarlier:
                    //: In Polish: Przed kursorem nie ma nagłówka.
                    return tr("There is no heading before the cursor.");
                default:
                    //: In Polish: Tekst ma %n nagłówek / nagłówki / nagłówków.
                    return tr("The text has %n heading(s).", nullptr, outcome.count);
            }
        case PlaceKind::Note:
            switch (outcome.result) {
                case GoToResult::Done:
                    //: %1: the number the note shows, %2 and %3: which one of the notes of
                    //: the text it is. In Polish: Przypis %1 (%2 z %3)
                    return tr("Note %1 (%2 of %3)")
                        .arg(outcome.number)
                        .arg(outcome.ordinal)
                        .arg(outcome.count);
                case GoToResult::NoPlaces:
                    //: In Polish: Tekst nie ma przypisów.
                    return tr("The text has no footnotes or endnotes.");
                case GoToResult::NoFurther:
                    //: In Polish: Za kursorem nie ma już przypisu.
                    return tr("There is no note after the cursor.");
                case GoToResult::NoEarlier:
                    //: In Polish: Przed kursorem nie ma przypisu.
                    return tr("There is no note before the cursor.");
                default:
                    //: In Polish: Tekst nie ma przypisu o tym numerze; ma %n przypis / przypisy
                    //: / przypisów.
                    return tr("The text has no note of this number; it has %n note(s).",
                              nullptr, outcome.count);
            }
        case PlaceKind::Image:
            switch (outcome.result) {
                case GoToResult::Done:
                    //: In Polish: Obraz %1 z %2
                    return tr("Image %1 of %2").arg(outcome.ordinal).arg(outcome.count);
                case GoToResult::NoPlaces:
                    //: In Polish: Tekst nie ma obrazów.
                    return tr("The text has no images.");
                case GoToResult::NoFurther:
                    //: In Polish: Za kursorem nie ma już obrazu.
                    return tr("There is no image after the cursor.");
                case GoToResult::NoEarlier:
                    //: In Polish: Przed kursorem nie ma obrazu.
                    return tr("There is no image before the cursor.");
                default:
                    //: In Polish: Tekst ma %n obraz / obrazy / obrazów.
                    return tr("The text has %n image(s).", nullptr, outcome.count);
            }
        case PlaceKind::Table:
            switch (outcome.result) {
                case GoToResult::Done:
                    //: In Polish: Tabela %1 z %2
                    return tr("Table %1 of %2").arg(outcome.ordinal).arg(outcome.count);
                case GoToResult::NoPlaces:
                    //: In Polish: Tekst nie ma tabel.
                    return tr("The text has no tables.");
                case GoToResult::NoFurther:
                    //: In Polish: Za kursorem nie ma już tabeli.
                    return tr("There is no table after the cursor.");
                case GoToResult::NoEarlier:
                    //: In Polish: Przed kursorem nie ma tabeli.
                    return tr("There is no table before the cursor.");
                default:
                    //: In Polish: Tekst ma %n tabelę / tabele / tabel.
                    return tr("The text has %n table(s).", nullptr, outcome.count);
            }
        case PlaceKind::Annotation:
            switch (outcome.result) {
                case GoToResult::Done:
                    //: In Polish: Uwaga %1 z %2
                    return tr("Annotation %1 of %2").arg(outcome.ordinal).arg(outcome.count);
                case GoToResult::NoPlaces:
                    //: In Polish: Tekst nie ma uwag.
                    return tr("The text has no annotations.");
                case GoToResult::NoFurther:
                    //: In Polish: Za kursorem nie ma już uwagi.
                    return tr("There is no annotation after the cursor.");
                case GoToResult::NoEarlier:
                    //: In Polish: Przed kursorem nie ma uwagi.
                    return tr("There is no annotation before the cursor.");
                default:
                    //: In Polish: Tekst ma %n uwagę / uwagi / uwag.
                    return tr("The text has %n annotation(s).", nullptr, outcome.count);
            }
    }
    return QString();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
