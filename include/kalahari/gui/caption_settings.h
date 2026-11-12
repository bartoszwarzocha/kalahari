/// @file caption_settings.h
/// @brief The settings of new images and of the captions of new books (Settings > Editor >
///        Images), and the book's captions as the editor numbers them

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_captions.h"

#include <QString>
#include <QStringList>

namespace kalahari::gui {

// =============================================================================
// Setting keys
// =============================================================================

/// @brief How a new image is laid out: "inline", in a paragraph of its own (the only way yet)
inline constexpr const char* IMAGE_LAYOUT_KEY = "editor.images.layout";
/// @brief How a new image is aligned: "left", "center" or "right"
inline constexpr const char* IMAGE_ALIGNMENT_KEY = "editor.images.alignment";
/// @brief The width of a new image: 0, as its file and at most the width of the text; else a
///        share of the width of the text in percent (100, 75, 50)
inline constexpr const char* IMAGE_WIDTH_KEY = "editor.images.width";
/// @brief Whether a new image gets a numbered caption under it
inline constexpr const char* IMAGE_CAPTION_KEY = "editor.images.addCaption";
/// @brief The label of that caption; empty: the first label of the captions of new books
inline constexpr const char* IMAGE_CAPTION_LABEL_KEY = "editor.images.captionLabel";
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
///        Illustration, Map, Diagram
QStringList defaultCaptionLabels();

/// @brief The labels of the captions of new books, from the settings
QStringList settingsCaptionLabels();

/// @brief The captions of a new book, from the settings: its labels, each numbered alike, and
///        what a cross-reference shows
core::BookCaptions settingsBookCaptions();

/// @brief The captions of the open book: its own, else those of the settings (no book: the
///        settings')
core::BookCaptions currentBookCaptions();

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

/// @brief How a new image goes into the text, from the settings: its alignment, its width, the
///        label of its caption (none when the settings add none)
editor::BookEditor::NewImageOptions settingsNewImageOptions();

}  // namespace kalahari::gui
