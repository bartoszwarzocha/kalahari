/// @file test_editor_settings.cpp
/// @brief The editors follow the editor settings, wherever they are changed; the Settings
///        dialog greys out the editor options the editor does not use yet

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings_dialog.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QSpinBox>
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
