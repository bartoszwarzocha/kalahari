// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_files.cpp
/// @brief PDF files as the program shows them: whether it can, their first page, their data,
/// and the settings of their preview

#include "kalahari/gui/pdf_files.h"

#include "kalahari/core/settings_manager.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

#ifdef KALAHARI_HAS_PDF
#include "kalahari/gui/panels/pdf_preview_panel.h"
#include "kalahari/gui/pdf_document.h"

#include <QRegularExpression>
#endif

#include <initializer_list>
#include <optional>
#include <string>

namespace kalahari::gui {

#ifdef KALAHARI_HAS_PDF
namespace {

/// The year of a date of a PDF file ("D:20190301120000Z"); empty: none
QString yearOf(const QString& date) {
    static const QRegularExpression year(QStringLiteral("^\\s*(?:D:)?(\\d{4})"));
    const QRegularExpressionMatch match = year.match(date);
    return match.hasMatch() && match.capturedView(1) != QLatin1String("0000")
               ? match.captured(1)
               : QString();
}

}  // namespace
#endif

bool PdfFiles::previewAvailable() {
#ifdef KALAHARI_HAS_PDF
    return true;
#else
    return false;
#endif
}

bool PdfFiles::isPdf(const QString& path) {
    if (path.isEmpty()) {
        return false;
    }
    const QFileInfo info(path);
    if (info.suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    return info.isFile() &&
           QMimeDatabase().mimeTypeForFile(info).inherits(QStringLiteral("application/pdf"));
}

bool PdfFiles::opensInPreview() {
    return previewAvailable() && choice(OPEN_SETTING, PREVIEW, {PREVIEW, SYSTEM}) == PREVIEW;
}

QString PdfFiles::choice(const char* key, const char* fallback,
                         std::initializer_list<const char*> values) {
    const auto value =
        core::SettingsManager::getInstance().get<std::string>(key, std::string(fallback));
    for (const char* each : values) {
        if (value == each) {
            return QString::fromLatin1(each);
        }
    }
    return QString::fromLatin1(fallback);
}

bool PdfFiles::highlightsOn() {
    return core::SettingsManager::getInstance().get<bool>(HIGHLIGHTS_SETTING, false);
}

QImage PdfFiles::firstPage(const QString& path, const QSize& bounds, QString* error) {
#ifdef KALAHARI_HAS_PDF
    PdfDocument document;
    if (const PdfDocument::Error loaded = document.load(path);
        loaded != PdfDocument::Error::None || document.pageCount() < 1) {
        if (error != nullptr) {
            *error = loadProblem(static_cast<int>(loaded));
        }
        return {};
    }
    const QSizeF points = document.pagePointSize(0);
    if (points.isEmpty()) {
        return {};
    }
    // The page at the size of the bounds, its sides as in the file
    const QSize size = bounds.isValid() ? points.scaled(QSizeF(bounds), Qt::KeepAspectRatio)
                                              .toSize()
                                        : points.toSize();
    QImage page = document.render(0, size.expandedTo(QSize(1, 1)));
    if (page.isNull() && error != nullptr) {
        *error = loadProblem(static_cast<int>(PdfDocument::Error::Unknown));
    }
    return page;
#else
    Q_UNUSED(path)
    Q_UNUSED(bounds)
    Q_UNUSED(error)
    return {};
#endif
}

int PdfFiles::pageCount(const QString& path, QString* error) {
#ifdef KALAHARI_HAS_PDF
    PdfDocument document;
    if (const PdfDocument::Error loaded = document.load(path);
        loaded != PdfDocument::Error::None || document.pageCount() < 1) {
        if (error != nullptr) {
            *error = loadProblem(static_cast<int>(loaded));
        }
        return 0;
    }
    return document.pageCount();
#else
    Q_UNUSED(path)
    Q_UNUSED(error)
    return 0;
#endif
}

std::optional<PdfMetaData> PdfFiles::metaData(const QString& path) {
#ifdef KALAHARI_HAS_PDF
    PdfDocument document;
    if (document.load(path) != PdfDocument::Error::None) {
        return std::nullopt;
    }
    PdfMetaData data;
    data.title = document.metaData(PdfDocument::MetaDataField::Title).simplified();
    data.author = document.metaData(PdfDocument::MetaDataField::Author).simplified();
    data.subject = document.metaData(PdfDocument::MetaDataField::Subject).simplified();
    data.year = yearOf(document.metaData(PdfDocument::MetaDataField::CreationDate));
    data.pages = document.pageCount();
    return data;
#else
    Q_UNUSED(path)
    return std::nullopt;
#endif
}

QString PdfFiles::loadProblem(int error) {
#ifdef KALAHARI_HAS_PDF
    switch (static_cast<PdfDocument::Error>(error)) {
    case PdfDocument::Error::FileNotFound:
        //: In Polish: Nie ma pliku.
        return tr("The file is missing.");
    case PdfDocument::Error::IncorrectPassword:
    case PdfDocument::Error::UnsupportedSecurityScheme:
        //: In Polish: Plik jest chroniony hasłem.
        return tr("The file is protected with a password.");
    default:
        break;
    }
#else
    Q_UNUSED(error)
#endif
    //: In Polish: Nie da się odczytać pliku PDF. Może być uszkodzony.
    return tr("The PDF file cannot be read. It may be damaged.");
}

PreviewPanel* PdfFiles::newPreview(core::ProjectManager& book, const QString& elementId,
                                   QWidget* parent) {
#ifdef KALAHARI_HAS_PDF
    return new PdfPreviewPanel(book, elementId, parent);
#else
    Q_UNUSED(book)
    Q_UNUSED(elementId)
    Q_UNUSED(parent)
    return nullptr;
#endif
}

}  // namespace kalahari::gui
