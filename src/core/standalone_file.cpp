/// @file standalone_file.cpp
/// @brief Chapters and plain text files opened on their own, outside a book project

#include <kalahari/core/standalone_file.h>

#include <kalahari/core/chapter_document.h>
#include <kalahari/editor/clipboard_handler.h>
#include <QFileInfo>

namespace kalahari::core {

StandaloneFile::Type StandaloneFile::typeOf(const QString& path) {
    const QString suffix = QFileInfo(path).suffix();
    if (suffix.compare(QLatin1String("kchapter"), Qt::CaseInsensitive) == 0) {
        return Type::Chapter;
    }
    if (suffix.compare(QLatin1String("txt"), Qt::CaseInsensitive) == 0) {
        return Type::PlainText;
    }
    return Type::Unsupported;
}

std::optional<StandaloneFile> StandaloneFile::open(const QString& path, QString& kml,
                                                   QString* error) {
    StandaloneFile file;
    file.m_path = path;
    file.m_type = typeOf(path);
    switch (file.m_type) {
    case Type::Chapter: {
        const auto chapter = ChapterDocument::load(path);
        if (!chapter) {
            return std::nullopt;
        }
        kml = chapter->kml();
        return file;
    }
    case Type::PlainText: {
        const auto content = readTextFile(path, error);
        if (!content) {
            return std::nullopt;
        }
        kml = editor::ClipboardHandler::textToKml(content->text);
        file.m_textFormat = content->format;
        return file;
    }
    case Type::Unsupported:
        break;
    }
    return std::nullopt;
}

bool StandaloneFile::save(const QString& kml, const QString& text, QString* error) {
    return saveAs(m_path, kml, text, error);
}

bool StandaloneFile::saveAs(const QString& path, const QString& kml, const QString& text,
                            QString* error) {
    const Type type = typeOf(path);
    switch (type) {
    case Type::Chapter: {
        // The chapter's other data comes from the file the content was opened from
        ChapterDocument chapter;
        if (m_type == Type::Chapter) {
            chapter = ChapterDocument::load(m_path).value_or(ChapterDocument());
        }
        if (chapter.title().isEmpty()) {
            chapter.setTitle(QFileInfo(path).completeBaseName());
        }
        chapter.setKml(kml);
        if (!chapter.save(path)) {
            return false;  // the log says why
        }
        break;
    }
    case Type::PlainText: {
        TextFileFormat format = m_type == Type::PlainText ? m_textFormat : TextFileFormat::newFile();
        if (!writeTextFile(path, text, format, error)) {
            return false;
        }
        m_textFormat = format;
        break;
    }
    case Type::Unsupported:
        return false;
    }
    m_path = path;
    m_type = type;
    return true;
}

}  // namespace kalahari::core
