/// @file test_properties_panel.cpp
/// @brief The Properties panel: the counts of the editor in front

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/utils/reading_time.h"

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

/// Run the event loop past the panel's wait after an edit or a new selection
void waitForPanel() {
    test::runEventLoop(300);
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

    SECTION("no editor in front: nothing to count") {
        panel.setActiveEditor(nullptr);
        panel.refresh();
        CHECK(labelText(panel, "editorParagraphCount") == QStringLiteral("0"));
        CHECK(labelText(panel, "editorReadingTime") == QStringLiteral("0 min"));
    }
}
