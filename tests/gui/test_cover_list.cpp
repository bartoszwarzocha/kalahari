/// @file test_cover_list.cpp
/// @brief The materials of the cover in the program: their names, the list grouped by kind with
/// its plan, their states, the kinds hidden in the Navigator, the card and the settings

#include "../editor/editor_test_utils.h"
#include "document_window.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/widgets/cover_plan.h"

#include <catch2/catch_test_macros.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QRadioButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <nlohmann/json.hpp>

#include <array>
#include <cstdio>
#include <string>
#include <vector>
#include "test_support/window_parts.h"

using namespace kalahari;
using core::CoverMaterial;
using gui::CoverNames;
using gui::MaterialList;
using gui::MaterialListPanel;

namespace {

/// The list of the cover: "kalahari.base:cover"
const QString COVER_LIST = QStringLiteral("kalahari.base:cover");

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

/// The settings of the materials as they were before a test, back after it
class KeptSettings {
public:
    KeptSettings() {
        for (const char* key : KEYS) {
            m_values.push_back(settings().get<nlohmann::json>(key, nlohmann::json()));
        }
        settings().set<nlohmann::json>(MaterialList::HIDDEN_SETTING, nlohmann::json::array());
        settings().set<std::string>(CoverMaterial::UNIT_SETTING, "characters");
    }
    ~KeptSettings() {
        try {
            for (std::size_t i = 0; i < KEYS.size(); ++i) {
                settings().set<nlohmann::json>(KEYS.at(i), m_values.at(i));
            }
        } catch (...) {
            std::fputs("KeptSettings: the settings of the test stay\n", stderr);
        }
    }
    KeptSettings(const KeptSettings&) = delete;
    KeptSettings& operator=(const KeptSettings&) = delete;

private:
    static constexpr std::array<const char*, 4> KEYS{
        MaterialList::HIDDEN_SETTING, CoverMaterial::UNIT_SETTING, "materials.cover.targets",
        MaterialList::LISTS_SETTING};
    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }
    std::vector<nlohmann::json> m_values;
};

/// The names of the groups of the rows of @p panel, in their order
QStringList headingsOf(const MaterialListPanel& panel) {
    QStringList headings;
    const QSortFilterProxyModel* rows = panel.rows();
    for (int row = 0; row < rows->rowCount(); ++row) {
        const QString heading =
            rows->index(row, 0).data(MaterialListPanel::HEADING_ROLE).toString();
        if (!heading.isEmpty()) {
            headings << heading;
        }
    }
    return headings;
}

/// The materials of the rows of @p panel, in their order
QStringList idsOf(const MaterialListPanel& panel) {
    QStringList ids;
    const QSortFilterProxyModel* rows = panel.rows();
    for (int row = 0; row < rows->rowCount(); ++row) {
        const QString id = rows->index(row, 0).data(MaterialListPanel::ID_ROLE).toString();
        if (!id.isEmpty()) {
            ids << id;
        }
    }
    return ids;
}

/// What the queued calls (a setting changed, the lists filled again) have done
void settle() {
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents();
    }
}

}  // namespace

TEST_CASE("Cover materials: the words of their parts, places, states and lengths",
          "[gui][cover]") {
    CHECK(CoverNames::partName(QStringLiteral("blurb")) == QStringLiteral("Cover description"));
    CHECK(CoverNames::partName(QStringLiteral("nothing")).isEmpty());
    CHECK(CoverNames::groupName(QStringLiteral("opinion")) ==
          QStringLiteral("Opinions and recommendations"));
    CHECK(CoverNames::groupName(QStringLiteral("nothing")) == QStringLiteral("Other"));
    CHECK(CoverNames::placeName(QStringLiteral("frontFlap")) == QStringLiteral("Front flap"));
    CHECK(CoverNames::placeName(QString()) == QStringLiteral("Not set"));
    CHECK(CoverNames::stateName(QString()) == QStringLiteral("draft"));
    // Values a later version adds are shown as they are written, not as none
    CHECK(CoverNames::placeName(QStringLiteral("belly band")) == QStringLiteral("belly band"));
    CHECK(CoverNames::stateName(QStringLiteral("printed")) == QStringLiteral("printed"));
    CHECK(CoverNames::consentName(QStringLiteral("oral")) == QStringLiteral("oral"));
    CHECK(CoverNames::shortConsentName(QString()) == QStringLiteral("not said"));
    CHECK(CoverNames::shortStateName(QStringLiteral("chosen")) == QStringLiteral("★ chosen"));
    CHECK(CoverNames::shortConsentName(QStringLiteral("asked")) == QStringLiteral("waiting"));
    CHECK(CoverNames::lengthText(311, false) == QStringLiteral("311 ch."));
    CHECK(CoverNames::countsText(311, 50) ==
          QStringLiteral("311 characters with spaces · 50 words"));
    CHECK(CoverNames::targetText(600, false) == QStringLiteral("target: up to 600 characters"));
    CHECK(CoverNames::overText(12, false) == QStringLiteral("12 characters too many"));
    CHECK(CoverNames::overText(3, true) == QStringLiteral("3 words too many"));

    core::CoverData opinion;
    opinion.author = QStringLiteral("Anna Wiśniewska");
    opinion.role = QStringLiteral("critic");
    CHECK(CoverNames::printedOpinion(QStringLiteral(" Dense\nprose. "), opinion) ==
          QStringLiteral("“Dense prose.” – Anna Wiśniewska, critic"));
    CHECK(CoverNames::stateMessage(QStringLiteral("Description 2"), QStringLiteral("chosen"),
                                   {QStringLiteral("Description 1")}) ==
          QStringLiteral("Chosen for the cover: “Description 2” · “Description 1” is a draft "
                         "again"));
    CHECK(CoverNames::stateMessage(QStringLiteral("Opinion"), QStringLiteral("rejected"), {}) ==
          QStringLiteral("Rejected: “Opinion”"));

    // A new material is numbered after those of its part
    test::OpenNovel novel(QStringLiteral("Sand"));
    CHECK(CoverNames::newTitle(kalahari::test::book(), QStringLiteral("blurb")) ==
          QStringLiteral("Description 1"));
    addCover(QStringLiteral("Back"), CoverMaterial::BLURB, CoverMaterial::BACK);
    CHECK(CoverNames::newTitle(kalahari::test::book(), QStringLiteral("blurb")) ==
          QStringLiteral("Description 2"));
    CHECK(CoverNames::newTitle(kalahari::test::book(), QStringLiteral("tagline")) ==
          QStringLiteral("Tagline 1"));
}

TEST_CASE("Cover materials: the list in groups of kinds, its filter, the plan and the states",
          "[gui][cover][materials]") {
    gui::registerAllIcons();
    KeptSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    QImage image(60, 90, QImage::Format_RGB32);
    image.fill(Qt::darkYellow);
    const QString picture = QDir(novel.dir.path()).filePath(QStringLiteral("design.png"));
    REQUIRE(image.save(picture));
    const QString first = addCover(QStringLiteral("Description 1"), CoverMaterial::BLURB,
                                   CoverMaterial::BACK, QStringLiteral("Three days of road."));
    const QString second = addCover(QStringLiteral("Description 2"), CoverMaterial::BLURB,
                                    CoverMaterial::BACK, QStringLiteral("One well."));
    const QString tagline = addCover(QStringLiteral("Tagline 1"), CoverMaterial::TAGLINE,
                                     CoverMaterial::FRONT, QStringLiteral("Sand remembers."));
    const QString opinion = addCover(QStringLiteral("Opinion 1"), CoverMaterial::OPINION,
                                     CoverMaterial::BACK, QStringLiteral("Dense prose."));
    const QString design = addCover(QStringLiteral("Design 1"), CoverMaterial::DESIGN,
                                    CoverMaterial::WHOLE, QString(), picture);
    const QString note = addCover(QStringLiteral("Note 1"), CoverMaterial::NOTE, "",
                                  QStringLiteral("Warmer colors."));
    CHECK(MaterialList::isCover(COVER_LIST));
    CHECK(MaterialList::kinds(kalahari::test::book()).contains(COVER_LIST));
    CHECK(MaterialList::countOf(kalahari::test::book(), COVER_LIST) == 6);

    test::Window window;
    gui::MaterialListCoordinator lists(kalahari::test::book(), window.tabs, &window.navigator,
                                       &window.statusBar, [](bool) {});
    MaterialListPanel* panel = lists.openList(COVER_LIST, false);
    REQUIRE(panel != nullptr);

    // The groups of the kinds, in the order of the kinds, each with its number
    panel->showGroups(true);
    CHECK(headingsOf(*panel) ==
          QStringList{"Cover descriptions · 2", "Taglines · 1", "Opinions and recommendations · 1",
                      "Designs and sketches · 1", "Notes · 1"});
    CHECK(idsOf(*panel).size() == 6);
    // A row that names a group cannot be chosen
    CHECK_FALSE(panel->rows()->index(0, 0).flags().testFlag(Qt::ItemIsEnabled));

    // Kind: one kind only
    panel->setPartFilter(QStringLiteral("opinion"));
    CHECK(idsOf(*panel) == QStringList{opinion});
    CHECK(headingsOf(*panel) == QStringList{"Opinions and recommendations · 1"});
    panel->setPartFilter(QString());
    panel->showGroups(false);
    CHECK(headingsOf(*panel).isEmpty());
    CHECK(idsOf(*panel).size() == 6);

    // The plan: what goes on the cover, in its places, the design behind it
    panel->showPlan(true);
    gui::CoverPlan* plan = panel->plan();
    REQUIRE(plan != nullptr);
    CHECK(plan->hasCard(first));
    CHECK(plan->columnOf(first) == QStringLiteral("back"));
    CHECK(plan->columnOf(tagline) == QStringLiteral("front"));
    CHECK(plan->columnOf(opinion) == QStringLiteral("back"));
    CHECK_FALSE(plan->hasCard(note));
    CHECK_FALSE(plan->hasCard(design));
    CHECK(plan->designShown() == design);

    // Chosen for the cover: the description chosen before is a draft again
    emit panel->requestCoverState(first, QStringLiteral("chosen"));
    CHECK(pm.findElement(first)->cover.state == QStringLiteral("chosen"));
    emit panel->requestCoverState(second, QStringLiteral("chosen"));
    CHECK(pm.findElement(second)->cover.state == QStringLiteral("chosen"));
    CHECK(pm.findElement(first)->cover.state.isEmpty());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Chosen for the cover: “Description 2” · “Description 1” is a draft "
                         "again"));
    emit panel->requestCoverState(opinion, QStringLiteral("rejected"));
    CHECK(pm.findElement(opinion)->cover.state == QStringLiteral("rejected"));
    settle();
    // The rejected are on the plan on request
    CHECK_FALSE(plan->hasCard(opinion));
    plan->rejectedBox()->setChecked(true);
    CHECK(plan->hasCard(opinion));
    panel->showPlan(false);
}

TEST_CASE("Cover materials: a kind hidden in the Navigator, its list and Settings > Navigator",
          "[gui][cover][materials]") {
    gui::registerAllIcons();
    KeptSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    addCover(QStringLiteral("Description 1"), CoverMaterial::BLURB, CoverMaterial::BACK);
    test::Window window;
    gui::MaterialListCoordinator lists(kalahari::test::book(), window.tabs, &window.navigator,
                                       &window.statusBar, [](bool) {});
    MaterialListPanel* panel = lists.openList(COVER_LIST, false);
    REQUIRE(panel != nullptr);
    const auto kindNode = [&window]() -> QTreeWidgetItem* {
        for (QTreeWidgetItemIterator it(window.navigatorPanel->tree()); *it; ++it) {
            if ((*it)->data(0, Qt::UserRole).toString() ==
                QStringLiteral("workshop:") + COVER_LIST) {
                return *it;
            }
        }
        return nullptr;
    };
    CHECK(kindNode() != nullptr);

    // Hidden: not in the Navigator nor among the kinds; its materials stay in the book
    lists.hideKind(COVER_LIST);
    settle();
    CHECK(MaterialList::isHidden(COVER_LIST));
    CHECK_FALSE(MaterialList::kinds(kalahari::test::book()).contains(COVER_LIST));
    CHECK(MaterialList::allKinds(kalahari::test::book()).contains(COVER_LIST));
    CHECK(MaterialList::countOf(kalahari::test::book(), COVER_LIST) == 1);
    CHECK(kindNode() == nullptr);
    CHECK(window.statusBar.currentMessage().startsWith(
        QStringLiteral("The kind \"Cover\" is hidden.")));
    // Its list says so, with a button that shows it again
    CHECK(panel->hiddenPage()->isVisibleTo(panel));

    // Settings > Navigator: the kind unchecked; checked, it comes back
    gui::NavigatorPage page(kalahari::test::book());
    page.load();
    auto* box = page.findChild<QCheckBox*>(QStringLiteral("navigatorKind_") + COVER_LIST);
    REQUIRE(box != nullptr);
    CHECK(box->text() == QStringLiteral("Cover (1)"));
    CHECK_FALSE(box->isChecked());
    box->setChecked(true);
    CHECK(page.isChanged());
    page.apply();
    settle();
    CHECK_FALSE(MaterialList::isHidden(COVER_LIST));
    CHECK(kindNode() != nullptr);
    CHECK_FALSE(panel->hiddenPage()->isVisibleTo(panel));

    // The unit of the texts of the cover and the targets of their kinds
    auto* words = page.findChild<QRadioButton*>(QStringLiteral("navigatorCoverWords"));
    auto* blurbWords =
        page.findChild<QSpinBox*>(QStringLiteral("navigatorCoverTarget_blurb_words"));
    auto* blurbCharacters =
        page.findChild<QSpinBox*>(QStringLiteral("navigatorCoverTarget_blurb_characters"));
    REQUIRE(words != nullptr);
    REQUIRE(blurbWords != nullptr);
    REQUIRE(blurbCharacters != nullptr);
    page.load();
    CHECK(blurbCharacters->value() == 600);
    CHECK(blurbCharacters->isVisibleTo(&page));
    CHECK_FALSE(blurbWords->isVisibleTo(&page));
    words->setChecked(true);
    CHECK(blurbWords->isVisibleTo(&page));
    CHECK_FALSE(blurbCharacters->isVisibleTo(&page));
    blurbWords->setValue(120);
    page.apply();
    CHECK(CoverMaterial::countsWords());
    CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("blurb"), true) == 120);
    CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("blurb"), false) == 600);
}

TEST_CASE("Cover materials: the text, its counts and target, and the data on the card",
          "[gui][cover][card]") {
    gui::registerAllIcons();
    KeptSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString first = addCover(QStringLiteral("Description 1"), CoverMaterial::BLURB,
                                   CoverMaterial::BACK, QStringLiteral("Three days of road."));
    const QString second = addCover(QStringLiteral("Description 2"), CoverMaterial::BLURB,
                                    CoverMaterial::BACK);
    REQUIRE(pm.setCoverState(second, QStringLiteral("chosen")));

    gui::MaterialPage card(kalahari::test::book(), nullptr, true);
    int changed = 0;
    QStringList messages;
    QObject::connect(&card, &gui::MaterialPage::changed, [&changed]() { ++changed; });
    QObject::connect(&card, &gui::MaterialPage::message,
                     [&messages](const QString& text) { messages << text; });
    card.showMaterial(first);

    // The description of a text is its text, with its counts and its target
    CHECK(card.descriptionGroup()->title() == QStringLiteral("Text"));
    CHECK(card.coverGroup()->isVisibleTo(&card));
    CHECK(card.countsLabel()->isVisibleTo(&card));
    CHECK(card.countsLabel()->text() == QStringLiteral("19 characters with spaces · 4 words"));
    CHECK(card.targetLabel()->text() == QStringLiteral("target: up to 600 characters"));
    CHECK(card.targetBar()->maximum() == 600);
    CHECK(card.targetBar()->value() == 19);
    CHECK(card.targetSpin()->value() == 0);
    CHECK(card.targetSpin()->specialValueText() == QStringLiteral("as in the settings (600)"));

    // Its own target, and the text past it
    card.targetSpin()->setValue(10);
    CHECK(pm.findElement(first)->cover.targetCharacters == 10);
    CHECK(card.targetLabel()->text() == QStringLiteral("9 characters too many"));
    CHECK(card.targetBar()->value() == 10);
    CHECK(card.targetLabel()->font().bold());

    // The kinds among the texts; a description has a place and a state
    CHECK(card.coverPartBox()->count() == 4);
    CHECK(card.coverPlaceBox()->currentData().toString() == QStringLiteral("back"));
    CHECK(card.coverStateBox()->currentData().toString().isEmpty());
    CHECK_FALSE(card.coverAuthorEdit()->isVisibleTo(&card));

    // Chosen: the one chosen before is a draft again
    card.coverStateBox()->setCurrentIndex(card.coverStateBox()->findData(QStringLiteral("chosen")));
    emit card.coverStateBox()->activated(card.coverStateBox()->currentIndex());
    CHECK(pm.findElement(first)->cover.state == QStringLiteral("chosen"));
    CHECK(pm.findElement(second)->cover.state.isEmpty());
    REQUIRE(messages.size() == 1);
    CHECK(messages.at(0) ==
          QStringLiteral("Chosen for the cover: “Description 1” · “Description 2” is a draft "
                         "again"));
    CHECK(changed >= 2);

    // An opinion: its author and who the author is, and how the cover prints it
    card.coverPartBox()->setCurrentIndex(card.coverPartBox()->findData(QStringLiteral("opinion")));
    emit card.coverPartBox()->activated(card.coverPartBox()->currentIndex());
    CHECK(pm.findElement(first)->cover.part == QStringLiteral("opinion"));
    CHECK(pm.findElement(first)->cover.place == QStringLiteral("back"));
    CHECK(card.coverAuthorEdit()->isVisibleTo(&card));
    CHECK(card.coverConsentBox()->isVisibleTo(&card));
    card.coverAuthorEdit()->setText(QStringLiteral(" Anna "));
    emit card.coverAuthorEdit()->editingFinished();
    card.coverRoleEdit()->setText(QStringLiteral("critic"));
    emit card.coverRoleEdit()->editingFinished();
    CHECK(pm.findElement(first)->cover.author == QStringLiteral("Anna"));
    CHECK(card.printedLabel()->isVisibleTo(&card));
    CHECK(card.printedLabel()->text() ==
          QStringLiteral("On the cover: “Three days of road.” – Anna, critic"));

    // A note leaves the cover: no place, no state, no target
    card.coverPartBox()->setCurrentIndex(card.coverPartBox()->findData(QStringLiteral("note")));
    emit card.coverPartBox()->activated(card.coverPartBox()->currentIndex());
    const core::CoverData& noted = pm.findElement(first)->cover;
    CHECK(noted.part == QStringLiteral("note"));
    CHECK(noted.place.isEmpty());
    CHECK(noted.state.isEmpty());
    CHECK_FALSE(card.coverPlaceBox()->isVisibleTo(&card));
    CHECK_FALSE(card.targetBar()->isVisibleTo(&card));
    CHECK(card.countsLabel()->isVisibleTo(&card));

    // In words, as the settings say
    core::SettingsManager::getInstance().set<std::string>(CoverMaterial::UNIT_SETTING, "words");
    card.showMaterial(second);
    CHECK(card.targetLabel()->text() == QStringLiteral("target: up to 100 words"));

    // Another material has its description
    gui::MaterialPage other(kalahari::test::book());
    core::NewMaterial quote;
    quote.title = QStringLiteral("Quote");
    const QString quoteId = pm.addMaterial(
        quote, pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("quote")));
    REQUIRE_FALSE(quoteId.isEmpty());
    other.showMaterial(quoteId);
    CHECK(other.descriptionGroup()->title() == QStringLiteral("Description"));
    CHECK_FALSE(other.coverGroup()->isVisibleTo(&other));
    CHECK_FALSE(other.countsLabel()->isVisibleTo(&other));
}
