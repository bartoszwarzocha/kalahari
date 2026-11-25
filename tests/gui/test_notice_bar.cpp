// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_notice_bar.cpp
/// @brief The bar at the bottom of the text (StandaloneInfoBar): its own colors of the theme,
///        which stand out from the text and from the other bars, its button, the access keys
///        of the buttons of the bars and its gear

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/diagnostic_menus.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QTranslator>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

using namespace kalahari;
using gui::StandaloneInfoBar;

namespace {

void settle()
{
    for (int i = 0; i < 5; ++i) {
        QApplication::processEvents();
    }
}

/// The five keys of the bar
constexpr std::array<const char*, 5> KEYS{
    StandaloneInfoBar::BACKGROUND_COLOR_KEY, StandaloneInfoBar::BORDER_COLOR_KEY,
    StandaloneInfoBar::TEXT_COLOR_KEY, StandaloneInfoBar::BUTTON_COLOR_KEY,
    StandaloneInfoBar::BUTTON_TEXT_COLOR_KEY};

/// The contrast of two colors, as WCAG counts it (1 to 21)
double contrast(const QColor& a, const QColor& b)
{
    const auto luminance = [](const QColor& color) {
        const auto channel = [](double value) {
            return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
        };
        return (0.2126 * channel(color.redF())) + (0.7152 * channel(color.greenF())) +
               (0.0722 * channel(color.blueF()));
    };
    const double first = luminance(a);
    const double second = luminance(b);
    return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
}

/// A theme file of the program
nlohmann::json themeFile(const char* name)
{
    QFile file(QStringLiteral(KALAHARI_SOURCE_DIR "/resources/themes/") +
               QString::fromLatin1(name) + QStringLiteral(".json"));
    REQUIRE(file.open(QIODevice::ReadOnly));
    return nlohmann::json::parse(file.readAll().toStdString());
}

QColor colorOf(const nlohmann::json& section, const char* key)
{
    REQUIRE(section.contains(key));
    return QColor(QString::fromStdString(section.at(key).get<std::string>()));
}

/// The letter of the access key of @p text ("&Undo": "u"); none: an empty string
QString accessKey(const QString& text)
{
    return QKeySequence::mnemonic(text).toString().right(1).toLower();
}

/// The access keys of the menus of the window, as they read now
QStringList menuKeys()
{
    QStringList keys;
    for (const char* menu : {"FILE", "EDIT", "BOOK", "INSERT", "FORMAT", "TABLE", "IMAGE", "TOOLS",
                             "ASSISTANT", "VIEW", "WINDOW", "HELP"}) {
        keys << accessKey(gui::MenuBuilder::menuText(menu));
    }
    keys << accessKey(gui::diagnosticsMenuTitle()) << accessKey(gui::devToolsMenuTitle());
    return keys;
}

/// The buttons of the bars under the text and under the plot board, by their contexts
constexpr std::array<std::array<const char*, 2>, 10> BAR_BUTTONS{{
    {"kalahari::gui::BookSearchCoordinator", "&Undo"},
    {"kalahari::gui::ChapterBreakCoordinator", "&Undo"},
    {"kalahari::gui::ClippingsCoordinator", "&Undo"},
    {"kalahari::gui::ClippingsCoordinator", "I&nsert Here"},
    {"kalahari::gui::FieldCoordinator", "&Undo"},
    {"kalahari::gui::FootnotesPanelCoordinator", "&Undo"},
    {"kalahari::gui::ImageFilterCoordinator", "&Undo"},
    {"kalahari::gui::PlotBoardPanel", "&Undo"},
    {"kalahari::gui::StyleCoordinator", "&Undo"},
    {"kalahari::gui::VersionsCoordinator", "&Undo"},
}};

/// The buttons' access keys differ from the menus': a key of both only moves the focus to the
/// button, and the next press opens the menu (Alt+C of "&Cofnij" and "Pomo&c")
void checkBarKeys()
{
    const QStringList menus = menuKeys();
    for (const auto& [context, source] : BAR_BUTTONS) {
        const QString text = QCoreApplication::translate(context, source);
        INFO(context << ": " << text.toStdString());
        CHECK_FALSE(accessKey(text).isEmpty());
        CHECK_FALSE(menus.contains(accessKey(text)));
    }
}

}  // namespace

TEST_CASE("Bar under the text: in both themes its colors stand out and read well",
          "[gui][widgets][noticeBar]")
{
    for (const char* name : {"Light", "Dark"}) {
        INFO("Theme " << name);
        const nlohmann::json theme = themeFile(name);
        const nlohmann::json& editor = theme.at("editor");
        const QColor background = colorOf(editor, StandaloneInfoBar::BACKGROUND_COLOR_KEY);
        const QColor border = colorOf(editor, StandaloneInfoBar::BORDER_COLOR_KEY);
        const QColor text = colorOf(editor, StandaloneInfoBar::TEXT_COLOR_KEY);
        const QColor button = colorOf(editor, StandaloneInfoBar::BUTTON_COLOR_KEY);
        const QColor buttonText = colorOf(editor, StandaloneInfoBar::BUTTON_TEXT_COLOR_KEY);

        // Not the colors of the other bars, nor the paper of either kind
        CHECK(background != colorOf(theme.at("colors"), "infoBarBackground"));
        CHECK(background != colorOf(editor, "notificationBackground"));
        CHECK(background != colorOf(editor, editor::LIGHT_PAPER_COLOR_KEYS.paper));
        CHECK(background != colorOf(editor, editor::DARK_PAPER_COLOR_KEYS.paper));
        // The message and the button's text read well; the border and the button are seen
        CHECK(contrast(text, background) >= 7.0);
        CHECK(contrast(buttonText, button) >= 4.5);
        CHECK(contrast(border, background) >= 3.0);
        CHECK(contrast(button, background) >= 3.0);
    }
}

TEST_CASE("Bar under the text: the colors of the theme, the writer's in their place",
          "[gui][widgets][noticeBar]")
{
    auto& themes = core::ThemeManager::getInstance();
    for (const char* key : KEYS) {
        INFO("Key " << key);
        CHECK(themes.editorColor(key, QColor()).isValid());
    }
    const QColor background = themes.editorColor(StandaloneInfoBar::BACKGROUND_COLOR_KEY, {});
    const QColor text = themes.editorColor(StandaloneInfoBar::TEXT_COLOR_KEY, {});
    const QColor button = themes.editorColor(StandaloneInfoBar::BUTTON_COLOR_KEY, {});
    const QColor buttonText = themes.editorColor(StandaloneInfoBar::BUTTON_TEXT_COLOR_KEY, {});

    StandaloneInfoBar bar;
    bar.showNotice(QStringLiteral("Deleted: Footnote 1."), QStringLiteral("&Undo"));
    bar.resize(600, bar.sizeHint().height());
    bar.show();
    settle();
    QPushButton* action = bar.actionButton();
    REQUIRE(action != nullptr);
    REQUIRE(action->isVisibleTo(&bar));
    const QLabel* message = nullptr;
    for (const QLabel* label : bar.findChildren<QLabel*>()) {
        if (label->text() == bar.message()) {
            message = label;
        }
    }
    REQUIRE(message != nullptr);

    // As painted: the bar's background before its icon, the button filled with its color, in
    // bold; the message in its color
    const auto barPixel = [&bar]() {
        const QImage image = bar.grab().toImage();
        return image.pixelColor(2, image.height() / 2);
    };
    const auto buttonPixel = [action]() {
        const QImage image = action->grab().toImage();
        return image.pixelColor(4, image.height() / 2);
    };
    CHECK(barPixel() == background);
    CHECK(buttonPixel() == button);
    CHECK(action->styleSheet().contains(QStringLiteral("font-weight: bold")));
    CHECK(action->styleSheet().contains(buttonText.name()));
    CHECK(message->styleSheet().contains(text.name()));

    // The writer's colors, kept for the theme, at once
    auto& settings = core::SettingsManager::getInstance();
    const std::string theme = themes.getCurrentTheme().name;
    const std::string backgroundSetting =
        core::ThemeManager::editorColorSetting(theme, StandaloneInfoBar::BACKGROUND_COLOR_KEY);
    const std::string buttonSetting =
        core::ThemeManager::editorColorSetting(theme, StandaloneInfoBar::BUTTON_COLOR_KEY);
    settings.set<std::string>(backgroundSetting, "#123456");
    settings.set<std::string>(buttonSetting, "#AA3300");
    settle();
    CHECK(barPixel() == QColor(QStringLiteral("#123456")));
    CHECK(buttonPixel() == QColor(QStringLiteral("#AA3300")));
    settings.removeKey(backgroundSetting);
    settings.removeKey(buttonSetting);
    settle();
    CHECK(barPixel() == background);
    CHECK(buttonPixel() == button);

    // Greyed out, the button still looks so (a file the closed book cannot take)
    bar.setFilePath(QStringLiteral("/notes/notes.txt"));
    bar.setBookOpen(false);
    settle();
    CHECK_FALSE(action->isEnabled());
    CHECK(buttonPixel() != button);
}

TEST_CASE("Bars under the text: the access keys of their buttons are not those of the menus",
          "[gui][widgets][noticeBar][keys]")
{
    SECTION("in English") { checkBarKeys(); }
    SECTION("in Polish, the translation the program ships")
    {
        QTranslator polish;
        REQUIRE(polish.load(QStringLiteral(":/i18n/kalahari_pl.qm")));
        QCoreApplication::installTranslator(&polish);
        CHECK(QCoreApplication::translate("kalahari::gui::FootnotesPanelCoordinator", "&Undo") ==
              QStringLiteral("Cofn&ij"));
        checkBarKeys();
        QCoreApplication::removeTranslator(&polish);
    }
}

TEST_CASE("Bar under the text: its gear opens its colors in the Settings",
          "[gui][widgets][noticeBar][settings]")
{
    StandaloneInfoBar bar;
    QToolButton* gear = bar.settingsButton();
    REQUIRE(gear != nullptr);
    CHECK(gear->toolTip() == QStringLiteral("Colors of the Bar"));
    int asked = 0;
    QObject::connect(&bar, &gui::InfoBar::settingsClicked, [&asked]() { ++asked; });
    gear->click();
    CHECK(asked == 1);

    // Editor > Colors, the group of the bar in view, with its five colors
    gui::SettingsDialog dialog(kalahari::test::book(), kalahari::test::commands(), nullptr);
    REQUIRE(dialog.openPage(QString::fromLatin1(gui::SettingsDialog::NOTICE_BAR_PAGE)));
    auto* group = dialog.findChild<QWidget*>(QStringLiteral("noticeBarGroup"));
    REQUIRE(group != nullptr);
    CHECK(group->isVisibleTo(&dialog));
    CHECK(group->findChildren<gui::ColorConfigWidget*>().size() == 5);
    QStringList labels;
    for (const QLabel* label : group->findChildren<QLabel*>()) {
        labels << label->text();
    }
    for (const char* label : {"Background", "Border", "Message", "Button", "Button text"}) {
        INFO("Label " << label);
        CHECK(labels.contains(QString::fromLatin1(label)));
    }
    auto* restore = group->findChild<QPushButton*>();
    CHECK(restore != nullptr);
}
