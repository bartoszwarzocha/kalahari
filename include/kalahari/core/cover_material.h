// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file cover_material.h
/// @brief The materials of the cover (MaterialKinds::COVER): what each is and where it goes

#ifndef KALAHARI_CORE_COVER_MATERIAL_H
#define KALAHARI_CORE_COVER_MATERIAL_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <string>

namespace kalahari::core {

/// @brief What a material of the cover is and where it goes, saved with the material as
/// "cover", apart from the writer's own fields
///
/// In the .klh file:
/// @code
/// "cover": { "part": "opinion", "place": "back", "state": "chosen",
///            "author": "Anna Wiśniewska", "role": "krytyczka literacka",
///            "source": "kwartalnik „Wydmy”, nr 3/2026", "date": "2026-10-02",
///            "consent": "granted" }
/// @endcode
/// The text of a description, a tagline, an opinion or a note is the description of the
/// material, as the words of a quote are.
struct CoverData {
    QString part;       ///< CoverMaterial::BLURB...; empty: a material that is not of the cover
    QString place;      ///< CoverMaterial::FRONT...; empty: no place on the cover yet
    QString state;      ///< CoverMaterial::CHOSEN or REJECTED; empty: a draft
    QString author;     ///< Who wrote the opinion, made the design or the picture
    QString role;       ///< Of an opinion: who the author is, "krytyczka literacka"
    QString source;     ///< Of an opinion: where it comes from; of a picture: its source
    QString date;       ///< Of an opinion or a design: when, as the writer puts it
    QString consent;    ///< Of an opinion: CoverMaterial::GRANTED...; empty: not said
    QString license;    ///< Of a reference picture: its license, "CC BY 4.0"
    QString about;      ///< Of a reference picture: what it shows for the cover, "colors"
    int targetCharacters = 0;  ///< Target length of its text in characters with spaces;
                               ///< 0: the default of its part
    int targetWords = 0;       ///< Target length of its text in words; 0: the default
    QJsonObject extra;  ///< Fields this version does not know, saved back

    /// @brief Whether it says nothing: the material is not a material of the cover
    [[nodiscard]] bool isEmpty() const;

    bool operator==(const CoverData&) const = default;
};

struct CoverParts;

/// @brief The parts, places, states and lengths of the materials of the cover
struct CoverMaterial {
    /// @name Parts: what a material of the cover is
    /// @{
    static constexpr const char* BLURB = "blurb";          ///< Text on the back or a flap
    static constexpr const char* TAGLINE = "tagline";      ///< A short sentence on the front
    static constexpr const char* OPINION = "opinion";      ///< Words of a critic, a writer...
    static constexpr const char* BIO = "bio";              ///< A note about the author for a
                                                           ///< flap or the back
    static constexpr const char* DESIGN = "design";        ///< A design or a sketch of it
    static constexpr const char* AUTHOR_PHOTO = "authorPhoto";  ///< A portrait of the author
    static constexpr const char* REFERENCE = "reference";  ///< A picture of mood, colors, type
    static constexpr const char* NOTE = "note";            ///< Ideas and remarks
    /// @}

    /// @name Places on the cover
    /// @{
    static constexpr const char* FRONT = "front";
    static constexpr const char* SPINE = "spine";
    static constexpr const char* BACK = "back";
    static constexpr const char* FRONT_FLAP = "frontFlap";
    static constexpr const char* BACK_FLAP = "backFlap";
    static constexpr const char* WHOLE = "whole";  ///< The whole cover: a design
    /// @}

    /// @name Places on the jacket (CoverParts::jacket), which always has flaps
    /// @{
    static constexpr const char* JACKET_FRONT = "jacketFront";
    static constexpr const char* JACKET_SPINE = "jacketSpine";
    static constexpr const char* JACKET_BACK = "jacketBack";
    static constexpr const char* JACKET_FRONT_FLAP = "jacketFrontFlap";
    static constexpr const char* JACKET_BACK_FLAP = "jacketBackFlap";
    static constexpr const char* JACKET_WHOLE = "jacketWhole";  ///< The whole jacket: a design
    /// @}

    /// @name States; a draft has none
    /// @{
    static constexpr const char* CHOSEN = "chosen";      ///< Chosen for the cover
    static constexpr const char* REJECTED = "rejected";  ///< Rejected
    /// @}

    /// @name Consent to print an opinion
    /// @{
    static constexpr const char* GRANTED = "granted";
    static constexpr const char* ASKED = "asked";      ///< Asked for, no answer yet
    static constexpr const char* REFUSED = "refused";  ///< None
    /// @}

    /// @brief The setting of the unit of the lengths of the texts: "characters" (with
    /// spaces, as publishers count them) or "words"
    static constexpr const char* UNIT_SETTING = "materials.cover.lengthUnit";

    /// @brief The settings of the default target lengths, one for each part with a target and
    /// each unit: this, the part and the unit, "materials.cover.targets.blurb.characters"
    static constexpr const char* TARGETS_SETTING = "materials.cover.targets";

    /// @brief The parts, in the order of the lists: the texts, the opinions, the notes about
    /// the author, the pictures and the notes
    static const QStringList& parts();

    /// @brief The places on the cover, then those on the jacket, in the order of the menus
    static const QStringList& places();

    /// @brief The places of the cover itself, in the order of the menus
    static const QStringList& coverPlaces();

    /// @brief The places of the jacket, in the order of the menus
    static const QStringList& jacketPlaces();

    /// @brief Whether @p place is on the jacket
    static bool isJacketPlace(const QString& place);

    /// @brief Whether @p place is a flap of the cover itself (CoverParts::flaps); the flaps of
    /// the jacket go with the jacket
    static bool isCoverFlap(const QString& place);

    /// @brief Whether @p place is the place of a design: the whole cover or the whole jacket
    static bool isWhole(const QString& place);

    /// @brief Whether @p place is on a part of the cover that @p parts has: the flaps of the
    /// cover with its flaps, the jacket with the jacket; any other place, also one this
    /// version does not know, is
    static bool isOn(const QString& place, const CoverParts& parts);

    /// @brief Whether a material of @p part is a text written in the program: a description,
    /// a tagline, an opinion, a note about the author or a note
    static bool isText(const QString& part);

    /// @brief Whether a material of @p part is a picture: a design, a photo of the author or a
    /// reference picture
    static bool isPicture(const QString& part);

    /// @brief Whether the text of @p part has a target length: a description, a tagline, an
    /// opinion and a note about the author
    static bool hasTarget(const QString& part);

    /// @brief Whether a material of @p part has a place on the cover: not a reference picture
    /// and not a note
    static bool goesOnCover(const QString& part);

    /// @brief Whether one material of @p part is chosen for a place: a description, a tagline,
    /// a note about the author, a photo of the author and a design; a place has many opinions
    static bool choosesOne(const QString& part);

    /// @brief The place a new material of @p part gets on a cover of @p parts: the back for a
    /// description and an opinion, the front for a tagline, the whole cover for a design; for
    /// a note about the author and a photo of the author the back flap of the jacket, else the
    /// back flap of the cover, else the back; none for the others
    static QString defaultPlaceOf(const QString& part, const CoverParts& parts);

    /// @brief The place a new material of @p part gets on a cover with its flaps, without a
    /// jacket
    static QString defaultPlaceOf(const QString& part);

    /// @brief The part of a material whose kind changes to @p part keeps its kind of
    /// material: whether @p from and @p to are both texts or both pictures
    static bool canChange(const QString& from, const QString& to);

    /// @brief Whether the lengths are counted in words (UNIT_SETTING), else in characters
    static bool countsWords();

    /// @brief The setting of the default target length of @p part in words or in characters
    static std::string targetSettingOf(const QString& part, bool words);

    /// @brief The default target length of @p part in words or in characters
    /// (TARGETS_SETTING): 600 characters (100 words) for a description, 60 (10) for a tagline,
    /// 200 (35) for an opinion, 500 (80) for a note about the author; 0 for a part without a
    /// target
    static int defaultTargetOf(const QString& part, bool words);

    /// @brief The target length of @p cover in words or in characters: its own, else the
    /// default of its part; 0: none
    static int targetOf(const CoverData& cover, bool words);

    /// @brief The length of @p text: its words, or its characters with spaces
    static int lengthOf(QStringView text, bool words);
};

/// @brief How an endpaper of a book is filled: a spread of two pages, the one pasted to the
/// cover and the free leaf
///
/// In the .klh file, in "coverParts":
/// @code
/// "frontEndpaper": { "fill": "pattern", "pattern": "diagonal",
///                    "colors": ["#9a5a2e", "#efe2c2"] }
/// @endcode
struct Endpaper {
    /// @name Fills; none: white paper
    /// @{
    static constexpr const char* PICTURE = "picture";  ///< One picture over both pages
    static constexpr const char* TILE = "tile";        ///< A picture repeated like a wallpaper
    static constexpr const char* PATTERN = "pattern";  ///< A pattern of the program in two colors
    static constexpr const char* COLOR = "color";      ///< One color
    /// @}

    /// @name The patterns of the program
    /// @{
    static constexpr const char* DOTS = "dots";
    static constexpr const char* STRIPES = "stripes";
    static constexpr const char* GRID = "grid";
    static constexpr const char* DIAGONAL = "diagonal";
    static constexpr const char* CHECKER = "checker";
    static constexpr const char* WAVES = "waves";
    /// @}

    /// @brief The size of a repeated picture at first, in percent of the width of a page
    static constexpr int DEFAULT_SCALE = 25;
    static constexpr int MIN_SCALE = 5;
    static constexpr int MAX_SCALE = 100;

    QString fill;            ///< PICTURE...; empty: none
    QString picture;         ///< The material whose picture fills it (PICTURE, TILE)
    int scale = DEFAULT_SCALE;  ///< Of TILE: the width of the picture in percent of a page
    QString pattern;         ///< Of PATTERN: DOTS...; empty: DOTS
    QString patternColor;    ///< Of PATTERN: "#RRGGBB"; empty: the theme's
    QString paperColor;      ///< Of PATTERN, under it: "#RRGGBB"; empty: the theme's
    QString color;           ///< Of COLOR: "#RRGGBB"; empty: the theme's paper color
    bool sameAsFront = false;  ///< Of the back endpaper: filled as the front one
    QJsonObject extra;       ///< Fields this version does not know, saved back

    /// @brief The patterns, in the order of their buttons
    static const QStringList& patterns();

    /// @brief The fills, in the order of their list; the empty one, none, last
    static const QStringList& fills();

    /// @brief The pattern it shows: its own, DOTS when it has none or one this version does
    /// not know
    [[nodiscard]] QString shownPattern() const;

    [[nodiscard]] QJsonObject toJson() const;
    static Endpaper fromJson(const QJsonObject& json, const QString& field,
                             QStringList& problems);

    bool operator==(const Endpaper&) const = default;
};

/// @brief The parts of the cover a book has besides the cover itself, each on its own: the
/// flaps of the cover, the jacket and the endpapers; with how its endpapers are filled
///
/// In the .klh file, a field of the book:
/// @code
/// "coverParts": { "flaps": true, "jacket": true, "endpapers": true,
///                 "frontEndpaper": { "fill": "picture", "picture": "m-17" },
///                 "backEndpaper": { "sameAsFront": true } }
/// @endcode
/// A book without it has the flaps of the cover and neither a jacket nor endpapers. Turning a
/// part off keeps its materials and its endpapers: they come back when it is on again.
struct CoverParts {
    bool flaps = true;       ///< The flaps of the cover itself
    bool jacket = false;     ///< A jacket: front, spine, back and two flaps
    bool endpapers = false;  ///< The front and the back endpaper
    Endpaper front;          ///< The front endpaper, between the cover and the first page
    Endpaper back;           ///< The back endpaper, between the last page and the cover
    QJsonObject extra;       ///< Fields this version does not know, saved back

    /// @brief How the back endpaper is filled: as the front one when it says so
    [[nodiscard]] const Endpaper& backShown() const { return back.sameAsFront ? front : back; }

    [[nodiscard]] QJsonObject toJson() const;
    static CoverParts fromJson(const QJsonObject& json, const QString& field,
                               QStringList& problems);

    bool operator==(const CoverParts&) const = default;
};

} // namespace kalahari::core

#endif  // KALAHARI_CORE_COVER_MATERIAL_H
