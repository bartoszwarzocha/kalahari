/// @file test_theme_manager_apply.cpp
/// @brief ThemeManager::reloadTheme applies a theme with the user's stored colors
/// and extra overrides in one pass (one themeChanged, colors visible in the palette);
/// the editor colors of a theme (the writer's own ones are one for all themes), and the colors
/// its palette gives disabled widgets.

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QObject>
#include <QPalette>
#include <QToolTip>

#include <map>
#include <string>

using kalahari::core::Theme;
using kalahari::core::ThemeManager;

TEST_CASE("ThemeManager applies a theme with overrides in one pass",
          "[core][theme]") {
    auto& themeManager = ThemeManager::getInstance();
    const QString originalTheme = QString::fromStdString(themeManager.getCurrentTheme().name);
    const QString otherTheme = (originalTheme == "Dark") ? "Light" : "Dark";
    const QColor TEST_WINDOW("#ff00aa");
    const QColor TEST_PRIMARY("#00ffcc");

    int themeChangedCount = 0;
    QObject guard;
    QObject::connect(&themeManager, &ThemeManager::themeChanged, &guard,
                     [&themeChangedCount]() { ++themeChangedCount; });

    SECTION("switching theme applies stored colors and overrides with a single themeChanged") {
        auto& settings = kalahari::core::SettingsManager::getInstance();
        settings.setPaletteColorForTheme(otherTheme.toStdString(), "window", TEST_WINDOW.name().toStdString());

        REQUIRE(themeManager.reloadTheme(otherTheme, {{"primary", TEST_PRIMARY}}));
        settings.clearCustomPaletteColorsForTheme(otherTheme.toStdString());

        CHECK(themeChangedCount == 1);
        CHECK(QString::fromStdString(themeManager.getCurrentTheme().name) == otherTheme);
        CHECK(themeManager.getCurrentTheme().palette.window == TEST_WINDOW);
        CHECK(themeManager.getCurrentTheme().colors.primary == TEST_PRIMARY);
        CHECK(QApplication::palette().color(QPalette::Window) == TEST_WINDOW);
    }

    SECTION("stored tooltip colors are applied (stored by the Settings dialog under ui)") {
        auto& settings = kalahari::core::SettingsManager::getInstance();
        settings.setUiColorForTheme(originalTheme.toStdString(), "toolTipBase", TEST_WINDOW.name().toStdString());

        REQUIRE(themeManager.reloadTheme(originalTheme));
        settings.clearCustomUiColorsForTheme(originalTheme.toStdString());

        CHECK(themeManager.getCurrentTheme().palette.toolTipBase == TEST_WINDOW);
        // Tooltips take their colors from the palette (there is no application style sheet)
        CHECK(QToolTip::palette().color(QPalette::ToolTipBase) == TEST_WINDOW);
        CHECK(qApp->styleSheet().isEmpty());
    }

    SECTION("stored info panel colors are applied") {
        auto& settings = kalahari::core::SettingsManager::getInstance();
        const std::string key = "themes." + originalTheme.toStdString() + ".colors.infoHeader";
        settings.set(key, TEST_WINDOW.name().toStdString());

        REQUIRE(themeManager.reloadTheme(originalTheme));
        settings.removeKey(key);

        CHECK(themeManager.getCurrentTheme().colors.infoHeader == TEST_WINDOW);
    }

    SECTION("the Dashboard and info icon colors stored before the Dashboard icons took the icon "
            "colors change nothing") {
        REQUIRE(themeManager.reloadTheme(originalTheme));
        const Theme before = themeManager.getCurrentTheme();
        auto& settings = kalahari::core::SettingsManager::getInstance();
        const std::string prefix = "themes." + originalTheme.toStdString() + ".colors.";
        settings.set(prefix + "dashboardPrimary", TEST_WINDOW.name().toStdString());
        settings.set(prefix + "dashboardSecondary", TEST_PRIMARY.name().toStdString());
        settings.set(prefix + "infoPrimary", TEST_WINDOW.name().toStdString());
        settings.set(prefix + "infoSecondary", TEST_PRIMARY.name().toStdString());

        REQUIRE(themeManager.reloadTheme(originalTheme));
        for (const char* key :
             {"dashboardPrimary", "dashboardSecondary", "infoPrimary", "infoSecondary"}) {
            settings.removeKey(prefix + key);
        }

        const Theme::Colors& colors = themeManager.getCurrentTheme().colors;
        CHECK(colors.primary == before.colors.primary);
        CHECK(colors.secondary == before.colors.secondary);
        CHECK(colors.infoHeader == before.colors.infoHeader);
        CHECK(colors.infoBarBackground == before.colors.infoBarBackground);
        CHECK(themeManager.getCurrentTheme().palette.window == before.palette.window);
    }

    SECTION("re-applying without overrides restores the theme colors") {
        REQUIRE(themeManager.reloadTheme(originalTheme, {{"palette.window", TEST_WINDOW}}));
        REQUIRE(themeManager.reloadTheme(originalTheme, {}));

        CHECK(themeChangedCount == 2);
        CHECK(themeManager.getCurrentTheme().palette.window != TEST_WINDOW);
    }

    SECTION("an unknown theme is rejected and the current theme stays") {
        REQUIRE_FALSE(themeManager.reloadTheme("NoSuchTheme", {}));
        CHECK(themeChangedCount == 0);
    }

    // Leave the suite's baseline theme in place for the following tests
    themeManager.reloadTheme(originalTheme, {});
    CHECK(QString::fromStdString(themeManager.getCurrentTheme().name) == originalTheme);
}

TEST_CASE("Theme editor colors are an open list from the theme file", "[core][theme]") {
    const nlohmann::json json = {
        {"name", "Test"},
        {"colors", {{"background", "#ffffff"}}},
        {"editor", {{"commentMarker", "#ffcc00"}, {"todoMarker", "#ff0000"}}},
    };

    const Theme theme = Theme::fromJson(json);
    REQUIRE(theme.editor.size() == 2);
    CHECK(theme.editor.at("commentMarker") == QColor("#ffcc00"));

    const nlohmann::json written = theme.toJson();
    CHECK(written["editor"]["todoMarker"] == "#ff0000");

    auto& themeManager = ThemeManager::getInstance();
    const QString currentTheme = QString::fromStdString(themeManager.getCurrentTheme().name);
    themeManager.setColorOverride("editor.noteMarker", QColor("#00ff00"));
    CHECK(themeManager.editorColor("noteMarker", QColor()) == QColor("#00ff00"));
    CHECK(themeManager.editorColor("missingMarker", QColor("#123456")) == QColor("#123456"));
    themeManager.reloadTheme(currentTheme);
}

TEST_CASE("The writer's editor colors are one for all themes and reach the theme by themselves",
          "[core][theme]") {
    // The paper is light or dark whatever the theme (View > Dark Paper): a color chosen for it
    // stays when the theme changes, and a changed setting reaches the editors and the panels
    auto& themeManager = ThemeManager::getInstance();
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const QString originalTheme = QString::fromStdString(themeManager.getCurrentTheme().name);
    const QString otherTheme = (originalTheme == "Dark") ? "Light" : "Dark";
    const std::string spelling = "spellingLightPaper";
    const std::string grammar = "grammarLightPaper";
    const std::string spellingSetting = ThemeManager::editorColorSetting(spelling);
    const std::string grammarSetting = ThemeManager::editorColorSetting(grammar);
    CHECK(spellingSetting == "editor.colors.spellingLightPaper");
    const QColor themeColor = themeManager.defaultEditorColor(spelling, QColor());
    REQUIRE(themeColor.isValid());
    CHECK(themeManager.defaultEditorColor("missingMarker", QColor("#123456")) == QColor("#123456"));

    int themeChangedCount = 0;
    QObject guard;
    QObject::connect(&themeManager, &ThemeManager::themeChanged, &guard,
                     [&themeChangedCount]() { ++themeChangedCount; });

    // Colors written together (Apply in the Settings dialog) reach the theme together, later
    settings.set<std::string>(spellingSetting, "#abcdef");
    settings.set<std::string>(grammarSetting, "#fedcba");
    CHECK(themeChangedCount == 0);
    QCoreApplication::processEvents();
    CHECK(themeChangedCount == 1);
    CHECK(themeManager.editorColor(spelling, QColor()) == QColor("#abcdef"));
    CHECK(themeManager.editorColor(grammar, QColor()) == QColor("#fedcba"));
    CHECK(themeManager.defaultEditorColor(spelling, QColor()) == themeColor);

    // Another theme keeps them
    REQUIRE(themeManager.reloadTheme(otherTheme));
    CHECK(themeManager.editorColor(spelling, QColor()) == QColor("#abcdef"));
    CHECK(themeManager.editorColor(grammar, QColor()) == QColor("#fedcba"));

    // Without the settings, the theme's own colors come back
    settings.removeKey(spellingSetting);
    settings.removeKey(grammarSetting);
    QCoreApplication::processEvents();
    CHECK(themeManager.editorColor(spelling, QColor()) ==
          themeManager.defaultEditorColor(spelling, QColor()));
    CHECK(themeManager.editorColor(grammar, QColor()) ==
          themeManager.defaultEditorColor(grammar, QColor()));

    // A setting that changes no color of the theme changes nothing
    const int before = themeChangedCount;
    settings.set<std::string>("editor.colors.backgroundLight", "#fafafa");
    QCoreApplication::processEvents();
    CHECK(themeChangedCount == before);
    settings.removeKey("editor.colors.backgroundLight");

    REQUIRE(themeManager.reloadTheme(originalTheme));
}

TEST_CASE("A theme file with the Dashboard and info icon colors of before loads as without them",
          "[core][theme]") {
    // The Dashboard icons took the icon colors; a theme file written before still has them
    const nlohmann::json json = {
        {"name", "Test"},
        {"colors",
         {{"primary", "#111111"}, {"secondary", "#222222"}, {"dashboardPrimary", "#ff00aa"},
          {"dashboardSecondary", "#00ffcc"}, {"infoPrimary", "#ff00aa"},
          {"infoSecondary", "#00ffcc"}}},
    };

    const Theme theme = Theme::fromJson(json);
    CHECK(theme.colors.primary == QColor("#111111"));
    CHECK(theme.colors.secondary == QColor("#222222"));

    // And it is written again without them
    const nlohmann::json written = theme.toJson();
    for (const char* key :
         {"dashboardPrimary", "dashboardSecondary", "infoPrimary", "infoSecondary"}) {
        INFO(key);
        CHECK_FALSE(written["colors"].contains(key));
    }
}

TEST_CASE("A theme gives disabled widgets opaque backgrounds", "[core][theme]") {
    Theme::Palette palette;
    palette.window = QColor("#f0f0f0");
    palette.base = QColor("#ffffff");
    palette.text = QColor("#000000");
    palette.highlight = QColor("#0078d4");
    const QPalette qtPalette = palette.toQPalette();

    // A list paints its background over nothing: a half transparent one came out dark
    CHECK(qtPalette.color(QPalette::Disabled, QPalette::Base) == QColor(247, 247, 247));
    CHECK(qtPalette.color(QPalette::Disabled, QPalette::Window) == QColor("#f0f0f0"));
    CHECK(qtPalette.color(QPalette::Disabled, QPalette::Highlight).alpha() == 255);
    CHECK(qtPalette.color(QPalette::Active, QPalette::Base) == QColor("#ffffff"));

    // The texts are half transparent over their backgrounds
    QColor disabledText("#000000");
    disabledText.setAlpha(128);
    CHECK(qtPalette.color(QPalette::Disabled, QPalette::Text) == disabledText);
}
