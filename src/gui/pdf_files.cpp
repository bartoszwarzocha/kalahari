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

#include <QDateTime>
#include <QPdfDocument>
#endif

#include <string>

namespace kalahari::gui {

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
    const std::string value =
        core::SettingsManager::getInstance().get<std::string>(key, std::string(fallback));
    for (const char* each : values) {
        if (value == each) {
            return QString::fromLatin1(each);
        }
    }
    return QString::fromLatin1(fallback);
}

QImage PdfFiles::firstPage(const QString& path, const QSize& bounds, QString* error) {
#ifdef KALAHARI_HAS_PDF
    QPdfDocument document(nullptr);
    if (const QPdfDocument::Error loaded = document.load(path);
        loaded != QPdfDocument::Error::None || document.pageCount() < 1) {
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
        *error = loadProblem(static_cast<int>(QPdfDocument::Error::Unknown));
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
    QPdfDocument document(nullptr);
    if (const QPdfDocument::Error loaded = document.load(path);
        loaded != QPdfDocument::Error::None || document.pageCount() < 1) {
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
    QPdfDocument document(nullptr);
    if (document.load(path) != QPdfDocument::Error::None) {
        return std::nullopt;
    }
    PdfMetaData data;
    data.title = document.metaData(QPdfDocument::MetaDataField::Title).toString().simplified();
    data.author = document.metaData(QPdfDocument::MetaDataField::Author).toString().simplified();
    data.subject =
        document.metaData(QPdfDocument::MetaDataField::Subject).toString().simplified();
    const QDateTime created =
        document.metaData(QPdfDocument::MetaDataField::CreationDate).toDateTime();
    if (created.isValid()) {
        data.year = QString::number(created.date().year());
    }
    data.pages = document.pageCount();
    return data;
#else
    Q_UNUSED(path)
    return std::nullopt;
#endif
}

QString PdfFiles::loadProblem(int error) {
#ifdef KALAHARI_HAS_PDF
    switch (static_cast<QPdfDocument::Error>(error)) {
    case QPdfDocument::Error::FileNotFound:
        //: In Polish: Nie ma pliku.
        return tr("The file is missing.");
    case QPdfDocument::Error::IncorrectPassword:
    case QPdfDocument::Error::UnsupportedSecurityScheme:
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
