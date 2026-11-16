// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file edit_dialogs.cpp
/// @brief The windows of the Edit menu: Paste Special... and Go To...

#include "kalahari/gui/dialogs/edit_dialogs.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/text_places.h"
#include "kalahari/editor/view_modes.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/settings_dialog.h"

#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPalette>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace kalahari::gui::dialogs {

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

/// A list as high as its lines, never lower: it never scrolls
class LinesList : public QListWidget {
public:
    LinesList(int lines, int widthChars, QWidget* parent)
        : QListWidget(parent),
          m_lines(lines),
          m_widthChars(widthChars)
    {
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {(fontMetrics().averageCharWidth() * m_widthChars) + (2 * frameWidth()),
                linesHeight()};
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return {QListWidget::minimumSizeHint().width(), linesHeight()};
    }

private:
    /// Each of its rows as high as it is: a letter in another font (one the font of the list
    /// does not have) makes its row higher than the others; a line it has no row for yet as
    /// high as the highest one
    [[nodiscard]] int linesHeight() const
    {
        int height = 0;
        int highest = fontMetrics().lineSpacing();
        for (int row = 0; row < count(); ++row) {
            const int each = sizeHintForRow(row) > 0 ? sizeHintForRow(row) : highest;
            highest = std::max(highest, each);
            height += each;
        }
        height += std::max(0, m_lines - count()) * highest;
        return height + (2 * frameWidth());
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

/// The name of a form of Paste Special in its list
QString formName(editor::PasteForm form)
{
    switch (form) {
        case editor::PasteForm::Kalahari:
            //: In Polish: Format Kalahari (z uwagami)
            return PasteSpecialDialog::tr("Kalahari Format (with annotations)");
        case editor::PasteForm::Source:
            //: In Polish: Zachowaj formatowanie źródła
            return PasteSpecialDialog::tr("Keep Source Formatting");
        case editor::PasteForm::Merge:
            //: In Polish: Scal formatowanie
            return PasteSpecialDialog::tr("Merge Formatting");
        case editor::PasteForm::Text:
            //: In Polish: Tylko tekst
            return PasteSpecialDialog::tr("Text Only");
        case editor::PasteForm::Image:
            //: In Polish: Obraz
            return PasteSpecialDialog::tr("Image");
    }
    return {};
}

/// Why a form of Paste Special is greyed out: the clipboard has no such content
QString whyNoForm(editor::PasteForm form, const PasteSpecialDialog::Data& data)
{
    switch (form) {
        case editor::PasteForm::Kalahari:
            //: In Polish: Format Kalahari jest dostępny, gdy tekst skopiowano w Kalahari.
            return PasteSpecialDialog::tr(
                "The Kalahari format is there when the text was copied in Kalahari.");
        case editor::PasteForm::Source:
        case editor::PasteForm::Merge:
            //: In Polish: Schowek nie ma tekstu z formatowaniem.
            return PasteSpecialDialog::tr("The clipboard has no formatted text.");
        case editor::PasteForm::Text:
            //: In Polish: Schowek nie ma tekstu.
            return PasteSpecialDialog::tr("The clipboard has no text.");
        case editor::PasteForm::Image:
            //: In Polish: Schowek nie ma obrazu.
            return data.picture && !data.whyNoImage.isEmpty()
                       ? data.whyNoImage
                       : PasteSpecialDialog::tr("The clipboard has no picture.");
    }
    return {};
}

/// What a form of Paste Special keeps; a picture from @p source
QString formHint(editor::PasteForm form, editor::ClipboardSource source)
{
    switch (form) {
        case editor::PasteForm::Kalahari:
            //: In Polish: Wszystko, co tekst miał w Kalahari: style, uwagi, przypisy, obrazy
            //: i pola.
            return PasteSpecialDialog::tr("Everything the text had in Kalahari: its styles, "
                                          "annotations, notes, images and fields.");
        case editor::PasteForm::Source:
            //: In Polish: Zostają nagłówki, cytaty, akapity, wyróżnienia, listy i tabele,
            //: a także czcionka, rozmiar, kolor i wyrównanie źródła.
            return PasteSpecialDialog::tr("Headings, quotes, paragraphs, emphasis, lists and "
                                          "tables stay, and so do the font, size, color and "
                                          "alignment of the source.");
        case editor::PasteForm::Merge:
            //: In Polish: Zostają nagłówki, cytaty, akapity, wyróżnienia, listy i tabele;
            //: czcionka, rozmiar i kolor są takie jak w książce.
            return PasteSpecialDialog::tr("Headings, quotes, paragraphs, emphasis, lists and "
                                          "tables stay; the font, size and color are those of "
                                          "the book.");
        case editor::PasteForm::Text:
            //: In Polish: Sam tekst z akapitami, w wyglądzie miejsca, do którego trafia.
            return PasteSpecialDialog::tr("Only the text, with its paragraphs, in the look of "
                                          "the place it goes to.");
        case editor::PasteForm::Image:
            return source == editor::ClipboardSource::ImageFiles
                       //: In Polish: Pliki obrazów jako obrazy w tekście.
                       ? PasteSpecialDialog::tr("The files of pictures as images of the text.")
                       //: In Polish: Obraz ze schowka (np. zrzut ekranu) jako obraz w tekście.
                       : PasteSpecialDialog::tr("The picture of the clipboard (a screenshot, "
                                                "say) as an image of the text.");
    }
    return {};
}

/// The form of Paste Special of a paste mode
editor::PasteForm formOfMode(editor::PasteMode mode)
{
    switch (mode) {
        case editor::PasteMode::Source:
            return editor::PasteForm::Source;
        case editor::PasteMode::Text:
            return editor::PasteForm::Text;
        case editor::PasteMode::Merge:
            break;
    }
    return editor::PasteForm::Merge;
}

/// The form chosen first, as Paste gives it: text copied in Kalahari as it was, else in the
/// form of the paste mode of the setting (@p ofMode), else the first of @p forms; @p none
/// without forms
editor::PasteForm firstForm(const std::vector<editor::PasteForm>& forms,
                            editor::PasteForm ofMode, editor::PasteForm none)
{
    for (const editor::PasteForm form : {editor::PasteForm::Kalahari, ofMode}) {
        if (std::ranges::find(forms, form) != forms.end()) {
            return form;
        }
    }
    return forms.empty() ? none : forms.front();
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

    addForms();
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

void PasteSpecialDialog::addForms()
{
    m_forms = new LinesList(static_cast<int>(PASTE_FORMS.size()), FORMS_WIDTH_CHARS, this);
    for (const editor::PasteForm form : PASTE_FORMS) {
        auto* item = new QListWidgetItem(formName(form), m_forms);
        item->setData(Qt::UserRole, static_cast<int>(form));
        if (std::ranges::find(m_data.forms, form) == m_data.forms.end()) {
            item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            item->setToolTip(whyNoForm(form, m_data));
        }
    }
    const auto row = std::ranges::find(PASTE_FORMS, m_data.chosen) - PASTE_FORMS.begin();
    m_forms->setCurrentRow(static_cast<int>(row));
    //: The list of the forms. In Polish: &Wklej jako
    addField(tr("Paste &as"), m_forms);

    m_hint = hintLabel(this, HINT_LINES);
    contentLayout()->addWidget(m_hint);
}

editor::PasteForm PasteSpecialDialog::form() const
{
    const QListWidgetItem* item = m_forms->currentItem();
    return item != nullptr ? static_cast<editor::PasteForm>(item->data(Qt::UserRole).toInt())
                           : m_data.chosen;
}

void PasteSpecialDialog::showHint()
{
    const editor::PasteForm chosen = form();
    QStringList hint{formHint(chosen, m_data.source)};
    const bool fromKalahari = m_data.source == editor::ClipboardSource::Kalahari;
    if (fromKalahari &&
        (chosen == editor::PasteForm::Source || chosen == editor::PasteForm::Merge)) {
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
    shown.picture = data != nullptr && (data->hasImage() ||
                                        !editor::ClipboardHandler::pictureFilesOf(*data).isEmpty());
    if (shown.picture && !text.isReadOnly() && text.imagesFolder().isEmpty()) {
        //: Why Image is greyed out in a new file. In Polish: Najpierw zapisz plik: jego obrazy
        //: są przechowywane w folderze obok niego
        shown.whyNoImage =
            tr("Save the file first: its images are kept in a folder next to it");
    }
    // As Paste gives it: text copied in Kalahari as it was, else in the mode of the setting
    const editor::PasteMode mode = editor::ClipboardHandler::pasteModeFromId(
        QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
            editor::PASTE_MODE_KEY, std::string(editor::PASTE_MODE_MERGE))));
    shown.chosen = firstForm(shown.forms, formOfMode(mode), shown.chosen);
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
    return {};
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
    return {};
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
    return {};
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
    return {};
}

/// Why Page is greyed out in the views without pages
QString noPagesText()
{
    //: In Polish: Strony są widoczne w widoku „Układ strony” (Widok > Tryb widoku).
    return GoToDialog::tr("The pages show in the Page Layout view (View > View Mode).");
}

/// The sentence of an outcome of Go To at a page
QString pageOutcomeText(const editor::GoToOutcome& outcome)
{
    switch (outcome.result) {
        case editor::GoToResult::Done:
            //: Where Go To went. In Polish: Strona %1 z %2
            return GoToDialog::tr("Page %1 of %2").arg(outcome.ordinal).arg(outcome.count);
        case editor::GoToResult::NoPlaces:
            return noPagesText();
        case editor::GoToResult::NoFurther:
            //: In Polish: To ostatnia strona tekstu.
            return GoToDialog::tr("This is the last page of the text.");
        case editor::GoToResult::NoEarlier:
            //: In Polish: To pierwsza strona tekstu.
            return GoToDialog::tr("This is the first page of the text.");
        default:
            //: In Polish: Tekst ma %n stronę / strony / stron.
            return GoToDialog::tr("The text has %n page(s).", nullptr, outcome.count);
    }
}

/// The sentence of an outcome of Go To at a heading
QString headingOutcomeText(const editor::GoToOutcome& outcome)
{
    switch (outcome.result) {
        case editor::GoToResult::Done:
            //: In Polish: Nagłówek %1 z %2
            return GoToDialog::tr("Heading %1 of %2").arg(outcome.ordinal).arg(outcome.count);
        case editor::GoToResult::NoPlaces:
            //: In Polish: Tekst nie ma nagłówków.
            return GoToDialog::tr("The text has no headings.");
        case editor::GoToResult::NoFurther:
            //: In Polish: Za kursorem nie ma już nagłówka.
            return GoToDialog::tr("There is no heading after the cursor.");
        case editor::GoToResult::NoEarlier:
            //: In Polish: Przed kursorem nie ma nagłówka.
            return GoToDialog::tr("There is no heading before the cursor.");
        default:
            //: In Polish: Tekst ma %n nagłówek / nagłówki / nagłówków.
            return GoToDialog::tr("The text has %n heading(s).", nullptr, outcome.count);
    }
}

/// The sentence of an outcome of Go To at a note
QString noteOutcomeText(const editor::GoToOutcome& outcome)
{
    switch (outcome.result) {
        case editor::GoToResult::Done:
            //: %1: the number the note shows, %2 and %3: which one of the notes of
            //: the text it is. In Polish: Przypis %1 (%2 z %3)
            return GoToDialog::tr("Note %1 (%2 of %3)")
                .arg(outcome.number)
                .arg(outcome.ordinal)
                .arg(outcome.count);
        case editor::GoToResult::NoPlaces:
            //: In Polish: Tekst nie ma przypisów.
            return GoToDialog::tr("The text has no footnotes or endnotes.");
        case editor::GoToResult::NoFurther:
            //: In Polish: Za kursorem nie ma już przypisu.
            return GoToDialog::tr("There is no note after the cursor.");
        case editor::GoToResult::NoEarlier:
            //: In Polish: Przed kursorem nie ma przypisu.
            return GoToDialog::tr("There is no note before the cursor.");
        default:
            //: In Polish: Tekst nie ma przypisu o tym numerze; ma %n przypis / przypisy
            //: / przypisów.
            return GoToDialog::tr("The text has no note of this number; it has %n note(s).",
                      nullptr, outcome.count);
    }
}

/// The sentence of an outcome of Go To at an image
QString imageOutcomeText(const editor::GoToOutcome& outcome)
{
    switch (outcome.result) {
        case editor::GoToResult::Done:
            //: In Polish: Obraz %1 z %2
            return GoToDialog::tr("Image %1 of %2").arg(outcome.ordinal).arg(outcome.count);
        case editor::GoToResult::NoPlaces:
            //: In Polish: Tekst nie ma obrazów.
            return GoToDialog::tr("The text has no images.");
        case editor::GoToResult::NoFurther:
            //: In Polish: Za kursorem nie ma już obrazu.
            return GoToDialog::tr("There is no image after the cursor.");
        case editor::GoToResult::NoEarlier:
            //: In Polish: Przed kursorem nie ma obrazu.
            return GoToDialog::tr("There is no image before the cursor.");
        default:
            //: In Polish: Tekst ma %n obraz / obrazy / obrazów.
            return GoToDialog::tr("The text has %n image(s).", nullptr, outcome.count);
    }
}

/// The sentence of an outcome of Go To at a table
QString tableOutcomeText(const editor::GoToOutcome& outcome)
{
    switch (outcome.result) {
        case editor::GoToResult::Done:
            //: In Polish: Tabela %1 z %2
            return GoToDialog::tr("Table %1 of %2").arg(outcome.ordinal).arg(outcome.count);
        case editor::GoToResult::NoPlaces:
            //: In Polish: Tekst nie ma tabel.
            return GoToDialog::tr("The text has no tables.");
        case editor::GoToResult::NoFurther:
            //: In Polish: Za kursorem nie ma już tabeli.
            return GoToDialog::tr("There is no table after the cursor.");
        case editor::GoToResult::NoEarlier:
            //: In Polish: Przed kursorem nie ma tabeli.
            return GoToDialog::tr("There is no table before the cursor.");
        default:
            //: In Polish: Tekst ma %n tabelę / tabele / tabel.
            return GoToDialog::tr("The text has %n table(s).", nullptr, outcome.count);
    }
}

/// The sentence of an outcome of Go To at an annotation
QString annotationOutcomeText(const editor::GoToOutcome& outcome)
{
    switch (outcome.result) {
        case editor::GoToResult::Done:
            //: In Polish: Uwaga %1 z %2
            return GoToDialog::tr("Annotation %1 of %2").arg(outcome.ordinal).arg(outcome.count);
        case editor::GoToResult::NoPlaces:
            //: In Polish: Tekst nie ma uwag.
            return GoToDialog::tr("The text has no annotations.");
        case editor::GoToResult::NoFurther:
            //: In Polish: Za kursorem nie ma już uwagi.
            return GoToDialog::tr("There is no annotation after the cursor.");
        case editor::GoToResult::NoEarlier:
            //: In Polish: Przed kursorem nie ma uwagi.
            return GoToDialog::tr("There is no annotation before the cursor.");
        default:
            //: In Polish: Tekst ma %n uwagę / uwagi / uwag.
            return GoToDialog::tr("The text has %n annotation(s).", nullptr, outcome.count);
    }
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
    row->addLayout(kindsColumn());
    row->addLayout(numberColumn(), 1);
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

QVBoxLayout* GoToDialog::kindsColumn()
{
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
    return kinds;
}

QVBoxLayout* GoToDialog::numberColumn()
{
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
    return number;
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
    if (const QListWidgetItem* const item = m_kinds->item(row);
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
    if (outcome.result == editor::GoToResult::Invalid) {
        //: In Polish: Wpisz numer, np. 3, +2 albo -1.
        return kind == editor::PlaceKind::Note ? tr("Type the note's number, as 3, +2 or -1.")
                                               : tr("Type a number, as 3, +2 or -1.");
    }
    switch (kind) {
        case editor::PlaceKind::Page:
            return pageOutcomeText(outcome);
        case editor::PlaceKind::Heading:
            return headingOutcomeText(outcome);
        case editor::PlaceKind::Note:
            return noteOutcomeText(outcome);
        case editor::PlaceKind::Image:
            return imageOutcomeText(outcome);
        case editor::PlaceKind::Table:
            return tableOutcomeText(outcome);
        case editor::PlaceKind::Annotation:
            return annotationOutcomeText(outcome);
    }
    return {};
}

}  // namespace kalahari::gui::dialogs
