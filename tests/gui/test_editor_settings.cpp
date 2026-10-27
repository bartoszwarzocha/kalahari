/// @file test_editor_settings.cpp
/// @brief The editors follow the editor settings, wherever they are changed; the Settings
///        dialog greys out the editor options the editor does not use yet

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kalahari_text_document_layout.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/utils/setting_toggle.h"
#include "kalahari/gui/widgets/color_config_widget.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <initializer_list>
#include <map>
#include <memory>
#include <string>

using namespace kalahari;

namespace {

/// Values of the settings a test changes
std::map<std::string, nlohmann::json> valuesOf(std::initializer_list<const char*> keys) {
    std::map<std::string, nlohmann::json> values;
    for (const char* key : keys) {
        values[key] = core::SettingsManager::getInstance().get<nlohmann::json>(key);
    }
    return values;
}

/// Give the settings back their values, and the open editors the settings
void restore(const std::map<std::string, nlohmann::json>& values) {
    for (const auto& [key, value] : values) {
        core::SettingsManager::getInstance().set<nlohmann::json>(key, value);
    }
    QCoreApplication::processEvents();
}

/// The dialog's check box with a text
QCheckBox* checkBoxOf(const gui::SettingsDialog& dialog, const QString& text) {
    for (QCheckBox* box : dialog.findChildren<QCheckBox*>()) {
        if (box->text() == text) {
            return box;
        }
    }
    return nullptr;
}

/// The dialog's margin field with a tooltip
QDoubleSpinBox* marginOf(const gui::SettingsDialog& dialog, const QString& toolTip) {
    for (QDoubleSpinBox* field : dialog.findChildren<QDoubleSpinBox*>()) {
        if (field->toolTip() == toolTip) {
            return field;
        }
    }
    return nullptr;
}

/// The dialog with every page built (pages are built when first opened)
void openEveryPage(gui::SettingsDialog& dialog) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        tree->setCurrentItem(*it);
    }
}

/// The text layout of a panel's editor
editor::KalahariTextDocumentLayout* layoutOf(gui::EditorPanel& panel) {
    return qobject_cast<editor::KalahariTextDocumentLayout*>(
        panel.getBookEditor()->textDocument()->documentLayout());
}

}  // anonymous namespace

TEST_CASE("Editor settings: an editor follows the changed settings once the event loop runs",
          "[gui][editor][settings]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.focus.enabled", "editor.lineHeight",
                                  "editor.typewriter.enabled", "dashboard.maxItems"});
    settings.set<bool>("editor.focus.enabled", false);
    settings.set<double>("editor.lineHeight", 1.5);
    settings.set<bool>("editor.typewriter.enabled", false);

    gui::EditorPanel panel;
    editor::BookEditor* bookEditor = panel.getBookEditor();
    int applied = 0;
    QObject::connect(bookEditor, &editor::BookEditor::appearanceChanged,
                     [&applied]() { ++applied; });

    // The Settings dialog changes many settings at once, the View menu one: the editor
    // takes them all in one go
    settings.set<bool>("editor.focus.enabled", true);
    settings.set<double>("editor.lineHeight", 2.0);
    settings.set<bool>("editor.typewriter.enabled", true);
    CHECK(applied == 0);
    QCoreApplication::processEvents();
    CHECK(applied == 1);
    CHECK(bookEditor->isFocusModeEnabled());
    CHECK(bookEditor->isTypewriterEnabled());
    CHECK(bookEditor->appearance().typography.lineHeight == 2.0);

    // Other settings leave the editor alone
    settings.set<int>("dashboard.maxItems", settings.get<int>("dashboard.maxItems") + 1);
    QCoreApplication::processEvents();
    CHECK(applied == 1);

    restore(before);
}

TEST_CASE("Editor settings: the waves and the marks in the theme's colors, optimized for each "
          "paper or not",
          "[gui][editor][settings][colors]") {
    auto& settings = core::SettingsManager::getInstance();
    const std::string theme = core::ThemeManager::getInstance().getCurrentTheme().name;
    const std::string spelling = core::ThemeManager::editorColorSetting(theme, "spelling");
    const std::string comment = core::ThemeManager::editorColorSetting(theme, "annotationComment");
    const std::string optimize = core::ThemeManager::OPTIMIZE_COLORS_SETTING;
    const auto before = valuesOf({"editor.colors.backgroundLight", "editor.colors.backgroundDark"});
    const QColor lightPaper(QStringLiteral("#ffffff"));
    const QColor darkPaper(QStringLiteral("#232328"));
    settings.set<std::string>("editor.colors.backgroundLight", lightPaper.name().toStdString());
    settings.set<std::string>("editor.colors.backgroundDark", darkPaper.name().toStdString());

    // A spelling color too faint for the light paper, a comment color too dark for the dark one
    const QColor faint(QStringLiteral("#ffd0d0"));
    const QColor deep(QStringLiteral("#202060"));
    settings.set<std::string>(spelling, faint.name().toStdString());
    settings.set<std::string>(comment, deep.name().toStdString());
    settings.removeKey(optimize);
    QCoreApplication::processEvents();  // the theme follows the settings

    gui::EditorPanel panel;
    const auto colors = [&panel]() { return panel.getBookEditor()->appearance().colors; };

    // Optimized by default: darker on the light paper and lighter on the dark one, as far as
    // they need to be seen there; as set where they are seen already
    CHECK(colors().issuesLight.spelling != faint);
    CHECK(gui::contrastRatio(colors().issuesLight.spelling, lightPaper) >= gui::MIN_MARK_CONTRAST);
    CHECK(colors().issuesDark.spelling == faint);
    CHECK(colors().annotationsDark.comment != deep);
    CHECK(gui::contrastRatio(colors().annotationsDark.comment, darkPaper) >= gui::MIN_MARK_CONTRAST);
    CHECK(colors().annotationsLight.comment == deep);

    // Not optimized: as set on both papers
    settings.set<bool>(optimize, false);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    CHECK(colors().issuesLight.spelling == faint);
    CHECK(colors().annotationsDark.comment == deep);

    settings.removeKey(spelling);
    settings.removeKey(comment);
    settings.removeKey(optimize);
    restore(before);
}

TEST_CASE("Editor settings: a switch of the Tools menu leaves the view where it is",
          "[gui][editor][settings]") {
    // Regression: with typewriter scrolling on, turning the spelling (also the grammar or
    // auto-correct) on or off brought a cursor scrolled out of the view back into it
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.typewriter.enabled", "editor.spellCheck.enabled"});
    settings.set<bool>("editor.typewriter.enabled", true);

    gui::EditorPanel panel;
    editor::BookEditor* bookEditor = panel.getBookEditor();
    test::resizeWidget(*bookEditor, QSize(600, 400));
    QStringList paragraphs;
    for (int i = 0; i < 200; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 of a chapter long enough to scroll.").arg(i);
    }
    bookEditor->fromKml(test::kmlOf(paragraphs));
    bookEditor->moveCursorToDocEnd();
    bookEditor->setScrollOffset(0.0);  // the wheel, up to the start of the text

    for (int i = 0; i < 2; ++i) {
        settings.set<bool>("editor.spellCheck.enabled",
                           !settings.get<bool>("editor.spellCheck.enabled"));
        QCoreApplication::processEvents();
        CHECK(bookEditor->scrollOffset() == 0.0);
    }

    // What is done at the cursor brings it back
    bookEditor->insertText(QStringLiteral("x"));
    CHECK(bookEditor->scrollOffset() > 0.0);

    restore(before);
}

TEST_CASE("Editor settings: the paper chosen in one editor reaches the others",
          "[gui][editor][settings]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.darkMode"});
    settings.set<bool>("editor.darkMode", true);

    gui::EditorPanel first;
    gui::EditorPanel second;
    CHECK(second.getBookEditor()->editorColorMode() == editor::EditorColorMode::Dark);

    // The editor's context menu
    first.getBookEditor()->toggleEditorColorMode();
    CHECK_FALSE(settings.get<bool>("editor.darkMode"));
    QCoreApplication::processEvents();
    CHECK(first.getBookEditor()->editorColorMode() == editor::EditorColorMode::Light);
    CHECK(second.getBookEditor()->editorColorMode() == editor::EditorColorMode::Light);

    // View > Dark Paper changes only the setting
    settings.set<bool>("editor.darkMode", true);
    QCoreApplication::processEvents();
    CHECK(first.getBookEditor()->editorColorMode() == editor::EditorColorMode::Dark);
    CHECK(second.getBookEditor()->editorColorMode() == editor::EditorColorMode::Dark);

    restore(before);
}

TEST_CASE("Editor settings: View > Text Frame Border and the Settings dialog show and hide the "
          "same border in every editor",
          "[gui][editor][settings]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.textFrameBorder.show"});
    settings.set<bool>("editor.textFrameBorder.show", false);

    // The toggle of the View menu and the Views toolbar, as the main window makes it
    QAction toggle(QStringLiteral("Text Frame Border"));
    gui::utils::followSetting(&toggle, "editor.textFrameBorder.show");
    gui::EditorPanel first;
    gui::EditorPanel second;
    CHECK_FALSE(toggle.isChecked());
    CHECK_FALSE(first.getBookEditor()->appearance().textFrameBorder.show);

    // The toggle changes only the setting: every editor draws the border
    settings.set<bool>("editor.textFrameBorder.show", true);
    QCoreApplication::processEvents();
    CHECK(toggle.isChecked());
    CHECK(first.getBookEditor()->appearance().textFrameBorder.show);
    CHECK(second.getBookEditor()->appearance().textFrameBorder.show);

    // The check box of the Settings dialog shows the same setting and changes the toggle too
    gui::SettingsDialog dialog(nullptr);
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        tree->setCurrentItem(*it);  // Pages are built when first opened
    }
    QCheckBox* show = checkBoxOf(dialog, QStringLiteral("Show text frame border"));
    REQUIRE(show != nullptr);
    CHECK(show->isChecked());
    show->setChecked(false);
    dialog.applyButton()->click();
    QCoreApplication::processEvents();
    CHECK_FALSE(toggle.isChecked());
    CHECK_FALSE(first.getBookEditor()->appearance().textFrameBorder.show);
    CHECK_FALSE(second.getBookEditor()->appearance().textFrameBorder.show);

    restore(before);
}

TEST_CASE("Editor settings: a closed editor no longer follows the settings",
          "[gui][editor][settings]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.focus.enabled"});

    // A change waits for the event loop when the editor closes
    auto panel = std::make_unique<gui::EditorPanel>();
    settings.set<bool>("editor.focus.enabled", !settings.get<bool>("editor.focus.enabled"));
    panel.reset();

    // Would reach the closed editor if it still followed the settings
    settings.set<bool>("editor.focus.enabled", !settings.get<bool>("editor.focus.enabled"));
    QCoreApplication::processEvents();

    gui::EditorPanel reopened;
    CHECK(reopened.getBookEditor()->isFocusModeEnabled() ==
          settings.get<bool>("editor.focus.enabled"));

    restore(before);
}

TEST_CASE("Settings dialog: the editor options the editor does not use yet are greyed out",
          "[gui][settings]") {
    // Tab size, line numbers, word wrap and mirror margins change nothing in the editor yet
    const auto before = valuesOf({"editor.margins.mirrorEnabled"});
    core::SettingsManager::getInstance().set<bool>("editor.margins.mirrorEnabled", true);
    gui::SettingsDialog dialog(nullptr);
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        tree->setCurrentItem(*it);  // Pages are built when first opened
    }

    for (const QString& text : {QStringLiteral("Show Line Numbers"),
                                QStringLiteral("Enable Word Wrap"),
                                QStringLiteral("Mirror margins (for book binding)")}) {
        INFO(text.toStdString());
        const QCheckBox* box = checkBoxOf(dialog, text);
        REQUIRE(box != nullptr);
        CHECK_FALSE(box->isEnabled());
        CHECK(box->toolTip() == QStringLiteral("Coming in future version"));
    }

    const QSpinBox* tabSize = nullptr;
    for (const QSpinBox* field : dialog.findChildren<QSpinBox*>()) {
        if (field->suffix() == QStringLiteral(" spaces")) {
            tabSize = field;
        }
    }
    REQUIRE(tabSize != nullptr);
    CHECK_FALSE(tabSize->isEnabled());
    CHECK(tabSize->toolTip() == QStringLiteral("Coming in future version"));

    // The pages keep their left and right margins, whatever the mirror margins setting says
    const QDoubleSpinBox* left = marginOf(dialog, QStringLiteral("Left margin of the page (up to 100 mm)"));
    const QDoubleSpinBox* inner = marginOf(dialog, QStringLiteral("Inner margin of the page (up to 100 mm)"));
    const QDoubleSpinBox* outer = marginOf(dialog, QStringLiteral("Outer margin of the page (up to 100 mm)"));
    REQUIRE(left != nullptr);
    REQUIRE(inner != nullptr);
    REQUIRE(outer != nullptr);
    CHECK_FALSE(left->isHidden());
    CHECK(inner->isHidden());
    CHECK(outer->isHidden());

    restore(before);
}

TEST_CASE("Settings dialog: the dialogue dashes counted as words, in the open editors too",
          "[gui][settings][statistics]") {
    // Microsoft Word counts a dash standing alone as a word, LibreOffice does not
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.wordCount.dashesAsWords"});
    settings.set<bool>("editor.wordCount.dashesAsWords", false);
    gui::EditorPanel panel;
    editor::BookEditor* bookEditor = panel.getBookEditor();
    bookEditor->fromKml(QStringLiteral("<kml><p>– Tak – powiedział.</p></kml>"));
    REQUIRE(bookEditor->wordCount() == 2);

    gui::SettingsDialog dialog(nullptr);
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->text(0) == QStringLiteral("General") && (*it)->parent() &&
            (*it)->parent()->text(0) == QStringLiteral("Editor")) {
            tree->setCurrentItem(*it);
        }
    }
    QCheckBox* dashes =
        checkBoxOf(dialog, QStringLiteral("Count standalone dashes as words"));
    REQUIRE(dashes != nullptr);
    CHECK_FALSE(dashes->isChecked());

    dashes->setChecked(true);
    dialog.applyButton()->click();
    CHECK(settings.get<bool>("editor.wordCount.dashesAsWords"));
    QCoreApplication::processEvents();
    CHECK(bookEditor->wordCount() == 4);

    dashes->setChecked(false);
    dialog.applyButton()->click();
    QCoreApplication::processEvents();
    CHECK(bookEditor->wordCount() == 2);

    restore(before);
}

TEST_CASE("Editor settings: View > Show Formatting Marks and the look of the scene breaks reach "
          "every editor",
          "[gui][editor][settings][marks]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.showFormattingMarks", "editor.sceneBreak.style",
                                  "editor.colors.formattingMarksLight",
                                  "editor.colors.formattingMarksDark"});
    settings.set<bool>("editor.showFormattingMarks", false);
    settings.set<std::string>("editor.sceneBreak.style", "asterisks");

    // The toggle of the View menu, as the main window makes it
    QAction toggle(QStringLiteral("Show Formatting Marks"));
    gui::utils::followSetting(&toggle, "editor.showFormattingMarks");
    gui::EditorPanel first;
    gui::EditorPanel second;
    for (gui::EditorPanel* panel : {&first, &second}) {
        panel->getBookEditor()->fromKml(QStringLiteral("<kml><p>a</p><scene-break/></kml>"));
    }
    CHECK_FALSE(toggle.isChecked());
    CHECK_FALSE(first.getBookEditor()->appearance().showFormattingMarks);

    // The marks, in every editor; the setting keeps them after a restart
    settings.set<bool>("editor.showFormattingMarks", true);
    QCoreApplication::processEvents();
    CHECK(toggle.isChecked());
    for (gui::EditorPanel* panel : {&first, &second}) {
        CHECK(panel->getBookEditor()->appearance().showFormattingMarks);
    }

    // Their color on each paper
    settings.set<std::string>("editor.colors.formattingMarksLight", "#123456");
    settings.set<std::string>("editor.colors.formattingMarksDark", "#abcdef");
    QCoreApplication::processEvents();
    CHECK(first.getBookEditor()->appearance().colors.formattingMarks.light ==
          QColor(QStringLiteral("#123456")));
    CHECK(first.getBookEditor()->appearance().colors.formattingMarks.dark ==
          QColor(QStringLiteral("#abcdef")));

    // How the scene breaks look; a name the program does not know: three asterisks
    settings.set<std::string>("editor.sceneBreak.style", "fleuron");
    QCoreApplication::processEvents();
    CHECK(first.getBookEditor()->appearance().sceneBreakLook == editor::SceneBreakLook::Fleuron);
    CHECK(layoutOf(second)->sceneBreakLook() == editor::SceneBreakLook::Fleuron);
    settings.set<std::string>("editor.sceneBreak.style", "stars");
    QCoreApplication::processEvents();
    CHECK(layoutOf(first)->sceneBreakLook() == editor::SceneBreakLook::Asterisks);

    // The file keeps the break
    CHECK(first.getBookEditor()->toKml() == QStringLiteral("<kml><p>a</p><scene-break/></kml>"));

    restore(before);
}

TEST_CASE("Editor settings: the color of links on each paper, and links made by autocorrect",
          "[gui][editor][settings][links]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.colors.linkLight", "editor.colors.linkDark",
                                  "editor.autoCorrect.links"});
    gui::EditorPanel panel;
    const editor::BookEditor* bookEditor = panel.getBookEditor();
    CHECK(bookEditor->appearance().colors.links.light == QColor(QStringLiteral("#0563c1")));
    CHECK(bookEditor->appearance().colors.links.dark == QColor(QStringLiteral("#8ab4f8")));
    CHECK(bookEditor->autoCorrect().links);

    settings.set<std::string>("editor.colors.linkLight", "#123456");
    settings.set<std::string>("editor.colors.linkDark", "#abcdef");
    settings.set<bool>("editor.autoCorrect.links", false);
    QCoreApplication::processEvents();
    CHECK(bookEditor->appearance().colors.links.light == QColor(QStringLiteral("#123456")));
    CHECK(bookEditor->appearance().colors.links.dark == QColor(QStringLiteral("#abcdef")));
    CHECK_FALSE(bookEditor->autoCorrect().links);

    restore(before);
}

TEST_CASE("Settings dialog: the look of the scene breaks, with its preview, and the color of the "
          "formatting marks",
          "[gui][settings][marks]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.sceneBreak.style"});
    settings.set<std::string>("editor.sceneBreak.style", "asterisks");
    gui::EditorPanel panel;
    gui::SettingsDialog dialog(nullptr);
    openEveryPage(dialog);

    // Editor > General > Typography: the five looks, three asterisks chosen
    QComboBox* looks = nullptr;
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Scene Break Look:")) {
            looks = qobject_cast<QComboBox*>(label->buddy());
        }
    }
    REQUIRE(looks != nullptr);
    QStringList names;
    for (int index = 0; index < looks->count(); ++index) {
        names << looks->itemData(index).toString();
    }
    CHECK(names == QStringList{QStringLiteral("asterisks"), QStringLiteral("asterism"),
                               QStringLiteral("fleuron"), QStringLiteral("line"),
                               QStringLiteral("blank")});
    CHECK(looks->currentData().toString() == QStringLiteral("asterisks"));

    // The preview shows the look chosen
    auto* preview = dialog.findChild<QWidget*>(QStringLiteral("sceneBreakPreview"));
    REQUIRE(preview != nullptr);
    preview->resize(preview->sizeHint());
    const QImage asterisks = preview->grab().toImage();
    looks->setCurrentIndex(looks->findData(QStringLiteral("line")));
    CHECK(preview->grab().toImage() != asterisks);

    // Apply: the setting and the open editors
    dialog.applyButton()->click();
    QCoreApplication::processEvents();
    CHECK(settings.get<std::string>("editor.sceneBreak.style") == "line");
    CHECK(panel.getBookEditor()->appearance().sceneBreakLook == editor::SceneBreakLook::Line);

    // Editor > Colors: the formatting marks on the light and on the dark paper
    int markColors = 0;
    for (const gui::ColorConfigWidget* widget : dialog.findChildren<gui::ColorConfigWidget*>()) {
        for (const QLabel* label : widget->findChildren<QLabel*>()) {
            if (label->text().startsWith(QStringLiteral("Formatting marks"))) {
                ++markColors;
            }
        }
    }
    CHECK(markColors == 2);

    restore(before);
}
