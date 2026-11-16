// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_caption_dialogs.cpp
/// @brief The windows of the numbered captions: Insert Caption (its label, number, position
///        and labels of the book), Caption Numbering and Insert Cross-Reference, with their
///        keys, in English and in Polish, on a small screen

#include <catch2/catch_test_macros.hpp>
#include "kalahari/editor/text_captions.h"
#include "kalahari/gui/dialogs/caption_dialogs.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/settings_dialog.h"

#include "document_window.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>

#include <functional>
#include <map>

using namespace kalahari;
using editor::CaptionNumbering;
using editor::ImageCaption;
using editor::ReferenceShows;
using gui::dialogs::CaptionNumberingDialog;
using gui::dialogs::InsertCaptionDialog;
using gui::dialogs::InsertCrossReferenceDialog;
using gui::dialogs::KalahariDialog;
using gui::dialogs::TextInputDialog;

namespace {

/// The field a label names
template <typename Field>
Field* fieldOf(const QWidget& parent, const QString& label)
{
    for (QLabel* each : parent.findChildren<QLabel*>()) {
        if (each->text() == label) {
            return qobject_cast<Field*>(each->buddy());
        }
    }
    return nullptr;
}

/// The button with the text
template <typename Button>
Button* buttonOf(const QWidget& parent, const QString& text)
{
    for (Button* each : parent.findChildren<Button*>()) {
        if (each->text() == text) {
            return each;
        }
    }
    return nullptr;
}

/// Whether a label with this text is shown
bool showsLabel(const QWidget& parent, const QString& text)
{
    for (const QLabel* each : parent.findChildren<QLabel*>()) {
        if (each->text() == text && each->isVisibleTo(&parent)) {
            return true;
        }
    }
    return false;
}

/// The access key of a text: the letter after a single "&"
QChar accessKey(const QString& text)
{
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            if (text.at(i + 1) == QLatin1Char('&')) {
                ++i;
                continue;
            }
            return text.at(i + 1).toLower();
        }
    }
    return QChar();
}

/// The texts of the labels with a field, the buttons and the groups of a window
QStringList keyTexts(const QWidget& root)
{
    QStringList texts;
    for (const QWidget* widget : root.findChildren<QWidget*>()) {
        if (const auto* label = qobject_cast<const QLabel*>(widget); label && label->buddy()) {
            texts << label->text();
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            texts << button->text();
        } else if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
            texts << group->title();
        }
    }
    return texts;
}

/// The texts with the same access key; empty when each key is the only one
QString sameKeys(const QWidget& root)
{
    std::map<QChar, QStringList> texts;
    for (const QString& text : keyTexts(root)) {
        const QChar key = accessKey(text);
        if (!key.isNull()) {
            texts[key] << text;
        }
    }
    QStringList same;
    for (const auto& [key, each] : texts) {
        if (each.size() > 1) {
            same << each.join(QStringLiteral(" / "));
        }
    }
    return same.join(QStringLiteral("; "));
}

/// The texts with an access key that is not a Latin letter or digit (Alt does not reach "ś")
QString foreignKeys(const QWidget& root)
{
    QStringList foreign;
    for (const QString& text : keyTexts(root)) {
        const QChar key = accessKey(text);
        if (!key.isNull() && (key.unicode() >= 0x80 || !key.isLetterOrNumber())) {
            foreign << text;
        }
    }
    return foreign.join(QStringLiteral(" / "));
}

/// 1366x768 at 150%: 911 pixels wide and 460 high (a Mac's larger controls need a few more)
void checkFitsSmallScreen(const QWidget& dialog)
{
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 480;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    CHECK(dialog.sizeHint().width() <= 911);
    CHECK(dialog.sizeHint().height() <= MAX_HEIGHT);
}

/// The windows in Polish while it lives: the translation the program ships
class Polish {
public:
    Polish()
    {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~Polish() { QCoreApplication::removeTranslator(&m_translator); }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

/// The program's way of opening the Settings dialog, until the test ends
class SettingsOpener {
public:
    explicit SettingsOpener(std::function<void(QWidget*, const QString&)> opener)
    {
        KalahariDialog::setSettingsOpener(std::move(opener));
    }
    ~SettingsOpener() { KalahariDialog::setSettingsOpener(nullptr); }
    SettingsOpener(const SettingsOpener&) = delete;
    SettingsOpener& operator=(const SettingsOpener&) = delete;
};

/// Type @p text into the window of a text shown next, and accept it
void typeNext(const QString& text)
{
    QTimer::singleShot(0, [text]() {
        auto* dialog = qobject_cast<TextInputDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            if (QDialog* other = test::shownDialog()) {
                other->reject();
            }
            return;
        }
        dialog->field()->setText(text);
        dialog->acceptButton()->click();
    });
}

/// Let the layouts take in what changed
void settle()
{
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
    }
}

/// What Insert Caption asks of a book with three labels, Figure used by its captions: Figure
/// gives the number 3, the others 1
InsertCaptionDialog::Data captionData(const ImageCaption& caption)
{
    InsertCaptionDialog::Data data;
    data.labels = {QStringLiteral("Figure"), QStringLiteral("Map"), QStringLiteral("Photo")};
    data.usedLabels = {QStringLiteral("Figure")};
    data.caption = caption;
    data.numberOf = [](const QString& label) {
        return label == QLatin1String("Figure") ? QStringLiteral("3") : QStringLiteral("1");
    };
    data.numberingOf = [](const QString& label) {
        CaptionNumbering numbering;
        numbering.perChapter = label == QLatin1String("Map");
        return numbering;
    };
    return data;
}

/// The captions of a book for Insert Cross-Reference: two figures, one of them in another
/// text, and a map; the cursor after the first figure
InsertCrossReferenceDialog::Data referenceData()
{
    InsertCrossReferenceDialog::Data data;
    data.labels = {QStringLiteral("Figure"), QStringLiteral("Map")};
    data.targets = {
        {QStringLiteral("f1"), QStringLiteral("Figure"), QStringLiteral("1"),
         QStringLiteral("Figure 1. The bridge"), QString(), true},
        {QStringLiteral("f2"), QStringLiteral("Figure"), QStringLiteral("2"),
         QStringLiteral("Figure 2. The river"), QStringLiteral("The Calm"), false},
        {QStringLiteral("m1"), QStringLiteral("Map"), QStringLiteral("1"),
         QStringLiteral("Map 1. The town"), QString(), false}};
    return data;
}

}  // namespace

// =============================================================================
// Insert Caption
// =============================================================================

TEST_CASE("Insert Caption window: the label and number before the text, below or above",
          "[gui][captions][dialog]") {
    QString openedPage;
    const SettingsOpener opener([&openedPage](QWidget*, const QString& page) { openedPage = page; });
    InsertCaptionDialog::Data data =
        captionData({false, true, QStringLiteral("Figure"), QStringLiteral("The bridge"), false});
    QStringList numbered;
    data.editNumbering = [&numbered](const QString& label, QWidget*) {
        numbered << label;
        return true;
    };
    InsertCaptionDialog dialog(data);
    dialog.show();
    settle();
    CHECK(dialog.windowTitle() == QStringLiteral("Insert Caption"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("OK"));

    // The fields as the image's caption has them, with the number the program gives
    auto* text = fieldOf<QLineEdit>(dialog, QStringLiteral("&Caption"));
    auto* label = fieldOf<QComboBox>(dialog, QStringLiteral("&Label"));
    auto* position = fieldOf<QComboBox>(dialog, QStringLiteral("&Position"));
    REQUIRE(text != nullptr);
    REQUIRE(label != nullptr);
    REQUIRE(position != nullptr);
    CHECK(text->text() == QStringLiteral("The bridge"));
    CHECK(label->currentText() == QStringLiteral("Figure"));
    CHECK(showsLabel(dialog, QStringLiteral("Figure 3.")));
    CHECK(position->currentText() == QStringLiteral("Below the image"));
    CHECK(showsLabel(dialog, QStringLiteral("Number format: 1, 2, 3 · through the whole book")));
    // A new caption: no Remove Caption
    CHECK(buttonOf<QPushButton>(dialog, QStringLiteral("Remove Capt&ion")) == nullptr);
    CHECK(dialog.caption() ==
          ImageCaption{true, true, QStringLiteral("Figure"), QStringLiteral("The bridge"), false});

    // Another label: its number and its numbering
    label->setCurrentIndex(1);
    CHECK(showsLabel(dialog, QStringLiteral("Map 1.")));
    CHECK(showsLabel(dialog, QStringLiteral("Number format: 1, 2, 3 · anew in each chapter")));
    position->setCurrentIndex(1);
    CHECK(dialog.caption() ==
          ImageCaption{true, true, QStringLiteral("Map"), QStringLiteral("The bridge"), true});

    // Numbering... asks the book about the label chosen
    QPushButton* numbering = buttonOf<QPushButton>(dialog, QStringLiteral("Nu&mbering..."));
    REQUIRE(numbering != nullptr);
    CHECK(numbering->isEnabled());
    numbering->click();
    CHECK(numbered == QStringList{QStringLiteral("Map")});

    // Without a label and number: the text alone, which it then needs
    auto* unnumbered =
        buttonOf<QCheckBox>(dialog, QStringLiteral("&Without a label and number – the text alone"));
    REQUIRE(unnumbered != nullptr);
    unnumbered->setChecked(true);
    CHECK_FALSE(label->isEnabled());
    CHECK_FALSE(numbering->isEnabled());
    CHECK_FALSE(showsLabel(dialog, QStringLiteral("Map 1.")));
    CHECK(dialog.caption() ==
          ImageCaption{true, false, QString(), QStringLiteral("The bridge"), true});
    text->setText(QStringLiteral("  "));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    text->setText(QStringLiteral("The bridge in 1931"));
    CHECK(dialog.acceptButton()->isEnabled());
    unnumbered->setChecked(false);
    CHECK(label->isEnabled());

    // The gear: Settings > Editor > Images
    QToolButton* gear = nullptr;
    for (QToolButton* each : dialog.findChildren<QToolButton*>()) {
        if (each->toolTip() == QStringLiteral("Image Settings")) {
            gear = each;
        }
    }
    REQUIRE(gear != nullptr);
    gear->click();
    CHECK(openedPage == QString::fromLatin1(gui::SettingsDialog::IMAGES_PAGE));

    CHECK(sameKeys(dialog).isEmpty());
    CHECK(foreignKeys(dialog).isEmpty());
    checkFitsSmallScreen(dialog);
}

TEST_CASE("Insert Caption window: the labels of the book, added and removed",
          "[gui][captions][dialog]") {
    // A caption of a label the book's list does not have keeps it
    InsertCaptionDialog dialog(
        captionData({true, true, QStringLiteral("Chart"), QStringLiteral("Sales"), true}));
    dialog.show();
    settle();
    auto* label = fieldOf<QComboBox>(dialog, QStringLiteral("&Label"));
    QPushButton* remove = buttonOf<QPushButton>(dialog, QStringLiteral("&Remove"));
    QPushButton* add = buttonOf<QPushButton>(dialog, QStringLiteral("&New Label..."));
    REQUIRE(label != nullptr);
    REQUIRE(remove != nullptr);
    REQUIRE(add != nullptr);
    CHECK(dialog.labels() == QStringList{QStringLiteral("Figure"), QStringLiteral("Map"),
                                         QStringLiteral("Photo"), QStringLiteral("Chart")});
    CHECK(label->currentText() == QStringLiteral("Chart"));
    CHECK(fieldOf<QComboBox>(dialog, QStringLiteral("&Position"))->currentIndex() == 1);

    // A label the captions of the book have stays
    label->setCurrentIndex(0);
    CHECK_FALSE(remove->isEnabled());
    CHECK(remove->toolTip() == QStringLiteral("Captions of the book have this label"));
    label->setCurrentIndex(1);
    REQUIRE(remove->isEnabled());
    remove->click();
    CHECK(dialog.labels() == QStringList{QStringLiteral("Figure"), QStringLiteral("Photo"),
                                         QStringLiteral("Chart")});
    CHECK(label->currentText() == QStringLiteral("Photo"));

    // A new label is chosen; one the book has in other letters is that one
    typeNext(QStringLiteral("  Table "));
    add->click();
    CHECK(dialog.labels().last() == QStringLiteral("Table"));
    CHECK(label->currentText() == QStringLiteral("Table"));
    typeNext(QStringLiteral("figure"));
    add->click();
    CHECK(dialog.labels().size() == 4);
    CHECK(label->currentText() == QStringLiteral("Figure"));

    // Without the book's numbering (a file outside a book): the Settings number it
    QPushButton* numbering = buttonOf<QPushButton>(dialog, QStringLiteral("Nu&mbering..."));
    REQUIRE(numbering != nullptr);
    CHECK_FALSE(numbering->isEnabled());
    CHECK(numbering->toolTip() ==
          QStringLiteral("A file outside a book is numbered as the Settings say (the gear)."));

    // Remove Caption: the image loses it
    QPushButton* removeCaption = buttonOf<QPushButton>(dialog, QStringLiteral("Remove Capt&ion"));
    REQUIRE(removeCaption != nullptr);
    removeCaption->click();
    CHECK(dialog.result() == QDialog::Accepted);
    CHECK_FALSE(dialog.caption().shown);
}

TEST_CASE("Insert Caption window in Polish: keys of its own, Latin letters, a small screen",
          "[gui][captions][dialog]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    InsertCaptionDialog::Data data =
        captionData({true, true, QStringLiteral("Figure"), QStringLiteral("Most"), false});
    data.editNumbering = [](const QString&, QWidget*) { return false; };
    InsertCaptionDialog dialog(data);
    dialog.show();
    settle();
    CHECK(dialog.windowTitle() == QStringLiteral("Wstaw podpis"));
    CHECK(buttonOf<QPushButton>(dialog, QStringLiteral("Usuń p&odpis")) != nullptr);
    CHECK(fieldOf<QLineEdit>(dialog, QStringLiteral("Po&dpis")) != nullptr);
    CHECK(sameKeys(dialog).isEmpty());
    CHECK(foreignKeys(dialog).isEmpty());
    checkFitsSmallScreen(dialog);
}

// =============================================================================
// Caption Numbering
// =============================================================================

TEST_CASE("Caption Numbering window: the format, anew in each chapter, the chapter's number",
          "[gui][captions][dialog]") {
    CaptionNumberingDialog dialog(QStringLiteral("Figure"), CaptionNumbering{});
    dialog.show();
    settle();
    CHECK(dialog.windowTitle() == QStringLiteral("Caption Numbering"));
    auto* format = fieldOf<QComboBox>(dialog, QStringLiteral("&Format"));
    auto* separator = fieldOf<QComboBox>(dialog, QStringLiteral("&Separator"));
    auto* continuous = buttonOf<QRadioButton>(dialog, QStringLiteral("&Continuous through the whole book"));
    auto* perChapter = buttonOf<QRadioButton>(dialog, QStringLiteral("Anew in each c&hapter"));
    auto* withChapter = buttonOf<QCheckBox>(dialog, QStringLiteral("With the chapter's &number"));
    REQUIRE(format != nullptr);
    REQUIRE(separator != nullptr);
    REQUIRE(continuous != nullptr);
    REQUIRE(perChapter != nullptr);
    REQUIRE(withChapter != nullptr);

    // Through the whole book: the chapter's number is not there to choose
    CHECK(continuous->isChecked());
    CHECK_FALSE(withChapter->isEnabled());
    CHECK_FALSE(separator->isEnabled());
    CHECK(format->currentText() == QStringLiteral("1, 2, 3"));
    CHECK(showsLabel(dialog, QStringLiteral("Example: Figure 3, Figure 4")));
    CHECK(dialog.numbering() == CaptionNumbering{});

    // Anew in each chapter, after its number and an en dash
    perChapter->setChecked(true);
    CHECK(withChapter->isEnabled());
    CHECK_FALSE(separator->isEnabled());
    withChapter->setChecked(true);
    CHECK(separator->isEnabled());
    separator->setCurrentIndex(1);
    format->setCurrentIndex(format->count() - 1);
    CHECK(format->currentText() == QStringLiteral("I, II, III"));
    const CaptionNumbering numbering = dialog.numbering();
    CHECK(numbering.format == editor::ListNumberFormat::UpperRoman);
    CHECK(numbering.perChapter);
    CHECK(numbering.withChapter);
    CHECK(numbering.separator == QStringLiteral("–"));
    CHECK(showsLabel(dialog, QStringLiteral("Example: Figure %1, Figure %2")
                                 .arg(editor::captionNumberText(3, numbering, 2),
                                      editor::captionNumberText(4, numbering, 2))));

    // Through the book again: without the chapter's number
    continuous->setChecked(true);
    CHECK_FALSE(dialog.numbering().perChapter);
    CHECK_FALSE(dialog.numbering().withChapter);

    // How Insert Caption says it
    CHECK(CaptionNumberingDialog::describe(CaptionNumbering{}) ==
          QStringLiteral("Number format: 1, 2, 3 · through the whole book"));
    CaptionNumbering chapters;
    chapters.perChapter = true;
    CHECK(CaptionNumberingDialog::describe(chapters) ==
          QStringLiteral("Number format: 1, 2, 3 · anew in each chapter"));
    chapters.withChapter = true;
    CHECK(CaptionNumberingDialog::describe(chapters) ==
          QStringLiteral("Number format: 1, 2, 3 · anew in each chapter, with the chapter's "
                         "number (2.3)"));

    CHECK(sameKeys(dialog).isEmpty());
    CHECK(foreignKeys(dialog).isEmpty());
    checkFitsSmallScreen(dialog);
}

TEST_CASE("Caption Numbering window in Polish: keys of its own, Latin letters, a small screen",
          "[gui][captions][dialog]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    CaptionNumbering numbering;
    numbering.perChapter = true;
    numbering.withChapter = true;
    CaptionNumberingDialog dialog(QStringLiteral("Rysunek"), numbering);
    dialog.show();
    settle();
    CHECK(dialog.windowTitle() == QStringLiteral("Numeracja podpisów"));
    CHECK(showsLabel(dialog, QStringLiteral("Przykład: Rysunek 2.3, Rysunek 2.4")));
    CHECK(sameKeys(dialog).isEmpty());
    CHECK(foreignKeys(dialog).isEmpty());
    checkFitsSmallScreen(dialog);
}

// =============================================================================
// Insert Cross-Reference
// =============================================================================

TEST_CASE("Insert Cross-Reference window: the kind, the caption and what the field shows",
          "[gui][captions][dialog]") {
    InsertCrossReferenceDialog dialog(referenceData());
    dialog.show();
    settle();
    CHECK(dialog.windowTitle() == QStringLiteral("Insert Cross-Reference"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Insert"));
    auto* kind = fieldOf<QComboBox>(dialog, QStringLiteral("&Kind"));
    auto* shows = fieldOf<QComboBox>(dialog, QStringLiteral("&Insert"));
    auto* targets = fieldOf<QListWidget>(dialog, QStringLiteral("&Which caption"));
    QCheckBox* link = nullptr;
    for (QCheckBox* each : dialog.findChildren<QCheckBox*>()) {
        if (each->text().startsWith(QStringLiteral("As a &link – "))) {
            link = each;
        }
    }
    REQUIRE(kind != nullptr);
    REQUIRE(shows != nullptr);
    REQUIRE(targets != nullptr);
    REQUIRE(link != nullptr);

    // The last caption before the cursor first, the number alone, as a link
    CHECK(kind->currentText() == QStringLiteral("Figure"));
    REQUIRE(targets->count() == 2);
    CHECK(targets->currentRow() == 0);
    CHECK(targets->item(1)->text() == QStringLiteral("Figure 2. The river"));
    CHECK(targets->item(1)->toolTip() == QStringLiteral("In the text: The Calm"));
    CHECK(targets->item(0)->toolTip().isEmpty());
    CHECK(shows->currentText() == QStringLiteral("The number alone – \"1\""));
    CHECK(link->isChecked());
    CHECK(dialog.reference() == editor::CrossReference{QStringLiteral("f1"), ReferenceShows::Number, true});

    // The choices name the caption chosen
    targets->setCurrentRow(1);
    CHECK(shows->itemText(0) == QStringLiteral("The number alone – \"2\""));
    CHECK(shows->itemText(1) == QStringLiteral("The label and the number – \"Figure 2\""));
    CHECK(shows->itemText(2) == QStringLiteral("The whole caption"));
    CHECK(shows->itemText(3) == QStringLiteral("\"above\" or \"below\""));
    shows->setCurrentIndex(3);
    link->setChecked(false);
    CHECK(dialog.reference() ==
          editor::CrossReference{QStringLiteral("f2"), ReferenceShows::Position, false});

    // Another kind: its captions
    kind->setCurrentIndex(1);
    REQUIRE(targets->count() == 1);
    CHECK(targets->currentRow() == 0);
    CHECK(shows->itemText(1) == QStringLiteral("The label and the number – \"Map 1\""));
    CHECK(dialog.reference().target == QStringLiteral("m1"));
    CHECK(dialog.acceptButton()->isEnabled());

    // A double click inserts it
    emit targets->itemActivated(targets->item(0));
    CHECK(dialog.result() == QDialog::Accepted);

    CHECK(sameKeys(dialog).isEmpty());
    CHECK(foreignKeys(dialog).isEmpty());
    checkFitsSmallScreen(dialog);
}

TEST_CASE("Insert Cross-Reference window: what the book's cross-references show first",
          "[gui][captions][dialog]") {
    InsertCrossReferenceDialog::Data data = referenceData();
    data.shows = ReferenceShows::LabelAndNumber;
    // No caption before the cursor: the first one
    for (InsertCrossReferenceDialog::Target& target : data.targets) {
        target.before = false;
    }
    InsertCrossReferenceDialog dialog(data);
    auto* shows = fieldOf<QComboBox>(dialog, QStringLiteral("&Insert"));
    REQUIRE(shows != nullptr);
    CHECK(shows->currentText() == QStringLiteral("The label and the number – \"Figure 1\""));
    CHECK(dialog.reference() ==
          editor::CrossReference{QStringLiteral("f1"), ReferenceShows::LabelAndNumber, true});
}

TEST_CASE("Insert Cross-Reference window in Polish: keys of its own, Latin letters, a small "
          "screen",
          "[gui][captions][dialog]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    InsertCrossReferenceDialog dialog(referenceData());
    dialog.show();
    settle();
    CHECK(dialog.windowTitle() == QStringLiteral("Wstaw odsyłacz"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Wstaw"));
    auto* shows = fieldOf<QComboBox>(dialog, QStringLiteral("&Wstaw"));
    REQUIRE(shows != nullptr);
    CHECK(shows->currentText() == QStringLiteral("Sam numer – „1”"));
    CHECK(sameKeys(dialog).isEmpty());
    CHECK(foreignKeys(dialog).isEmpty());
    checkFitsSmallScreen(dialog);
}
