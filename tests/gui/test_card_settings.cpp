// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_settings.cpp
/// @brief Settings > Cards: the full card in two columns or on tabs, the shape of the
/// portraits and what a portrait without a picture shows, the fields of new books, the
/// portraits of parts

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/changed_setting.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_data.h"
#include "kalahari/core/card_field_set.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/avatar.h"
#include "kalahari/gui/avatar_art.h"
#include "kalahari/gui/dialogs/card_fields_dialog.h"
#include "kalahari/gui/dialogs/portrait_palettes_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/panels/cursor_card_box.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/widgets/card_chapters_box.h"
#include "kalahari/gui/widgets/own_fields_box.h"
#include "kalahari/gui/widgets/palette_edit.h"
#include "kalahari/gui/widgets/relations_box.h"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <iterator>
#include <string>

namespace kalahari {

using test::ChangedSetting;

namespace {

/// A new card @p title of kind @p kindId of the Base in the Workshop
QString addCard(core::ProjectManager& pm, const char* kindId, const QString& title) {
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QLatin1String(kindId));
    REQUIRE(kind);
    const QString id = pm.addElement(kind, title, core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// Set setting @p key to @p value, and let the cards take it in
void setChoice(const char* key, const char* value) {
    core::SettingsManager::getInstance().set<std::string>(key, value);
    QCoreApplication::processEvents();
}

/// The portrait beside the name of the first card of @p list of book @p pm
QImage listPortrait(const core::ProjectManager& pm, const gui::MaterialListPanel& list) {
    const QList<gui::MaterialColumn> columns = gui::MaterialList::columnsOf(pm, list.list());
    const auto found =
        std::find_if(columns.cbegin(), columns.cend(), [](const gui::MaterialColumn& column) {
            return column.id == QStringLiteral("title");
        });
    // REQUIRE and not FAIL before a return, which a compiler finds unreachable
    REQUIRE(found != columns.cend());
    const QModelIndex name =
        list.rows()->index(0, static_cast<int>(std::distance(columns.cbegin(), found)));
    return name.data(Qt::DecorationRole).value<QPixmap>().toImage();
}

/// Whether @p widget is inside @p ancestor
/// The value of the item chosen in @p list
QString shownIn(const QComboBox* list) {
    return list->currentData().toString();
}

/// Choose the item of value @p value in @p list, as the writer does
void pick(QComboBox* list, const char* value) {
    const int index = list->findData(QString::fromLatin1(value));
    REQUIRE(index >= 0);
    list->setCurrentIndex(index);
}

bool isIn(const QWidget* widget, const QWidget* ancestor) {
    return ancestor != nullptr && ancestor->isAncestorOf(widget);
}

}  // namespace

TEST_CASE("Settings > Cards: the full card in two columns or on tabs", "[gui][cards][settings]") {
    gui::registerAllIcons();
    const ChangedSetting<std::string> layout(gui::CardPage::LAYOUT_SETTING,
                                             gui::CardPage::LAYOUT_COLUMNS);
    test::OpenNovel novel(QStringLiteral("Tabs"));
    auto& pm = novel.pm;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    REQUIRE(pm.setFields(anna, {core::ElementField{QStringLiteral("Eyes"),
                                                   QStringLiteral("green"), {}}}));

    gui::CardPage page(pm, true);
    page.showCard(anna);
    CHECK(page.tabs() == nullptr);
    CHECK_FALSE(page.descriptionGroup()->isHidden());

    // On tabs: General, Description, Relations, Chapters and Materials, under the header
    setChoice(gui::CardPage::LAYOUT_SETTING, gui::CardPage::LAYOUT_TABS);
    QTabWidget* tabs = page.tabs();
    REQUIRE(tabs != nullptr);
    REQUIRE(tabs->count() == 4);
    CHECK(tabs->tabText(0) == QStringLiteral("&General"));
    CHECK(tabs->tabText(1) == QStringLiteral("&Description"));
    CHECK(tabs->tabText(2) == QStringLiteral("&Relations"));
    CHECK(tabs->tabText(3) == QStringLiteral("Chapters and &Materials"));
    CHECK(isIn(page.ownFields(), tabs->widget(0)));
    CHECK(isIn(page.descriptionGroup(), tabs->widget(1)));
    CHECK(isIn(page.relationsBox(), tabs->widget(2)));
    CHECK(isIn(page.chaptersBox(), tabs->widget(3)));
    CHECK(isIn(page.materialsBox(), tabs->widget(3)));
    CHECK_FALSE(isIn(page.nameEdit(), tabs));
    // The card still shows its data
    CHECK(page.ownFields()->table()->rowCount() == 1);
    REQUIRE(pm.setFields(anna, {}));
    CHECK(page.ownFields()->table()->rowCount() == 0);

    // A new full card has them too; the short card has none
    const gui::CardPage other(pm, true);
    CHECK(other.tabs() != nullptr);
    const gui::CardPage shortCard(pm, false);
    CHECK(shortCard.tabs() == nullptr);

    // Back in two columns
    setChoice(gui::CardPage::LAYOUT_SETTING, gui::CardPage::LAYOUT_COLUMNS);
    CHECK(page.tabs() == nullptr);
    CHECK_FALSE(page.descriptionGroup()->isHidden());
    CHECK_FALSE(page.ownFields()->isHidden());
}

TEST_CASE("Settings > Cards: the shape of the portraits, and a portrait without a picture",
          "[gui][cards][settings]") {
    gui::registerAllIcons();
    const ChangedSetting<std::string> shape(gui::CardPage::SHAPE_SETTING,
                                            gui::CardPage::SHAPE_SQUARE);
    const ChangedSetting<std::string> without(gui::CardPage::NO_PICTURE_SETTING,
                                              gui::CardPage::NO_PICTURE_INITIALS);
    test::OpenNovel novel(QStringLiteral("Portraits"));
    auto& pm = novel.pm;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));
    core::CardData colored = pm.findElement(anna)->card;
    colored.color = QStringLiteral("#2e9e6a");
    REQUIRE(pm.setCard(anna, colored));
    const core::ProjectElement& card = *pm.findElement(anna);
    const QColor green(QStringLiteral("#2e9e6a"));

    // A square of the color of the card, with the initials
    const QImage initials = gui::CardPage::portraitOf(pm, card, 64, 1.0).toImage();
    CHECK(initials.pixelColor(0, 0) == green);
    CHECK(initials.pixelColor(63, 63) == green);

    // A circle: the corners are clear
    core::SettingsManager::getInstance().set<std::string>(gui::CardPage::SHAPE_SETTING,
                                                          gui::CardPage::SHAPE_CIRCLE);
    const QImage circle = gui::CardPage::portraitOf(pm, card, 64, 1.0).toImage();
    CHECK(circle.pixelColor(0, 0).alpha() == 0);
    CHECK(circle.pixelColor(63, 63).alpha() == 0);
    CHECK(circle.pixelColor(32, 4) == green);

    // The icon of the kind in place of the initials
    core::SettingsManager::getInstance().set<std::string>(gui::CardPage::SHAPE_SETTING,
                                                          gui::CardPage::SHAPE_SQUARE);
    core::SettingsManager::getInstance().set<std::string>(gui::CardPage::NO_PICTURE_SETTING,
                                                          gui::CardPage::NO_PICTURE_ICON);
    const QImage icon = gui::CardPage::portraitOf(pm, card, 64, 1.0).toImage();
    CHECK(icon.pixelColor(0, 0) == green);
    CHECK(icon != initials);

    // The card shows the change at once
    gui::CardPage page(pm, true);
    page.showCard(anna);
    const auto shownPortrait = [&page]() {
        return page.portraitButton()->icon().pixmap(gui::CardPage::FULL_PORTRAIT).toImage();
    };
    const QImage before = shownPortrait();
    setChoice(gui::CardPage::NO_PICTURE_SETTING, gui::CardPage::NO_PICTURE_INITIALS);
    const QImage after = shownPortrait();
    CHECK(before != after);
}

TEST_CASE("Settings > Cards: the layout, the portraits and the fields of new books on the page",
          "[gui][cards][settings]") {
    gui::registerAllIcons();
    using json = nlohmann::json;
    const ChangedSetting<std::string> layout(gui::CardPage::LAYOUT_SETTING,
                                             gui::CardPage::LAYOUT_COLUMNS);
    const ChangedSetting<std::string> shape(gui::CardPage::SHAPE_SETTING,
                                            gui::CardPage::SHAPE_SQUARE);
    const ChangedSetting<std::string> without(gui::CardPage::NO_PICTURE_SETTING,
                                              gui::CardPage::NO_PICTURE_INITIALS);
    const ChangedSetting<json> fields(core::card_fields::DEFAULTS_SETTING, json::object());
    const test::OpenNovel novel(QStringLiteral("Settings"));

    gui::CardsPage page;
    page.load();
    CHECK(shownIn(page.layoutList()) == QLatin1String(gui::CardPage::LAYOUT_COLUMNS));
    CHECK(shownIn(page.shapeList()) == QLatin1String(gui::CardPage::SHAPE_SQUARE));
    CHECK(shownIn(page.noPictureList()) == QLatin1String(gui::CardPage::NO_PICTURE_INITIALS));
    CHECK(page.layoutList()->count() == 2);
    CHECK(page.shapeList()->count() == 2);
    CHECK(page.noPictureList()->count() == 2);
    CHECK(page.defaultFieldsLabel()->text().startsWith(
        QStringLiteral("New books have the fields of the book types.")));
    CHECK_FALSE(page.isChanged());

    pick(page.layoutList(), gui::CardPage::LAYOUT_TABS);
    pick(page.shapeList(), gui::CardPage::SHAPE_CIRCLE);
    pick(page.noPictureList(), gui::CardPage::NO_PICTURE_ICON);
    CHECK(page.isChanged());
    page.apply();
    CHECK(gui::CardPage::hasTabs());
    CHECK(gui::CardPage::roundPortraits());
    CHECK(gui::CardPage::kindIconWithoutPicture());
    CHECK_FALSE(page.isChanged());

    // The fields of new books: in the window Card Fields, saved by Apply
    int answered = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&answered]() {
        auto* dialog = qobject_cast<gui::dialogs::CardFieldsDialog*>(test::shownDialog());
        if (dialog == nullptr || dialog->property("answered").toBool()) {
            return;
        }
        dialog->setProperty("answered", true);
        ++answered;
        QTableWidget* table = dialog->fieldTable();
        for (int row = gui::dialogs::CardFieldsDialog::FIXED_ROWS; row < table->rowCount();
             ++row) {
            if (table->item(row, 0)->data(Qt::UserRole).toString() == QStringLiteral("age")) {
                table->item(row, gui::dialogs::CardFieldsDialog::CARD_COLUMN)
                    ->setCheckState(Qt::Unchecked);
            }
        }
        QMetaObject::invokeMethod(dialog->acceptButton(), "click", Qt::QueuedConnection);
    });
    timer.start();
    page.defaultFieldsButton()->click();
    timer.stop();
    CHECK(answered == 1);
    CHECK(page.isChanged());
    CHECK(page.defaultFieldsLabel()->text().startsWith(
        QStringLiteral("Fields of their own in new books: Characters.")));
    CHECK(core::card_fields::defaults().isEmpty());
    page.apply();
    const core::CardFieldSets defaults = core::card_fields::defaults();
    CHECK(defaults.keys() == QStringList{QStringLiteral("kalahari.base:character")});
}

TEST_CASE("Settings > Cards: the portraits of the list of cards and of the card at the cursor",
          "[gui][cards][settings]") {
    gui::registerAllIcons();
    const ChangedSetting<std::string> shape(gui::CardPage::SHAPE_SETTING,
                                            gui::CardPage::SHAPE_SQUARE);
    const ChangedSetting<std::string> without(gui::CardPage::NO_PICTURE_SETTING,
                                              gui::CardPage::NO_PICTURE_INITIALS);
    test::OpenNovel novel(QStringLiteral("Lists"));
    auto& pm = novel.pm;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));

    const gui::MaterialListPanel list(pm, QStringLiteral("kalahari.base:character"));
    const gui::PropertiesPanel properties(pm);
    gui::CursorCardBox* atCursor = properties.cursorCard();
    atCursor->showCard(anna);
    const QImage listBefore = listPortrait(pm, list);
    const QImage cursorBefore = atCursor->portraitLabel()->pixmap().toImage();
    REQUIRE_FALSE(listBefore.isNull());
    REQUIRE_FALSE(cursorBefore.isNull());

    // The icon of the kind: both draw their portraits again
    setChoice(gui::CardPage::NO_PICTURE_SETTING, gui::CardPage::NO_PICTURE_ICON);
    const QImage listIcon = listPortrait(pm, list);
    const QImage cursorIcon = atCursor->portraitLabel()->pixmap().toImage();
    CHECK(listIcon != listBefore);
    CHECK(cursorIcon != cursorBefore);

    // A circle: the corners are clear
    setChoice(gui::CardPage::SHAPE_SETTING, gui::CardPage::SHAPE_CIRCLE);
    CHECK(listPortrait(pm, list).pixelColor(0, 0).alpha() == 0);
    CHECK(atCursor->portraitLabel()->pixmap().toImage().pixelColor(0, 0).alpha() == 0);
}

TEST_CASE("Settings > Cards: the style, the frame and the palettes of the portraits of parts",
          "[gui][cards][settings][avatar]") {
    gui::registerAllIcons();
    using json = nlohmann::json;
    using gui::Avatar;
    using gui::AvatarLook;
    using gui::AvatarPalettes;
    const ChangedSetting<std::string> style(Avatar::STYLE_SETTING, AvatarLook::FLAT);
    const ChangedSetting<std::string> frame(Avatar::FRAME_SETTING, Avatar::FRAME_BUST);
    const ChangedSetting<json> palettes(AvatarPalettes::SETTING,
                                        AvatarPalettes::defaults().toSetting());
    const test::OpenNovel novel(QStringLiteral("Portraits of parts"));

    gui::CardsPage page;
    page.load();
    CHECK(shownIn(page.portraitStyleList()) == QLatin1String(AvatarLook::FLAT));
    CHECK(shownIn(page.portraitFrameList()) == QLatin1String(Avatar::FRAME_BUST));
    CHECK(page.portraitStyleList()->count() == 2);
    CHECK(page.portraitFrameList()->count() == 2);
    CHECK_FALSE(page.isChanged());

    // The style of new portraits and the frame of the window Portrait
    pick(page.portraitStyleList(), AvatarLook::LINE);
    pick(page.portraitFrameList(), Avatar::FRAME_FULL);
    CHECK(page.isChanged());
    page.apply();
    CHECK(Avatar::newStyle() == QLatin1String(AvatarLook::LINE));
    CHECK(core::SettingsManager::getInstance().get<std::string>(Avatar::FRAME_SETTING, "") ==
          Avatar::FRAME_FULL);
    CHECK_FALSE(page.isChanged());

    // The palettes: in the window Portrait Palettes, saved by Apply; Restore Defaults there
    // gives back the colors of the program
    const QString hair = QStringLiteral("hair");
    const QString blue = QStringLiteral("#123456");
    int answered = 0;
    bool restored = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        auto* dialog = qobject_cast<gui::dialogs::PortraitPalettesDialog*>(test::shownDialog());
        if (dialog == nullptr || dialog->property("answered").toBool()) {
            return;
        }
        dialog->setProperty("answered", true);
        ++answered;
        CHECK(dialog->paletteEdit(QStringLiteral("other")) == nullptr);
        gui::PaletteEdit* edit = dialog->paletteEdit(hair);
        REQUIRE(edit != nullptr);
        if (answered == 1) {
            edit->setColors({blue});
        } else {
            CHECK(edit->colors() == QStringList{blue});
            dialog->restoreButton()->click();
            restored = edit->colors() == AvatarPalettes::defaults().colors(hair);
        }
        QMetaObject::invokeMethod(dialog->acceptButton(), "click", Qt::QueuedConnection);
    });
    timer.start();
    page.portraitPalettesButton()->click();
    CHECK(answered == 1);
    CHECK(page.isChanged());
    CHECK(AvatarPalettes::current().colors(hair) != QStringList{blue});
    page.apply();
    CHECK(AvatarPalettes::current().colors(hair) == QStringList{blue});
    CHECK(AvatarPalettes::current().colors(QStringLiteral("skin")) ==
          AvatarPalettes::defaults().colors(QStringLiteral("skin")));

    page.portraitPalettesButton()->click();
    timer.stop();
    CHECK(answered == 2);
    CHECK(restored);
    page.apply();
    CHECK(AvatarPalettes::current() == AvatarPalettes::defaults());

    // A setting without a palette, or with a wrong color: the colors of the program
    const AvatarPalettes partial =
        AvatarPalettes::fromSetting(json{{"hair", json::array({"wrong"})}, {"eyes", 3}});
    CHECK(partial == AvatarPalettes::defaults());
}

}  // namespace kalahari
