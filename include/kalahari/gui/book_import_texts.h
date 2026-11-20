// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_import_texts.h
/// @brief The texts of an imported file added to a book, and taken out again (Undo import)
///
/// The import window (File > Import) reads a file (editor/book_import.h), and the writer
/// chooses its texts, their titles and kinds and the place in the book. These functions add
/// them: the images of the file to the book's folder of images, each text a text element of
/// the book (a part a group with its texts inside), or a file of the book outside its text
/// ("Other files"). What they add is what Undo import takes out.

#pragma once

#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/editor/book_import.h>

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <vector>

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The names of the text kinds of a book, in all their languages, for the kinds of the
///        imported titles (editor::kindOfTitle()): the kinds the book offers in its front,
///        main and back part, but its main text kind (a chapter's title is any title)
std::vector<editor::KindTitle> importKindTitles(const core::ProjectManager& book);

/// @brief The names of @p kinds, as importKindTitles() gives them: the text kinds but @p main
std::vector<editor::KindTitle> importKindTitles(const QList<core::KindRef>& kinds, const core::KindRef& main);

/// @brief Save the images of an imported document in the book's folder of images
/// @param written Gets the sources of the files it wrote, not those the folder had already
///        (Undo import deletes them); may be nullptr
/// @return The source of each image of the document (editor::ImportDocument::images), in its
///         order, as editor::ImportKmlOptions::imageSources takes them; empty for an image
///         without data (a missing file, one from the web) and one that could not be saved
std::vector<QString> saveImportedImages(const core::ProjectManager& book, const editor::ImportDocument& document,
                                        QStringList* written);

/// @brief An earlier version of an imported text
struct ImportedVersion {
    QString kml;        ///< Its text
    QString name;       ///< What is said of it (a snapshot's title); empty: nothing
    QDateTime created;  ///< When it was kept; invalid: at the import
};

/// @brief A text or a part that an import adds to the book
struct ImportedText {
    QString title;
    core::KindRef kind;               ///< A text kind; a group kind for a part
    QString kml;                      ///< Its text (a text element); empty: an empty text
    std::vector<ImportedText> texts;  ///< The texts of a part, in order
    QString status;       ///< Of a text: "draft", "revision" or "final"; empty: "draft"
    QString notes;        ///< Its notes (of a part, its description); empty: none
    std::vector<ImportedVersion> versions;  ///< Of a text: its earlier versions, the oldest
                                            ///< first
};

/// @brief A material that an import adds to the Workshop of the book
struct ImportedMaterial {
    QString kind;         ///< The id of its kind: "work_note" (a text), "picture",
                          ///< "document", "web_page", "other_file"
    QString title;
    QString kml;          ///< Of a work note: its text
    QString file;         ///< Of the other kinds: the file whose copy it gets; empty: none
    QString description;  ///< Of the other kinds: its description; empty: none
    QString notes;        ///< Of a work note: its notes; empty: none
};

/// @brief Where an import adds its texts
struct ImportPlace {
    bool otherFiles = false;  ///< As files of the book outside its text ("Other files"): with
                              ///< no kinds, the texts of the parts one after another and the
                              ///< parts' titles left out
    core::ElementPlace at;    ///< Where the texts go in the book, one after another; a text of
                              ///< a kind with a place of its own (a prologue, acknowledgments)
                              ///< or one that cannot be there goes to its kind's place
};

/// @brief What an import added to the book: what Undo import takes out
struct ImportedElements {
    QStringList elements;  ///< The ids of the elements and files it added at the top (not
                           ///< those inside its parts), in order
    QStringList images;    ///< The sources of the image files it wrote
    QStringList styles;    ///< The ids of the paragraph styles it added to the book

    bool isEmpty() const { return elements.isEmpty() && images.isEmpty() && styles.isEmpty(); }
};

/// @brief Add the texts to the book at a place; saves the .klh file and the texts
/// @param added Gets what it added, also when it stops at a text it cannot add; may be nullptr
/// @return false when a text or a part could not be added (a kind the book cannot have
///         there, a file that cannot be written): the texts after it are not added
bool addImportedTexts(core::ProjectManager& book, const std::vector<ImportedText>& texts, const ImportPlace& place,
                      ImportedElements* added);

/// @brief Add the materials to the Workshop of the book, in their order; a material whose kind
///        the book has not is a file of another kind ("other_file"), a work note stays one
/// @param added Gets the ids of the materials it added (Undo import takes them out); may be
///        nullptr
/// @return The materials it could not add: no kind of the book, a file that cannot be read or
///         written; the others are added
int addImportedMaterials(core::ProjectManager& book, const std::vector<ImportedMaterial>& materials,
                         ImportedElements* added);

/// @brief Make room in a new book for the imported texts: the text elements it started with of
///        the kinds of @p texts go, and so does a text of its main kind with no text in it
///        ("Chapter 1"), with their files
void clearNewBookStart(core::ProjectManager& book, const std::vector<ImportedText>& texts);

/// @brief Take out of the book what an import added: its elements with their files and the
///        versions of their texts, the image files it wrote and the paragraph styles it added
/// @return false when something could not be taken out or deleted; the rest is
bool removeImported(core::ProjectManager& book, const ImportedElements& imported);

}  // namespace kalahari::gui
