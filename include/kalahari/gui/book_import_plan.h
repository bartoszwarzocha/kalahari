// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_import_plan.h
/// @brief What an import will add to a book: the files read, their texts and parts with the
///        kinds and titles they get, and the new styles of the book made of the files' styles
///
/// The import window (File > Import) reads the files with readImportFile() and shows the
/// texts importRows() makes of them. The writer checks the texts, changes their titles and
/// kinds; importedTexts() writes their KML for addImportedTexts() (book_import_texts.h).

#pragma once

#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/editor/book_import.h>
#include <kalahari/gui/book_import_texts.h>

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <vector>

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The formats the program imports
enum class ImportFormat {
    Docx,       ///< A Word document (.docx)
    Markdown,   ///< Markdown (.md)
    PlainText,  ///< Plain text (.txt)
};

/// @brief The extensions of the files of a format, with their dots (".md", ".markdown")
QStringList importExtensions(ImportFormat format);

/// @brief How the files of an import are read
struct ImportReading {
    ImportFormat format = ImportFormat::Docx;
    /// The encoding of Markdown and plain text; none: each file's own (detectTextEncoding())
    std::optional<editor::TextEncoding> encoding;
    editor::TextReadOptions text;  ///< Of plain text
    bool rejectChanges = false;    ///< The tracked changes rejected; false: accepted
};

/// @brief A file of an import as the program read it
struct ImportFile {
    QString path;
    editor::TextEncoding encoding = editor::TextEncoding::Utf8;  ///< Of Markdown and plain
                                                                 ///< text: the one it was
                                                                 ///< read in
    /// As the file has it, with both versions of its tracked changes
    std::shared_ptr<const editor::ImportDocument> read;
    /// With its tracked changes accepted or rejected (ImportReading::rejectChanges): the
    /// document the texts are made of
    std::shared_ptr<const editor::ImportDocument> document;
    editor::ImportProblem problem;  ///< Why it could not be read; Kind::None: it was

    /// @brief Whether it was read
    bool isRead() const { return document != nullptr; }
};

/// @brief Read file @p path as @p reading says
ImportFile readImportFile(const QString& path, const ImportReading& reading);

/// @brief The document of @p file with its tracked changes accepted or rejected
void resolveChanges(ImportFile& file, bool reject);

/// @brief The start of a Markdown or plain text file in @p encoding, for the writer to see
///        whether its letters are right: at most @p lines of its first lines with text
QString textPreview(const QString& path, editor::TextEncoding encoding, int lines);

/// @brief The files in the order of their names, the numbers in them as numbers ("Chapter 2"
///        before "Chapter 10")
QStringList sortedByName(QStringList paths);

// =============================================================================
// The kinds of the texts
// =============================================================================

/// @brief A kind of text the book can have, as the import offers it
struct ImportKind {
    core::KindRef kind;
    QString name;    ///< Its name in the program's language
    QString iconId;  ///< ArtProvider id of its icon
    int room = -1;   ///< How many more texts of it the book can have; -1: any number
};

/// @brief The kinds of the texts of the book an import adds to
struct ImportKinds {
    QList<ImportKind> kinds;  ///< Its text kinds (front, main, back part), each once
    core::KindRef main;       ///< Its main text kind (a chapter)
    core::KindRef part;       ///< Its kind of a part (a group); none: the book has no parts
    core::KindRef titlePage;  ///< The kind of a title page, when the book can have it
    QString partName;         ///< The name of the kind of a part in the program's language
    std::vector<editor::KindTitle> titles;  ///< The names of the kinds as titles

    /// @brief The kind of @p reference (core::KindRef::reference()), or nullptr
    const ImportKind* find(const QString& reference) const;
};

/// @brief The kinds of the book open in @p book; the room of each is what its limit leaves
///        the book
ImportKinds importKindsOf(const core::ProjectManager& book);

/// @brief The kinds of a new book of type @p typeId (a package of @p registry); the room of
///        each is its limit: the elements a new book starts with make room for the imported
///        texts of their kinds (newBookStart())
ImportKinds importKindsOf(const core::BookTypeRegistry& registry, const QString& typeId);

// =============================================================================
// The texts and the parts
// =============================================================================

/// @brief The most words of a text before the first title that is a title page
inline constexpr int TITLE_PAGE_WORDS = 120;

/// @brief A text or a part an import adds, as the window lists it
struct ImportRow {
    int file = 0;               ///< Its file (an index of the files)
    editor::ImportPiece piece;  ///< Where it is in its file's document
    QString title;              ///< Its title in the book
    QString kind;               ///< The reference of its kind (ImportKind); of a part, the part
                                ///< kind's; empty: none (a part of a book without parts)
    bool checked = true;        ///< It goes to the book
    bool full = false;          ///< It was unchecked: the book can have no more of its kind
};

/// @brief The texts and parts of the files, each file divided as @p split says, or one text
///        each when @p wholeFiles
///
/// A text that has no title takes the name of its file. The kind of a text is the one its
/// title names ("Prolog"), of a short text before the first title (TITLE_PAGE_WORDS) a title
/// page, else the main text kind. A text of a kind the book can have no more of is unchecked
/// (ImportRow::full), and so are those after it of the kind.
std::vector<ImportRow> importRows(const std::vector<ImportFile>& files,
                                  const editor::SplitOptions& split, bool wholeFiles,
                                  const ImportKinds& kinds);

/// @brief The kinds whose checked texts are more than the book can have, each once, in the
///        order of the rows
QStringList fullKinds(const std::vector<ImportRow>& rows, const ImportKinds& kinds);

/// @brief The checked rows of a part: the part and those of its texts; of a text, itself
bool isChecked(const std::vector<ImportRow>& rows, size_t row);

/// @brief The texts of the checked rows, with their KML
/// @param options How the KML of the texts of each file is written (by the index of its file)
std::vector<ImportedText> importedTexts(const std::vector<ImportFile>& files,
                                        const std::vector<ImportRow>& rows,
                                        const ImportKinds& kinds,
                                        const std::vector<editor::ImportKmlOptions>& options);

/// @brief The words of the checked texts
int checkedWords(const std::vector<ImportRow>& rows);

// =============================================================================
// The styles
// =============================================================================

/// @brief A style of the files, as the window lists it: those of the same name in more files
///        once
struct ImportFileStyle {
    QString key;                                 ///< Its key in the first file that has it
    QString name;                                ///< Its name in the file
    int paragraphs = 0;                          ///< The paragraphs in it in all the files
    const editor::ImportStyle* style = nullptr;  ///< Of the first file that has it
};

/// @brief The styles of the paragraphs of the files, in the order of the first file, then of
///        the others; those of no paragraph left out
std::vector<ImportFileStyle> importFileStyles(const std::vector<ImportFile>& files);

/// @brief The prefix of a choice of the style mapping that makes a new style of the book
inline constexpr const char* NEW_STYLE_PREFIX = "+";

/// @brief The id of a new style of the book made of a style of the file named @p name, not one
///        of @p taken: "imported_" with the letters and digits of the name
QString newStyleId(const QString& name, const QStringList& taken);

/// @brief The new styles of the book the mapping makes (a choice NEW_STYLE_PREFIX + the key of
///        the file's style), with their ids in @p mapping in place of the choices
/// @param book The styles of the book; a new style is based on the style suggestedBookStyle()
///        gives its file's style
QList<core::BookParagraphStyle> newBookStyles(const std::vector<ImportFile>& files,
                                              QHash<QString, QString>* mapping,
                                              const editor::ParagraphStyleSet& book);

/// @brief The mapping of the styles of a file: those of the window, by the names of the
///        styles, as the keys of the file
QHash<QString, QString> fileMapping(const editor::ImportDocument& document,
                                    const QHash<QString, QString>& byName);

}  // namespace kalahari::gui
