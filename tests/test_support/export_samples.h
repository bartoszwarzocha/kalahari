// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file export_samples.h
/// @brief The sample book of the export tests and the helpers that read what they write

#pragma once

#include <kalahari/editor/book_export.h>

#include <QByteArray>
#include <QHash>
#include <QString>

#include <vector>

namespace kalahari::test {

/// @brief The styles of a novel, as its pack has them
editor::ParagraphStyleSet novelStyles();

/// @brief A text with every kind of element the export knows (images/red.png, a missing image)
QString richKml();

/// @brief A book of a contents, a part and two chapters, its images in @p folder
editor::BookExport sampleBook(const QString& folder);

/// @brief A text of @p paragraphs paragraphs of a few lines
QString longKml(int paragraphs);

/// @brief A text item of a book, its title in the style of a chapter's title
editor::ExportItem textItem(const QString& id, const QString& title, const QString& kml);

/// @brief A book of @p items in the novel's styles
editor::BookExport bookOf(std::vector<editor::ExportItem> items);

/// @brief The files of a ZIP archive by name
QHash<QString, QByteArray> zipEntries(const QString& path);

/// @brief Whether the XML is well-formed
bool wellFormed(const QByteArray& xml);

/// @brief A red square of 40 pixels in images/red.png of @p folder
void writeRedSquare(const QString& folder);

/// @brief Copy a file to the folder of KALAHARI_EXPORT_SAMPLES, when the environment names one
void keepSample(const QString& path, const QString& name = QString());

/// @brief How many times @p part is in @p text
int count(const QString& text, const QString& part);

}  // namespace kalahari::test
