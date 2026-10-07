/// @file text_file.cpp
/// @brief Plain text files read and written in their own encoding and line ends

#include <kalahari/core/text_file.h>

#include <QFile>
#include <QSaveFile>
#include <QStringDecoder>
#include <QStringEncoder>

namespace kalahari::core {

namespace {

/// Decodes @p data; nullopt when it is not valid text in @p encoding
std::optional<QString> decode(QByteArrayView data, QStringConverter::Encoding encoding) {
    QStringDecoder decoder(encoding, QStringConverter::Flag::Stateless);
    QString text = decoder(data);
    if (decoder.hasError()) {
        return std::nullopt;
    }
    return text;
}

/// Encodes @p text in @p format; @p exact says whether the result reads back as the same text
QByteArray encode(const QString& text, const TextFileFormat& format, bool& exact) {
    // Not stateless: a stateless UTF-8 encoder writes no byte order mark
    QStringEncoder encoder(format.encoding, format.byteOrderMark
                                                ? QStringConverter::Flag::WriteBom
                                                : QStringConverter::Flag::Default);
    QByteArray data = encoder(text);
    // An 8-bit encoding may put a substitute for a character it lacks without reporting
    // an error, so the result is read back
    exact = !encoder.hasError() && decode(data, format.encoding) == text;
    return data;
}

}  // namespace

TextFileFormat TextFileFormat::newFile() {
    TextFileFormat format;
#ifdef Q_OS_WIN
    format.crlf = true;
#endif
    return format;
}

std::optional<TextFileContent> readTextFile(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return std::nullopt;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        if (error) {
            *error = file.errorString();
        }
        return std::nullopt;
    }

    TextFileContent content;
    if (const auto marked = QStringConverter::encodingForData(data)) {
        // The byte order mark is dropped; invalid sequences become replacement characters
        QStringDecoder decoder(*marked, QStringConverter::Flag::Stateless);
        content.text = decoder(data);
        content.format.encoding = *marked;
        content.format.byteOrderMark = true;
    } else if (auto utf8 = decode(data, QStringConverter::Utf8)) {
        content.text = std::move(*utf8);
        content.format.encoding = QStringConverter::Utf8;
    } else if (auto system = decode(data, QStringConverter::System)) {
        content.text = std::move(*system);
        content.format.encoding = QStringConverter::System;
    } else {
        // Any bytes are Latin-1 text
        content.text = QString::fromLatin1(data);
        content.format.encoding = QStringConverter::Latin1;
    }

    content.format.crlf = content.text.contains(QStringLiteral("\r\n"));
    content.text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content.text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return content;
}

bool writeTextFile(const QString& path, const QString& text, TextFileFormat& format,
                   QString* error) {
    QString fileText = text;
    if (format.crlf) {
        fileText.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
    }

    bool exact = false;
    QByteArray data = encode(fileText, format, exact);
    if (!exact && format.encoding != QStringConverter::Utf8) {
        // UTF-8 holds any text
        format.encoding = QStringConverter::Utf8;
        data = encode(fileText, format, exact);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

}  // namespace kalahari::core
