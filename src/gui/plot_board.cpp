// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board.cpp
/// @brief What the plot board and the outline show of the book, the places a card moves to,
/// and the colors of the cards

#include "kalahari/gui/plot_board.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/kind_names.h"

#include <QColor>
#include <QHash>
#include <QList>
#include <QLocale>
#include <QPalette>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kalahari::gui {

namespace {

/// @brief The kind of the Workshop's cards of characters and of places (any package)
constexpr const char* CHARACTER_KIND = "character";
constexpr const char* LOCATION_KIND = "location";

/// @brief The keys of the colors in the theme files, in the order of PlotColors
constexpr std::array<const char*, 10> COLOR_KEYS{
    {"plotDraft", "plotRevision", "plotFinal", "plotPlanned", "plotBlue", "plotTeal",
     "plotGreen", "plotAmber", "plotRose", "plotViolet"}};

/// @brief How many colors come before the writer's own in COLOR_KEYS
constexpr std::size_t STATUS_COLORS = 4;

/// @brief What joins the id of a text and the number of its scene in the id of a scene's card
constexpr char SCENE_MARK = '#';

/// @brief What comes before the number of a scene in the plan in the id of its card, after
/// SCENE_MARK
constexpr char PLANNED_MARK = 'p';

/// @brief The titles of the elements of the Workshop of kind @p kindId, each once, sorted
QStringList workshopTitles(const core::ProjectManager& book, const QString& kindId) {
    QStringList titles;
    const core::BookProject* project = book.project();
    if (project == nullptr) {
        return titles;
    }
    const std::function<void(const QList<core::ProjectElement>&)> collect =
        [&](const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                if (element.kind.kindId == kindId && !element.title.trimmed().isEmpty()) {
                    titles << element.title.trimmed();
                }
                collect(element.elements);
            }
        };
    collect(project->workshop.elements);
    titles.removeDuplicates();
    std::ranges::sort(titles, [](const QString& one, const QString& other) {
        return QString::localeAwareCompare(one, other) < 0;
    });
    return titles;
}

/// @brief The place @p place of text @p elementId in the list without it: a place of the list
/// the element is in comes one back when the element is before it
core::ElementPlace withoutElement(const core::BookProject& project, const QString& elementId,
                                  core::ElementPlace place) {
    const std::optional<core::ElementPlace> own = project.placeOf(elementId);
    if (own && own->place == place.place && own->groupId == place.groupId &&
        own->index < place.index) {
        --place.index;
    }
    return place;
}

/// @brief What the cards of the texts of a book are made from
struct CardSource {
    const core::ProjectManager* book = nullptr;          ///< The book
    const QHash<QString, QString>* unsaved = nullptr;    ///< The texts of the open texts with
                                                         ///< changes, by their ids
    QString language;                                    ///< Of the names of the kinds
    core::WordCountRules rules;                          ///< Of the words of the scenes
    QString variant;                                     ///< Which scenes there are
};

/// @brief The card of scene @p index of the text of card @p text
PlotCard sceneCard(const PlotCard& text, const core::KmlScene& scene, int index,
                   const core::WordCountRules& rules) {
    const core::ScenePlot& plot = scene.plot;
    PlotCard card;
    card.id = PlotBoard::sceneId(text.id, index);
    card.kindName = PlotBoard::sceneName(index);
    card.title = plot.title;
    card.description = plot.description;
    card.plot.pov = plot.pov;
    card.plot.place = plot.place;
    card.plot.time = plot.time;
    card.plot.color = plot.color;
    card.status = plot.status.isEmpty() ? text.status : plot.status;
    card.words = scene.words.wordsUnder(rules);
    card.partId = text.partId;
    card.number = text.number;
    card.textId = text.id;
    card.scene = index;
    return card;
}

/// @brief The card of scene @p index in the plan of the text of card @p text, named
/// @p kindName
PlotCard plannedCard(const PlotCard& text, const core::PlannedScene& scene, int index,
                     const QString& kindName) {
    const core::ScenePlot& plot = scene.plot;
    PlotCard card;
    card.id = PlotBoard::plannedId(text.id, index);
    card.kindName = kindName;
    card.title = plot.title;
    card.description = plot.description;
    card.plot.pov = plot.pov;
    card.plot.place = plot.place;
    card.plot.time = plot.time;
    card.plot.color = plot.color;
    card.partId = text.partId;
    card.number = text.number;
    card.textId = text.id;
    card.scene = index;
    card.planned = true;
    return card;
}

/// @brief The cards of the scenes of the text of card @p text that the board shows: the
/// scenes @p written of its text and those @p planned in its plan, as @p variant says
QList<PlotCard> sceneCards(const PlotCard& text, const QList<core::KmlScene>& written,
                           const QList<core::PlannedScene>& planned, const QString& variant,
                           const core::WordCountRules& rules) {
    QList<PlotCard> cards;
    if (variant == QLatin1String(PlotBoard::SCENES_PLAN)) {
        // Only the plan: its scenes are the scenes of the text, numbered
        const QList<PlotBoard::SceneSlot> order = PlotBoard::sceneOrder(0, planned);
        for (qsizetype i = 0; i < order.size(); ++i) {
            const int index = order.at(i).index;
            cards << plannedCard(text, planned.at(index), index,
                                 PlotBoard::sceneName(static_cast<int>(i)));
        }
        return cards;
    }
    const bool withPlan = variant != QLatin1String(PlotBoard::SCENES_TEXT) && !planned.isEmpty();
    // A text of one scene has no cards of scenes, unless its plan has some
    if (!withPlan && written.size() < 2) {
        return cards;
    }
    const QList<PlotBoard::SceneSlot> order = PlotBoard::sceneOrder(
        static_cast<int>(written.size()), withPlan ? planned : QList<core::PlannedScene>());
    for (const PlotBoard::SceneSlot& slot : order) {
        cards << (slot.planned ? plannedCard(text, planned.at(slot.index), slot.index,
                                             PlotBoard::plannedName())
                               : sceneCard(text, written.at(slot.index), slot.index, rules));
    }
    return cards;
}

/// @brief The card of text @p element of @p book without its scenes; the names of the kinds
/// in @p language
PlotCard plainCard(const core::ProjectManager& book, const core::ProjectElement& element,
                   const QString& language) {
    PlotCard card;
    card.id = element.id;
    const core::KindRef kind = book.kindOf(element);
    card.kindName = kind ? kind.kind->name.text(language) : QString();
    card.title = element.title;
    card.description = element.description;
    card.plot = element.plot;
    card.status = core::ProjectManager::statusOf(element);
    card.words = book.wordCount(element.id);
    return card;
}

/// @brief The card of text @p element of part @p partId, the @p number -th of the board, with
/// the cards of its scenes
PlotCard textCard(const CardSource& source, const core::ProjectElement& element,
                  const QString& partId, int number) {
    const core::ProjectManager& book = *source.book;
    PlotCard card = plainCard(book, element, source.language);
    card.partId = partId;
    card.number = number;
    // The scenes of the text the writer sees: of an open text with changes, its own
    const auto own = source.unsaved->constFind(element.id);
    const QList<core::KmlScene> scenes =
        book.scenesOf(element.id, own != source.unsaved->cend() ? std::optional<QString>(*own)
                                                                 : std::nullopt);
    card.scenes = sceneCards(card, scenes, element.plot.planned, source.variant, source.rules);
    return card;
}

/// @brief Put the texts of @p elements, also those of the groups among them, on @p band;
/// @p number is the number of the last text of the board
void collectTexts(const CardSource& source, const QList<core::ProjectElement>& elements,
                  PlotBand& band, int& number) {
    const std::function<void(const QList<core::ProjectElement>&)> collect =
        [&](const QList<core::ProjectElement>& list) {
            for (const core::ProjectElement& element : list) {
                const core::ElementForm form = source.book->formOf(element);
                if (form == core::ElementForm::Text) {
                    band.cards << textCard(source, element, band.partId, ++number);
                } else if (form == core::ElementForm::Group) {
                    collect(element.elements);
                }
            }
        };
    collect(elements);
}

}  // namespace

// =============================================================================
// PlotCard
// =============================================================================

QStringList PlotCard::scenePovs() const {
    QStringList povs;
    for (const PlotCard& each : scenes) {
        if (!each.plot.pov.isEmpty() && !povs.contains(each.plot.pov)) {
            povs << each.plot.pov;
        }
    }
    return povs;
}

// =============================================================================
// PlotBand
// =============================================================================

int PlotBand::words() const {
    int total = 0;
    for (const PlotCard& card : cards) {
        total += card.words;
    }
    return total;
}

// =============================================================================
// PlotBoard
// =============================================================================

QString PlotBoard::scenesVariant() {
    auto& settings = core::SettingsManager::getInstance();
    const auto variant = settings.get<std::string>(SCENES_VARIANT_SETTING, SCENES_BOTH);
    if (variant == SCENES_TEXT || variant == SCENES_PLAN) {
        return QString::fromStdString(variant);
    }
    return QString::fromLatin1(SCENES_BOTH);
}

QString PlotBoard::startView() {
    auto& settings = core::SettingsManager::getInstance();
    const auto start = settings.get<std::string>(START_VIEW_SETTING, START_LAST);
    const auto view = start == VIEW_BOARD || start == VIEW_OUTLINE
                          ? start
                          : settings.get<std::string>(VIEW_SETTING, VIEW_BOARD);
    return QString::fromLatin1(view == VIEW_OUTLINE ? VIEW_OUTLINE : VIEW_BOARD);
}

std::string PlotBoard::fieldSetting(const char* field) {
    return std::string(FIELDS_SETTING) + field;
}

std::string PlotBoard::outlineSetting(const char* column) {
    return std::string(OUTLINE_SETTING) + column;
}

QList<PlotBand> PlotBoard::bandsOf(const core::ProjectManager& book,
                                   const QHash<QString, QString>& unsaved,
                                   const QString& variant) {
    const core::BookProject* project = book.project();
    if (project == nullptr || project->books.isEmpty()) {
        return {};
    }
    CardSource source;
    source.book = &book;
    source.unsaved = &unsaved;
    source.language = programLanguage();
    source.rules = core::wordCountRules();
    source.variant = variant.isEmpty() ? scenesVariant() : variant;
    QList<PlotBand> bands;
    int number = 0;
    for (const core::ProjectElement& element : project->books.constFirst().mainElements) {
        const core::ElementForm form = book.formOf(element);
        if (form == core::ElementForm::Group) {
            PlotBand band;
            band.partId = element.id;
            band.title = element.title;
            collectTexts(source, element.elements, band, number);
            bands << band;
        } else if (form == core::ElementForm::Text) {
            // The texts outside the parts one after another in a band
            if (bands.isEmpty() || !bands.constLast().partId.isEmpty()) {
                bands << PlotBand{};
            }
            bands.last().cards << textCard(source, element, QString(), ++number);
        }
    }
    return bands;
}

PlotBand PlotBoard::sectionBand(const core::ProjectManager& book, core::BookPlace place) {
    PlotBand band;
    const core::ProjectBook* first = book.book();
    if (first == nullptr) {
        return band;
    }
    band.title = first->sectionName(place);
    const QString language = programLanguage();
    const std::function<void(const QList<core::ProjectElement>&)> collect =
        [&](const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                const core::ElementForm form = book.formOf(element);
                if (form == core::ElementForm::Text) {
                    band.cards << plainCard(book, element, language);
                } else if (form == core::ElementForm::Group) {
                    collect(element.elements);
                }
            }
        };
    collect(place == core::BookPlace::Back ? first->backElements : first->frontElements);
    return band;
}

QList<PlotCard> PlotBoard::cardsOf(const QList<PlotBand>& bands) {
    QList<PlotCard> cards;
    for (const PlotBand& band : bands) {
        cards << band.cards;
    }
    return cards;
}

QList<PlotCard> PlotBoard::allCardsOf(const QList<PlotBand>& bands) {
    QList<PlotCard> cards;
    for (const PlotBand& band : bands) {
        for (const PlotCard& card : band.cards) {
            cards << card << card.scenes;
        }
    }
    return cards;
}

QString PlotBoard::sceneId(const QString& textId, int scene) {
    return textId + QLatin1Char(SCENE_MARK) + QString::number(scene);
}

std::optional<std::pair<QString, int>> PlotBoard::sceneOf(const QString& cardId) {
    const qsizetype mark = cardId.lastIndexOf(QLatin1Char(SCENE_MARK));
    if (mark <= 0) {
        return std::nullopt;
    }
    bool number = false;
    const int scene = cardId.mid(mark + 1).toInt(&number);
    if (!number || scene < 0) {
        return std::nullopt;
    }
    return std::make_pair(cardId.left(mark), scene);
}

QString PlotBoard::plannedId(const QString& textId, int index) {
    return textId + QLatin1Char(SCENE_MARK) + QLatin1Char(PLANNED_MARK) + QString::number(index);
}

std::optional<std::pair<QString, int>> PlotBoard::plannedOf(const QString& cardId) {
    const qsizetype mark = cardId.lastIndexOf(QLatin1Char(SCENE_MARK));
    if (mark <= 0 || cardId.mid(mark + 1, 1) != QLatin1Char(PLANNED_MARK)) {
        return std::nullopt;
    }
    bool number = false;
    const int index = cardId.mid(mark + 2).toInt(&number);
    if (!number || index < 0) {
        return std::nullopt;
    }
    return std::make_pair(cardId.left(mark), index);
}

QString PlotBoard::textOf(const QString& cardId) {
    if (const auto scene = sceneOf(cardId)) {
        return scene->first;
    }
    if (const auto planned = plannedOf(cardId)) {
        return planned->first;
    }
    return cardId;
}

PlotBoard::SceneSlot PlotBoard::slotOf(const QString& cardId) {
    if (const auto planned = plannedOf(cardId)) {
        return SceneSlot{.planned = true, .index = planned->second};
    }
    if (const auto scene = sceneOf(cardId)) {
        return SceneSlot{.planned = false, .index = scene->second};
    }
    return SceneSlot{.planned = false, .index = -1};
}

QString PlotBoard::slotId(const QString& textId, const SceneSlot& slot) {
    return slot.planned ? plannedId(textId, slot.index) : sceneId(textId, slot.index);
}

QList<PlotBoard::SceneSlot> PlotBoard::sceneOrder(int written,
                                                  const QList<core::PlannedScene>& planned) {
    // The scenes in the plan by the scenes of the text before them, in the order of the plan
    QList<int> byPlace(planned.size());
    for (qsizetype i = 0; i < planned.size(); ++i) {
        byPlace[i] = static_cast<int>(i);
    }
    std::ranges::stable_sort(byPlace, [&planned, written](int one, int other) {
        return std::min(planned.at(one).after, written) <
               std::min(planned.at(other).after, written);
    });
    QList<SceneSlot> order;
    qsizetype next = 0;
    for (int scene = 0; scene <= written; ++scene) {
        while (next < byPlace.size() &&
               std::min(planned.at(byPlace.at(next)).after, written) <= scene) {
            order << SceneSlot{.planned = true, .index = byPlace.at(next)};
            ++next;
        }
        if (scene < written) {
            order << SceneSlot{.planned = false, .index = scene};
        }
    }
    return order;
}

QList<core::PlannedScene> PlotBoard::planInOrder(const QList<SceneSlot>& order,
                                                 const QList<core::PlannedScene>& planned) {
    const bool withText =
        std::ranges::any_of(order, [](const SceneSlot& slot) { return !slot.planned; });
    // Without the scenes of the text: the places the plan had, in the order of the scenes
    QList<int> places;
    for (const core::PlannedScene& scene : planned) {
        places << scene.after;
    }
    std::ranges::sort(places);
    QList<core::PlannedScene> plan;
    int written = 0;
    for (const SceneSlot& slot : order) {
        if (!slot.planned) {
            ++written;
            continue;
        }
        if (slot.index < 0 || slot.index >= planned.size()) {
            continue;
        }
        core::PlannedScene scene = planned.at(slot.index);
        scene.after = withText ? written : places.value(plan.size(), scene.after);
        plan << scene;
    }
    return plan;
}

QString PlotBoard::sceneName(int scene) {
    //: The kind of a card of a scene on the plot board; %1 its number in its text. In Polish:
    //: Scena %1
    return tr("Scene %1").arg(scene + 1);
}

QString PlotBoard::plannedName() {
    //: The kind and the state of a card of a scene in the plan of its text, on the plot board
    //: and in the outline. In Polish: W planie
    return tr("In the Plan");
}

QString PlotBoard::scenesText(int scenes) {
    return scenes == 1
               //: The scenes of a text on its card on the plot board. In Polish: 1 scena
               ? tr("1 scene")
               //: In Polish: %n scena / sceny / scen
               : tr("%n scenes", nullptr, scenes);
}

std::optional<core::ElementPlace> PlotBoard::stepPlace(const core::ProjectManager& book,
                                                       const QString& elementId, int steps) {
    const core::BookProject* project = book.project();
    if (project == nullptr || steps == 0) {
        return std::nullopt;
    }
    const std::optional<core::ElementPlace> own = project->placeOf(elementId);
    if (!own || own->place != core::BookPlace::Main) {
        return std::nullopt;
    }
    // The places of the main section, in reading order; the one the text has is among them
    QList<core::ElementPlace> places = book.movePlacesOf(elementId);
    places.removeIf([](const core::ElementPlace& place) {
        return place.place != core::BookPlace::Main;
    });
    const qsizetype at = places.indexOf(*own);
    if (at < 0) {
        return std::nullopt;
    }
    const qsizetype to = std::clamp<qsizetype>(at + steps, 0, places.size() - 1);
    if (to == at) {
        return std::nullopt;
    }
    return places.at(to);
}

std::optional<core::ElementPlace> PlotBoard::placeBeside(const core::ProjectManager& book,
                                                         const QString& elementId,
                                                         const QString& anchorId, bool after) {
    const core::BookProject* project = book.project();
    if (project == nullptr || elementId == anchorId) {
        return std::nullopt;
    }
    std::optional<core::ElementPlace> anchor = project->placeOf(anchorId);
    if (!anchor || !project->placeOf(elementId)) {
        return std::nullopt;
    }
    if (after) {
        ++anchor->index;
    }
    core::ElementPlace place = withoutElement(*project, elementId, *anchor);
    if (!book.canMoveTo(elementId, place)) {
        return std::nullopt;
    }
    return place;
}

std::optional<core::ElementPlace> PlotBoard::placeInPart(const core::ProjectManager& book,
                                                         const QString& elementId,
                                                         const QString& partId, bool atEnd) {
    const core::BookProject* project = book.project();
    const core::ProjectElement* part =
        project != nullptr ? project->findElement(partId) : nullptr;
    const std::optional<core::ElementPlace> partPlace =
        part != nullptr ? project->placeOf(partId) : std::nullopt;
    if (!partPlace || partPlace->place != core::BookPlace::Main ||
        !project->placeOf(elementId)) {
        return std::nullopt;
    }
    core::ElementPlace place{.place = core::BookPlace::Main, .groupId = partId,
                             .index = atEnd ? part->elements.size() : 0};
    place = withoutElement(*project, elementId, place);
    if (!book.canMoveTo(elementId, place)) {
        return std::nullopt;
    }
    return place;
}

QStringList PlotBoard::povSuggestions(const core::ProjectManager& book) {
    return workshopTitles(book, QString::fromLatin1(CHARACTER_KIND));
}

QStringList PlotBoard::placeSuggestions(const core::ProjectManager& book) {
    return workshopTitles(book, QString::fromLatin1(LOCATION_KIND));
}

QString PlotBoard::statusName(const QString& status) {
    if (status == QLatin1String("draft")) {
        //: The state of a text on the plot board. In Polish: Szkic
        return tr("Draft");
    }
    if (status == QLatin1String("revision")) {
        //: In Polish: Poprawki
        return tr("Revision");
    }
    if (status == QLatin1String("final")) {
        //: In Polish: Gotowy
        return tr("Final");
    }
    return status;
}

QString PlotBoard::wordsText(int words) {
    return words == 1
               //: The words of a text on the plot board. In Polish: 1 słowo
               ? tr("1 word")
               //: The words of a text or of a part on the plot board; %Ln the number with the
               //: separators of thousands. In Polish: %Ln słowo / słowa / słów
               : tr("%Ln words", nullptr, words);
}

QString PlotBoard::textsText(int texts) {
    return texts == 1
               //: The texts of a part or of the book on the plot board. In Polish: 1 tekst
               ? tr("1 text")
               //: The texts of a part or of the book on the plot board. In Polish: %n tekst /
               //: teksty / tekstów
               : tr("%n texts", nullptr, texts);
}

// =============================================================================
// PlotColors
// =============================================================================

const std::vector<std::string>& PlotColors::keys() {
    static const std::vector<std::string> KEYS(COLOR_KEYS.cbegin(), COLOR_KEYS.cend());
    return KEYS;
}

const QStringList& PlotColors::ownNames() {
    static const QStringList NAMES{QStringLiteral("blue"),  QStringLiteral("teal"),
                                   QStringLiteral("green"), QStringLiteral("amber"),
                                   QStringLiteral("rose"),  QStringLiteral("violet")};
    return NAMES;
}

QString PlotColors::ownTitle(const QString& name) {
    if (name == QLatin1String("blue")) {
        //: An own color of a card of the plot board. In Polish: Niebieski
        return PlotBoard::tr("Blue");
    }
    if (name == QLatin1String("teal")) {
        //: In Polish: Morski
        return PlotBoard::tr("Teal");
    }
    if (name == QLatin1String("green")) {
        //: In Polish: Zielony
        return PlotBoard::tr("Green");
    }
    if (name == QLatin1String("amber")) {
        //: In Polish: Bursztynowy
        return PlotBoard::tr("Amber");
    }
    if (name == QLatin1String("rose")) {
        //: In Polish: Różowy
        return PlotBoard::tr("Rose");
    }
    if (name == QLatin1String("violet")) {
        //: In Polish: Fioletowy
        return PlotBoard::tr("Violet");
    }
    return {};
}

PlotColors PlotColors::current() {
    const auto& themes = core::ThemeManager::getInstance();
    // A theme file without the colors of the board: the colors of its palette
    const QPalette palette = themes.getCurrentTheme().palette.toQPalette();
    const QColor fallback = palette.color(QPalette::Mid);
    PlotColors colors;
    colors.draft = themes.editorColor(COLOR_KEYS[0], fallback);
    colors.revision = themes.editorColor(COLOR_KEYS[1], palette.color(QPalette::Highlight));
    colors.finished = themes.editorColor(COLOR_KEYS[2], palette.color(QPalette::Link));
    colors.planned = themes.editorColor(COLOR_KEYS[3], fallback);
    for (std::size_t i = STATUS_COLORS; i < COLOR_KEYS.size(); ++i) {
        colors.own << themes.editorColor(COLOR_KEYS.at(i), fallback);
    }
    colors.cork = themes.editorColor(CORK_KEY, palette.color(QPalette::Window));
    colors.background = themes.editorColor(BACKGROUND_KEY, palette.color(QPalette::Window));
    return colors;
}

QColor PlotColors::ofStatus(const QString& status) const {
    if (status == QLatin1String("draft")) {
        return draft;
    }
    if (status == QLatin1String("revision")) {
        return revision;
    }
    if (status == QLatin1String("final")) {
        return finished;
    }
    return {};
}

QColor PlotColors::ofStatus(const PlotCard& card) const {
    return card.planned ? planned : ofStatus(card.status);
}

QColor PlotColors::ofOwn(const QString& name) const {
    const qsizetype index = ownNames().indexOf(name);
    return index >= 0 && index < own.size() ? own.at(index) : QColor();
}

QColor PlotColors::ofPov(const QString& pov, const QStringList& povs) const {
    const qsizetype index = povs.indexOf(pov.trimmed());
    if (pov.trimmed().isEmpty() || index < 0 || own.isEmpty()) {
        return {};
    }
    return own.at(index % own.size());
}

QColor PlotColors::ofCard(const PlotCard& card, const QString& colorBy,
                          const QStringList& povs) const {
    if (colorBy == QLatin1String(PlotBoard::COLOR_STATUS)) {
        return ofStatus(card);
    }
    if (colorBy == QLatin1String(PlotBoard::COLOR_POV)) {
        return ofPov(card.plot.pov, povs);
    }
    if (colorBy == QLatin1String(PlotBoard::COLOR_OWN)) {
        return ofOwn(card.plot.color);
    }
    return {};
}

QStringList povsOf(const QList<PlotCard>& cards) {
    QStringList povs;
    for (const PlotCard& card : cards) {
        const QString pov = card.plot.pov.trimmed();
        if (!pov.isEmpty() && !povs.contains(pov)) {
            povs << pov;
        }
    }
    std::ranges::sort(povs, [](const QString& one, const QString& other) {
        return QString::localeAwareCompare(one, other) < 0;
    });
    return povs;
}

}  // namespace kalahari::gui
