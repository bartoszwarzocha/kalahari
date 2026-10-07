/// @file test_theme_manager_apply.cpp
/// @brief ThemeManager::reloadTheme applies a theme with the user's stored colors
/// and extra overrides in one pass (one themeChanged, colors visible in the palette).

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>

#include <QApplication>
#include <QColor>
#include <QObject>
#include <QPalette>

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
    }

    SECTION("stored info panel and Dashboard colors are applied") {
        auto& settings = kalahari::core::SettingsManager::getInstance();
        const std::string key = "themes." + originalTheme.toStdString() + ".colors.dashboardPrimary";
        settings.set(key, TEST_WINDOW.name().toStdString());

        REQUIRE(themeManager.reloadTheme(originalTheme));
        settings.removeKey(key);

        CHECK(themeManager.getCurrentTheme().colors.dashboardPrimary == TEST_WINDOW);
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
