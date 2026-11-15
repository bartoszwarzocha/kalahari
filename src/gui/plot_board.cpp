// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board.cpp
/// @brief What the plot board and the outline show of the book, the places a card moves to,
/// and the colors of the cards

#include "kalahari/gui/plot_board.h"

#include "kalahari/core/project_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/kind_names.h"

#include <QLocale>
#include <QPalette>

#include <algorithm>
#include <array>
#include <functional>

namespace kalahari::gui {

namespace {

/// @brief The kind of the Workshop's cards of characters and of places (any package)
constexpr const char* CHARACTER_KIND = "character";
constexpr const char* LOCATION_KIND = "location";

/// @brief The keys of the colors in the theme files, in the order of PlotColors
constexpr std::array<const char*, 9> COLOR_KEYS{{"plotDraft", "plotRevision", "plotFinal",
                                                 "plotBlue", "plotTeal", "plotGreen",
                                                 "plotAmber", "plotRose", "plotViolet"}};

/// @brief How many colors come before the writer's own in COLOR_KEYS
constexpr std::size_t STATUS_COLORS = 3;

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
    std::sort(titles.begin(), titles.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(a, b) < 0;
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

}  // namespace

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

QList<PlotBand> PlotBoard::bandsOf(const core::ProjectManager& book) {
    QList<PlotBand> bands;
    const core::BookProject* project = book.project();
    if (project == nullptr || project->books.isEmpty()) {
        return bands;
    }
    const QString language = programLanguage();
    int number = 0;
    const auto cardOf = [&](const core::ProjectElement& element, const QString& partId) {
        PlotCard card;
        card.id = element.id;
        const core::KindRef kind = book.kindOf(element);
        card.kindName = kind ? kind.kind->name.text(language) : QString();
        card.title = element.title;
        card.description = element.description;
        card.plot = element.plot;
        card.status = core::ProjectManager::statusOf(element);
        card.words = book.wordCount(element.id);
        card.partId = partId;
        card.number = ++number;
        return card;
    };
    // The texts of a part, also those of the groups inside it
    const std::function<void(const QList<core::ProjectElement>&, PlotBand&)> collect =
        [&](const QList<core::ProjectElement>& elements, PlotBand& band) {
            for (const core::ProjectElement& element : elements) {
                const core::ElementForm form = book.formOf(element);
                if (form == core::ElementForm::Text) {
                    band.cards << cardOf(element, band.partId);
                } else if (form == core::ElementForm::Group) {
                    collect(element.elements, band);
                }
            }
        };
    for (const core::ProjectElement& element : project->books.constFirst().mainElements) {
        const core::ElementForm form = book.formOf(element);
        if (form == core::ElementForm::Group) {
            PlotBand band;
            band.partId = element.id;
            band.title = element.title;
            collect(element.elements, band);
            bands << band;
        } else if (form == core::ElementForm::Text) {
            if (bands.isEmpty() || !bands.constLast().partId.isEmpty()) {
                bands << PlotBand{};
            }
            bands.last().cards << cardOf(element, QString());
        }
    }
    return bands;
}

QList<PlotCard> PlotBoard::cardsOf(const QList<PlotBand>& bands) {
    QList<PlotCard> cards;
    for (const PlotBand& band : bands) {
        cards << band.cards;
    }
    return cards;
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
    core::ElementPlace place{core::BookPlace::Main, partId, atEnd ? part->elements.size() : 0};
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
    for (std::size_t i = STATUS_COLORS; i < COLOR_KEYS.size(); ++i) {
        colors.own << themes.editorColor(COLOR_KEYS[i], fallback);
    }
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
        return ofStatus(card.status);
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
    std::sort(povs.begin(), povs.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    return povs;
}

}  // namespace kalahari::gui
