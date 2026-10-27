/// @file test_theme_manager_apply.cpp
/// @brief ThemeManager::reloadTheme applies a theme with the user's stored colors
/// and extra overrides in one pass (one themeChanged, colors visible in the palette);
/// the editor colors of a theme (the writer's own ones are kept for each theme, also the colors of
/// the papers, which moved into the themes), and the colors its palette gives disabled widgets.

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>
#include <kalahari/editor/editor_appearance.h>

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QObject>
#include <QPalette>
#include <QToolTip>

#include <map>
#include <string>
#include <utility>

using kalahari::core::Theme;
using kalahari::core::ThemeManager;
using namespace kalahari::editor;

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

TEST_CASE("The writer's editor colors are kept for each theme and reach the theme by themselves",
          "[core][theme]") {
    // A color chosen in one theme does not change another theme, and a changed setting reaches
    // the editors and the panels
    auto& themeManager = ThemeManager::getInstance();
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const QString originalTheme = QString::fromStdString(themeManager.getCurrentTheme().name);
    const QString otherTheme = (originalTheme == "Dark") ? "Light" : "Dark";
    const std::string spelling = "spelling";
    const std::string grammar = "grammar";
    const std::string spellingSetting =
        ThemeManager::editorColorSetting(originalTheme.toStdString(), spelling);
    const std::string grammarSetting =
        ThemeManager::editorColorSetting(originalTheme.toStdString(), grammar);
    CHECK(ThemeManager::editorColorSetting("Light", spelling) == "themes.Light.editor.spelling");
    const QColor themeColor = themeManager.defaultEditorColor(spelling, QColor());
    REQUIRE(themeColor.isValid());
    CHECK(themeManager.defaultEditorColor("missingMarker", QColor("#123456")) == QColor("#123456"));

    // The editors and the panels show them anew; the rest of the window (its icons drawn
    // anew, ...) is not themed again for them
    int colorsChangedCount = 0;
    int themeChangedCount = 0;
    QObject guard;
    QObject::connect(&themeManager, &ThemeManager::editorColorsChanged, &guard,
                     [&colorsChangedCount]() { ++colorsChangedCount; });
    QObject::connect(&themeManager, &ThemeManager::themeChanged, &guard,
                     [&themeChangedCount]() { ++themeChangedCount; });

    // Colors written together (Apply in the Settings dialog) reach the theme together, later
    settings.set<std::string>(spellingSetting, "#abcdef");
    settings.set<std::string>(grammarSetting, "#fedcba");
    CHECK(colorsChangedCount == 0);
    QCoreApplication::processEvents();
    CHECK(colorsChangedCount == 1);
    CHECK(themeChangedCount == 0);
    CHECK(themeManager.editorColor(spelling, QColor()) == QColor("#abcdef"));
    CHECK(themeManager.editorColor(grammar, QColor()) == QColor("#fedcba"));
    CHECK(themeManager.defaultEditorColor(spelling, QColor()) == themeColor);

    // Another theme has its own colors; a color of a theme not in use changes nothing
    REQUIRE(themeManager.reloadTheme(otherTheme));
    CHECK(themeManager.editorColor(spelling, QColor()) ==
          themeManager.defaultEditorColor(spelling, QColor()));
    CHECK(themeManager.editorColor(grammar, QColor()) ==
          themeManager.defaultEditorColor(grammar, QColor()));
    int before = colorsChangedCount;
    settings.set<std::string>(spellingSetting, "#123123");
    QCoreApplication::processEvents();
    CHECK(colorsChangedCount == before);

    // Back in the first theme, its colors are there again
    REQUIRE(themeManager.reloadTheme(originalTheme));
    CHECK(themeManager.editorColor(spelling, QColor()) == QColor("#123123"));
    CHECK(themeManager.editorColor(grammar, QColor()) == QColor("#fedcba"));

    // Without the settings, the theme's own colors come back
    settings.removeKey(spellingSetting);
    settings.removeKey(grammarSetting);
    QCoreApplication::processEvents();
    CHECK(themeManager.editorColor(spelling, QColor()) == themeColor);
    CHECK(themeManager.editorColor(grammar, QColor()) ==
          themeManager.defaultEditorColor(grammar, QColor()));

    // A setting that changes no color of the theme changes nothing
    before = colorsChangedCount;
    const std::string frame = "editor.textFrameBorder.color";
    const std::string frameColor = settings.get<std::string>(frame);
    settings.set<std::string>(frame, "#fafafa");
    QCoreApplication::processEvents();
    CHECK(colorsChangedCount == before);
    settings.set<std::string>(frame, frameColor);
    QCoreApplication::processEvents();
}

TEST_CASE("Both themes give the colors of both papers as the editor had them",
          "[core][theme][colors]") {
    // The theme in use is not changed: the files are read as they are
    const EditorColors own;
    const std::pair<std::string, QColor> colors[] = {
        {LIGHT_PAPER_COLOR_KEYS.paper, own.continuous.backgroundLight},
        {LIGHT_PAPER_COLOR_KEYS.text, own.continuous.textLight},
        {LIGHT_PAPER_COLOR_KEYS.focusDimmed, own.focus.inactiveLight},
        {LIGHT_PAPER_COLOR_KEYS.formattingMarks, own.formattingMarks.light},
        {LIGHT_PAPER_COLOR_KEYS.desk, own.continuous.deskLight},
        {DARK_PAPER_COLOR_KEYS.paper, own.continuous.backgroundDark},
        {DARK_PAPER_COLOR_KEYS.text, own.continuous.textDark},
        {DARK_PAPER_COLOR_KEYS.focusDimmed, own.focus.inactiveDark},
        {DARK_PAPER_COLOR_KEYS.formattingMarks, own.formattingMarks.dark},
        {DARK_PAPER_COLOR_KEYS.desk, own.continuous.deskDark},
        {SELECTION_COLOR_KEY, underTextColor(own.selection, 255)},
        {SEARCH_MATCH_COLOR_KEY, underTextColor(own.searchMatch, 255)},
        {CURRENT_SEARCH_MATCH_COLOR_KEY, underTextColor(own.currentSearchMatch, 255)},
    };
    for (const char* name : {"Light", "Dark"}) {
        const Theme theme = ThemeManager::getInstance().loadTheme(QString::fromLatin1(name));
        for (const auto& [key, color] : colors) {
            INFO(name << " " << key);
            REQUIRE(theme.editor.count(key) == 1);
            CHECK(theme.editor.at(key) == color);
        }
    }

    // The desk is the shade the editor gave each paper before it had a color of its own
    CHECK(own.continuous.deskLight.rgb() == own.continuous.backgroundLight.darker(118).rgb());
    CHECK(own.continuous.deskDark.rgb() == own.continuous.backgroundDark.lighter(160).rgb());
    // Drawn under the text as before: translucent
    CHECK(own.selection == QColor(66, 133, 244, 80));
    CHECK(own.searchMatch == QColor(255, 214, 0, 96));
    CHECK(own.currentSearchMatch == QColor(255, 140, 0, 140));
}

TEST_CASE("The colors of the papers kept outside the themes move into each theme",
          "[core][theme][colors]") {
    auto& themeManager = ThemeManager::getInstance();
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const auto setting = [](const char* theme, const char* key) {
        return ThemeManager::editorColorSetting(theme, key);
    };
    const std::string keptInDark = setting("Dark", LIGHT_PAPER_COLOR_KEYS.paper);
    const std::string added[] = {
        setting("Light", LIGHT_PAPER_COLOR_KEYS.paper),
        setting("Light", DARK_PAPER_COLOR_KEYS.text),
        setting("Dark", DARK_PAPER_COLOR_KEYS.text),
        setting("Light", LIGHT_PAPER_COLOR_KEYS.formattingMarks),
        setting("Dark", LIGHT_PAPER_COLOR_KEYS.formattingMarks),
    };
    const auto clear = [&]() {
        settings.removeKey(keptInDark);
        for (const std::string& key : added) {
            settings.removeKey(key);
        }
    };
    clear();

    // A cream paper of the writer's, the dark paper's text as the themes have it, a broken
    // color; the dark theme has its own light paper already
    settings.set<std::string>("editor.colors.backgroundLight", "#f5ecd7");
    settings.set<std::string>("editor.colors.textDark", "#e0e0e0");
    settings.set<std::string>("editor.colors.formattingMarksLight", "not a color");
    settings.set<std::string>(keptInDark, "#101010");

    themeManager.moveFormerPaperColors();
    for (const char* former : {"editor.colors.backgroundLight", "editor.colors.textDark",
                               "editor.colors.formattingMarksLight"}) {
        INFO(former);
        CHECK_FALSE(settings.hasKey(former));
    }
    CHECK(settings.get<std::string>(added[0], std::string()) == "#f5ecd7");
    CHECK(settings.get<std::string>(keptInDark, std::string()) == "#101010");
    for (const std::string& key : {added[1], added[2], added[3], added[4]}) {
        INFO(key);
        CHECK_FALSE(settings.hasKey(key));
    }

    // Once moved, nothing more to move
    settings.removeKey(added[0]);
    themeManager.moveFormerPaperColors();
    CHECK_FALSE(settings.hasKey(added[0]));

    clear();
    QCoreApplication::processEvents();  // the theme follows the settings
}

TEST_CASE("Optimize colors in themes is on by default and its change reaches the editors",
          "[core][theme]") {
    auto& themeManager = ThemeManager::getInstance();
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string optimize = ThemeManager::OPTIMIZE_COLORS_SETTING;
    settings.removeKey(optimize);
    QCoreApplication::processEvents();
    CHECK(themeManager.optimizesEditorColors());

    int colorsChangedCount = 0;
    int themeChangedCount = 0;
    QObject guard;
    QObject::connect(&themeManager, &ThemeManager::editorColorsChanged, &guard,
                     [&colorsChangedCount]() { ++colorsChangedCount; });
    QObject::connect(&themeManager, &ThemeManager::themeChanged, &guard,
                     [&themeChangedCount]() { ++themeChangedCount; });

    // The editors and the panels show the colors anew, not the rest of the window
    settings.set<bool>(optimize, false);
    CHECK_FALSE(themeManager.optimizesEditorColors());
    QCoreApplication::processEvents();
    CHECK(colorsChangedCount == 1);
    CHECK(themeChangedCount == 0);

    // Written again unchanged: nothing to show anew
    settings.set<bool>(optimize, false);
    QCoreApplication::processEvents();
    CHECK(colorsChangedCount == 1);

    settings.removeKey(optimize);
    QCoreApplication::processEvents();
    CHECK(themeManager.optimizesEditorColors());
    CHECK(colorsChangedCount == 2);
    CHECK(themeChangedCount == 0);
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
