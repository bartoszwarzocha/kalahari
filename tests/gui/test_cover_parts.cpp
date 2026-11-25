/// @file test_cover_parts.cpp
/// @brief The parts of the cover on its plan: the flaps, the jacket and the endpapers – their
/// switches, sheets and places, the fills of the endpapers, their Properties, the card of a
/// material with the places in groups, the note about the author, and the access keys

#include "kalahari_tests/editor/editor_test_utils.h"
#include "kalahari_tests/gui/document_window.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/diagnostic_menus.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/widgets/cover_plan.h"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabBar>
#include <QToolButton>
#include <QTranslator>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <string>
#include "kalahari_tests/test_support/window_parts.h"

using namespace kalahari;
using core::CoverMaterial;
using core::CoverParts;
using core::Endpaper;
using gui::CoverNames;
using gui::CoverPlan;
using gui::MaterialList;
using gui::MaterialListPanel;

namespace {

/// The list of the cover: "kalahari.base:cover"
const QString COVER_LIST = QStringLiteral("kalahari.base:cover");

const QString FRONT = QString::fromLatin1(CoverPlan::FRONT_ENDPAPER);
const QString BACK = QString::fromLatin1(CoverPlan::BACK_ENDPAPER);

/// A material of the cover of the open book: @p part at @p place, with @p text
QString addCover(const QString& title, const char* part, const char* place,
                 const QString& text = QString(), const QString& file = QString()) {
    auto& pm = test::book();
    core::NewMaterial material;
    material.title = title;
    material.filePath = file;
    material.cover.part = QString::fromLatin1(part);
    material.cover.place = QString::fromLatin1(place);
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("cover"));
    const QString id = pm.addMaterial(material, kind);
    REQUIRE_FALSE(id.isEmpty());
    if (!text.isEmpty()) {
        REQUIRE(pm.setDescription(id, text));
    }
    return id;
}

/// The lists the writer hid, none during a test, back after it
class KeptHidden {
public:
    KeptHidden() : m_hidden(settings().get<nlohmann::json>(MaterialList::HIDDEN_SETTING,
                                                            nlohmann::json())) {
        settings().set<nlohmann::json>(MaterialList::HIDDEN_SETTING, nlohmann::json::array());
    }
    ~KeptHidden() {
        try {
            settings().set<nlohmann::json>(MaterialList::HIDDEN_SETTING, m_hidden);
        } catch (...) {
            std::fputs("KeptHidden: the hidden lists of the test stay\n", stderr);
        }
    }
    KeptHidden(const KeptHidden&) = delete;
    KeptHidden& operator=(const KeptHidden&) = delete;

private:
    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }
    nlohmann::json m_hidden;
};

/// What the queued calls (a setting changed, the lists filled again) have done
void settle() {
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents();
    }
}

/// The texts of the tabs of the sheets of @p plan
QStringList tabsOf(const CoverPlan& plan) {
    QStringList texts;
    for (int i = 0; i < plan.sheetTabs()->count(); ++i) {
        texts << plan.sheetTabs()->tabText(i);
    }
    return texts;
}

/// The remark on the card of @p id under the plan ("(flaps turned off)"); none: empty
QString remarkOf(const CoverPlan& plan, const QString& id) {
    for (const gui::CoverPlanCard* card : plan.findChildren<gui::CoverPlanCard*>()) {
        if (card->id() == id) {
            const auto* remark = card->findChild<QLabel*>(QStringLiteral("coverPlanRemark"));
            return remark != nullptr ? remark->text() : QString();
        }
    }
    return QString();
}

/// The place of @p id in the list of the cover, as its column reads
QString listPlaceOf(const QString& id) {
    for (const gui::MaterialRow& row : MaterialList::rowsOf(test::book(), COVER_LIST)) {
        if (row.id == id) {
            return row.texts.value(QString::fromLatin1(MaterialList::COVER_PLACE));
        }
    }
    return QString();
}

/// Choose the item of @p box with @p data, as a click does
void chooseData(QComboBox* box, const QString& data) {
    const int index = box->findData(data);
    REQUIRE(index >= 0);
    box->setCurrentIndex(index);
    emit box->activated(index);
}

/// The tests of the keys need an active window; the offscreen platform has one without a
/// window on screen
bool canActivateWindows() {
    return QGuiApplication::platformName() == QStringLiteral("offscreen");
}

constexpr const char* NEEDS_OFFSCREEN =
    "needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen";

void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent pressed(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &released);
}

/// The letter of the access key of @p text ("&Jacket": "j"); none: an empty string
QString accessKey(const QString& text) {
    return QKeySequence::mnemonic(text).toString().right(1).toLower();
}

/// The access keys of the menus of the window, as they read now
QStringList menuKeys() {
    QStringList keys;
    for (const char* menu : {"FILE", "EDIT", "BOOK", "INSERT", "FORMAT", "TABLE", "IMAGE", "TOOLS",
                             "ASSISTANT", "VIEW", "WINDOW", "HELP"}) {
        keys << accessKey(gui::MenuBuilder::menuText(menu));
    }
    keys << accessKey(gui::diagnosticsMenuTitle()) << accessKey(gui::devToolsMenuTitle());
    return keys;
}

/// The switches of the parts above the plan have keys of their own, none of a menu: a key of
/// both would only move the focus to the switch
void checkPartKeys() {
    CoverPlan plan;
    const QStringList menus = menuKeys();
    QStringList keys;
    for (const QCheckBox* box : {plan.flapsBox(), plan.jacketBox(), plan.endpapersBox()}) {
        INFO(box->text().toStdString());
        const QString key = accessKey(box->text());
        CHECK_FALSE(key.isEmpty());
        CHECK_FALSE(menus.contains(key));
        CHECK_FALSE(keys.contains(key));
        keys << key;
    }
}

/// A theme file of the program
nlohmann::json themeFile(const char* name) {
    QFile file(QStringLiteral(KALAHARI_SOURCE_DIR "/resources/themes/") +
               QString::fromLatin1(name) + QStringLiteral(".json"));
    REQUIRE(file.open(QIODevice::ReadOnly));
    return nlohmann::json::parse(file.readAll().toStdString());
}

}  // namespace

TEST_CASE("Cover parts: the flaps and the jacket above the plan, their sheets and places",
          "[gui][cover][coverParts]") {
    gui::registerAllIcons();
    KeptHidden kept;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString blurb = addCover(QStringLiteral("Description 1"), CoverMaterial::BLURB,
                                   CoverMaterial::BACK, QStringLiteral("Three days of road."));
    const QString flap = addCover(QStringLiteral("Opinion 1"), CoverMaterial::OPINION,
                                  CoverMaterial::FRONT_FLAP, QStringLiteral("Dense prose."));
    const QString onJacket = addCover(QStringLiteral("Tagline 1"), CoverMaterial::TAGLINE,
                                      CoverMaterial::JACKET_FRONT, QStringLiteral("Sand."));

    // A book without its parts written has the flaps of the cover, no jacket, no endpapers
    CHECK(pm.coverParts().flaps);
    CHECK_FALSE(pm.coverParts().jacket);
    CHECK_FALSE(pm.coverParts().endpapers);

    test::Window window;
    gui::MaterialListCoordinator lists(kalahari::test::book(), window.tabs, &window.navigator,
                                       &window.statusBar, [](bool) {});
    MaterialListPanel* panel = lists.openList(COVER_LIST, false);
    REQUIRE(panel != nullptr);
    panel->showPlan(true);
    CoverPlan* plan = panel->plan();
    REQUIRE(plan != nullptr);
    CHECK(plan->flapsBox()->isChecked());
    CHECK_FALSE(plan->jacketBox()->isChecked());
    CHECK_FALSE(plan->endpapersBox()->isChecked());
    CHECK(plan->sheets() == QStringList{QStringLiteral("cover")});
    CHECK(plan->sheet() == QStringLiteral("cover"));
    CHECK(tabsOf(*plan) == QStringList{QStringLiteral("Cover · 2")});
    CHECK(plan->columnOf(blurb) == QStringLiteral("back"));
    CHECK(plan->columnOf(flap) == QStringLiteral("frontFlap"));
    // On the jacket, which the book has not: it waits under the plan and says why
    CHECK(plan->hasCard(onJacket));
    CHECK(plan->columnOf(onJacket).isEmpty());
    CHECK(remarkOf(*plan, onJacket) == QStringLiteral("(jacket turned off)"));
    CHECK(listPlaceOf(onJacket) == QStringLiteral("Jacket – front (jacket turned off)"));

    // The flaps off: saved with the book, the material of a flap waits under the plan
    plan->flapsBox()->click();
    CHECK_FALSE(pm.coverParts().flaps);
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The part is off; its materials wait under the plan."));
    settle();
    CHECK(plan->hasCard(flap));
    CHECK(plan->columnOf(flap).isEmpty());
    CHECK(remarkOf(*plan, flap) == QStringLiteral("(flaps turned off)"));
    CHECK(listPlaceOf(flap) == QStringLiteral("Cover – front flap (flaps turned off)"));
    CHECK(tabsOf(*plan) == QStringList{QStringLiteral("Cover · 1")});
    // Back on: in its place again
    plan->flapsBox()->click();
    CHECK(pm.coverParts().flaps);
    settle();
    CHECK(plan->columnOf(flap) == QStringLiteral("frontFlap"));
    CHECK(listPlaceOf(flap) == QStringLiteral("Cover – front flap"));

    // The jacket on: its sheet is shown, with its flaps, and a place for the author bio
    plan->jacketBox()->click();
    CHECK(pm.coverParts().jacket);
    settle();
    CHECK(plan->sheets() == QStringList{QStringLiteral("cover"), QStringLiteral("jacket")});
    CHECK(plan->sheet() == QStringLiteral("jacket"));
    CHECK(tabsOf(*plan) ==
          QStringList{QStringLiteral("Cover · 2"), QStringLiteral("Jacket · 1")});
    CHECK(plan->columnOf(onJacket) == QStringLiteral("jacketFront"));
    CHECK(plan->findChild<QWidget*>(QStringLiteral("coverPlanColumn_jacketFrontFlap")) !=
          nullptr);
    CHECK(plan->findChild<QLabel*>(QStringLiteral("coverPlanBioPlace")) != nullptr);
    CHECK(listPlaceOf(onJacket) == QStringLiteral("Jacket – front"));
    // A bio on the back flap of the jacket takes the place waiting for it
    const QString bio = addCover(QStringLiteral("Bio 1"), CoverMaterial::BIO,
                                 CoverMaterial::JACKET_BACK_FLAP,
                                 QStringLiteral("She lives by the sea."));
    lists.refresh();  // as the Navigator asks for a material added
    settle();
    CHECK(plan->columnOf(bio) == QStringLiteral("jacketBackFlap"));
    CHECK(plan->findChild<QLabel*>(QStringLiteral("coverPlanBioPlace")) == nullptr);

    // The tabs show the sheets; Ctrl+Page Up and Ctrl+Page Down go between them
    plan->sheetTabs()->setCurrentIndex(0);
    CHECK(plan->sheet() == QStringLiteral("cover"));
    CHECK(plan->columnOf(onJacket).isEmpty());
    CHECK(plan->columnOf(blurb) == QStringLiteral("back"));
    if (canActivateWindows()) {
        window.window.show();
        window.window.activateWindow();
        REQUIRE(test::waitUntil(
            [&window] { return QApplication::activeWindow() == &window.window; }));
        plan->flapsBox()->setFocus(Qt::OtherFocusReason);
        REQUIRE(test::waitUntil(
            [plan] { return QApplication::focusWidget() == plan->flapsBox(); }));
        press(plan->flapsBox(), Qt::Key_PageDown, Qt::ControlModifier);
        CHECK(plan->sheet() == QStringLiteral("jacket"));
        press(QApplication::focusWidget(), Qt::Key_PageUp, Qt::ControlModifier);
        CHECK(plan->sheet() == QStringLiteral("cover"));
    }

    // A change of the parts elsewhere (another window of the book) shows on the plan
    CoverParts parts = pm.coverParts();
    parts.jacket = false;
    REQUIRE(pm.setCoverParts(parts));
    settle();
    CHECK_FALSE(plan->jacketBox()->isChecked());
    CHECK(plan->sheets() == QStringList{QStringLiteral("cover")});
    CHECK(remarkOf(*plan, bio) == QStringLiteral("(jacket turned off)"));
    panel->showPlan(false);
}

TEST_CASE("Cover parts: the endpapers, their fills on the plan and in the Properties",
          "[gui][cover][coverParts][endpapers]") {
    gui::registerAllIcons();
    KeptHidden kept;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    QImage image(60, 90, QImage::Format_RGB32);
    image.fill(Qt::darkYellow);
    const QString file = QDir(novel.dir.path()).filePath(QStringLiteral("map.png"));
    REQUIRE(image.save(file));
    const QString design = addCover(QStringLiteral("Map of the dunes"), CoverMaterial::DESIGN,
                                    CoverMaterial::WHOLE, QString(), file);

    test::Window window;
    gui::MaterialListCoordinator lists(kalahari::test::book(), window.tabs, &window.navigator,
                                       &window.statusBar, [](bool) {});
    MaterialListPanel* panel = lists.openList(COVER_LIST, false);
    REQUIRE(panel != nullptr);
    panel->showPlan(true);
    CoverPlan* plan = panel->plan();
    REQUIRE(plan != nullptr);
    CHECK_FALSE(plan->sheets().contains(QStringLiteral("endpapers")));

    // On: the sheet of the endpapers, the front one and the back one, neither filled
    plan->endpapersBox()->click();
    CHECK(pm.coverParts().endpapers);
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Endpapers on: the sheet \"Endpapers\""));
    settle();
    CHECK(plan->sheet() == QStringLiteral("endpapers"));
    CHECK(tabsOf(*plan).last() == QStringLiteral("Endpapers · 2"));
    REQUIRE(plan->spread(FRONT) != nullptr);
    REQUIRE(plan->spread(BACK) != nullptr);
    CHECK(plan->spread(FRONT)->endpaper().fill.isEmpty());
    CHECK(plan->fillBox(FRONT)->currentData().toString().isEmpty());

    // A pattern: the colors of the theme, kept with the book from then on
    chooseData(plan->fillBox(FRONT), QString::fromLatin1(Endpaper::PATTERN));
    Endpaper front = pm.coverParts().front;
    CHECK(front.fill == QStringLiteral("pattern"));
    CHECK(QColor(front.patternColor) == CoverPlan::patternColorOf(Endpaper{}));
    CHECK(QColor(front.paperColor) == CoverPlan::paperColorOf(Endpaper{}));
    REQUIRE(plan->patternButton(FRONT, QStringLiteral("stripes")) != nullptr);
    plan->patternButton(FRONT, QStringLiteral("stripes"))->click();
    CHECK(pm.coverParts().front.pattern == QStringLiteral("stripes"));
    CHECK(plan->patternButton(FRONT, QStringLiteral("stripes"))->isChecked());
    CHECK(plan->spread(FRONT)->endpaper().pattern == QStringLiteral("stripes"));

    // A picture: the first picture of the materials of the cover, its size when repeated
    chooseData(plan->fillBox(BACK), QString::fromLatin1(Endpaper::TILE));
    CHECK(pm.coverParts().back.fill == QStringLiteral("tile"));
    CHECK(pm.coverParts().back.picture == design);
    CHECK(plan->pictureBox(BACK)->currentData().toString() == design);
    plan->scaleBox(BACK)->setValue(40);
    CHECK(pm.coverParts().back.scale == 40);

    // The back one as the front one: shows the front's fill and keeps its own
    REQUIRE(plan->sameAsFrontBox() != nullptr);
    plan->sameAsFrontBox()->click();
    CHECK(pm.coverParts().back.sameAsFront);
    CHECK(pm.coverParts().back.fill == QStringLiteral("tile"));
    CHECK(plan->spread(BACK)->endpaper().fill == QStringLiteral("pattern"));

    // A click on a spread shows the endpaper in the Properties
    emit plan->spread(BACK)->clicked();
    CHECK(plan->currentEndpaper() == BACK);
    CHECK(window.properties->currentPage() == gui::PropertiesPanel::Page::Endpaper);
    const auto* title = window.properties->findChild<QLabel*>(
        QStringLiteral("propertiesEndpaperTitle"));
    const auto* fill = window.properties->findChild<QLabel*>(
        QStringLiteral("propertiesEndpaperFill"));
    REQUIRE(title != nullptr);
    REQUIRE(fill != nullptr);
    CHECK(title->text().contains(QStringLiteral("Back endpaper")));
    CHECK(fill->text() == QStringLiteral("pattern: stripes (same as the front one)"));
    // A change on the plan shows there at once
    plan->sameAsFrontBox()->click();
    CHECK(fill->text() == QStringLiteral("repeated picture: Map of the dunes, 40%"));

    // Off: the sheet goes, the fills stay with the book
    plan->endpapersBox()->click();
    CHECK_FALSE(pm.coverParts().endpapers);
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Endpapers off; their fills are kept."));
    settle();
    CHECK(plan->sheets() == QStringList{QStringLiteral("cover")});
    CHECK(pm.coverParts().front.pattern == QStringLiteral("stripes"));
    CHECK(pm.coverParts().back.picture == design);
    panel->showPlan(false);
}

TEST_CASE("Cover parts: the names of the fills and the places in groups",
          "[gui][cover][coverParts]") {
    Endpaper endpaper;
    CHECK(CoverNames::fillSummary(endpaper, QString()) == QStringLiteral("no fill"));
    endpaper.fill = QString::fromLatin1(Endpaper::PICTURE);
    CHECK(CoverNames::fillSummary(endpaper, QStringLiteral("Map")) ==
          QStringLiteral("picture: Map"));
    endpaper.fill = QString::fromLatin1(Endpaper::PATTERN);
    endpaper.pattern = QStringLiteral("checker");
    CHECK(CoverNames::fillSummary(endpaper, QString()) == QStringLiteral("pattern: checkerboard"));
    endpaper.fill = QString::fromLatin1(Endpaper::COLOR);
    CHECK(CoverNames::fillSummary(endpaper, QString()) == QStringLiteral("one color"));
    CHECK(CoverNames::placeGroupName(QStringLiteral("jacketSpine")) == QStringLiteral("Jacket"));
    CHECK(CoverNames::placeGroupName(QStringLiteral("spine")) == QStringLiteral("Cover"));
    CHECK(CoverNames::shortPlaceName(QStringLiteral("jacketWhole")) ==
          QStringLiteral("Whole jacket"));
    CoverParts parts;
    CHECK(CoverNames::partOffText(QStringLiteral("backFlap"), parts).isEmpty());
    parts.flaps = false;
    CHECK(CoverNames::partOffText(QStringLiteral("backFlap"), parts) ==
          QStringLiteral("flaps turned off"));
    CHECK(CoverNames::partOffText(QStringLiteral("jacketBack"), parts) ==
          QStringLiteral("jacket turned off"));
    CHECK(CoverNames::partName(QString::fromLatin1(CoverMaterial::BIO)) ==
          QStringLiteral("Author bio"));
    CHECK(CoverNames::partName(QString::fromLatin1(CoverMaterial::AUTHOR_PHOTO)) ==
          QStringLiteral("Author photo"));
}

TEST_CASE("Cover parts: the card of a material, its places in groups, the note about the author",
          "[gui][cover][coverParts][card]") {
    gui::registerAllIcons();
    KeptHidden kept;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString tagline = addCover(QStringLiteral("Tagline 1"), CoverMaterial::TAGLINE,
                                     CoverMaterial::FRONT, QStringLiteral("Sand."));
    const QString bio = addCover(QStringLiteral("Bio 1"), CoverMaterial::BIO, "");

    gui::MaterialPage card(kalahari::test::book(), nullptr, true);
    QStringList messages;
    QObject::connect(&card, &gui::MaterialPage::message,
                     [&messages](const QString& text) { messages << text; });
    card.showMaterial(tagline);

    // The places of the cover, then those of the jacket, each under the name of its part
    QComboBox* places = card.coverPlaceBox();
    const auto* model = qobject_cast<QStandardItemModel*>(places->model());
    REQUIRE(model != nullptr);
    CHECK(places->itemText(0) == QStringLiteral("Cover"));
    CHECK_FALSE(model->item(0)->isEnabled());
    CHECK(places->currentData().toString() == QStringLiteral("front"));
    CHECK(places->itemText(places->currentIndex()) == QStringLiteral("Cover – front"));
    const int jacket = places->findText(QStringLiteral("Jacket (turned off)"));
    REQUIRE(jacket > 0);
    CHECK_FALSE(model->item(jacket)->isEnabled());
    const int jacketFront = places->findData(QStringLiteral("jacketFront"));
    REQUIRE(jacketFront > jacket);
    CHECK(places->itemText(jacketFront) == QStringLiteral("Jacket – front"));
    CHECK_FALSE(model->item(jacketFront)->isEnabled());
    // The whole cover is for a design only
    CHECK(places->findData(QStringLiteral("whole")) < 0);
    // With the jacket on, its places can be chosen
    CoverParts parts = pm.coverParts();
    parts.jacket = true;
    REQUIRE(pm.setCoverParts(parts));
    card.showMaterial(tagline);
    CHECK(places->findText(QStringLiteral("Jacket")) > 0);
    CHECK(model->item(places->findData(QStringLiteral("jacketFront")))->isEnabled());

    // A bio takes the text of the note About the Author of the back part, when the book has one
    card.showMaterial(bio);
    REQUIRE(card.bioButton() != nullptr);
    CHECK(card.bioButton()->isVisibleTo(&card));
    CHECK_FALSE(card.bioButton()->isEnabled());
    const core::KindRef about =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("about_author"));
    const QString note = pm.addElement(about, QStringLiteral("About the Author"),
                                       core::BookPlace::Back);
    REQUIRE_FALSE(note.isEmpty());
    pm.setChapterContent(note, QStringLiteral("<kml><p>She lives by the sea.</p></kml>"));
    REQUIRE(pm.saveChapterContent(note));
    // The open card sees the note added in the Navigator
    CHECK(card.bioButton()->isEnabled());
    CHECK(card.bioButton()->toolTip().isEmpty());
    card.bioButton()->click();
    CHECK(pm.findElement(bio)->description == QStringLiteral("She lives by the sea."));
    REQUIRE_FALSE(messages.isEmpty());
    CHECK(messages.last() ==
          QStringLiteral("The text of \"About the Author\" of the back part was copied."));

    // Other kinds have no such button
    card.showMaterial(tagline);
    CHECK_FALSE(card.bioButton()->isVisibleTo(&card));
}

TEST_CASE("Cover parts: the switches above the plan have access keys of their own",
          "[gui][cover][coverParts][keys]") {
    SECTION("in English") { checkPartKeys(); }
    SECTION("in Polish, the translation the program ships") {
        QTranslator polish;
        REQUIRE(polish.load(QStringLiteral(":/i18n/kalahari_pl.qm")));
        QCoreApplication::installTranslator(&polish);
        CHECK(QCoreApplication::translate("kalahari::gui::CoverPlan", "&Jacket") !=
              QStringLiteral("&Jacket"));
        checkPartKeys();
        QCoreApplication::removeTranslator(&polish);
    }
}

TEST_CASE("Cover parts: both themes have the colors of the endpapers",
          "[gui][cover][coverParts][theme]") {
    for (const char* name : {"Light", "Dark"}) {
        INFO(name);
        const nlohmann::json theme = themeFile(name);
        REQUIRE(theme.contains("editor"));
        for (const char* key : {CoverPlan::PATTERN_COLOR_KEY, CoverPlan::PAPER_COLOR_KEY,
                                CoverPlan::BLANK_COLOR_KEY}) {
            INFO(key);
            REQUIRE(theme.at("editor").contains(key));
            CHECK(QColor(QString::fromStdString(theme.at("editor").at(key).get<std::string>()))
                      .isValid());
        }
    }
}
