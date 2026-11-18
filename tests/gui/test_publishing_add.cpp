// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_publishing_add.cpp
/// @brief Add Element of a text for publishers: its part and its template, the description and
///        the tagline that go to the cover, and Add of the list of the texts

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/add_target.h"
#include "kalahari/gui/dialogs/new_file_dialog.h"
#include "kalahari/gui/dialogs/publishing_choice.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/panels/material_list_panel.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QTimer>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::PublishingTexts;
using gui::dialogs::NewFileDialog;
using gui::dialogs::PublishingChoice;

namespace {

/// The settings of the window as they were before a test, back after it
class KeptSettings {
public:
    KeptSettings() {
        for (const char* key : KEYS) {
            m_values.push_back(settings().get<nlohmann::json>(key, nlohmann::json()));
        }
        settings().set<std::string>(PublishingChoice::PART_SETTING, PublishingTexts::PITCH);
        settings().set<nlohmann::json>(PublishingChoice::TEMPLATE_SETTING,
                                       nlohmann::json::object());
        settings().set<std::string>(core::CoverMaterial::UNIT_SETTING, "characters");
    }
    ~KeptSettings() {
        try {
            for (std::size_t i = 0; i < KEYS.size(); ++i) {
                settings().set<nlohmann::json>(KEYS.at(i), m_values.at(i));
            }
        } catch (...) {
            (void)std::fputs("KeptSettings: the settings of the test stay\n", stderr);
        }
    }
    KeptSettings(const KeptSettings&) = delete;
    KeptSettings& operator=(const KeptSettings&) = delete;
    KeptSettings(KeptSettings&&) = delete;
    KeptSettings& operator=(KeptSettings&&) = delete;

private:
    static constexpr std::array<const char*, 5> KEYS{
        PublishingChoice::PART_SETTING, PublishingChoice::TEMPLATE_SETTING,
        core::CoverMaterial::UNIT_SETTING, PublishingTexts::TARGETS_SETTING,
        gui::MaterialList::LISTS_SETTING};
    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }
    std::vector<nlohmann::json> m_values;
};

/// Add Element on the Workshop of the open book, without a choice at its top
NewFileDialog::Book workshop(const QString& part = QString(), bool kind = false) {
    gui::AddTarget target;
    target.scope = gui::AddTarget::Scope::Workshop;
    target.publishingPart = part;
    if (kind) {
        target.kind = test::book().publishingKind();
    }
    return NewFileDialog::forAdding(test::book(), target, gui::AddChooser::None);
}

/// A click on the tile named @p name
void choose(const NewFileDialog& dialog, const QString& name) {
    for (int row = 0; row < dialog.kindList()->count(); ++row) {
        QListWidgetItem* item = dialog.kindList()->item(row);
        if (item->text() == name && item->flags().testFlag(Qt::ItemIsEnabled)) {
            dialog.kindList()->setCurrentItem(item);
            return;
        }
    }
    FAIL_CHECK("No tile " << name.toStdString());
}

/// The items of @p box, between " | "
std::string itemsOf(const QComboBox& box) {
    QStringList items;
    for (int item = 0; item < box.count(); ++item) {
        items << box.itemText(item);
    }
    return items.join(QStringLiteral(" | ")).toStdString();
}

/// The texts of the buttons of the parts of @p choice, between " | "
std::string partsOf(const PublishingChoice& choice) {
    QStringList parts;
    for (const QAbstractButton* button : choice.partButtons()->buttons()) {
        parts << button->text();
    }
    return parts.join(QStringLiteral(" | ")).toStdString();
}

/// Run @p command and answer the window of Add Element it shows with @p answer; the window is
/// rejected after it when it is still open
/// @return Whether the window was shown
bool answering(const std::function<void()>& command,
               const std::function<void(NewFileDialog&)>& answer) {
    bool shown = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        auto* dialog = qobject_cast<NewFileDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        timer.stop();
        shown = true;
        answer(*dialog);
        if (dialog->isVisible()) {
            dialog->reject();
        }
    });
    timer.start();
    command();
    return shown;
}

}  // namespace

TEST_CASE("Add Element: a text for publishers of its kind and template, or a text of the Cover",
          "[gui][newfile][addelement][publishing]") {
    gui::registerAllIcons();
    const test::OpenNovel book(QStringLiteral("Sand"));
    const KeptSettings kept;
    const core::BookTypeRegistry& types = book.pm.bookTypes();

    // Other kinds do not have it
    NewFileDialog dialog(workshop(), types);
    PublishingChoice* choice = dialog.publishingChoice();
    REQUIRE(choice != nullptr);
    CHECK_FALSE(choice->isVisibleTo(&dialog));
    CHECK(dialog.publishingPart().isEmpty());

    // A sentence about the book with the first template of the book's type
    choose(dialog, QStringLiteral("Text for publishers"));
    CHECK(choice->isVisibleTo(&dialog));
    CHECK(partsOf(*choice) == "Sentence about the book | Synopsis | Cover description – to the "
                              "Cover | Tagline – to the Cover");
    CHECK(choice->partButtons()->exclusive());
    REQUIRE(dialog.kind());
    CHECK(dialog.kind().kind->id == QStringLiteral("publishing"));
    CHECK(dialog.publishingPart() == QStringLiteral("pitch"));
    CHECK(dialog.coverPart().isEmpty());
    CHECK(itemsOf(*choice->templateBox()) == "When… must… before… | Hero, goal, obstacle | "
                                             "Comparison with another book | No template");
    CHECK(dialog.publishingTemplate() == QStringLiteral("kalahari.novel:when_must"));
    CHECK(choice->note()->text().startsWith(QStringLiteral("Pattern: When [")));
    CHECK(choice->note()->text().endsWith(QStringLiteral(" · target: 200 ch.")));
    CHECK(dialog.titleEdit()->text() == QStringLiteral("Sentence about the book – version 1"));
    // The window fits 1366x768 at 150%, 911 pixels wide and 460 high: the parts go in lines,
    // and the content scrolls
    dialog.show();
    QCoreApplication::processEvents();
    INFO("at least " << dialog.minimumSizeHint().width() << "x"
                     << dialog.minimumSizeHint().height());
    CHECK(dialog.minimumSizeHint().width() <= 911);
    CHECK(dialog.minimumSizeHint().height() <= 460);

    // A synopsis: its templates, the points and the target of each
    choice->setPart(QStringLiteral("synopsis"));
    CHECK(dialog.publishingPart() == QStringLiteral("synopsis"));
    CHECK(dialog.titleEdit()->text() == QStringLiteral("Synopsis – version 1"));
    CHECK(itemsOf(*choice->templateBox()) ==
          "One page – five steps | Full synopsis – up to three pages | Chapter by chapter | "
          "No template");
    CHECK(choice->note()->text().startsWith(QStringLiteral("Points: ")));
    // The number as the locale of the test writes it, with or without a group separator
    const QString chars1800 = QLocale().toString(1800) + QStringLiteral(" ch.");
    CHECK(choice->note()->text().endsWith(QStringLiteral(" · target: ") + chars1800));
    choice->setTemplate(QStringLiteral("kalahari.novel:each_chapter"));
    CHECK(choice->note()->text() ==
          QStringLiteral("A point for each text of the book · target: 600 ch. for each text"));
    choice->setTemplate(QString());
    CHECK(dialog.publishingTemplate().isEmpty());
    CHECK(choice->note()->text() ==
          QStringLiteral("Only the text; the target length from the Settings: ") + chars1800);

    // A description goes to the cover: a material of the cover, without a template
    choice->setPart(QStringLiteral("blurb"));
    REQUIRE(dialog.kind());
    CHECK(dialog.kind().kind->id == QStringLiteral("cover"));
    CHECK(dialog.coverPart() == QStringLiteral("blurb"));
    CHECK(dialog.publishingPart().isEmpty());
    CHECK(dialog.publishingTemplate().isEmpty());
    CHECK_FALSE(choice->templateBox()->isVisibleTo(&dialog));
    CHECK(dialog.titleEdit()->text() == QStringLiteral("Description 1"));
    CHECK(choice->note()->text().startsWith(
        QStringLiteral("Cover description goes to the Cover as a material of the cover")));

    // A click on a part chooses it, as a radio button; a title typed stays
    QAbstractButton* synopsis = choice->partButtons()->button(1);
    REQUIRE(synopsis != nullptr);
    synopsis->click();
    CHECK(dialog.publishingPart() == QStringLiteral("synopsis"));
    choice->setTemplate(QStringLiteral("kalahari.novel:each_chapter"));
    dialog.titleEdit()->setText(QStringLiteral("For the contest"));
    emit dialog.titleEdit()->textEdited(QStringLiteral("For the contest"));
    choice->setPart(QStringLiteral("pitch"));
    CHECK(dialog.titleEdit()->text() == QStringLiteral("For the contest"));
    choice->setPart(QStringLiteral("synopsis"));

    // Add remembers the part and its template
    dialog.accept();
    auto& settings = core::SettingsManager::getInstance();
    CHECK(settings.get<std::string>(PublishingChoice::PART_SETTING, "") == "synopsis");
    CHECK(settings.get<nlohmann::json>(PublishingChoice::TEMPLATE_SETTING, nlohmann::json()) ==
          nlohmann::json{{"synopsis", "kalahari.novel:each_chapter"}});
    const NewFileDialog last(workshop(QString(), true), types);
    CHECK(last.publishingPart() == QStringLiteral("synopsis"));
    CHECK(last.publishingTemplate() == QStringLiteral("kalahari.novel:each_chapter"));
    // The part asked for, with the template it had
    const NewFileDialog asked(workshop(QStringLiteral("pitch"), true), types);
    CHECK(asked.publishingPart() == QStringLiteral("pitch"));
    CHECK(asked.publishingTemplate() == QStringLiteral("kalahari.novel:when_must"));
}

TEST_CASE("Add Element: Add of the list of the texts for publishers adds a text or a material "
          "of the cover",
          "[gui][newfile][addelement][publishing]") {
    gui::registerAllIcons();
    test::OpenNovel book(QStringLiteral("Sand"));
    const KeptSettings kept;
    auto& pm = book.pm;
    test::Window window;
    gui::MaterialListCoordinator lists(pm, window.tabs, &window.navigator, &window.statusBar,
                                       [](bool) {});
    gui::MaterialListPanel* panel =
        lists.openList(QStringLiteral("kalahari.base:publishing"), false);
    REQUIRE(panel != nullptr);

    // A synopsis with the template chosen, a text of the Workshop
    CHECK(answering(
        [panel]() { emit panel->requestAddCover(panel->list(), QStringLiteral("synopsis")); },
        [](NewFileDialog& dialog) {
            CHECK(dialog.publishingPart() == QStringLiteral("synopsis"));
            dialog.publishingChoice()->setTemplate(QStringLiteral("kalahari.novel:one_page"));
            dialog.accept();
        }));
    const QList<const core::ProjectElement*> synopses =
        PublishingTexts::textsOf(*pm.project(), QStringLiteral("synopsis"));
    REQUIRE(synopses.size() == 1);
    CHECK(synopses.first()->title == QStringLiteral("Synopsis – version 1"));
    CHECK(synopses.first()->publishing.templateId == QStringLiteral("kalahari.novel:one_page"));
    CHECK(panel->currentMaterial() == synopses.first()->id);

    // A tagline: a material of the cover at its place
    CHECK(answering(
        [panel]() { emit panel->requestAddCover(panel->list(), QStringLiteral("tagline")); },
        [](NewFileDialog& dialog) {
            CHECK(dialog.coverPart() == QStringLiteral("tagline"));
            dialog.accept();
        }));
    const QList<const core::ProjectElement*> taglines =
        PublishingTexts::versionsOf(*pm.project(), QStringLiteral("tagline"));
    REQUIRE(taglines.size() == 1);
    CHECK(taglines.first()->title == QStringLiteral("Tagline 1"));
    CHECK(taglines.first()->cover.place == core::CoverMaterial::defaultPlaceOf(
                                               QStringLiteral("tagline")));
}
