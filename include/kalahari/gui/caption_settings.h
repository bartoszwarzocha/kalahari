// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file caption_settings.h
/// @brief The settings of new images and of the captions of new books (Settings > Editor >
///        Images), and the book's captions as the editor numbers them

#ifndef KALAHARI_GUI_CAPTION_SETTINGS_H
#define KALAHARI_GUI_CAPTION_SETTINGS_H

#include "kalahari/core/book_project.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_captions.h"

#include <QString>
#include <QStringList>

#include <string>
#include <utility>
#include <vector>

namespace kalahari::core {
class ProjectManager;
} // namespace kalahari::core

namespace kalahari::gui {

// =============================================================================
// Setting keys
// =============================================================================

/// @brief How the text flows around a new image: "inline" (none, the image in a paragraph of
///        its own), "left" or "right" (the image on that side, the text beside it), "topBottom"
///        (the text above and below it)
inline constexpr const char* IMAGE_LAYOUT_KEY = "editor.images.layout";
/// @brief How a new image is aligned: "left", "center" or "right"
inline constexpr const char* IMAGE_ALIGNMENT_KEY = "editor.images.alignment";
/// @brief The width of a new image: 0, as its file and at most the width of the text; else a
///        share of the width of the text in percent (100, 75, 50)
inline constexpr const char* IMAGE_WIDTH_KEY = "editor.images.width";
/// @brief The room a new image keeps from the text above it, below it and beside it, in
///        points (Image Properties, Spacing)
inline constexpr const char* IMAGE_SPACE_ABOVE_KEY = "editor.images.spaceAbove";
inline constexpr const char* IMAGE_SPACE_BELOW_KEY = "editor.images.spaceBelow";
inline constexpr const char* IMAGE_SPACE_BESIDE_KEY = "editor.images.spaceBeside";
/// @brief Whether a new image gets a numbered caption under it
inline constexpr const char* IMAGE_CAPTION_KEY = "editor.images.addCaption";
/// @brief The label of that caption; empty: the first label of the captions of new books
inline constexpr const char* IMAGE_CAPTION_LABEL_KEY = "editor.images.captionLabel";
/// @brief Whether Insert > Image... asks for the caption of the new image (or none) before it
///        goes in; false: the caption IMAGE_CAPTION_KEY says
inline constexpr const char* IMAGE_ASK_CAPTION_KEY = "editor.images.askCaption";
/// @brief Whether a copy of a very large image is made smaller (IMAGE_MAX_DPI_KEY)
inline constexpr const char* IMAGE_DOWNSCALE_KEY = "editor.images.downscale";
/// @brief Above how many dots per inch, at its size in the text, a copy is made smaller
inline constexpr const char* IMAGE_MAX_DPI_KEY = "editor.images.maxDpi";
/// @brief The lowest and the highest resolution of IMAGE_MAX_DPI_KEY (dots per inch)
inline constexpr int IMAGE_MIN_MAX_DPI = 72;
inline constexpr int IMAGE_MAX_MAX_DPI = 2400;
/// @brief How the manuscript for a publisher has its images: "inline", or "markers" (not used
///        yet)
inline constexpr const char* IMAGE_MANUSCRIPT_KEY = "editor.images.manuscript";

/// @brief The room a new table keeps from the text above it and below it, in points (Table
///        Properties, Table)
inline constexpr const char* TABLE_SPACE_ABOVE_KEY = "editor.tables.spaceAbove";
inline constexpr const char* TABLE_SPACE_BELOW_KEY = "editor.tables.spaceBelow";
/// @brief Where Insert Caption puts the caption of a table without one: "above" or "below" it
inline constexpr const char* TABLE_CAPTION_POSITION_KEY = "editor.tables.captionPosition";

/// @brief The labels of the captions of new books, in the order Insert Caption offers them;
///        an empty list: defaultCaptionLabels()
inline constexpr const char* CAPTION_LABELS_KEY = "editor.captions.labels";
/// @brief The number format of the captions of new books: "1", "a", "A", "i" or "I"
inline constexpr const char* CAPTION_FORMAT_KEY = "editor.captions.format";
/// @brief Whether new books number their captions anew in each chapter, after the chapter's
///        number ("2.3")
inline constexpr const char* CAPTION_WITH_CHAPTER_KEY = "editor.captions.withChapter";
/// @brief What a cross-reference of new books shows: "number", "label", "caption", "position"
inline constexpr const char* CAPTION_REFERENCE_KEY = "editor.captions.reference";

// =============================================================================
// Captions
// =============================================================================

/// @brief The labels a new book starts with, in the user's language: Figure, Photo,
///        Illustration, Map, Diagram, Table
QStringList defaultCaptionLabels();

/// @brief The label of the caption of a table, in the user's language: Table
QString defaultTableCaptionLabel();

/// @brief The labels of the captions of new books, from the settings
QStringList settingsCaptionLabels();

/// @brief The captions of a new book, from the settings: its labels, each numbered alike, and
///        what a cross-reference shows
core::BookCaptions settingsBookCaptions();

/// @brief The captions of the book open in @p book: its own, else those of the settings (no
///        book: the settings')
core::BookCaptions currentBookCaptions(const core::ProjectManager& book);

/// @brief The numbering of a label of the book as the editor counts it
editor::CaptionNumbering numberingOf(const core::CaptionLabelNumbering& numbering);

/// @brief The numbering of a label of the book as the book keeps it
core::CaptionLabelNumbering bookNumberingOf(const QString& label,
                                            const editor::CaptionNumbering& numbering);

/// @brief The numbering of every label the captions name (their labels and their numbering)
QHash<QString, editor::CaptionNumbering> numberingsOf(const core::BookCaptions& captions);

/// @brief What a new cross-reference of the captions shows
editor::ReferenceShows referenceShowsOf(const core::BookCaptions& captions);

// =============================================================================
// New images
// =============================================================================

/// @brief How a new image goes into the text, from the settings: its alignment, how the text
///        flows around it, its width, the label of its caption (none when the settings add none)
editor::BookEditor::NewImageOptions settingsNewImageOptions();

/// @brief The ways the text flows around an image, in order, each with its name as the
///        windows show it (Image Properties, Settings > Editor > Images)
std::vector<std::pair<editor::ImageWrap, QString>> imageWrapChoices();

/// @brief The value of IMAGE_LAYOUT_KEY of a wrap, and the wrap of a value (any other value:
///        ImageWrap::Inline)
std::string imageWrapSetting(editor::ImageWrap wrap);
editor::ImageWrap imageWrapOfSetting(const std::string& value);

// =============================================================================
// New tables
// =============================================================================

/// @brief How a new table goes into the text, from the settings: the room it keeps from the
///        text above it and below it
editor::BookEditor::NewTableOptions settingsNewTableOptions();

/// @brief Whether Insert Caption puts the caption of a table above it (the settings)
bool settingsTableCaptionAbove();

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_CAPTION_SETTINGS_H
