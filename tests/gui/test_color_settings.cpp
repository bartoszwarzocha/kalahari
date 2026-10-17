/// @file test_color_settings.cpp
/// @brief The colors the writer can change in the Settings dialog: the waves under the
///        misspelled words and the grammar issues (and the frames of their cards in the Spelling
///        and Grammar panel) and the colors of the annotations, kept for each theme, and
///        "Optimize colors in themes"

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
#include <QGroupBox>
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

/// No color of the writer's in any theme while it lives, and the colors optimized as by
/// default: the themes' colors; the theme in use stays
class ThemeColorsOnly {
public:
    ThemeColorsOnly()
        : m_theme(QString::fromStdString(ThemeManager::getInstance().getCurrentTheme().name)) {
        clear();
    }
    ~ThemeColorsOnly() {
        ThemeManager::getInstance().reloadTheme(m_theme);
        clear();
    }
    ThemeColorsOnly(const ThemeColorsOnly&) = delete;
    ThemeColorsOnly& operator=(const ThemeColorsOnly&) = delete;

private:
    static void clear() {
        auto& settings = SettingsManager::getInstance();
        for (const QString& theme : ThemeManager::getInstance().getAvailableThemes()) {
            for (const auto& [key, color] : ThemeManager::getInstance().getCurrentTheme().editor) {
                settings.removeKey(ThemeManager::editorColorSetting(theme.toStdString(), key));
            }
        }
        settings.removeKey(ThemeManager::OPTIMIZE_COLORS_SETTING);
        QCoreApplication::processEvents();  // The theme follows the settings
    }

    QString m_theme;
};

/// Open the page with a title under a category ("" for a top-level page); return it
QWidget* openPage(SettingsDialog& dialog, const QString& category, const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    QTreeWidgetItem* found = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it != nullptr && found == nullptr; ++it) {
        QTreeWidgetItem* parent = (*it)->parent();
        if ((*it)->text(0) == title && (parent ? parent->text(0) : QString()) == category) {
            found = *it;
        }
    }
    INFO("The page " << title.toStdString());
    REQUIRE(found != nullptr);
    tree->setCurrentItem(found);
    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    return stack->currentWidget();
}

/// The colors of a page's group with a title
QList<ColorConfigWidget*> colorsOfGroup(QWidget* page, const QString& title) {
    const QGroupBox* found = nullptr;
    for (const QGroupBox* group : page->findChildren<QGroupBox*>()) {
        if (found == nullptr && group->title() == title) {
            found = group;
        }
    }
    INFO("The group " << title.toStdString());
    REQUIRE(found != nullptr);
    return found->findChildren<ColorConfigWidget*>();
}

/// The color of a page with a label
ColorConfigWidget* colorNamed(QWidget* page, const QString& label) {
    ColorConfigWidget* found = nullptr;
    for (ColorConfigWidget* widget : page->findChildren<ColorConfigWidget*>()) {
        for (const QLabel* text : widget->findChildren<QLabel*>()) {
            if (found == nullptr && text->text() == label) {
                found = widget;
            }
        }
    }
    INFO("The color " << label.toStdString());
    REQUIRE(found != nullptr);
    return found;
}

template <typename Widget>
Widget* namedChild(QWidget* page, const QString& text) {
    Widget* found = nullptr;
    for (Widget* widget : page->findChildren<Widget*>()) {
        if (found == nullptr && widget->text() == text) {
            found = widget;
        }
    }
    INFO("The control " << text.toStdString());
    REQUIRE(found != nullptr);
    return found;
}

/// The writer picks a color in the color window: the widget tells it
void pick(ColorConfigWidget* widget, const QColor& color) {
    widget->setColor(color);
    emit widget->colorChanged(color);
}

QColor themeColor(const std::string& key) {
    return ThemeManager::getInstance().defaultEditorColor(key, QColor());
}

/// The setting of a color of the theme in use
std::string settingOf(const std::string& key) {
    return ThemeManager::editorColorSetting(ThemeManager::getInstance().getCurrentTheme().name,
                                            key);
}

/// The check box "Optimize colors in themes" of a page
QCheckBox* optimizeBox(QWidget* page) {
    auto* box = page->findChild<QCheckBox*>(QStringLiteral("optimizeThemeColors"));
    REQUIRE(box != nullptr);
    CHECK(box->text() == QStringLiteral("Optimize colors in themes"));
    return box;
}

std::string stored(const std::string& key) {
    return SettingsManager::getInstance().get<std::string>(key, std::string());
}

}  // namespace

TEST_CASE("Settings: the color of the misspelled words, on the Spelling page",
          "[gui][settings][colors]") {
    const ThemeColorsOnly themeColors;
    auto& settings = SettingsManager::getInstance();
    auto& themes = ThemeManager::getInstance();
    const std::string key = kalahari::editor::issueColorKey(TextIssueKind::Spelling);
    const std::string setting = settingOf(key);
    REQUIRE(themeColor(key).isValid());

    SettingsDialog dialog(nullptr);
    QWidget* page = openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Spelling"));
    ColorConfigWidget* underline = colorNamed(page, QStringLiteral("Underline"));
    auto* restore = namedChild<QPushButton>(page, QStringLiteral("Restore Default Colors"));

    // The theme's color: the frames of the cards in the panel have it too, without a color of
    // their own; the colors optimized by default
    CHECK(underline->color() == themeColor(key));
    CHECK(colorsOfGroup(page, QStringLiteral("Colors")).size() == 1);
    CHECK(optimizeBox(page)->isChecked());
    CHECK_FALSE(dialog.hasChanges());

    int themeChanges = 0;
    QObject guard;
    QObject::connect(&themes, &ThemeManager::themeChanged, &guard,
                     [&themeChanges]() { ++themeChanges; });

    SECTION("a new underline color reaches the editors and the panel") {
        const QColor chosen(QStringLiteral("#123456"));
        pick(underline, chosen);
        CHECK(dialog.hasChanges());
        CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(setting)});
        CHECK(stored(setting) == "#123456");

        QCoreApplication::processEvents();
        CHECK(themeChanges == 1);
        CHECK(themes.editorColor(key, QColor()) == chosen);
        CHECK(issueColor(TextIssueKind::Spelling) == chosen);
        // The grammar keeps its color
        CHECK(issueColor(TextIssueKind::Grammar) ==
              themeColor(kalahari::editor::issueColorKey(TextIssueKind::Grammar)));

        // The theme's own color again: nothing is kept, the theme decides it
        pick(underline, themeColor(key));
        CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(setting)});
        CHECK_FALSE(settings.hasKey(setting));
        QCoreApplication::processEvents();
        CHECK(themes.editorColor(key, QColor()) == themeColor(key));
    }

    SECTION("the default color comes back with a button") {
        pick(underline, QColor(QStringLiteral("#123456")));
        dialog.applyChanges();
        REQUIRE(settings.hasKey(setting));

        restore->click();
        CHECK(underline->color() == themeColor(key));
        // Shown only: Apply and OK keep it, as any change
        CHECK(settings.hasKey(setting));
        CHECK(optimizeBox(page)->isChecked());  // not a color
        dialog.applyChanges();
        CHECK_FALSE(settings.hasKey(setting));
    }
}

TEST_CASE("Settings: the color of the grammar issues, on the Grammar page",
          "[gui][settings][colors]") {
    const ThemeColorsOnly themeColors;
    auto& settings = SettingsManager::getInstance();
    const std::string key = kalahari::editor::issueColorKey(TextIssueKind::Grammar);
    const std::string setting = settingOf(key);

    SettingsDialog dialog(nullptr);
    QWidget* page = openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Grammar"));
    ColorConfigWidget* underline = colorNamed(page, QStringLiteral("Underline"));
    CHECK(underline->color() == themeColor(key));
    CHECK(colorsOfGroup(page, QStringLiteral("Colors")).size() == 1);
    CHECK(optimizeBox(page)->isChecked());
    CHECK_FALSE(dialog.hasChanges());

    pick(underline, QColor(QStringLiteral("#00a0ff")));
    CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(setting)});
    CHECK(stored(setting) == "#00a0ff");
    QCoreApplication::processEvents();
    CHECK(issueColor(TextIssueKind::Grammar) == QColor(QStringLiteral("#00a0ff")));
    // The spelling keeps its color
    CHECK(issueColor(TextIssueKind::Spelling) ==
          themeColor(kalahari::editor::issueColorKey(TextIssueKind::Spelling)));

    namedChild<QPushButton>(page, QStringLiteral("Restore Default Colors"))->click();
    CHECK(underline->color() == themeColor(key));
    dialog.applyChanges();
    CHECK_FALSE(settings.hasKey(setting));
}

TEST_CASE("Settings: the colors of the annotations, on the Annotations page",
          "[gui][settings][colors]") {
    const ThemeColorsOnly themeColors;
    auto& settings = SettingsManager::getInstance();
    auto& themes = ThemeManager::getInstance();

    SettingsDialog dialog(nullptr);
    QWidget* page = openPage(dialog, QString(), QStringLiteral("Annotations"));
    const std::pair<const char*, std::string> colors[] = {
        {"Comment", kalahari::editor::annotationColorKey(AnnotationKind::Comment)},
        {"To-do", kalahari::editor::annotationColorKey(AnnotationKind::Todo)},
        {"Note", kalahari::editor::annotationColorKey(AnnotationKind::Note)},
    };
    for (const auto& [label, key] : colors) {
        INFO(label);
        REQUIRE(themeColor(key).isValid());
        CHECK(colorNamed(page, QString::fromLatin1(label))->color() == themeColor(key));
    }
    CHECK(colorsOfGroup(page, QStringLiteral("Colors")).size() == 3);
    CHECK(optimizeBox(page)->isChecked());
    CHECK_FALSE(dialog.hasChanges());

    const std::string todoKey = kalahari::editor::annotationColorKey(AnnotationKind::Todo);
    const std::string todo = settingOf(todoKey);
    pick(colorNamed(page, QStringLiteral("To-do")), QColor(QStringLiteral("#ff8800")));
    CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(todo)});
    CHECK(stored(todo) == "#ff8800");
    QCoreApplication::processEvents();
    CHECK(themes.editorColor(todoKey, QColor()) == QColor(QStringLiteral("#ff8800")));

    namedChild<QPushButton>(page, QStringLiteral("Restore Default Colors"))->click();
    CHECK(colorNamed(page, QStringLiteral("To-do"))->color() == themeColor(todoKey));
    CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(todo)});
    CHECK_FALSE(settings.hasKey(todo));
}

TEST_CASE("Settings: each theme has its own colors", "[gui][settings][colors]") {
    const ThemeColorsOnly themeColors;
    auto& settings = SettingsManager::getInstance();
    auto& themes = ThemeManager::getInstance();
    const std::string key = kalahari::editor::issueColorKey(TextIssueKind::Spelling);
    const std::string first = themes.getCurrentTheme().name;
    const QString other = QString::fromStdString(first == "Dark" ? "Light" : "Dark");
    const std::string firstSetting = ThemeManager::editorColorSetting(first, key);
    const std::string otherSetting = ThemeManager::editorColorSetting(other.toStdString(), key);

    SettingsDialog dialog(nullptr);
    QWidget* page = openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Spelling"));
    ColorConfigWidget* underline = colorNamed(page, QStringLiteral("Underline"));
    pick(underline, QColor(QStringLiteral("#123456")));
    dialog.applyChanges();
    CHECK(stored(firstSetting) == "#123456");

    SECTION("another theme in use: the page shows its colors and keeps them for it") {
        REQUIRE(themes.reloadTheme(other));
        CHECK(themes.editorColor(key, QColor()) == themeColor(key));
        QCoreApplication::processEvents();
        CHECK(underline->color() == themeColor(key));
        CHECK_FALSE(dialog.hasChanges());

        pick(underline, QColor(QStringLiteral("#654321")));
        CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(otherSetting)});
        CHECK(stored(otherSetting) == "#654321");
        CHECK(stored(firstSetting) == "#123456");
    }

    SECTION("a color changed together with the theme: kept for the theme it was shown for") {
        // Apply changes the theme first (the Theme page), then writes the color
        pick(underline, QColor(QStringLiteral("#abcdef")));
        REQUIRE(themes.reloadTheme(other));
        CHECK(dialog.applyChanges() == QStringList{QString::fromStdString(firstSetting)});
        CHECK(stored(firstSetting) == "#abcdef");
        CHECK_FALSE(settings.hasKey(otherSetting));

        // Then the page shows the colors of the theme in use
        QCoreApplication::processEvents();
        CHECK(underline->color() == themeColor(key));
        CHECK_FALSE(dialog.hasChanges());
    }
}

TEST_CASE("Settings: Optimize colors in themes, one setting on three pages",
          "[gui][settings][colors]") {
    const ThemeColorsOnly themeColors;
    auto& settings = SettingsManager::getInstance();
    auto& themes = ThemeManager::getInstance();
    const std::string optimize = ThemeManager::OPTIMIZE_COLORS_SETTING;

    SettingsDialog dialog(nullptr);
    QCheckBox* spelling =
        optimizeBox(openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Spelling")));
    REQUIRE(spelling->isChecked());

    // Turned off on one page and not applied yet: off on the pages opened after it too
    spelling->setChecked(false);
    QCheckBox* grammar =
        optimizeBox(openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Grammar")));
    CHECK_FALSE(grammar->isChecked());
    QCheckBox* annotations = optimizeBox(openPage(dialog, QString(), QStringLiteral("Annotations")));
    CHECK_FALSE(annotations->isChecked());

    // On again on the last page: on everywhere; off again
    annotations->setChecked(true);
    CHECK(spelling->isChecked());
    CHECK(grammar->isChecked());
    CHECK_FALSE(dialog.hasChanges());
    grammar->setChecked(false);
    CHECK_FALSE(spelling->isChecked());
    CHECK_FALSE(annotations->isChecked());

    // Applied: the editors and the panels show the colors exactly as set
    int themeChanges = 0;
    QObject guard;
    QObject::connect(&themes, &ThemeManager::themeChanged, &guard,
                     [&themeChanges]() { ++themeChanges; });
    const QStringList written = dialog.applyChanges();
    CHECK(written.contains(QString::fromStdString(optimize)));
    CHECK(settings.hasKey(optimize));
    CHECK_FALSE(settings.get<bool>(optimize, true));
    CHECK_FALSE(themes.optimizesEditorColors());
    QCoreApplication::processEvents();
    CHECK(themeChanges == 1);
    CHECK_FALSE(dialog.hasChanges());

    // A color too faint for the paper: shown as it is, or optimized
    const std::string key = kalahari::editor::issueColorKey(TextIssueKind::Spelling);
    settings.set<std::string>(settingOf(key), "#ffe8e8");
    QCoreApplication::processEvents();
    CHECK(issueColorOn(TextIssueKind::Spelling, Qt::white) == QColor(QStringLiteral("#ffe8e8")));
    settings.set<bool>(optimize, true);
    QCoreApplication::processEvents();
    const QColor shown = issueColorOn(TextIssueKind::Spelling, Qt::white);
    CHECK(shown != QColor(QStringLiteral("#ffe8e8")));
    CHECK(contrastRatio(shown, Qt::white) >= MIN_MARK_CONTRAST);
    // Seen well on the dark paper already: as it is
    CHECK(issueColorOn(TextIssueKind::Spelling, QColor(35, 35, 40)) ==
          QColor(QStringLiteral("#ffe8e8")));
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
