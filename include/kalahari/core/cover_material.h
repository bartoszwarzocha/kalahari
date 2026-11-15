// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cover_material.h
/// @brief The materials of the cover (MaterialKinds::COVER): what each is and where it goes

#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <string>

namespace kalahari {
namespace core {

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
    bool isEmpty() const;

    bool operator==(const CoverData&) const = default;
};

/// @brief The parts, places, states and lengths of the materials of the cover
struct CoverMaterial {
    /// @name Parts: what a material of the cover is
    /// @{
    static constexpr const char* BLURB = "blurb";          ///< Text on the back or a flap
    static constexpr const char* TAGLINE = "tagline";      ///< A short sentence on the front
    static constexpr const char* OPINION = "opinion";      ///< Words of a critic, a writer...
    static constexpr const char* DESIGN = "design";        ///< A design or a sketch of it
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

    /// @brief The parts, in the order of the lists: the texts, the opinions, the pictures and
    /// the notes
    static const QStringList& parts();

    /// @brief The places on the cover, in the order of the menus
    static const QStringList& places();

    /// @brief Whether a material of @p part is a text written in the program: a description,
    /// a tagline, an opinion or a note
    static bool isText(const QString& part);

    /// @brief Whether a material of @p part is a picture: a design or a reference picture
    static bool isPicture(const QString& part);

    /// @brief Whether the text of @p part has a target length: a description, a tagline and
    /// an opinion
    static bool hasTarget(const QString& part);

    /// @brief Whether a material of @p part has a place on the cover: not a reference picture
    /// and not a note
    static bool goesOnCover(const QString& part);

    /// @brief Whether one material of @p part is chosen for a place: a description, a tagline
    /// and a design; a place has many opinions
    static bool choosesOne(const QString& part);

    /// @brief The place a new material of @p part gets: the back for a description and an
    /// opinion, the front for a tagline, the whole cover for a design; none for the others
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
    /// 200 (35) for an opinion; 0 for a part without a target
    static int defaultTargetOf(const QString& part, bool words);

    /// @brief The target length of @p cover in words or in characters: its own, else the
    /// default of its part; 0: none
    static int targetOf(const CoverData& cover, bool words);

    /// @brief The length of @p text: its words, or its characters with spaces
    static int lengthOf(QStringView text, bool words);
};

} // namespace core
} // namespace kalahari
