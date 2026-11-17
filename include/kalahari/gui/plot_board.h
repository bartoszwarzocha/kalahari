// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board.h
/// @brief What the plot board and the outline show of the book, the places a card moves to,
/// and the colors of the cards

#ifndef KALAHARI_GUI_PLOT_BOARD_H
#define KALAHARI_GUI_PLOT_BOARD_H

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"

#include <QColor>
#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief A text of the main section of the book as a card of the plot board and a row of the
/// outline, or a scene of such a text
struct PlotCard {
    QString id;               ///< Id of the element; of a scene PlotBoard::sceneId()
    QString kindName;         ///< Name of its kind: "Chapter", "Prologue"; of a scene "Scene 2"
    QString iconId;           ///< ArtProvider id of its icon
    QString title;            ///< Its title
    QString description;      ///< Its short description (core::ProjectElement::description)
    core::ElementPlot plot;   ///< Point of view, place and time of the action, color of the card
    QString status;           ///< "draft", "revision" or "final"
    int words = 0;            ///< Words of its text (core::ProjectManager::wordCount())
    QString partId;           ///< The part it is in; empty: outside the parts
    int number = 0;           ///< Its place among the cards of the board, from 1
    QString textId;           ///< Of a scene: its text; empty for a text
    int scene = -1;           ///< Of a scene: its place in its text, from 0; of a scene in the
                              ///< plan, its place in the plan; -1 for a text
    bool planned = false;     ///< A scene in the plan of its text (core::PlannedScene): it has
                              ///< no text, no words and no state yet
    QList<PlotCard> scenes;   ///< Of a text: the scenes the board shows, in their order (those
                              ///< of a text with scene breaks, those in its plan); none: one
                              ///< scene

    /// @brief Whether it is a scene of a text
    [[nodiscard]] bool isScene() const { return scene >= 0; }

    /// @brief The points of view of its scenes, each once in their order: those of a text
    /// without its own
    [[nodiscard]] QStringList scenePovs() const;

    bool operator==(const PlotCard&) const = default;
};

/// @brief A part of the main section with its texts, or texts outside the parts one after
/// another
struct PlotBand {
    QString partId;         ///< Id of the part; empty: texts outside the parts
    QString title;          ///< Title of the part; empty outside the parts
    QList<PlotCard> cards;  ///< Its texts in the order of the book

    /// @brief Words of its texts
    [[nodiscard]] int words() const;

    bool operator==(const PlotBand&) const = default;
};

/// @brief What the plot board shows of the open book, and where its cards move
///
/// The board shows the texts of the main section of the book (chapters, a prologue, an
/// epilogue...) in the order of the book, the texts of a part together under its title. A card
/// moves as the element moves in the Navigator (core::ProjectManager::moveElementTo()): one step
/// back or forward goes past one place of the main section, so a text at the start of a part
/// first leaves the part, and the next step takes it to the end of the part before.
class PlotBoard {
    Q_DECLARE_TR_FUNCTIONS(PlotBoard)

public:
    /// @name Settings of the board
    /// @{
    /// The board or the outline, the one shown last: VIEW_BOARD or VIEW_OUTLINE
    static constexpr const char* VIEW_SETTING = "plotBoard.view";
    static constexpr const char* VIEW_BOARD = "board";      ///< The board of the cards
    static constexpr const char* VIEW_OUTLINE = "outline";  ///< The outline: a table
    /// The view a new tab opens on when no view is asked for: VIEW_BOARD, VIEW_OUTLINE or
    /// START_LAST (the one shown last, VIEW_SETTING)
    static constexpr const char* START_VIEW_SETTING = "plotBoard.startView";
    static constexpr const char* START_LAST = "last";  ///< The view shown last
    /// The layout of the cards: LAYOUT_COLUMNS (the parts side by side) or LAYOUT_GRID (the
    /// cards in rows under the titles of the parts)
    static constexpr const char* LAYOUT_SETTING = "plotBoard.layout";
    static constexpr const char* LAYOUT_COLUMNS = "columns";  ///< The parts side by side
    static constexpr const char* LAYOUT_GRID = "grid";        ///< The cards in rows
    /// Whether the board shows the scenes of the texts (in Grid: with their drawers open) and
    /// the outline their rows
    static constexpr const char* SCENES_SETTING = "plotBoard.showScenes";
    /// Which scenes there are: SCENES_BOTH (the scenes of the texts and those in their plans),
    /// SCENES_TEXT (those of the texts) or SCENES_PLAN (those in the plans only)
    static constexpr const char* SCENES_VARIANT_SETTING = "plotBoard.scenes";
    static constexpr const char* SCENES_BOTH = "both";  ///< The plan and the text
    static constexpr const char* SCENES_TEXT = "text";  ///< The scenes of the texts
    static constexpr const char* SCENES_PLAN = "plan";  ///< The scenes in the plans only
    /// The size of the cards: SIZE_SMALL, SIZE_MEDIUM or SIZE_LARGE
    static constexpr const char* SIZE_SETTING = "plotBoard.cardSize";
    static constexpr const char* SIZE_SMALL = "small";    ///< Small cards
    static constexpr const char* SIZE_MEDIUM = "medium";  ///< Medium cards
    static constexpr const char* SIZE_LARGE = "large";    ///< Large cards
    /// What colors the cards: COLOR_STATUS, COLOR_POV (a color for each point of view),
    /// COLOR_OWN (the writer's own color of each card) or COLOR_NONE
    static constexpr const char* COLOR_SETTING = "plotBoard.colorBy";
    static constexpr const char* COLOR_STATUS = "status";  ///< By the state of the text
    static constexpr const char* COLOR_POV = "pov";        ///< By the point of view
    static constexpr const char* COLOR_OWN = "own";        ///< By the own color of the card
    static constexpr const char* COLOR_NONE = "none";      ///< Without colors
    /// Where the color of a card is: COLOR_STRIP (a strip at its top) or COLOR_TINT (its
    /// background)
    static constexpr const char* COLOR_PLACE_SETTING = "plotBoard.colorPlace";
    static constexpr const char* COLOR_STRIP = "strip";  ///< A strip at the top of the card
    static constexpr const char* COLOR_TINT = "tint";    ///< The background of the card
    /// The background of the board: BACKGROUND_THEME (the window's), BACKGROUND_CORK or
    /// BACKGROUND_OWN (PlotColors::background)
    static constexpr const char* BACKGROUND_SETTING = "plotBoard.background";
    static constexpr const char* BACKGROUND_THEME = "theme";  ///< The color of the theme
    static constexpr const char* BACKGROUND_CORK = "cork";    ///< Cork (PlotColors::cork)
    static constexpr const char* BACKGROUND_OWN = "own";      ///< PlotColors::background
    /// What the cards show besides their title: FIELDS_SETTING followed by the name of a
    /// field (FIELD_DESCRIPTION...), true or false (fieldSetting())
    static constexpr const char* FIELDS_SETTING = "plotBoard.fields.";
    static constexpr const char* FIELD_DESCRIPTION = "description";  ///< The description
    static constexpr const char* FIELD_POV = "pov";                  ///< The point of view
    static constexpr const char* FIELD_PLACE = "place";              ///< The place
    static constexpr const char* FIELD_TIME = "time";                ///< The time of the action
    static constexpr const char* FIELD_WORDS = "words";              ///< The words
    static constexpr const char* FIELD_STATUS = "status";            ///< The state
    static constexpr const char* FIELD_SCENES = "scenes";            ///< "3 scenes"
    /// The columns of the outline shown: OUTLINE_SETTING followed by the name of a column
    /// (PlotOutlineModel::columnName()), true or false; the title is always shown
    static constexpr const char* OUTLINE_SETTING = "plotBoard.outline.";
    /// Whether the board shows the texts of the front and the back section of the book too,
    /// before and after those of the main section: a preview, whose cards cannot be chosen
    static constexpr const char* SECTIONS_SETTING = "plotBoard.showSections";
    /// What happens to the numbers in the titles the program gave ("Chapter 3") when a move on
    /// the board leaves them out of the order: RENUMBER_ASK, RENUMBER_AUTO or RENUMBER_NEVER
    static constexpr const char* RENUMBER_SETTING = "plotBoard.renumber";
    static constexpr const char* RENUMBER_ASK = "ask";      ///< The bar under the board offers it
    static constexpr const char* RENUMBER_AUTO = "auto";    ///< At once, with the move
    static constexpr const char* RENUMBER_NEVER = "never";  ///< The titles stay as they are
    /// @}

    /// @brief Which scenes there are (SCENES_VARIANT_SETTING): SCENES_BOTH, SCENES_TEXT or
    /// SCENES_PLAN
    [[nodiscard]] static QString scenesVariant();

    /// @brief The view a new tab opens on (START_VIEW_SETTING): VIEW_BOARD or VIEW_OUTLINE
    [[nodiscard]] static QString startView();

    /// @brief What happens to the numbers in the titles after a move (RENUMBER_SETTING):
    /// RENUMBER_ASK, RENUMBER_AUTO or RENUMBER_NEVER
    [[nodiscard]] static QString renumbering();

    /// @brief The key of the setting of field @p field (FIELD_DESCRIPTION...) of the cards
    [[nodiscard]] static std::string fieldSetting(const char* field);

    /// @brief The key of the setting of the column named @p column of the outline
    [[nodiscard]] static std::string outlineSetting(const char* column);

    /// @brief The texts of the main section of the open book in bands: each part with its
    /// texts, and the texts outside the parts between them, each text with its scenes; none
    /// without a book
    /// @param unsaved The texts (KML) of the open texts with changes their files do not have,
    ///        by the ids of their elements: their scenes are those of these texts
    /// @param variant Which scenes there are (SCENES_BOTH...); empty: scenesVariant()
    [[nodiscard]] static QList<PlotBand> bandsOf(const core::ProjectManager& book,
                                                 const QHash<QString, QString>& unsaved = {},
                                                 const QString& variant = QString());

    /// @brief The texts of the front (@p place core::BookPlace::Front) or the back section of
    /// the open book, also those in its groups, in a band with the name of the section; their
    /// cards have no scenes and no numbers
    [[nodiscard]] static PlotBand sectionBand(const core::ProjectManager& book,
                                              core::BookPlace place);

    /// @brief A new title of a text
    struct TitleChange {
        QString id;     ///< The text
        QString title;  ///< Its title

        bool operator==(const TitleChange&) const = default;
    };

    /// @brief Title @p title of an element of kind @p kind with number @p number, when it is a
    /// title the program gives in @p language ("Chapter 3", also "Chapter 3. Storm"): only the
    /// number changes; nullopt for another title, and for a kind without a number in its titles
    [[nodiscard]] static std::optional<QString> numberedTitle(const QString& title,
                                                              const core::ElementKind& kind,
                                                              const QString& language,
                                                              int number);

    /// @brief The texts of the main section of the open book with titles the program gave
    /// whose numbers no longer follow the order of the book, with their titles renumbered: the
    /// n-th text of a kind has number n (numberedTitle()), in the language of the book
    [[nodiscard]] static QList<TitleChange> renumbered(const core::ProjectManager& book);

    /// @brief The cards of the texts of @p bands one after another
    [[nodiscard]] static QList<PlotCard> cardsOf(const QList<PlotBand>& bands);

    /// @brief The cards of @p bands one after another, each text followed by its scenes
    [[nodiscard]] static QList<PlotCard> allCardsOf(const QList<PlotBand>& bands);

    /// @brief The id of the card of scene @p scene (from 0) of text @p textId
    [[nodiscard]] static QString sceneId(const QString& textId, int scene);

    /// @brief The text and the scene of card @p cardId (sceneId()); nullopt for another card
    [[nodiscard]] static std::optional<std::pair<QString, int>> sceneOf(const QString& cardId);

    /// @brief The id of the card of scene @p index (from 0) in the plan of text @p textId
    [[nodiscard]] static QString plannedId(const QString& textId, int index);

    /// @brief The text and the place in its plan of card @p cardId (plannedId()); nullopt for
    /// another card
    [[nodiscard]] static std::optional<std::pair<QString, int>> plannedOf(const QString& cardId);

    /// @brief The text of card @p cardId: of a card of a scene, its text
    [[nodiscard]] static QString textOf(const QString& cardId);

    /// @brief A scene of a text where the board shows it: a scene of the text, or one in its
    /// plan
    struct SceneSlot {
        bool planned = false;  ///< A scene in the plan
        int index = 0;         ///< Its place among the scenes of the text, or in the plan

        bool operator==(const SceneSlot&) const = default;
    };

    /// @brief The place of card @p cardId among the scenes of its text: a scene of the text
    /// (sceneId()) or one in its plan (plannedId()); with index -1 for another card
    [[nodiscard]] static SceneSlot slotOf(const QString& cardId);

    /// @brief The id of the card of the scene of text @p textId at @p slot
    [[nodiscard]] static QString slotId(const QString& textId, const SceneSlot& slot);

    /// @brief The scenes of a text with @p written scenes and the scenes @p planned in its
    /// plan in the order of the board: each scene in the plan after the scenes of the text it
    /// comes after (core::PlannedScene::after), in the order of the plan
    [[nodiscard]] static QList<SceneSlot> sceneOrder(int written,
                                                     const QList<core::PlannedScene>& planned);

    /// @brief The plan @p planned in order @p order (sceneOrder() rearranged): each scene in
    /// the plan comes after the scenes of the text before it; without them in @p order (the
    /// scenes in the plans only), the scenes keep the places in the text the plan had, in
    /// their new order
    [[nodiscard]] static QList<core::PlannedScene> planInOrder(
        const QList<SceneSlot>& order, const QList<core::PlannedScene>& planned);

    /// @brief The name of scene @p scene (from 0) on its card: "Scene 1"
    [[nodiscard]] static QString sceneName(int scene);

    /// @brief "In the Plan": the name and the state of a scene in the plan, among the scenes
    /// of its text
    [[nodiscard]] static QString plannedName();

    /// @brief "3 scenes" in the program's language
    [[nodiscard]] static QString scenesText(int scenes);

    /// @brief The place of the main section @p steps places before (less than 0) or after the
    /// place of text @p elementId, among the places its kind can be; as far as it can go
    /// @return nullopt at the start or the end of the main section, and for an element that is
    ///         not a text of it
    [[nodiscard]] static std::optional<core::ElementPlace> stepPlace(
        const core::ProjectManager& book, const QString& elementId, int steps);

    /// @brief The place right after text @p anchorId (or right before it, @p after false) in
    /// its list, for text @p elementId
    /// @return nullopt when the kind of @p elementId cannot be there, or one is not a text of
    ///         the book
    [[nodiscard]] static std::optional<core::ElementPlace> placeBeside(
        const core::ProjectManager& book, const QString& elementId, const QString& anchorId,
        bool after);

    /// @brief The first place (or the last, @p atEnd) of part @p partId, for text @p elementId
    /// @return nullopt when the kind of @p elementId cannot be there
    [[nodiscard]] static std::optional<core::ElementPlace> placeInPart(
        const core::ProjectManager& book, const QString& elementId, const QString& partId,
        bool atEnd);

    /// @brief The titles of the characters of the Workshop, sorted: points of view to choose
    [[nodiscard]] static QStringList povSuggestions(const core::ProjectManager& book);

    /// @brief The titles of the places of the Workshop, sorted: places of the action to choose
    [[nodiscard]] static QStringList placeSuggestions(const core::ProjectManager& book);

    /// @brief The name of status @p status ("draft"...) in the program's language
    [[nodiscard]] static QString statusName(const QString& status);

    /// @brief "4,210 words" in the program's language
    [[nodiscard]] static QString wordsText(int words);

    /// @brief "7 texts" in the program's language
    [[nodiscard]] static QString textsText(int texts);
};

/// @brief The colors of the cards of the plot board in the theme of the program
///
/// The theme files have them with the other colors of the editor (core::ThemeManager::
/// editorColor()): the colors of the three states, and the six colors the writer gives a card
/// or that the points of view take in turn; and the two backgrounds of the board.
struct PlotColors {
    QColor draft;     ///< A text in the draft state
    QColor revision;  ///< A text in revision
    QColor finished;  ///< A finished text
    QColor planned;   ///< A scene in the plan of its text
    QList<QColor> own;  ///< The writer's colors, in the order of ownNames()
    QColor cork;        ///< The cork under the cards (PlotBoard::BACKGROUND_CORK)
    QColor background;  ///< The writer's background of the board (PlotBoard::BACKGROUND_OWN)

    /// @brief The key of the cork in the theme files
    static constexpr const char* CORK_KEY = "plotCork";
    /// @brief The key of the writer's background of the board in the theme files
    static constexpr const char* BACKGROUND_KEY = "plotBackground";

    /// @brief The keys of the colors of the cards in the theme files: "plotDraft"...
    [[nodiscard]] static const std::vector<std::string>& keys();

    /// @brief The names of the writer's colors in the .klh file (core::ElementPlot::color):
    /// "blue", "teal", "green", "amber", "rose", "violet"
    [[nodiscard]] static const QStringList& ownNames();

    /// @brief The name of own color @p name in the program's language; empty for no name
    [[nodiscard]] static QString ownTitle(const QString& name);

    /// @brief The colors of the theme of the program
    [[nodiscard]] static PlotColors current();

    /// @brief The color of status @p status; invalid for no status
    [[nodiscard]] QColor ofStatus(const QString& status) const;

    /// @brief The color of the state of card @p card: of a scene in the plan, planned
    [[nodiscard]] QColor ofStatus(const PlotCard& card) const;

    /// @brief The writer's color named @p name; invalid for no color
    [[nodiscard]] QColor ofOwn(const QString& name) const;

    /// @brief The color of point of view @p pov among the points of view @p povs of the book
    /// (povsOf()): the writer's colors in turn; invalid for no point of view
    [[nodiscard]] QColor ofPov(const QString& pov, const QStringList& povs) const;

    /// @brief The color of card @p card as @p colorBy (PlotBoard::COLOR_STATUS...) says;
    /// invalid: the card has none
    [[nodiscard]] QColor ofCard(const PlotCard& card, const QString& colorBy,
                                const QStringList& povs) const;
};

/// @brief The points of view of @p cards, each once, sorted: each keeps its color when the
/// cards move
[[nodiscard]] QStringList povsOf(const QList<PlotCard>& cards);

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PLOT_BOARD_H
