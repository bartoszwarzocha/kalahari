/// @file text_link.h
/// @brief Links in the text: text whose characters carry the address of a web page
///
/// Every character of a link has the anchor of Qt's character format with the link's
/// address (QTextFormat::IsAnchor and QTextFormat::AnchorHref), so the document's undo,
/// the clipboard and drag and drop take the link with its text, and KML keeps it as
/// <a href="...">. Characters next to each other in a paragraph with one address make one
/// link. The format holds no look of a link: the editor draws its characters in the link
/// color, underlined.

#pragma once

#include <QString>

#include <optional>
#include <vector>

class QTextBlock;
class QTextCharFormat;
class QTextDocument;

namespace kalahari::editor {

/// @brief A link in a paragraph: its characters and its address
struct TextLink {
    int from = 0;     ///< Its first character, a position in the document
    int to = 0;       ///< The position after its last character
    QString address;  ///< The address it opens

    bool operator==(const TextLink&) const = default;
};

/// @brief What Insert > Link (Ctrl+K) changes at the cursor (BookEditor::linkTarget())
struct LinkTarget {
    int from = 0;             ///< The text the link goes on, positions in the document;
    int to = 0;               ///< from == to: a new link at the cursor
    QString text;             ///< The text the link shows
    QString address;          ///< The link's address; empty: a new link
    bool existing = false;    ///< A link of the text: the dialog edits it and can remove it
    bool textEditable = true; ///< false: a selection over paragraphs or with an object,
                              ///< whose text stays as it is

    bool operator==(const LinkTarget&) const = default;
};

/// @brief The address of the link a character with this format belongs to; empty: none
QString linkAddressOf(const QTextCharFormat& format);

/// @brief Make text with the format a link to @p address; an empty address takes the link
///        off
void setLinkAddress(QTextCharFormat& format, const QString& address);

/// @brief The links of a paragraph, in text order
std::vector<TextLink> linksIn(const QTextBlock& block);

/// @brief The link the character at @p position belongs to (a character of the text, not
///        a paragraph break)
std::optional<TextLink> linkAtCharacter(const QTextDocument& document, int position);

/// @brief The link at a cursor's position: the one of the character after it, else the one
///        of the character before it, in its paragraph
std::optional<TextLink> linkAtPosition(const QTextDocument& document, int position);

/// @brief The address the program keeps for one written in the Address field
///
/// Without spaces around it. An address without a scheme gets one: "mailto:" an e-mail
/// address ("jan@example.com"), "https://" the address of a page ("www.example.com"). An
/// address with a scheme, or one that starts with # or /, stays as it is.
QString normalizedLinkAddress(const QString& written);

/// @brief Whether the program opens the address in the system's program: a page (http,
///        https), a file server (ftp) or a new e-mail (mailto)
///
/// Another address (file:, javascript:...) could run a program on the computer, so a link
/// to it is only text.
bool isOpenableLinkAddress(const QString& address);

/// @brief The key held to open a link with a click, as the system names it in front of a
///        word: "Ctrl+" ("⌘" on macOS, where Qt's Ctrl is Cmd)
QString linkClickKeys();

}  // namespace kalahari::editor
