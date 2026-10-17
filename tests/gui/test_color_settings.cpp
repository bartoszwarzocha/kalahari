/// @file test_color_settings.cpp
/// @brief The colors the writer can change in the Settings dialog: the waves under the
///        misspelled words and the grammar issues, the frames of their cards in the Spelling and
///        Grammar panel and the colors of the annotations

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/annotation.h"
#include "kalahari/editor/text_issue.h"
#include "kalahari/gui/issue_colors.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/color_config_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <string>

using namespace kalahari::gui;
using kalahari::core::SettingsManager;
using kalahari::core::ThemeManager;
using kalahari::editor::AnnotationKind;
using kalahari::editor::TextIssueKind;

namespace {

/// No color of the writer's while it lives: the theme's colors, the frames in the underlines'
class ThemeColorsOnly {
public:
    ThemeColorsOnly() { clear(); }
    ~ThemeColorsOnly() { clear(); }
    ThemeColorsOnly(const ThemeColorsOnly&) = delete;
    ThemeColorsOnly& operator=(const ThemeColorsOnly&) = delete;

private:
    static void clear() {
        auto& settings = SettingsManager::getInstance();
        for (const auto& [key, color] : ThemeManager::getInstance().getCurrentTheme().editor) {
            settings.removeKey(ThemeManager::editorColorSetting(key));
        }
        for (const TextIssueKind kind : {TextIssueKind::Spelling, TextIssueKind::Grammar}) {
            for (const bool dark : {false, true}) {
                settings.removeKey(issueFrameColorSetting(kind, dark));
            }
        }
        QCoreApplication::processEvents();  // The theme follows the settings
    }
};

/// Open the page with a title under a category ("" for a top-level page); return it
QWidget* openPage(SettingsDialog& dialog, const QString& category, const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        QTreeWidgetItem* parent = (*it)->parent();
        if ((*it)->text(0) == title && (parent ? parent->text(0) : QString()) == category) {
            tree->setCurrentItem(*it);
            auto* stack = dialog.findChild<QStackedWidget*>();
            REQUIRE(stack != nullptr);
            return stack->currentWidget();
        }
    }
    FAIL("No page " << title.toStdString());
    return nullptr;
}

/// The color of a page with a label
ColorConfigWidget* colorNamed(QWidget* page, const QString& label) {
    for (ColorConfigWidget* widget : page->findChildren<ColorConfigWidget*>()) {
        for (const QLabel* text : widget->findChildren<QLabel*>()) {
            if (text->text() == label) {
                return widget;
            }
        }
    }
    FAIL("No color " << label.toStdString());
    return nullptr;
}

template <typename Widget>
Widget* namedChild(QWidget* page, const QString& text) {
    for (Widget* widget : page->findChildren<Widget*>()) {
        if (widget->text() == text) {
            return widget;
        }
    }
    FAIL("No control " << text.toStdString());
    return nullptr;
}

/// The writer picks a color in the color window: the widget tells it
void pick(ColorConfigWidget* widget, const QColor& color) {
    widget->setColor(color);
    emit widget->colorChanged(color);
}

QColor themeColor(const std::string& key) {
    return ThemeManager::getInstance().defaultEditorColor(key, QColor());
}

std::string stored(const std::string& key) {
    return SettingsManager::getInstance().get<std::string>(key, std::string());
}

}  // namespace

TEST_CASE("Settings: the colors of the misspelled words, on the Spelling page",
          "[gui][settings][colors]") {
    const ThemeColorsOnly themeColors;
    auto& settings = SettingsManager::getInstance();
    auto& themes = ThemeManager::getInstance();
    const std::string lightKey = kalahari::editor::issueColorKey(TextIssueKind::Spelling, false);
    const std::string darkKey = kalahari::editor::issueColorKey(TextIssueKind::Spelling, true);
    const std::string lightSetting = ThemeManager::editorColorSetting(lightKey);
    const std::string frameLightSetting = issueFrameColorSetting(TextIssueKind::Spelling, false);
    const std::string frameDarkSetting = issueFrameColorSetting(TextIssueKind::Spelling, true);
    REQUIRE(themeColor(lightKey).isValid());
    REQUIRE(themeColor(darkKey).isValid());

    SettingsDialog dialog(nullptr);
    QWidget* page = openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Spelling"));
    ColorConfigWidget* light = colorNamed(page, QStringLiteral("Underline on light paper"));
    ColorConfigWidget* dark = colorNamed(page, QStringLiteral("Underline on dark paper"));
    ColorConfigWidget* frameLight = colorNamed(page, QStringLiteral("Frame in a light panel"));
    ColorConfigWidget* frameDark = colorNamed(page, QStringLiteral("Frame in a dark panel"));
    auto* own = namedChild<QCheckBox>(page, QStringLiteral("Own color of the cards' frames in the panel"));
    auto* restore = namedChild<QPushButton>(page, QStringLiteral("Restore Default Colors"));

    // The theme's colors; the frames in the underlines' colors, not to be changed by themselves
    CHECK(light->color() == themeColor(lightKey));
    CHECK(dark->color() == themeColor(darkKey));
    CHECK_FALSE(own->isChecked());
    CHECK_FALSE(frameLight->isEnabled());
    CHECK_FALSE(frameDark->isEnabled());
    CHECK(frameLight->color() == light->color());
    CHECK(frameDark->color() == dark->color());
    CHECK_FALSE(dialog.hasChanges());

    int themeChanges = 0;
    QObject guard;
    QObject::connect(&themes, &ThemeManager::themeChanged, &guard,
                     [&themeChanges]() { ++themeChanges; });

    SECTION("a new underline color reaches the editors, the frames follow it") {
        const QColor chosen(QStringLiteral("#123456"));
        pick(light, chosen);
        CHECK(frameLight->color() == chosen);
        CHECK(dialog.hasChanges());
        CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(lightSetting)});
        CHECK(stored(lightSetting) == "#123456");
        CHECK_FALSE(settings.hasKey(frameLightSetting));

        QCoreApplication::processEvents();
        CHECK(themeChanges == 1);
        CHECK(themes.editorColor(lightKey, QColor()) == chosen);
        CHECK(issueColor(TextIssueKind::Spelling, false) == chosen);
        CHECK(issueFrameColor(TextIssueKind::Spelling, false) == chosen);

        // The theme's own color again: nothing is kept, the theme decides it
        pick(light, themeColor(lightKey));
        CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(lightSetting)});
        CHECK_FALSE(settings.hasKey(lightSetting));
        QCoreApplication::processEvents();
        CHECK(themes.editorColor(lightKey, QColor()) == themeColor(lightKey));
    }

    SECTION("the frames can have their own colors") {
        own->click();
        CHECK(frameLight->isEnabled());
        CHECK(frameDark->isEnabled());
        // They start from the underlines' colors
        CHECK(frameLight->color() == light->color());
        CHECK(frameDark->color() == dark->color());

        const QColor green(QStringLiteral("#43a047"));
        pick(frameDark, green);
        const QStringList written = dialog.applyChanges();
        CHECK(written.size() == 2);
        CHECK(written.contains(QString::fromStdString(frameLightSetting)));
        CHECK(written.contains(QString::fromStdString(frameDarkSetting)));
        CHECK(stored(frameDarkSetting) == "#43a047");
        CHECK(QColor(QString::fromStdString(stored(frameLightSetting))) == themeColor(lightKey));
        CHECK(issueFrameColor(TextIssueKind::Spelling, true) == green);
        // The underline keeps its color
        CHECK(issueColor(TextIssueKind::Spelling, true) == themeColor(darkKey));

        // A new underline color leaves the own frames as they are
        pick(dark, QColor(QStringLiteral("#ff00ff")));
        CHECK(frameDark->color() == green);

        // Opened again: the own colors are shown as such
        SettingsDialog again(nullptr);
        QWidget* pageAgain = openPage(again, QStringLiteral("Editor"), QStringLiteral("Spelling"));
        CHECK(namedChild<QCheckBox>(pageAgain, QStringLiteral("Own color of the cards' frames in the panel"))
                  ->isChecked());
        CHECK(colorNamed(pageAgain, QStringLiteral("Frame in a dark panel"))->color() == green);
        CHECK_FALSE(again.hasChanges());

        // Without their own colors again, the frames take the underlines' ones
        own->click();
        CHECK(frameDark->color() == dark->color());
        dialog.applyChanges();
        CHECK_FALSE(settings.hasKey(frameLightSetting));
        CHECK_FALSE(settings.hasKey(frameDarkSetting));
        QCoreApplication::processEvents();  // The theme follows the underline's new color
        CHECK(issueFrameColor(TextIssueKind::Spelling, true) == QColor(QStringLiteral("#ff00ff")));
    }

    SECTION("the default colors come back with a button") {
        pick(light, QColor(QStringLiteral("#123456")));
        own->click();
        pick(frameLight, QColor(QStringLiteral("#654321")));
        dialog.applyChanges();
        REQUIRE(settings.hasKey(lightSetting));
        REQUIRE(settings.hasKey(frameLightSetting));

        restore->click();
        CHECK(light->color() == themeColor(lightKey));
        CHECK_FALSE(own->isChecked());
        CHECK_FALSE(frameLight->isEnabled());
        CHECK(frameLight->color() == themeColor(lightKey));
        // Shown only: Apply and OK keep them, as any change
        CHECK(settings.hasKey(lightSetting));
        dialog.applyChanges();
        CHECK_FALSE(settings.hasKey(lightSetting));
        CHECK_FALSE(settings.hasKey(frameLightSetting));
        CHECK_FALSE(settings.hasKey(frameDarkSetting));
    }
}

TEST_CASE("Settings: the colors of the grammar issues, on the Grammar page",
          "[gui][settings][colors]") {
    const ThemeColorsOnly themeColors;
    auto& settings = SettingsManager::getInstance();
    const std::string darkKey = kalahari::editor::issueColorKey(TextIssueKind::Grammar, true);
    const std::string darkSetting = ThemeManager::editorColorSetting(darkKey);

    SettingsDialog dialog(nullptr);
    QWidget* page = openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Grammar"));
    ColorConfigWidget* light = colorNamed(page, QStringLiteral("Underline on light paper"));
    ColorConfigWidget* dark = colorNamed(page, QStringLiteral("Underline on dark paper"));
    CHECK(light->color() ==
          themeColor(kalahari::editor::issueColorKey(TextIssueKind::Grammar, false)));
    CHECK(dark->color() == themeColor(darkKey));
    CHECK_FALSE(dialog.hasChanges());

    pick(dark, QColor(QStringLiteral("#00a0ff")));
    CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(darkSetting)});
    CHECK(stored(darkSetting) == "#00a0ff");
    QCoreApplication::processEvents();
    CHECK(issueColor(TextIssueKind::Grammar, true) == QColor(QStringLiteral("#00a0ff")));
    // The spelling keeps its colors
    CHECK(issueColor(TextIssueKind::Spelling, true) ==
          themeColor(kalahari::editor::issueColorKey(TextIssueKind::Spelling, true)));

    // The frames of the grammar's cards, by themselves
    namedChild<QCheckBox>(page, QStringLiteral("Own color of the cards' frames in the panel"))->click();
    pick(colorNamed(page, QStringLiteral("Frame in a light panel")), QColor(QStringLiteral("#aa5500")));
    dialog.applyChanges();
    CHECK(issueFrameColor(TextIssueKind::Grammar, false) == QColor(QStringLiteral("#aa5500")));
    CHECK_FALSE(settings.hasKey(issueFrameColorSetting(TextIssueKind::Spelling, false)));

    namedChild<QPushButton>(page, QStringLiteral("Restore Default Colors"))->click();
    dialog.applyChanges();
    CHECK_FALSE(settings.hasKey(darkSetting));
    CHECK_FALSE(settings.hasKey(issueFrameColorSetting(TextIssueKind::Grammar, false)));
}

TEST_CASE("Settings: the colors of the annotations, on the Annotations page",
          "[gui][settings][colors]") {
    const ThemeColorsOnly themeColors;
    auto& settings = SettingsManager::getInstance();
    auto& themes = ThemeManager::getInstance();

    SettingsDialog dialog(nullptr);
    QWidget* page = openPage(dialog, QString(), QStringLiteral("Annotations"));
    const std::pair<const char*, std::string> colors[] = {
        {"Comment on light paper", kalahari::editor::annotationColorKey(AnnotationKind::Comment, false)},
        {"Comment on dark paper", kalahari::editor::annotationColorKey(AnnotationKind::Comment, true)},
        {"To-do on light paper", kalahari::editor::annotationColorKey(AnnotationKind::Todo, false)},
        {"To-do on dark paper", kalahari::editor::annotationColorKey(AnnotationKind::Todo, true)},
        {"Note on light paper", kalahari::editor::annotationColorKey(AnnotationKind::Note, false)},
        {"Note on dark paper", kalahari::editor::annotationColorKey(AnnotationKind::Note, true)},
    };
    for (const auto& [label, key] : colors) {
        INFO(label);
        REQUIRE(themeColor(key).isValid());
        CHECK(colorNamed(page, QString::fromLatin1(label))->color() == themeColor(key));
    }
    CHECK_FALSE(dialog.hasChanges());

    const std::string todoDark =
        ThemeManager::editorColorSetting(kalahari::editor::annotationColorKey(AnnotationKind::Todo, true));
    pick(colorNamed(page, QStringLiteral("To-do on dark paper")), QColor(QStringLiteral("#ff8800")));
    CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(todoDark)});
    CHECK(stored(todoDark) == "#ff8800");
    QCoreApplication::processEvents();
    CHECK(themes.editorColor(kalahari::editor::annotationColorKey(AnnotationKind::Todo, true),
                             QColor()) == QColor(QStringLiteral("#ff8800")));

    namedChild<QPushButton>(page, QStringLiteral("Restore Default Colors"))->click();
    CHECK(colorNamed(page, QStringLiteral("To-do on dark paper"))->color() ==
          themeColor(kalahari::editor::annotationColorKey(AnnotationKind::Todo, true)));
    CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(todoDark)});
    CHECK_FALSE(settings.hasKey(todoDark));
}

TEST_CASE("Settings: the options of the Spelling and Grammar panel, on the Spelling page",
          "[gui][settings]") {
    auto& settings = SettingsManager::getInstance();
    for (const char* key : {"spellingPanel.fixButtons", "spellingPanel.nextIssue",
                            "spellingPanel.grammarScope"}) {
        settings.removeKey(key);
    }

    SettingsDialog dialog(nullptr);
    QWidget* page = openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Spelling"));
    auto* fixButtons = page->findChild<QComboBox*>(QStringLiteral("spellingPanelFixButtons"));
    auto* nextIssue = page->findChild<QComboBox*>(QStringLiteral("spellingPanelNextIssue"));
    auto* grammarScope = page->findChild<QComboBox*>(QStringLiteral("spellingPanelGrammarScope"));
    REQUIRE(fixButtons != nullptr);
    REQUIRE(nextIssue != nullptr);
    REQUIRE(grammarScope != nullptr);

    // The defaults: the fix buttons on the selected card only, F7 shows the issue in the panel,
    // the grammar of the whole text while the panel is shown
    CHECK(fixButtons->currentData().toString() == QStringLiteral("selected"));
    CHECK(nextIssue->currentData().toString() == QStringLiteral("panel"));
    CHECK(grammarScope->currentData().toString() == QStringLiteral("whole"));
    CHECK_FALSE(dialog.hasChanges());

    fixButtons->setCurrentIndex(fixButtons->findData(QStringLiteral("every")));
    nextIssue->setCurrentIndex(nextIssue->findData(QStringLiteral("menu")));
    grammarScope->setCurrentIndex(grammarScope->findData(QStringLiteral("nearby")));
    CHECK(dialog.applyChanges().size() == 3);
    CHECK(settings.get<std::string>("spellingPanel.fixButtons") == "every");
    CHECK(settings.get<std::string>("spellingPanel.nextIssue") == "menu");
    CHECK(settings.get<std::string>("spellingPanel.grammarScope") == "nearby");

    for (const char* key : {"spellingPanel.fixButtons", "spellingPanel.nextIssue",
                            "spellingPanel.grammarScope"}) {
        settings.removeKey(key);
    }
}
