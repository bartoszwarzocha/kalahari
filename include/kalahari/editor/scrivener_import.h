// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file scrivener_import.h
/// @brief Import of a project of Scrivener (3): its binder, the texts of its draft with their
///        synopses, notes, labels, statuses, keywords and snapshots, and its research
///
/// A project is a folder Name.scriv with a file Name.scrivx (XML) of its binder: nested
/// BinderItem elements with their UUID, Type (DraftFolder, ResearchFolder, TrashFolder,
/// Folder, Text, PDF, Image...), Title and MetaData (LabelID, StatusID, IncludeInCompile).
/// The files of a document are in Files/Data/<UUID>/: its text content.rtf, its synopsis
/// synopsis.txt, its notes notes.rtf, its comments and linked footnotes content.comments, the
/// file of a document of research content.<extension>; its snapshots in
/// Snapshots/<UUID>.snapshots/ (index.xml and a file of RTF for each).
///
/// Scrivener does not publish its format: the reader follows what the programs that read its
/// projects agree on, and reads what is uncertain (the keywords, the comments) in the ways
/// they write it, without losing the text when a part is not as expected.

#pragma once

#include <kalahari/editor/book_import.h>

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <vector>

namespace kalahari::editor {

/// @brief A snapshot of a document: its text at a moment the writer kept
struct ScrivenerSnapshot {
    QString title;      ///< Its title; empty: none
    QDateTime created;  ///< When it was taken; invalid: unknown
    ImportDocument text;
};

/// @brief A document or a folder of the binder
struct ScrivenerItem {
    enum class Kind {
        Folder,  ///< A folder (with a text of its own, often empty)
        Text,    ///< A text (content.rtf)
        File,    ///< A file of research: a picture, a PDF, a page of the web (file)
    };

    Kind kind = Kind::Text;
    QString id;     ///< Its UUID
    QString type;   ///< Its type in the binder (Text, Folder, PDF, Image, WebArchive...)
    QString title;
    bool compiled = true;  ///< Include in Compile
    QString label;         ///< The name of its label; empty: none
    QString status;        ///< The name of its status; empty: none
    QStringList keywords;  ///< The names of its keywords
    QString synopsis;      ///< Its synopsis (plain text)
    QString notes;         ///< Its notes (plain text)
    ImportDocument text;   ///< Its text, with its comments and footnotes
    std::vector<ScrivenerSnapshot> snapshots;  ///< From the oldest
    QString file;          ///< Kind::File: the path of its file; empty: none
    std::vector<ScrivenerItem> children;

    /// @brief Whether it has a text: a paragraph with a character, a picture or a table
    bool hasText() const;
};

/// @brief A project of Scrivener
struct ScrivenerProject {
    QString title;      ///< Its title (of its properties, else of its folder)
    QString author;     ///< Its author (of its properties); empty: none
    QString draftName;  ///< The title of its draft ("Draft", "Manuscript")
    std::vector<ScrivenerItem> draft;     ///< The documents of the draft
    std::vector<ScrivenerItem> research;  ///< The documents of the research
    int trash = 0;      ///< The documents in the Trash (left out)
};

/// @brief The project folder or its .scrivx file @p path: the .scrivx file; empty: none
QString scrivenerProjectFile(const QString& path);

/// @brief Read a project of Scrivener: its folder Name.scriv or its file Name.scrivx
///
/// The documents of the draft and of the research with their texts and metadata, the
/// research's files; the Trash and the other top folders are left out.
/// @return false, with what went wrong in @p problem, when it cannot be read: no .scrivx file
///         (Kind::NotScrivener), a file that cannot be opened (CannotOpen) or is not XML of
///         a binder (Damaged)
bool readScrivenerProject(const QString& path, ScrivenerProject* project, ImportProblem* problem);

// =============================================================================
// The texts of the draft
// =============================================================================

/// @brief The status of the book a status of Scrivener is: "draft" (To Do, First Draft),
///        "revision" (Revised Draft), "final" (Final Draft, Done); empty: none it knows
QString scrivenerBookStatus(const QString& status);

/// @brief A document of a text made of several (a scene), with what it says of itself
struct ScrivenerScene {
    QString title;
    QString synopsis;
    QString notes;
    QString label;
    QString status;
    QStringList keywords;
};

/// @brief A text or a part the draft makes, with its metadata
struct ScrivenerText {
    /// Its place in ScrivenerDraft::document, its title and its words; a part's own text is a
    /// text of the part after it, with the part's title and no metadata
    ImportPiece piece;
    QString synopsis;
    QString notes;
    QString label;
    QString status;      ///< Of Scrivener; of a text of scenes without one, the one of its
                         ///< scenes that is the least done
    QStringList keywords;
    std::vector<ScrivenerScene> scenes;  ///< Its documents but the first, when it has more
    std::vector<ScrivenerSnapshot> snapshots;  ///< Of a text of one document
};

/// @brief How the draft is made into texts
struct ScrivenerDraftOptions {
    /// @brief What divides the documents of a text (its scenes)
    enum class Scenes {
        Breaks,  ///< A scene break
        Titles,  ///< The title of each scene, a heading a level lower than the text's
        None,    ///< Nothing: the text goes on
    };

    bool compiledOnly = true;       ///< Only the documents Included in Compile
    Scenes scenes = Scenes::Breaks;
};

/// @brief The texts and parts of the draft, as one document
struct ScrivenerDraft {
    /// Its headings are the titles: of the parts of level 1, of the texts of level 2, of the
    /// scenes (Scenes::Titles) of level 3; the documents' own headings are a level lower than
    /// those. The documents of a text (its scenes) are divided as the options say.
    ImportDocument document;
    std::vector<ScrivenerText> texts;  ///< The texts and the parts, in the order of the binder
    int excluded = 0;          ///< The documents left out: not Included in Compile
    int sceneSnapshots = 0;    ///< The snapshots of the scenes of texts of several documents,
                               ///< which are not versions of their texts
};

/// @brief The texts and parts of the draft
///
/// A folder at the top of the draft with folders (or documents with documents) in it is a
/// part; a folder or a document with documents in it is a text of its own text and theirs,
/// in the order of the binder (its scenes); a document a text. Deeper folders are the scenes
/// of their texts. A folder of no text and no documents is left out, and so is a document
/// that is not Included in Compile (@p options; its documents stay).
ScrivenerDraft scrivenerDraft(const ScrivenerProject& project, const ScrivenerDraftOptions& options);

/// @brief Append the blocks of @p from to @p into, with its notes, images, annotations, lists
///        and styles (a style of the same key and name is one; another of the same key gets a
///        key of its own)
void appendImportDocument(ImportDocument& into, ImportDocument from);

}  // namespace kalahari::editor
