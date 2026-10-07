/// @file test_settings_dialog.cpp
/// @brief The settings dialog: page layout and what Apply writes

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/utils/layout_utils.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>

using namespace kalahari::gui;

TEST_CASE("Settings dialog: no page squeezes its groups", "[gui][settings]") {
    // Regression: the dialog kept its size whatever a page needed, so the groups of
    // Editor > Pages and Margins were squeezed until their fields overlapped, and the
    // description of Editor > General > Typewriter Scrolling was cut off
    SettingsDialog dialog(nullptr, SettingsData{});
    dialog.resize(dialog.minimumSize());
    dialog.show();
    QApplication::processEvents();

    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    REQUIRE(stack->count() > 0);

    for (int index = 0; index < stack->count(); ++index) {
        stack->setCurrentIndex(index);
        dialog.layout()->activate();
        QApplication::processEvents();

        QWidget* page = stack->widget(index);
        if (auto* scrollArea = qobject_cast<QScrollArea*>(page)) {
            page = scrollArea->widget();
        }
        REQUIRE(page != nullptr);
        REQUIRE(page->layout() != nullptr);

        // The page is as tall as its contents need at its width (a word-wrapped label
        // needs more height in a narrower page)
        const int needed = page->hasHeightForWidth()
            ? page->heightForWidth(page->width())
            : page->minimumSizeHint().height();
        INFO("page " << index << ": " << page->height() << " px high, needs " << needed);
        CHECK(page->height() >= needed);
        CHECK(page->width() >= page->minimumSizeHint().width());
    }
}

TEST_CASE("Settings dialog: Apply writes only the changed options", "[gui][settings]") {
    // Regression: Apply wrote every option, including all theme colors, so the theme's
    // own colors were frozen into the settings file and later theme fixes never showed
    auto& settings = kalahari::core::SettingsManager::getInstance();

    // What the dialog shows for the current settings, as the coordinator passes it
    SettingsData current;
    {
        SettingsDialog probe(nullptr, SettingsData{});
        current = probe.collectSettings();
    }
    const std::string theme = current.theme.toStdString();
    settings.clearCustomPaletteColorsForTheme(theme);
    settings.clearCustomLogColorsForTheme(theme);
    settings.clearCustomUiColorsForTheme(theme);
    settings.clearCustomIconColorsForTheme(theme);
    settings.setLanguage("en");

    SettingsDialog dialog(nullptr, current);
    SettingsData applied;
    SettingsData previous;
    int appliedCount = 0;
    QObject::connect(&dialog, &SettingsDialog::settingsApplied,
                     [&](const SettingsData& now, const SettingsData& before) {
                         applied = now;
                         previous = before;
                         ++appliedCount;
                     });

    // Change only the language
    QComboBox* language = nullptr;
    for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
        if (combo->findData("pl") >= 0) {
            language = combo;
        }
    }
    REQUIRE(language != nullptr);
    language->setCurrentIndex(language->findData("pl"));

    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    REQUIRE(buttons != nullptr);
    QPushButton* apply = buttons->button(QDialogButtonBox::Apply);
    REQUIRE(apply != nullptr);
    apply->click();

    CHECK(appliedCount == 1);
    CHECK(applied.language == "pl");
    CHECK(previous.language == "en");
    CHECK(settings.getLanguage() == "pl");
    CHECK_FALSE(settings.hasCustomPaletteColorsForTheme(theme));
    CHECK_FALSE(settings.hasCustomLogColorsForTheme(theme));
    CHECK_FALSE(settings.hasCustomUiColorsForTheme(theme));
    CHECK_FALSE(settings.hasCustomIconColorsForTheme(theme));

    // Nothing changed since the last Apply: nothing is applied again
    apply->click();
    CHECK(appliedCount == 1);

    settings.setLanguage("en");
}

TEST_CASE("Settings dialog: a missing editor font keeps its name", "[gui][settings]") {
    // Regression: a font missing on this system was shown as its substitute, and the
    // substitute's name was saved on the next Apply as if the user had chosen it
    SettingsData current;
    {
        SettingsDialog probe(nullptr, SettingsData{});
        current = probe.collectSettings();
    }
    current.editorFontFamily = QStringLiteral("Kalahari Missing Font");

    SettingsDialog dialog(nullptr, current);
    CHECK(dialog.collectSettings().editorFontFamily == QStringLiteral("Kalahari Missing Font"));
}

TEST_CASE("clearLayout hides the widgets it removes", "[gui][settings]") {
    // Regression: the Settings icon preview is rebuilt with clearLayout while the modal
    // dialog runs, where deleteLater waits until the dialog closes; the removed icons
    // stayed visible and were drawn over the preview's top-left corner
    QWidget preview;
    auto* layout = new QHBoxLayout(&preview);
    auto* icon = new QLabel(QStringLiteral("icon"));
    layout->addWidget(icon);
    preview.show();
    QApplication::processEvents();
    REQUIRE(icon->isVisible());

    kalahari::gui::utils::clearLayout(layout);

    CHECK(layout->count() == 0);
    CHECK(icon->isHidden());
}
