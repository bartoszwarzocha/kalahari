/// @file test_properties_panel.cpp
/// @brief The Properties panel: the counts of the editor in front and the style of its
///        paragraph

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/utils/reading_time.h"

#include <QComboBox>
#include <QLabel>
#include <QLocale>
#include <QStringList>

using namespace kalahari;

namespace {

/// The text of the panel's label @p name
QString labelText(const gui::PropertiesPanel& panel, const char* name) {
    const auto* label = panel.findChild<QLabel*>(QLatin1String(name));
    REQUIRE(label != nullptr);
    return label->text();
}

/// Whether the panel shows its label @p name, that is the label is on the page in front
bool showsLabel(const gui::PropertiesPanel& panel, const char* name) {
    const auto* label = panel.findChild<QLabel*>(QLatin1String(name));
    REQUIRE(label != nullptr);
    return label->isVisibleTo(&panel);
}

/// Run the event loop past the panel's wait after an edit or a new selection
void waitForPanel() {
    test::runEventLoop(300);
}

/// A style of a package, named in English
core::PackageParagraphStyle packageStyle(const QString& id, const QString& name) {
    core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), name);
    return style;
}

}  // anonymous namespace

TEST_CASE("Properties panel: the counts of the editor in front, as the status bar gives them",
          "[gui][statistics]") {
    // The editor first: the panel goes before it
    gui::EditorPanel editorPanel;
    editor::BookEditor* bookEditor = editorPanel.getBookEditor();
    // An empty paragraph in the middle and one at the end, as a text file's last line end
    // leaves it
    bookEditor->fromKml(test::kmlOf({QStringLiteral("One two three"), QString(),
                                     QStringLiteral("Four"), QString()}));
    REQUIRE(bookEditor->paragraphCount() == 4);

    gui::PropertiesPanel panel;
    panel.setActiveEditor(&editorPanel);
    REQUIRE(showsLabel(panel, "editorWordCount"));
    CHECK(labelText(panel, "editorWordCount") == QStringLiteral("4"));
    CHECK(labelText(panel, "editorCharCount") == QStringLiteral("17"));
    CHECK(labelText(panel, "editorCharNoSpace") == QStringLiteral("15"));
    // Only the paragraphs with text, as in Word and LibreOffice
    CHECK(labelText(panel, "editorParagraphCount") == QStringLiteral("2"));
    CHECK(labelText(panel, "editorReadingTime") == QStringLiteral("1 min"));

    SECTION("a selection: its counts") {
        bookEditor->setSelection({{0, 4}, {2, 2}});  // "two three", the empty paragraph, "Fo"
        waitForPanel();
        CHECK(labelText(panel, "editorWordCount") == QStringLiteral("3"));
        CHECK(labelText(panel, "editorCharCount") == QStringLiteral("11"));
        CHECK(labelText(panel, "editorParagraphCount") == QStringLiteral("2"));
    }

    SECTION("large numbers in the system's way, an hour of reading in hours and minutes") {
        const int words = core::READING_WORDS_PER_MINUTE * 60 + 1;
        QStringList text;
        for (int i = 0; i < words; ++i) {
            text << QStringLiteral("word");
        }
        bookEditor->fromKml(test::kmlOf({text.join(QLatin1Char(' '))}));
        waitForPanel();
        CHECK(labelText(panel, "editorWordCount") == QLocale().toString(words));
        CHECK(labelText(panel, "editorCharCount") == QLocale().toString(words * 5 - 1));
        CHECK(labelText(panel, "editorReadingTime") == QStringLiteral("1 h 1 min"));
        // As the status bar writes it
        CHECK(labelText(panel, "editorReadingTime") == gui::utils::readingTimeText(words));
    }

    SECTION("new rules of counting: the numbers follow at once, without an edit") {
        bookEditor->fromKml(test::kmlOf({QStringLiteral("– Yes – he said.")}));
        waitForPanel();
        REQUIRE(labelText(panel, "editorWordCount") == QStringLiteral("3"));
        core::WordCountRules dashesAsWords;
        dashesAsWords.dashesAreWords = true;
        bookEditor->setWordCountRules(dashesAsWords);
        waitForPanel();
        CHECK(labelText(panel, "editorWordCount") == QStringLiteral("5"));
    }

    SECTION("no editor in front (the Dashboard): no counts of a document not in front") {
        panel.setActiveEditor(nullptr);
        CHECK_FALSE(showsLabel(panel, "editorWordCount"));
        CHECK_FALSE(showsLabel(panel, "editorReadingTime"));
        // Back with the editor in front
        panel.setActiveEditor(&editorPanel);
        CHECK(showsLabel(panel, "editorWordCount"));
        CHECK(labelText(panel, "editorWordCount") == QStringLiteral("4"));
    }
}

TEST_CASE("Properties panel: the style of the paragraph at the cursor, and a style chosen there",
          "[gui][styles]") {
    gui::EditorPanel editorPanel;
    editor::BookEditor* bookEditor = editorPanel.getBookEditor();
    bookEditor->setParagraphStyles(editor::ParagraphStyleSet::fromPackage(
        {packageStyle(QStringLiteral("normal"), QStringLiteral("Normal")),
         packageStyle(QStringLiteral("heading_1"), QStringLiteral("Heading 1")),
         packageStyle(QStringLiteral("quote"), QStringLiteral("Quote"))},
        QStringLiteral("en")));
    const QString kml = QStringLiteral("<kml><h1>Title</h1><p>Text</p><hr/></kml>");
    bookEditor->fromKml(kml);
    bookEditor->setCursorPosition({0, 1});

    gui::PropertiesPanel panel;
    panel.setActiveEditor(&editorPanel);
    auto* styles = panel.findChild<QComboBox*>(QStringLiteral("editorStyleCombo"));
    REQUIRE(styles != nullptr);

    // The styles of the text, the heading's chosen
    QStringList names;
    for (int index = 0; index < styles->count(); ++index) {
        names << styles->itemText(index);
    }
    CHECK(names == QStringList{QStringLiteral("Normal"), QStringLiteral("Heading 1"),
                               QStringLiteral("Quote")});
    CHECK(labelText(panel, "editorStyleLabel") == QStringLiteral("Heading 1"));
    CHECK(styles->currentData().toString() == QStringLiteral("heading_1"));

    // The cursor in another paragraph: its style; on a line: none
    bookEditor->setCursorPosition({1, 0});
    waitForPanel();
    CHECK(labelText(panel, "editorStyleLabel") == QStringLiteral("Normal"));
    bookEditor->setCursorPosition({2, 0});
    waitForPanel();
    CHECK(labelText(panel, "editorStyleLabel") == QStringLiteral("-"));
    CHECK(styles->currentIndex() == -1);

    // A style chosen: the paragraph takes it, as one step of the undo history
    bookEditor->setCursorPosition({1, 0});
    waitForPanel();
    styles->setCurrentIndex(styles->findData(QStringLiteral("quote")));
    CHECK(bookEditor->toKml() ==
          QStringLiteral("<kml><h1>Title</h1><p style=\"quote\">Text</p><hr/></kml>"));
    CHECK(labelText(panel, "editorStyleLabel") == QStringLiteral("Quote"));
    bookEditor->undo();
    CHECK(bookEditor->toKml() == kml);

    // A chapter of a book open read-only: greyed out, saying why
    bookEditor->setReadOnly(true);
    panel.setActiveEditor(&editorPanel);
    CHECK_FALSE(styles->isEnabled());
    CHECK(styles->toolTip() != QStringLiteral("Change paragraph style"));
}
