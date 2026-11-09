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
#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
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
    // The theme's papers
    const QColor lightPaper(QStringLiteral("#ffffff"));
    const QColor darkPaper(QStringLiteral("#232328"));
    const std::string papers[] = {
        core::ThemeManager::editorColorSetting(theme, editor::LIGHT_PAPER_COLOR_KEYS.paper),
        core::ThemeManager::editorColorSetting(theme, editor::DARK_PAPER_COLOR_KEYS.paper),
    };
    for (const std::string& paper : papers) {
        settings.removeKey(paper);
    }

    // A spelling color too faint for the light paper, a comment color too dark for the dark one
    const QColor faint(QStringLiteral("#ffd0d0"));
    const QColor deep(QStringLiteral("#202060"));
    settings.set<std::string>(spelling, faint.name().toStdString());
    settings.set<std::string>(comment, deep.name().toStdString());
    settings.removeKey(optimize);
    QCoreApplication::processEvents();  // the theme follows the settings

    gui::EditorPanel panel;
    const auto colors = [&panel]() { return panel.getBookEditor()->appearance().colors; };
    REQUIRE(colors().continuous.backgroundLight == lightPaper);
    REQUIRE(colors().continuous.backgroundDark == darkPaper);

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
    QCoreApplication::processEvents();
}

TEST_CASE("Editor settings: both papers, the desk, the selection and the search's hits in the "
          "theme's colors",
          "[gui][editor][settings][colors]") {
    auto& settings = core::SettingsManager::getInstance();
    const std::string theme = core::ThemeManager::getInstance().getCurrentTheme().name;
    const auto setting = [&theme](const char* key) {
        return core::ThemeManager::editorColorSetting(theme, key);
    };
    const std::string cream = setting(editor::LIGHT_PAPER_COLOR_KEYS.paper);
    const std::string ink = setting(editor::DARK_PAPER_COLOR_KEYS.text);
    const std::string desk = setting(editor::DARK_PAPER_COLOR_KEYS.desk);
    const std::string dimmed = setting(editor::LIGHT_PAPER_COLOR_KEYS.focusDimmed);
    const std::string selection = setting(editor::SELECTION_COLOR_KEY);
    const std::string hits = setting(editor::SEARCH_MATCH_COLOR_KEY);
    const std::string current = setting(editor::CURRENT_SEARCH_MATCH_COLOR_KEY);
    const std::string all[] = {cream, ink, desk, dimmed, selection, hits, current};
    const auto clear = [&]() {
        for (const std::string& key : all) {
            settings.removeKey(key);
        }
        QCoreApplication::processEvents();
    };
    clear();

    // Without the writer's colors: the theme's, as the editor has them by itself
    gui::EditorPanel panel;
    const auto colors = [&panel]() { return panel.getBookEditor()->appearance().colors; };
    const editor::EditorColors own;
    CHECK(colors().continuous.backgroundLight == own.continuous.backgroundLight);
    CHECK(colors().continuous.textDark == own.continuous.textDark);
    CHECK(colors().continuous.deskDark == own.continuous.deskDark);
    CHECK(colors().focus.inactiveLight == own.focus.inactiveLight);
    CHECK(colors().selection == own.selection);
    CHECK(colors().searchMatch == own.searchMatch);
    CHECK(colors().currentSearchMatch == own.currentSearchMatch);

    // The writer's colors of the theme reach the open editors; under the text they stay
    // translucent
    settings.set<std::string>(cream, "#f5ecd7");
    settings.set<std::string>(ink, "#c8c0b0");
    settings.set<std::string>(desk, "#101018");
    settings.set<std::string>(dimmed, "#c0b8a8");
    settings.set<std::string>(selection, "#00ff00");
    settings.set<std::string>(hits, "#ff00ff");
    settings.set<std::string>(current, "#0000ff");
    QCoreApplication::processEvents();  // the theme follows the settings
    QCoreApplication::processEvents();  // and the editors the theme
    CHECK(colors().continuous.backgroundLight == QColor(QStringLiteral("#f5ecd7")));
    CHECK(colors().continuous.textDark == QColor(QStringLiteral("#c8c0b0")));
    CHECK(colors().continuous.deskDark == QColor(QStringLiteral("#101018")));
    CHECK(colors().focus.inactiveLight == QColor(QStringLiteral("#c0b8a8")));
    CHECK(colors().selection == QColor(0, 255, 0, editor::SELECTION_OPACITY));
    CHECK(colors().searchMatch == QColor(255, 0, 255, editor::SEARCH_MATCH_OPACITY));
    CHECK(colors().currentSearchMatch == QColor(0, 0, 255, editor::CURRENT_SEARCH_MATCH_OPACITY));

    // The paper the text is written on: the light one in its colors
    const auto before = valuesOf({"editor.darkMode"});
    settings.set<bool>("editor.darkMode", false);
    QCoreApplication::processEvents();
    const editor::EditorAppearance& appearance = panel.getBookEditor()->appearance();
    CHECK(appearance.colors.background(appearance.colorMode) == QColor(QStringLiteral("#f5ecd7")));

    restore(before);
    clear();
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

TEST_CASE("Editor settings: View > Margin Numbering and its toolbar buttons, one choice of three "
          "in every editor",
          "[gui][editor][settings][marginNumbers]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.marginNumbers.mode"});
    settings.set<std::string>("editor.marginNumbers.mode", "none");

    // The three commands as the main window makes them: one menu item and one button each,
    // the command run before the item follows the setting
    QActionGroup group(nullptr);
    std::map<std::string, QAction*> actions;
    for (const char* mode : {"none", "lines", "paragraphs"}) {
        auto* action = new QAction(QString::fromLatin1(mode), &group);
        group.addAction(action);
        QObject::connect(action, &QAction::triggered, [mode]() {
            gui::utils::chooseSetting("editor.marginNumbers.mode", mode, "none");
        });
        gui::utils::followChoice(action, "editor.marginNumbers.mode", mode);
        actions[mode] = action;
    }
    const auto checked = [&actions]() {
        std::string shown;
        for (const auto& [mode, action] : actions) {
            if (action->isChecked()) {
                shown += mode;
            }
        }
        return shown;
    };
    const auto choose = [&actions](const char* mode) {
        actions.at(mode)->trigger();
        QCoreApplication::processEvents();
    };
    gui::EditorPanel first;
    gui::EditorPanel second;
    const auto numbering = [&first, &second]() {
        const editor::MarginNumbering shown =
            first.getBookEditor()->appearance().marginNumbers.numbering;
        CHECK(second.getBookEditor()->appearance().marginNumbers.numbering == shown);
        return shown;
    };
    CHECK(checked() == "none");

    choose("lines");
    CHECK(checked() == "lines");
    CHECK(settings.get<std::string>("editor.marginNumbers.mode") == "lines");
    CHECK(numbering() == editor::MarginNumbering::Lines);

    // The numbering on, chosen again (its button): off
    choose("lines");
    CHECK(checked() == "none");
    CHECK(settings.get<std::string>("editor.marginNumbers.mode") == "none");
    CHECK(numbering() == editor::MarginNumbering::None);

    choose("paragraphs");
    CHECK(checked() == "paragraphs");
    CHECK(numbering() == editor::MarginNumbering::Paragraphs);
    choose("lines");
    CHECK(checked() == "lines");

    // No Numbering: off, and it stays off
    choose("none");
    CHECK(checked() == "none");
    choose("none");
    CHECK(checked() == "none");
    CHECK(numbering() == editor::MarginNumbering::None);

    // Changed in the Settings dialog, the menu and the buttons show it
    settings.set<std::string>("editor.marginNumbers.mode", "paragraphs");
    QCoreApplication::processEvents();
    CHECK(checked() == "paragraphs");
    CHECK(numbering() == editor::MarginNumbering::Paragraphs);

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

TEST_CASE("Settings dialog: the mirror margins take the place of the left and the right margin, "
          "in the open editors too",
          "[gui][settings][margins]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.margins.mirrorEnabled"});
    settings.set<bool>("editor.margins.mirrorEnabled", false);
    gui::EditorPanel panel;
    gui::SettingsDialog dialog(nullptr);
    REQUIRE(dialog.openPage(QString::fromLatin1(gui::SettingsDialog::PAGES_PAGE)));

    auto* mirror = dialog.findChild<QCheckBox*>(QStringLiteral("mirrorMargins"));
    auto* left = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("pageLeft"));
    auto* right = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("pageRight"));
    auto* inner = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("pageInner"));
    auto* outer = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("pageOuter"));
    auto* preview = dialog.findChild<QWidget*>(QStringLiteral("mirrorMarginsPreview"));
    REQUIRE(mirror != nullptr);
    REQUIRE(left != nullptr);
    REQUIRE(right != nullptr);
    REQUIRE(inner != nullptr);
    REQUIRE(outer != nullptr);
    REQUIRE(preview != nullptr);

    // Usable now: no longer greyed out as coming in a future version
    CHECK(mirror->isEnabled());
    CHECK(mirror->toolTip() != QStringLiteral("Coming in future version"));
    CHECK_FALSE(mirror->isChecked());
    CHECK(left->isEnabled());
    CHECK(right->isEnabled());
    CHECK(inner->isHidden());
    CHECK(outer->isHidden());
    CHECK(preview->parentWidget()->isHidden());

    // Checked: the inner and the outer margin and the pages 2 and 3, the left and the right
    // margin greyed out
    mirror->setChecked(true);
    CHECK_FALSE(left->isEnabled());
    CHECK_FALSE(right->isEnabled());
    CHECK_FALSE(inner->isHidden());
    CHECK_FALSE(outer->isHidden());
    CHECK_FALSE(preview->parentWidget()->isHidden());
    CHECK(preview->sizeHint().width() > preview->sizeHint().height());

    // Applied: every editor has them
    CHECK_FALSE(panel.getBookEditor()->appearance().pageMargins.mirrored);
    dialog.applyButton()->click();
    QCoreApplication::processEvents();
    CHECK(settings.get<bool>("editor.margins.mirrorEnabled"));
    CHECK(panel.getBookEditor()->appearance().pageMargins.mirrored);

    restore(before);
}

TEST_CASE("Settings dialog: the margin numbering, its fields greyed out without it and its "
          "defaults, in the open editors too",
          "[gui][settings][marginNumbers]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.marginNumbers.mode", "editor.marginNumbers.interval",
                                  "editor.marginNumbers.size"});
    settings.set<std::string>("editor.marginNumbers.mode", "none");
    gui::EditorPanel panel;
    gui::SettingsDialog dialog(nullptr);
    REQUIRE(dialog.openPage(QString::fromLatin1(gui::SettingsDialog::MARGIN_NUMBERING_PAGE)));

    auto* none = dialog.findChild<QRadioButton*>(QStringLiteral("marginNumbers_none"));
    auto* lines = dialog.findChild<QRadioButton*>(QStringLiteral("marginNumbers_lines"));
    auto* paragraphs = dialog.findChild<QRadioButton*>(QStringLiteral("marginNumbers_paragraphs"));
    auto* interval = dialog.findChild<QSpinBox*>(QStringLiteral("marginNumbersInterval"));
    auto* restart = dialog.findChild<QCheckBox*>(QStringLiteral("marginNumbersRestart"));
    auto* size = dialog.findChild<QSpinBox*>(QStringLiteral("marginNumbersSize"));
    auto* color = dialog.findChild<gui::ColorConfigWidget*>(QStringLiteral("marginNumbersColor"));
    auto* preview = dialog.findChild<QWidget*>(QStringLiteral("marginNumbersPreview"));
    auto* defaults = dialog.findChild<QPushButton*>(QStringLiteral("marginNumbersDefaults"));
    REQUIRE(none != nullptr);
    REQUIRE(lines != nullptr);
    REQUIRE(paragraphs != nullptr);
    REQUIRE(interval != nullptr);
    REQUIRE(restart != nullptr);
    REQUIRE(size != nullptr);
    REQUIRE(color != nullptr);
    REQUIRE(preview != nullptr);
    REQUIRE(defaults != nullptr);

    // None: the fields greyed out
    CHECK(none->isChecked());
    for (const QWidget* field : std::initializer_list<const QWidget*>{
             interval, restart, size, color, preview, defaults}) {
        CHECK_FALSE(field->isEnabled());
    }

    // Lines: all of them; Paragraphs: all but Restart on every page, which counts lines
    lines->setChecked(true);
    for (const QWidget* field : std::initializer_list<const QWidget*>{
             interval, restart, size, color, preview, defaults}) {
        CHECK(field->isEnabled());
    }
    paragraphs->setChecked(true);
    CHECK(interval->isEnabled());
    CHECK_FALSE(restart->isEnabled());

    // Restore Defaults: the look as at first, the numbering as chosen
    interval->setValue(5);
    size->setValue(12);
    defaults->click();
    CHECK(interval->value() == 1);
    CHECK(size->value() == 8);
    CHECK(paragraphs->isChecked());

    // Applied: every editor numbers the paragraphs, every third one
    interval->setValue(3);
    dialog.applyButton()->click();
    QCoreApplication::processEvents();
    CHECK(settings.get<std::string>("editor.marginNumbers.mode") == "paragraphs");
    CHECK(settings.get<int>("editor.marginNumbers.interval") == 3);
    const editor::MarginNumbers& numbers = panel.getBookEditor()->appearance().marginNumbers;
    CHECK(numbers.numbering == editor::MarginNumbering::Paragraphs);
    CHECK(numbers.interval == 3);

    // Set elsewhere (View > Margin Numbering), it shows in a dialog opened after that
    settings.set<std::string>("editor.marginNumbers.mode", "lines");
    gui::SettingsDialog next(nullptr);
    REQUIRE(next.openPage(QString::fromLatin1(gui::SettingsDialog::MARGIN_NUMBERING_PAGE)));
    auto* nextLines = next.findChild<QRadioButton*>(QStringLiteral("marginNumbers_lines"));
    REQUIRE(nextLines != nullptr);
    CHECK(nextLines->isChecked());

    restore(before);
}

TEST_CASE("Settings dialog: the preview of the margin numbers shows the theme's light paper",
          "[gui][settings][marginNumbers][colors]") {
    auto& settings = core::SettingsManager::getInstance();
    const std::string theme = core::ThemeManager::getInstance().getCurrentTheme().name;
    const std::string cream =
        core::ThemeManager::editorColorSetting(theme, editor::LIGHT_PAPER_COLOR_KEYS.paper);
    const auto before = valuesOf({"editor.marginNumbers.mode"});
    settings.set<std::string>("editor.marginNumbers.mode", "lines");
    settings.set<std::string>(cream, "#f5ecd7");
    QCoreApplication::processEvents();  // the theme follows the settings

    gui::SettingsDialog dialog(nullptr);
    REQUIRE(dialog.openPage(QString::fromLatin1(gui::SettingsDialog::MARGIN_NUMBERING_PAGE)));
    auto* preview = dialog.findChild<QWidget*>(QStringLiteral("marginNumbersPreview"));
    REQUIRE(preview != nullptr);
    REQUIRE(preview->isEnabled());
    preview->resize(preview->sizeHint());

    // Inside the frame of the light paper, above its text
    const QImage image = preview->grab().toImage();
    CHECK(QColor(image.pixel(3, 3)) == QColor(QStringLiteral("#f5ecd7")));

    settings.removeKey(cream);
    restore(before);
}

TEST_CASE("Settings dialog: View > Margin Numbering > Numbering Settings... puts the cursor in "
          "the choice of the numbering",
          "[gui][settings][marginNumbers]") {
    auto& settings = core::SettingsManager::getInstance();
    const auto before = valuesOf({"editor.marginNumbers.mode"});
    settings.set<std::string>("editor.marginNumbers.mode", "paragraphs");

    gui::SettingsDialog dialog(nullptr);
    REQUIRE(dialog.openPage(QString::fromLatin1(gui::SettingsDialog::MARGIN_NUMBERING_PAGE)));
    auto* paragraphs = dialog.findChild<QRadioButton*>(QStringLiteral("marginNumbers_paragraphs"));
    REQUIRE(paragraphs != nullptr);
    dialog.show();
    dialog.activateWindow();
    QCoreApplication::processEvents();
    CHECK(dialog.focusWidget() == paragraphs);
    // On the page the dialog shows
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    REQUIRE(tree->currentItem() != nullptr);
    CHECK(tree->currentItem()->text(0) == QStringLiteral("Margin Numbering"));
    QScrollArea* area = nullptr;
    for (QWidget* parent = paragraphs->parentWidget(); parent != nullptr && area == nullptr;
         parent = parent->parentWidget()) {
        area = qobject_cast<QScrollArea*>(parent);
    }
    REQUIRE(area != nullptr);
    CHECK(area->viewport()->rect().contains(paragraphs->mapTo(area->viewport(), QPoint(0, 0))));
    dialog.close();

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
        if ((*it)->text(0) == QStringLiteral("Writing") && (*it)->parent() &&
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
    const std::string theme = core::ThemeManager::getInstance().getCurrentTheme().name;
    const std::string marksLight = core::ThemeManager::editorColorSetting(
        theme, editor::LIGHT_PAPER_COLOR_KEYS.formattingMarks);
    const std::string marksDark = core::ThemeManager::editorColorSetting(
        theme, editor::DARK_PAPER_COLOR_KEYS.formattingMarks);
    const auto before = valuesOf({"editor.showFormattingMarks", "editor.sceneBreak.style"});
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

    // Their color on each paper, the theme's
    settings.set<std::string>(marksLight, "#123456");
    settings.set<std::string>(marksDark, "#abcdef");
    QCoreApplication::processEvents();  // the theme follows the settings
    QCoreApplication::processEvents();  // and the editors the theme
    CHECK(first.getBookEditor()->appearance().colors.formattingMarks.light ==
          QColor(QStringLiteral("#123456")));
    CHECK(first.getBookEditor()->appearance().colors.formattingMarks.dark ==
          QColor(QStringLiteral("#abcdef")));
    settings.removeKey(marksLight);
    settings.removeKey(marksDark);

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
    const std::string theme = core::ThemeManager::getInstance().getCurrentTheme().name;
    const std::string light =
        core::ThemeManager::editorColorSetting(theme, editor::LIGHT_PAPER_COLOR_KEYS.link);
    const std::string dark =
        core::ThemeManager::editorColorSetting(theme, editor::DARK_PAPER_COLOR_KEYS.link);
    settings.removeKey(light);
    settings.removeKey(dark);
    QCoreApplication::processEvents();
    const auto before = valuesOf({"editor.autoCorrect.links"});

    // Without the writer's colors: the theme's, as the editor has them by itself
    gui::EditorPanel panel;
    const editor::BookEditor* bookEditor = panel.getBookEditor();
    CHECK(bookEditor->appearance().colors.links.light == QColor(QStringLiteral("#0563c1")));
    CHECK(bookEditor->appearance().colors.links.dark == QColor(QStringLiteral("#8ab4f8")));
    CHECK(bookEditor->autoCorrect().links);

    // The writer's colors of the theme reach the open editors
    settings.set<std::string>(light, "#123456");
    settings.set<std::string>(dark, "#abcdef");
    settings.set<bool>("editor.autoCorrect.links", false);
    QCoreApplication::processEvents();  // the theme follows the settings
    QCoreApplication::processEvents();  // and the editors the theme
    CHECK(bookEditor->appearance().colors.links.light == QColor(QStringLiteral("#123456")));
    CHECK(bookEditor->appearance().colors.links.dark == QColor(QStringLiteral("#abcdef")));
    CHECK_FALSE(bookEditor->autoCorrect().links);

    restore(before);
    settings.removeKey(light);
    settings.removeKey(dark);
    QCoreApplication::processEvents();
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

    // Editor > General > Scene Breaks: the five looks, three asterisks chosen
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
