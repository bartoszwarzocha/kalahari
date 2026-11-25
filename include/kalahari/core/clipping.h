// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file clipping.h
/// @brief Clippings: fragments cut from the texts to the Workshop (Edit > Cut to Clippings),
///        where each comes from and where it went back

#ifndef KALAHARI_CORE_CLIPPING_H
#define KALAHARI_CORE_CLIPPING_H

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace kalahari::core {

/// @brief Where a clipping was cut from: the text, the place in it and when
///
/// The place is the paragraph and the words around it: the last words before it and the
/// first words after it in that paragraph (each at most Clippings::PLACE_WORDS words), which
/// find it again when the paragraphs before it change (Clippings::placeIn()).
struct ClippingOrigin {
    QString element;     ///< The id of the text it was cut from
    QString title;       ///< The title of that text then, for a text deleted since
    int paragraph = 0;   ///< The paragraph of the place, from 1 (a block of the text: a
                         ///< heading, a paragraph, a line of a table); 0: none
    QString before;      ///< The last words of the paragraph before the place; empty: its start
    QString after;       ///< The first words of the paragraph after the place; empty: its end
    QDateTime cut;       ///< When it was cut
    QJsonObject extra;   ///< Fields this version does not know, saved back

    friend bool operator==(const ClippingOrigin&, const ClippingOrigin&) = default;
};

/// @brief Whether @p origin names no text: a clipping added by hand has no origin
[[nodiscard]] inline bool isEmpty(const ClippingOrigin& origin) {
    return origin.element.isEmpty();
}

/// @brief A time a clipping was inserted into a text
struct ClippingInsertion {
    QString element;    ///< The id of the text
    QDateTime at;       ///< When
    QJsonObject extra;  ///< Fields this version does not know, saved back

    friend bool operator==(const ClippingInsertion&, const ClippingInsertion&) = default;
};

/// @brief What a clipping knows besides its text, saved with it as "clipping"
///
/// In the .klh file:
/// @code
/// "clipping": {
///   "origin": { "element": "c91a44f0-...", "title": "Rozdział 2", "paragraph": 3,
///               "before": "długo patrzył na horyzont. ", "after": "Potem wstał.",
///               "cut": "2026-11-15T07:42:00Z" },
///   "inserted": [ { "element": "c91a44f0-...", "at": "2026-11-15T07:50:00Z" } ]
/// }
/// @endcode
struct ClippingData {
    ClippingOrigin origin;             ///< Where it comes from; empty: added by hand
    QList<ClippingInsertion> inserted;  ///< Where it was inserted, in the order of time
    QJsonObject extra;                  ///< Fields this version does not know, saved back

    friend bool operator==(const ClippingData&, const ClippingData&) = default;
};

/// @brief Whether @p data says nothing (no origin, never inserted)
[[nodiscard]] inline bool isEmpty(const ClippingData& data) {
    return isEmpty(data.origin) && data.inserted.isEmpty() && data.origin.extra.isEmpty() &&
           data.extra.isEmpty();
}

/// @brief A place in a text found again for a clipping (Clippings::placeIn())
struct ClippingPlace {
    int paragraph = 0;   ///< The paragraph, from 0 (the block of the text)
    int offset = 0;      ///< The character in it
    bool exact = false;  ///< The words around the place are as they were; false: only near it

    friend bool operator==(const ClippingPlace&, const ClippingPlace&) = default;
};

/// @brief The kind, the settings and the rules of the clippings
struct Clippings {
    /// @brief The kind of the clippings in the base package
    static constexpr const char* KIND = "clipping";

    /// @brief Words before and after the place of a clipping that find it again
    static constexpr int PLACE_WORDS = 8;

    /// @brief Words of the text of a clipping that its title has at most (titleOf())
    static constexpr int TITLE_WORDS = 6;

    /// @name Settings (Settings > Editor > Clippings, Statistics and Goals > Counting)
    /// @{
    /// What Cut to Clippings does: CUT_AT_ONCE, or CUT_WITH_WINDOW (the title and a note first)
    static constexpr const char* CUT_SETTING = "editor.clippings.cut";
    /// The cut goes at once, titled with its first words
    static constexpr const char* CUT_AT_ONCE = "atOnce";
    /// A small window first, with the title and a note
    static constexpr const char* CUT_WITH_WINDOW = "window";
    /// What Cut to Clippings does without a selection: NO_SELECTION_PARAGRAPH, the paragraph
    /// with the cursor, or NO_SELECTION_NOTHING (the command is greyed out)
    static constexpr const char* NO_SELECTION_SETTING = "editor.clippings.noSelection";
    /// Without a selection the paragraph with the cursor goes
    static constexpr const char* NO_SELECTION_PARAGRAPH = "paragraph";
    /// Without a selection the command is greyed out
    static constexpr const char* NO_SELECTION_NOTHING = "nothing";
    /// What Insert > Clipping opens: INSERT_WINDOW, the window with the preview, or
    /// INSERT_LIST, the list at the cursor
    static constexpr const char* INSERT_SETTING = "editor.clippings.insert";
    /// Insert > Clipping opens the window with the preview
    static constexpr const char* INSERT_WINDOW = "window";
    /// Insert > Clipping opens the list at the cursor
    static constexpr const char* INSERT_LIST = "list";
    /// What happens to a clipping inserted: AFTER_INSERT_KEEP, it stays marked as inserted,
    /// or AFTER_INSERT_REMOVE, it goes from the Workshop
    static constexpr const char* AFTER_INSERT_SETTING = "editor.clippings.afterInsert";
    /// A clipping inserted stays in the Workshop, marked as inserted
    static constexpr const char* AFTER_INSERT_KEEP = "keep";
    /// A clipping inserted goes from the Workshop
    static constexpr const char* AFTER_INSERT_REMOVE = "remove";
    /// Whether the editor shows scissors beside each paragraph a clipping was cut from (and
    /// not inserted since), as it shows the paperclips of the materials: on by default
    static constexpr const char* MARKS_SETTING = "editor.clippings.marks";
    /// Whether a cut to the clippings leaves the words of the day as they were, as it does by
    /// default (false: it lowers them as any deletion does with the counting "words added")
    static constexpr const char* KEEP_WORDS_SETTING = "stats.clippingsKeepWords";
    /// @}

    /// @brief The title of a clipping of @p text: its first TITLE_WORDS words on one line, with
    /// "…" when the text has more
    [[nodiscard]] static QString titleOf(const QString& text);

    /// @brief The last PLACE_WORDS words of @p text, from the start of the first of them to
    /// its end (spaces after them too); an object of the text (U+FFFC) ends them
    [[nodiscard]] static QString wordsBefore(const QString& text);

    /// @brief The first PLACE_WORDS words of @p text, from its start to the end of the last of
    /// them; an object of the text (U+FFFC) ends them
    [[nodiscard]] static QString wordsAfter(const QString& text);

    /// @brief The origin of a clipping cut at character @p offset of paragraph @p paragraph
    /// (from 0) of @p paragraphs, the texts of the paragraphs of the text after the cut
    [[nodiscard]] static ClippingOrigin originAt(const QString& element,
                                                 const QStringList& paragraphs, int paragraph,
                                                 int offset, const QDateTime& when);

    /// @brief Where the place of @p origin is in @p paragraphs, the texts of the paragraphs of
    /// its text now
    ///
    /// The place whose paragraph has the words before it and after it as they were, the one
    /// nearest to the paragraph of the origin; else a place with the words before it, or with
    /// the words after it (not exact); else the start of the paragraph of its number, or of
    /// the last paragraph (not exact).
    /// @return nullopt for a text without paragraphs
    [[nodiscard]] static std::optional<ClippingPlace> placeIn(const QStringList& paragraphs,
                                                              const ClippingOrigin& origin);
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_CLIPPING_H
