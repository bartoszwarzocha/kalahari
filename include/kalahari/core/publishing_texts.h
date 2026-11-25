// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file publishing_texts.h
/// @brief The texts for publishers: a sentence about the book and a synopsis, each in versions,
///        with a template and a target length, and the set of texts sent to a publisher

#ifndef KALAHARI_CORE_PUBLISHING_TEXTS_H
#define KALAHARI_CORE_PUBLISHING_TEXTS_H

#include <kalahari/core/localized_text.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <optional>
#include <string>
#include <utility>

namespace kalahari::core {

struct BookProject;
struct ProjectBook;
struct ProjectElement;

/// @brief What a text for publishers is, saved with the element as "publishing", apart from
/// its text
///
/// In the .klh file:
/// @code
/// "publishing": { "part": "pitch", "state": "chosen",
///                 "template": "kalahari.novel:when_must",
///                 "fields": { "event": "studnia na szlaku okazuje się sucha",
///                             "hero": "młoda kartografka Lejla" },
///                 "manual": true, "done": ["hero", "event"],
///                 "targetCharacters": 0, "targetWords": 0 }
/// @endcode
/// The text itself is the text of the element, as that of any material.
struct PublishingData {
    QString part;        ///< PublishingTexts::PITCH or SYNOPSIS; empty: no text for publishers
    QString state;       ///< PublishingTexts::CHOSEN or REJECTED; empty: a draft
    QString templateId;  ///< Its template (PublishingTemplate::reference()); empty: none
    QMap<QString, QString> fields;  ///< Of a sentence: the values of the fields of the
                                    ///< pattern of its template, by the ids of the fields
    bool manual = false;  ///< Of a sentence: changed by hand, so the fields no longer make it
    QStringList done;     ///< The points of its template the writer has written, by their ids
    int targetCharacters = 0;  ///< Its own target length in characters with spaces; 0: none
    int targetWords = 0;       ///< Its own target length in words; 0: none
    QJsonObject extra;         ///< Fields this version does not know, saved back

    /// @brief Whether it says nothing: the element is not a text for publishers
    [[nodiscard]] bool isEmpty() const;

    /// @brief "publishing" of the .klh file; an empty object when it says nothing
    [[nodiscard]] QJsonObject toJson() const;

    /// @brief Read "publishing" of the .klh file
    /// @param field Its path in the file, for the problems: "workshop.elements[3].publishing"
    /// @param problems Gets one line per problem: "<field>: <what is wrong>"
    static PublishingData fromJson(const QJsonObject& json, const QString& field,
                                   QStringList& problems);

    bool operator==(const PublishingData&) const = default;
};

/// @brief A point of a template of a synopsis: what to write about
struct PublishingTemplatePoint {
    QString id;          ///< Unique in its template; of a point of each text, the text's id
    LocalizedText name;  ///< "Bohater i świat"
    LocalizedText hint;  ///< "Kim jest bohater, gdzie i kiedy toczy się akcja."

    bool operator==(const PublishingTemplatePoint&) const = default;
};

/// @brief A field of the pattern of a template of a sentence about the book
struct PublishingTemplateField {
    QString id;          ///< As the pattern names it: {hero}
    LocalizedText name;  ///< "bohater"
    LocalizedText hint;  ///< "kim jest bohater"

    bool operator==(const PublishingTemplateField&) const = default;
};

/// @brief A template of a text for publishers: of a book type, or the writer's own
///
/// A template of a synopsis has its points, or a point for each text of the main part of the
/// book (eachText) with a target length for each of them. A template of a sentence has its
/// pattern, whose fields the writer fills in and which the sentence is made of.
///
/// In a package (booktype.json, "publishingTemplates"):
/// @code{.json}
/// { "id": "one_page", "part": "synopsis",
///   "name": { "pl": "Jedna strona – pięć kroków", "en": "One Page – Five Steps" },
///   "target": { "characters": 1800, "words": 300 },
///   "points": [ { "id": "hero", "name": { "pl": "Bohater i świat", "en": "..." },
///                 "hint": { "pl": "Kim jest bohater...", "en": "..." } } ] }
/// { "id": "each_chapter", "part": "synopsis", "eachText": true,
///   "name": { ... }, "hint": { "pl": "Dwa, trzy zdania: co się dzieje...", "en": "..." },
///   "target": { "characters": 600, "words": 100 } }
/// { "id": "when_must", "part": "pitch", "name": { ... },
///   "pattern": { "pl": "Gdy {event}, {hero} musi {goal}, zanim {stakes}.", "en": "..." },
///   "fields": [ { "id": "event", "name": { ... }, "hint": { ... } } ],
///   "target": { "characters": 200, "words": 35 } }
/// @endcode
/// The writer's own templates are in the settings (PublishingTexts::TEMPLATES_SETTING), with
/// one text each in place of the texts by language, and the book types they are for:
/// @code{.json}
/// { "id": "t1", "part": "synopsis", "name": "Konkurs „Złote Pióro”",
///   "types": ["kalahari.novel"], "target": { "characters": 1000, "words": 170 },
///   "points": [ { "id": "p1", "name": "Bohater i świat", "hint": "Dwa zdania." } ] }
/// @endcode
struct PublishingTemplate {
    QString package;     ///< Id of its package; empty: the writer's own template
    QString id;          ///< Unique among the templates of its package, or of the writer
    QString part;        ///< PublishingTexts::PITCH or SYNOPSIS
    LocalizedText name;  ///< "Jedna strona – pięć kroków"
    int targetCharacters = 0;  ///< Target length in characters with spaces (of each text)
    int targetWords = 0;       ///< Target length in words (of each text)
    bool eachText = false;     ///< Of a synopsis: a point for each text of the main part
    LocalizedText hint;        ///< Of the point of each text (eachText)
    QList<PublishingTemplatePoint> points;  ///< Of a synopsis, in order
    LocalizedText pattern;                  ///< Of a sentence: "Gdy {event}, {hero} musi..."
    QList<PublishingTemplateField> fields;  ///< Of a sentence: the fields of the pattern
    QStringList types;  ///< Of the writer's own template: the book types it is for; empty: all
    QJsonObject extra;  ///< Of the writer's own template: fields this version does not know

    /// @brief Whether it is the writer's own template, not one of a package
    [[nodiscard]] bool isOwn() const { return package.isEmpty(); }

    /// @brief How a text names it (PublishingData::templateId): "kalahari.novel:one_page", or
    /// "user:t1" for the writer's own template
    [[nodiscard]] QString reference() const;

    /// @brief Whether it is for a book of type @p typeId: a template of a package always, the
    /// writer's own one when it names the type or no type
    [[nodiscard]] bool isFor(const QString& typeId) const;

    /// @brief Its points for a book whose main part has the texts @p texts (their ids and
    /// titles): its own points, or one for each text, named with the text's title
    [[nodiscard]] QList<PublishingTemplatePoint> pointsFor(
        const QList<std::pair<QString, QString>>& texts) const;

    /// @brief Read a template of a package ("publishingTemplates" of booktype.json)
    /// @param field Its path in the file, for the problems: "publishingTemplates[0]"
    /// @param problems Gets one line per problem: "<field>: <what is wrong>"
    /// @return The template, or nullopt when it has problems
    static std::optional<PublishingTemplate> fromPackageJson(const QJsonObject& json,
                                                             const QString& packageId,
                                                             const QString& field,
                                                             QStringList& problems);

    /// @brief Read the templates of a package ("publishingTemplates" of booktype.json), each
    /// id once; none when it has none
    /// @param problems Gets one line per problem: "<field>: <what is wrong>"
    static QList<PublishingTemplate> listFromPackageJson(const QJsonValue& value,
                                                         const QString& packageId,
                                                         QStringList& problems);

    /// @brief Read the writer's own template, as the settings keep it; nullopt for one that
    /// has no id or no part
    static std::optional<PublishingTemplate> fromOwnJson(const QJsonObject& json);

    /// @brief The writer's own template as the settings keep it
    [[nodiscard]] QJsonObject toOwnJson() const;

    bool operator==(const PublishingTemplate&) const = default;
};

/// @brief The set of texts for a publisher of a book: which texts, in which order and which
/// version of each, saved with the book as "publishingSet"
///
/// @code
/// "publishingSet": { "order": ["pitch", "tagline", "blurb", "synopsis", "aboutAuthor"],
///                    "off": ["tagline"], "picks": { "blurb": "<id of a material>" } }
/// @endcode
/// A part of the set is the version chosen for it: a sentence or a synopsis chosen for the set
/// (PublishingTexts::CHOSEN), a description or a tagline chosen for the cover, the note about
/// the author of the book's back part. A pick names another version for the set alone.
struct PublishingSet {
    QStringList order;  ///< The parts (PublishingTexts::setParts()) in the writer's order
    QStringList off;    ///< The parts left out of the set
    QMap<QString, QString> picks;  ///< Part -> the element of the set in place of the chosen
    QJsonObject extra;             ///< Fields this version does not know, saved back

    /// @brief Its parts in its order: those of order() that the program knows, then those it
    /// does not have, in the order of PublishingTexts::setParts()
    [[nodiscard]] QStringList parts() const;

    /// @brief Whether part @p part is in the set (not off)
    [[nodiscard]] bool includes(const QString& part) const;

    /// @brief The set with part @p part in it (@p on) or left out
    [[nodiscard]] PublishingSet withPart(const QString& part, bool on) const;

    /// @brief The set with part @p part moved by @p step places in its order (-1: up, 1:
    /// down); the same set when the part is at that end or unknown
    [[nodiscard]] PublishingSet withPartMoved(const QString& part, int step) const;

    /// @brief The set of a new book: all parts, in the order of the settings
    /// (PublishingTexts::SET_SETTING), those not in the settings left out
    static PublishingSet defaults();

    /// @brief "publishingSet" of the .klh file
    [[nodiscard]] QJsonObject toJson() const;

    /// @brief Read "publishingSet" of the .klh file
    /// @param field Its path in the file, for the problems: "books[0].publishingSet"
    /// @param problems Gets one line per problem: "<field>: <what is wrong>"
    static PublishingSet fromJson(const QJsonObject& json, const QString& field,
                                  QStringList& problems);

    bool operator==(const PublishingSet&) const = default;
};

/// @brief A part of the set of a book: what it is and which element gives its text
struct PublishingSetPart {
    QString part;       ///< PublishingTexts::setParts()
    QString elementId;  ///< The element of its text; empty: the book has none
    bool on = true;     ///< Whether it is in the set (PublishingSet::includes())
    bool picked = false;  ///< Whether the element is the set's pick, not the chosen version

    bool operator==(const PublishingSetPart&) const = default;
};

/// @brief A part of the set as it is copied or exported: its heading and its text
struct PublishingSetText {
    QString heading;  ///< "Zdanie o książce"
    QString text;     ///< Paragraphs separated by a new line

    bool operator==(const PublishingSetText&) const = default;
};

/// @brief The parts, states, settings and lengths of the texts for publishers
struct PublishingTexts {
    /// @name Parts: what a text for publishers is
    /// @{
    static constexpr const char* PITCH = "pitch";        ///< One sentence about the book
    static constexpr const char* SYNOPSIS = "synopsis";  ///< The whole story with its end
    /// @}

    /// @name Parts of the set that come from elsewhere
    /// @{
    static constexpr const char* TAGLINE = "tagline";  ///< From the cover (CoverMaterial)
    static constexpr const char* BLURB = "blurb";      ///< From the cover (CoverMaterial)
    static constexpr const char* ABOUT_AUTHOR = "aboutAuthor";  ///< The note in the back part
    /// @}

    /// @name States; a draft has none
    /// @{
    static constexpr const char* CHOSEN = "chosen";      ///< Chosen for the set
    static constexpr const char* REJECTED = "rejected";  ///< Rejected
    /// @}

    /// @brief The kind of the texts for publishers in the base package
    static constexpr const char* KIND = "publishing";

    /// @brief How a text names the writer's own template: "user:t1"
    static constexpr const char* OWN_TEMPLATE_PREFIX = "user";

    /// @brief The kind of the note about the author of the back part of a book
    static constexpr const char* ABOUT_AUTHOR_KIND = "about_author";

    /// @brief Characters with spaces of a standard page, by which publishers measure a
    /// synopsis
    static constexpr int PAGE_CHARACTERS = 1800;

    /// @brief The settings of the default target lengths of the texts without a template, one
    /// for each part and each unit: this, the part and the unit,
    /// "materials.publishing.targets.synopsis.characters"
    static constexpr const char* TARGETS_SETTING = "materials.publishing.targets";

    /// @brief The setting of whether the length of a synopsis is also given in standard
    /// pages (PAGE_CHARACTERS)
    static constexpr const char* PAGES_SETTING = "materials.publishing.pages";

    /// @brief The setting of whether the fragments of a synopsis linked to the points of its
    /// template have a bar beside their lines on the left margin
    static constexpr const char* POINT_BARS_SETTING = "materials.publishing.pointBars";

    /// @brief The setting of whether copying and exporting the set warn of the points of its
    /// synopsis with no fragment in its text
    static constexpr const char* POINT_WARNINGS_SETTING = "materials.publishing.pointWarnings";

    /// @brief The setting of the parts of the set of a new book, in its order
    static constexpr const char* SET_SETTING = "materials.publishing.set";

    /// @brief The setting of the writer's own templates (PublishingTemplate::fromOwnJson())
    static constexpr const char* TEMPLATES_SETTING = "materials.publishing.templates";

    /// @brief The parts of a text for publishers: a sentence and a synopsis
    static const QStringList& parts();

    /// @brief The parts of a set, in the order of a new set
    static const QStringList& setParts();

    /// @brief Whether a text of @p part has a template with points (a synopsis)
    static bool hasPoints(const QString& part);

    /// @brief Whether a text of @p part has a template with a pattern (a sentence)
    static bool hasPattern(const QString& part);

    /// @brief Whether the length of a synopsis is also given in standard pages (PAGES_SETTING)
    static bool showsPages();

    /// @brief Whether the fragments linked to the points have a bar on the margin
    ///        (POINT_BARS_SETTING)
    static bool showsPointBars();

    /// @brief Whether copying and exporting the set warn of the points with no fragment
    ///        (POINT_WARNINGS_SETTING)
    static bool warnsOfPoints();

    /// @brief The ids of the points of the text @p kml has fragments of: <points ids> with
    ///        text in it (editor/text_points.h)
    static QSet<QString> linkedPointsOf(const QString& kml);

    /// @brief The setting of the default target length of @p part in words or in characters
    static std::string targetSettingOf(const QString& part, bool words);

    /// @brief The default target length of @p part in words or in characters
    /// (TARGETS_SETTING): 200 characters (35 words) for a sentence, 1800 (300) for a
    /// synopsis; 0 for another part
    static int defaultTargetOf(const QString& part, bool words);

    /// @brief The target length of text @p data in words or in characters: its own, else that
    /// of its template @p templ (of each text times @p texts, the texts of the main part, for a
    /// template of each text), else the default of its part; 0: none
    static int targetOf(const PublishingData& data, const PublishingTemplate* templ, int texts,
                        bool words);

    /// @brief The standard pages of @p characters characters with spaces, to one decimal
    static double pagesOf(int characters);

    /// @brief The sentence of @p pattern with the values of its fields: each {field} becomes
    /// its value without spaces at its ends, "…" while it is empty; the spaces run together,
    /// and the sentence starts with a capital letter
    static QString composePitch(const QString& pattern, const QMap<QString, QString>& fields);

    /// @brief The ids of the fields @p pattern names ({hero}), each once, in order
    static QStringList fieldsOf(const QString& pattern);

    /// @brief The writer's own templates (TEMPLATES_SETTING), each id once
    static QList<PublishingTemplate> ownTemplates();

    /// @brief Keep @p templates as the writer's own templates (TEMPLATES_SETTING)
    static void setOwnTemplates(const QList<PublishingTemplate>& templates);

    /// @brief A new id for one of the writer's own templates, unique among @p templates
    static QString newOwnTemplateId(const QList<PublishingTemplate>& templates);

    /// @brief The texts of @p part (PITCH, SYNOPSIS) of the Workshop of @p project, in the order
    /// of the Workshop
    static QList<const ProjectElement*> textsOf(const BookProject& project, const QString& part);

    /// @brief Give text @p elementId of @p project the state @p state (CHOSEN, REJECTED; empty:
    /// a draft); one chosen takes the place of the text of its part chosen before for its
    /// volume of a series (VolumeMaterials)
    /// @return The texts that are drafts again
    static QStringList setState(BookProject& project, const QString& elementId,
                                const QString& state);

    /// @brief The note about the author (ABOUT_AUTHOR_KIND) of the back part of @p book, the
    /// first one; nullptr: none
    static const ProjectElement* aboutAuthorOf(const ProjectBook& book);

    /// @brief The versions that can be the part @p part of a set (setParts()): the texts of the
    /// part, or the materials of the cover of a description or a tagline, in the order of the
    /// Workshop; the note about the author has none
    /// @param book Only the versions for this book of a series: of its volume and of the whole
    ///        series (VolumeMaterials::isFor()); nullptr: all
    static QList<const ProjectElement*> versionsOf(const BookProject& project,
                                                   const QString& part,
                                                   const ProjectBook* book = nullptr);

    /// @brief The parts of set @p set of book @p book of @p project, in its order, each with
    /// the element of its text: the set's pick while it is a version of the part for the book,
    /// else the version chosen (for the back cover, for the front, for a tagline and a
    /// description of more places; of the book's volume of a series first, else of the whole
    /// series), else the first note about the author of the book's back part
    static QList<PublishingSetPart> resolveSet(const BookProject& project,
                                               const ProjectBook& book,
                                               const PublishingSet& set);

    /// @brief The set as plain text: @p title, @p subtitle, then each part's heading in
    /// capitals and its text, with an empty line between them
    static QString setToPlainText(const QString& title, const QString& subtitle,
                                  const QList<PublishingSetText>& parts);

    /// @brief The set in Markdown: "# title", the subtitle, then "## heading" and the text of
    /// each part
    static QString setToMarkdown(const QString& title, const QString& subtitle,
                                 const QList<PublishingSetText>& parts);

    /// @brief The length of @p text in the unit of the materials of the cover: its words, or
    /// its characters with spaces (CoverMaterial::countsWords())
    static int lengthOf(QStringView text, bool words);
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_PUBLISHING_TEXTS_H
