/// @file text_link.cpp
/// @brief Links in the text: text whose characters carry the address of a web page

#include <kalahari/editor/text_link.h>

#include <QKeySequence>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>
#include <QUrl>

namespace kalahari::editor {

namespace {

/// @brief An address that starts with its scheme ("https:", "mailto:"); not a host with
///        its port ("localhost:8080")
bool hasScheme(const QString& address)
{
    static const QRegularExpression scheme(QStringLiteral("^[A-Za-z][A-Za-z0-9+-]*:(?!\\d)"));
    return scheme.match(address).hasMatch();
}

/// @brief An e-mail address without its scheme: "jan@example.com"
bool isEmailAddress(const QString& address)
{
    static const QRegularExpression email(QStringLiteral("^[\\w.%+-]+@[\\w-]+(\\.[\\w-]+)+$"),
                                          QRegularExpression::UseUnicodePropertiesOption);
    return email.match(address).hasMatch();
}

}  // namespace

QString linkAddressOf(const QTextCharFormat& format)
{
    return format.isAnchor() ? format.anchorHref() : QString();
}

void setLinkAddress(QTextCharFormat& format, const QString& address)
{
    if (address.isEmpty()) {
        format.clearProperty(QTextFormat::IsAnchor);
        format.clearProperty(QTextFormat::AnchorHref);
        format.clearProperty(QTextFormat::AnchorName);
        return;
    }
    format.setAnchor(true);
    format.setAnchorHref(address);
}

std::vector<TextLink> linksIn(const QTextBlock& block)
{
    std::vector<TextLink> links;
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const QString address = linkAddressOf(fragment.charFormat());
        if (address.isEmpty()) {
            continue;
        }
        const int from = fragment.position();
        const int to = from + fragment.length();
        if (!links.empty() && links.back().to == from && links.back().address == address) {
            links.back().to = to;  // a link of fragments with different formats
        } else {
            links.push_back({from, to, address});
        }
    }
    return links;
}

std::optional<TextLink> linkAtCharacter(const QTextDocument& document, int position)
{
    const QTextBlock block = document.findBlock(position);
    if (!block.isValid() || position < block.position() ||
        position >= block.position() + block.length() - 1) {
        return std::nullopt;
    }
    for (const TextLink& link : linksIn(block)) {
        if (link.from <= position && position < link.to) {
            return link;
        }
    }
    return std::nullopt;
}

std::optional<TextLink> linkAtPosition(const QTextDocument& document, int position)
{
    if (std::optional<TextLink> after = linkAtCharacter(document, position)) {
        return after;
    }
    const QTextBlock block = document.findBlock(position);
    if (block.isValid() && position > block.position()) {
        return linkAtCharacter(document, position - 1);
    }
    return std::nullopt;
}

QString normalizedLinkAddress(const QString& written)
{
    const QString address = written.trimmed();
    if (address.isEmpty() || hasScheme(address) || address.startsWith(QLatin1Char('#')) ||
        address.startsWith(QLatin1Char('/'))) {
        return address;
    }
    if (isEmailAddress(address)) {
        return QStringLiteral("mailto:") + address;
    }
    return QStringLiteral("https://") + address;
}

bool isOpenableLinkAddress(const QString& address)
{
    const QUrl url(address.trimmed(), QUrl::StrictMode);
    if (!url.isValid()) {
        return false;
    }
    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("mailto")) {
        return !url.path().isEmpty();
    }
    return (scheme == QStringLiteral("http") || scheme == QStringLiteral("https") ||
            scheme == QStringLiteral("ftp")) &&
           !url.host().isEmpty();
}

QString linkClickKeys()
{
    // The keys of Ctrl+A without the A: "Ctrl+" or "⌘"
    QString keys = QKeySequence(Qt::CTRL | Qt::Key_A).toString(QKeySequence::NativeText);
    keys.chop(1);
    return keys;
}

}  // namespace kalahari::editor
