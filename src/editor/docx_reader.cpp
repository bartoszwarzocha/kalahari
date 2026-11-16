// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file docx_reader.cpp
/// @brief Import of a Word document (DOCX): its paragraphs, styles, lists, tables, images,
///        notes, comments, fields and tracked changes

#include <kalahari/editor/book_import.h>

#include <kalahari/core/unicode_paths.h>
#include <kalahari/editor/text_link.h>

#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QXmlStreamReader>

#include <zip.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace kalahari::editor {

namespace {

using namespace Qt::Literals::StringLiterals;

// =============================================================================
// The parts of the package
// =============================================================================

const QString W_MAIN = QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main");
const QString W_STRICT = QStringLiteral("http://purl.oclc.org/ooxml/wordprocessingml/main");
const QString R_MAIN = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");
const QString R_STRICT = QStringLiteral("http://purl.oclc.org/ooxml/officeDocument/relationships");
const QString W14 = QStringLiteral("http://schemas.microsoft.com/office/word/2010/wordml");
const QString W15 = QStringLiteral("http://schemas.microsoft.com/office/word/2012/wordml");
const QString VML_OFFICE = QStringLiteral("urn:schemas-microsoft-com:office:office");

/// The largest part of the package the reader takes, in bytes
constexpr zip_uint64_t MAX_PART_BYTES = 1024ULL * 1024ULL * 1024ULL;

/// Twentieths of a point (twips) in a point, EMU in a point and in a centimetre
constexpr double TWIPS_PER_POINT = 20.0;
constexpr double EMU_PER_CENTIMETRE = 360000.0;
constexpr double SINGLE_LINE = 240.0;  ///< w:line of single line spacing
constexpr double CROP_UNITS_PER_PERCENT = 1000.0;
constexpr int ROTATION_UNITS_PER_DEGREE = 60000;
constexpr int OUTLINE_BODY_TEXT = 9;  ///< w:outlineLvl of a paragraph that is no heading
constexpr int MAX_HEADING_LEVEL = 9;

/// An element of a part, read whole (with the white space of its text, which a DOM of Qt 6.4
/// would drop)
struct Node {
    QString name;  ///< Its local name
    QString ns;    ///< Its namespace
    QXmlStreamAttributes attributes;
    std::vector<Node> children;
    QString text;  ///< Its own characters

    bool is(QLatin1StringView local) const { return name == local; }

    /// An attribute of WordprocessingML (w:val), or one without a namespace
    QString attr(QLatin1StringView local) const {
        for (const QXmlStreamAttribute& attribute : attributes) {
            if (attribute.name() == local) {
                const QStringView ns = attribute.namespaceUri();
                if (ns.isEmpty() || ns == W_MAIN || ns == W_STRICT) {
                    return attribute.value().toString();
                }
            }
        }
        return QString();
    }

    bool hasAttr(QLatin1StringView local) const {
        return std::any_of(attributes.begin(), attributes.end(),
                           [local](const QXmlStreamAttribute& attribute) { return attribute.name() == local; });
    }

    /// An attribute of the relationships (r:id, r:embed)
    QString relationship(QLatin1StringView local) const {
        for (const QXmlStreamAttribute& attribute : attributes) {
            if (attribute.name() == local &&
                (attribute.namespaceUri() == R_MAIN || attribute.namespaceUri() == R_STRICT)) {
                return attribute.value().toString();
            }
        }
        return QString();
    }

    /// An attribute of a namespace
    QString attrIn(const QString& ns, QLatin1StringView local) const {
        return attributes.value(ns, local).toString();
    }

    const Node* child(QLatin1StringView local) const {
        for (const Node& node : children) {
            if (node.name == local) {
                return &node;
            }
        }
        return nullptr;
    }

    /// The first element of this name at any depth
    const Node* find(QLatin1StringView local) const {
        for (const Node& node : children) {
            if (node.name == local) {
                return &node;
            }
            if (const Node* found = node.find(local)) {
                return found;
            }
        }
        return nullptr;
    }

    /// The w:val of a child; empty without it
    QString value(QLatin1StringView local) const {
        const Node* node = child(local);
        return node ? node->attr("val"_L1) : QString();
    }
};

/// A part read into its elements; std::nullopt when it is not XML
std::optional<Node> parseXml(const QByteArray& data) {
    QXmlStreamReader reader(data);
    Node root;
    std::vector<Node*> path{&root};
    while (!reader.atEnd()) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                Node& node = path.back()->children.emplace_back();
                node.name = reader.name().toString();
                node.ns = reader.namespaceUri().toString();
                node.attributes = reader.attributes();
                path.push_back(&node);
                break;
            }
            case QXmlStreamReader::EndElement:
                path.pop_back();
                break;
            case QXmlStreamReader::Characters:
                path.back()->text += reader.text();
                break;
            default:
                break;
        }
    }
    if (reader.hasError() || root.children.empty()) {
        return std::nullopt;
    }
    return std::move(root.children.front());
}

/// A switch of the properties: <w:b/> on, <w:b w:val="0"/> off
bool isOn(const Node* node) {
    if (!node) {
        return false;
    }
    const QString value = node->attr("val"_L1).toLower();
    return value.isEmpty() || (value != QLatin1String("0") && value != QLatin1String("false") &&
                               value != QLatin1String("off") && value != QLatin1String("none"));
}

std::optional<bool> switchOf(const Node* node) {
    return node ? std::optional<bool>(isOn(node)) : std::nullopt;
}

/// A number of an attribute; std::nullopt without one
std::optional<double> numberOf(const QString& text) {
    bool ok = false;
    const double value = text.toDouble(&ok);
    return ok ? std::optional<double>(value) : std::nullopt;
}

/// The ZIP package of the document
class Package {
public:
    Package() = default;
    Package(const Package&) = delete;
    Package& operator=(const Package&) = delete;
    ~Package() {
        if (m_zip) {
            zip_discard(m_zip);
        }
    }

    bool open(const QString& path) {
        int error = 0;
        m_zip = core::openZipFile(core::pathOfText(path.toStdString()), ZIP_RDONLY, &error);
        return m_zip != nullptr;
    }

    /// The bytes of a part; std::nullopt when the package has none of this name
    std::optional<QByteArray> read(const QString& name) const {
        const QByteArray utf8 = name.toUtf8();
        const zip_int64_t index = zip_name_locate(m_zip, utf8.constData(), ZIP_FL_NOCASE);
        if (index < 0) {
            return std::nullopt;
        }
        zip_stat_t stat;
        zip_stat_init(&stat);
        if (zip_stat_index(m_zip, static_cast<zip_uint64_t>(index), 0, &stat) != 0 ||
            !(stat.valid & ZIP_STAT_SIZE) || stat.size > MAX_PART_BYTES) {
            return std::nullopt;
        }
        zip_file_t* file = zip_fopen_index(m_zip, static_cast<zip_uint64_t>(index), 0);
        if (!file) {
            return std::nullopt;
        }
        QByteArray data(static_cast<qsizetype>(stat.size), Qt::Uninitialized);
        const zip_int64_t read = zip_fread(file, data.data(), stat.size);
        zip_fclose(file);
        if (read < 0 || static_cast<zip_uint64_t>(read) != stat.size) {
            return std::nullopt;
        }
        return data;
    }

    std::optional<Node> readXml(const QString& name) const {
        const std::optional<QByteArray> data = read(name);
        return data ? parseXml(*data) : std::nullopt;
    }

private:
    zip_t* m_zip = nullptr;
};

/// A relationship of a part: its type and target
struct Relationship {
    QString type;
    QString target;  ///< A part of the package, or an address (external)
    bool external = false;
};

/// The folder of a part ("word" for "word/document.xml")
QString folderOf(const QString& part) {
    const qsizetype slash = part.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : part.left(slash);
}

/// The relationships of a part, by their ids
QHash<QString, Relationship> relationshipsOf(const Package& package, const QString& part) {
    QHash<QString, Relationship> result;
    const QString folder = folderOf(part);
    const QString name = part.mid(folder.isEmpty() ? 0 : folder.size() + 1);
    const std::optional<Node> rels =
        package.readXml((folder.isEmpty() ? QString() : folder + QLatin1Char('/')) + QStringLiteral("_rels/") + name +
                        QStringLiteral(".rels"));
    if (!rels) {
        return result;
    }
    for (const Node& node : rels->children) {
        if (!node.is("Relationship"_L1)) {
            continue;
        }
        Relationship relationship;
        relationship.type = node.attr("Type"_L1);
        relationship.external = node.attr("TargetMode"_L1).compare(QLatin1String("External"), Qt::CaseInsensitive) == 0;
        QString target = node.attr("Target"_L1);
        if (!relationship.external) {
            QStringList parts = target.startsWith(QLatin1Char('/')) ? QStringList()
                                                                     : folder.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            for (const QString& segment : target.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
                if (segment == QLatin1String("..")) {
                    if (!parts.isEmpty()) {
                        parts.removeLast();
                    }
                } else if (segment != QLatin1String(".")) {
                    parts << segment;
                }
            }
            target = parts.join(QLatin1Char('/'));
        }
        relationship.target = target;
        result.insert(node.attr("Id"_L1), relationship);
    }
    return result;
}

/// The relationship of a part of a type ("…/footnotes")
QString targetOfType(const QHash<QString, Relationship>& rels, QLatin1StringView typeEnd) {
    for (const Relationship& relationship : rels) {
        if (!relationship.external && relationship.type.endsWith(typeEnd)) {
            return relationship.target;
        }
    }
    return QString();
}

// =============================================================================
// Styles and lists
// =============================================================================

/// A paragraph style of the file, as the reader needs it
struct WordStyle {
    QString name;
    QString basedOn;
    int outline = 0;  ///< Its own outline level 1-9; 0: none
    QString numId;    ///< Its own list; empty: none
    int numLevel = 0;
};

/// A character style: its emphasis
struct CharacterStyle {
    QString basedOn;
    const Node* properties = nullptr;  ///< Its w:rPr
};

/// The heading level of a style's name: "heading 1" (Word, LibreOffice, Google Docs), "Nagłówek 1"
int headingLevelOfName(const QString& name) {
    static const QRegularExpression heading(QStringLiteral("^(?:heading|nagłówek|überschrift|titre)\\s*([1-9])$"),
                                            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = heading.match(name.trimmed());
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

/// The bullet of a list's level of Word: its text, the characters of Symbol and Wingdings as
/// the bullets they show
QString bulletOf(const QString& text) {
    if (text.isEmpty()) {
        return QString(QChar(BULLET_DISC));
    }
    const QChar ch = text.at(0);
    switch (ch.unicode()) {
        case 0xF0B7:
        case 0xF0B6:
        case 0x2022:
            return QString(QChar(BULLET_DISC));
        case u'o':
        case 0xF06F:
            return QString(QChar(BULLET_CIRCLE));
        case 0xF0A7:
        case 0xF06E:
        case 0x25AA:
        case 0x25A0:
            return QString(QChar(BULLET_SQUARE));
        case u'-':
        case 0x2013:
        case 0x2014:
        case 0xF02D:
            return QString(QChar(BULLET_DASH));
        case 0xF0FC:
        case 0x2713:
            return QString(QChar(BULLET_CHECK));
        case 0xF0D8:
        case 0x27A2:
            return QString(QChar(BULLET_ARROW));
        default:
            break;
    }
    return ch.unicode() >= 0xE000 && ch.unicode() <= 0xF8FF ? QString(QChar(BULLET_DISC)) : QString(ch);
}

// =============================================================================
// The reader
// =============================================================================

/// The highlight colors of Word
QString highlightColor(const QString& name) {
    static const QHash<QString, QString> colors{
        {QStringLiteral("yellow"), QStringLiteral("#ffff00")},     {QStringLiteral("green"), QStringLiteral("#00ff00")},
        {QStringLiteral("cyan"), QStringLiteral("#00ffff")},       {QStringLiteral("magenta"), QStringLiteral("#ff00ff")},
        {QStringLiteral("blue"), QStringLiteral("#0000ff")},       {QStringLiteral("red"), QStringLiteral("#ff0000")},
        {QStringLiteral("darkBlue"), QStringLiteral("#000080")},   {QStringLiteral("darkCyan"), QStringLiteral("#008080")},
        {QStringLiteral("darkGreen"), QStringLiteral("#008000")},  {QStringLiteral("darkMagenta"), QStringLiteral("#800080")},
        {QStringLiteral("darkRed"), QStringLiteral("#800000")},    {QStringLiteral("darkYellow"), QStringLiteral("#808000")},
        {QStringLiteral("darkGray"), QStringLiteral("#808080")},   {QStringLiteral("lightGray"), QStringLiteral("#c0c0c0")},
        {QStringLiteral("black"), QStringLiteral("#000000")},      {QStringLiteral("white"), QStringLiteral("#ffffff")},
    };
    return colors.value(name);
}

/// A color of Word (RRGGBB) as #rrggbb; empty for "auto" or none
QString colorOf(const QString& value) {
    static const QRegularExpression hex(QStringLiteral("^[0-9A-Fa-f]{6}$"));
    return hex.match(value).hasMatch() ? QLatin1Char('#') + value.toLower() : QString();
}

/// What a field of Word becomes
struct FieldKind {
    enum class Type {
        Text,          ///< Its result stays as text
        Skip,          ///< Nothing of it: the table of contents
        Link,          ///< Its result is the text of a link
        KmlField,      ///< A field of KML
        CaptionNumber  ///< A caption's number
    };
    Type type = Type::Text;
    QString value;   ///< Link: its address; KmlField: its type; CaptionNumber: its label
    QString format;  ///< KmlField: its format
};

FieldKind fieldKindOf(const QString& instruction) {
    FieldKind kind;
    const QString text = instruction.trimmed();
    const QString word = text.section(QLatin1Char(' '), 0, 0, QString::SectionSkipEmpty).toUpper();
    const bool upper = text.contains(QLatin1String("\\* Upper"), Qt::CaseInsensitive);
    const auto kml = [&kind](const QString& type, const QString& format = QString()) {
        kind.type = FieldKind::Type::KmlField;
        kind.value = type;
        kind.format = format;
    };
    if (word == QLatin1String("TOC")) {
        kind.type = FieldKind::Type::Skip;
    } else if (word == QLatin1String("HYPERLINK")) {
        static const QRegularExpression address(QStringLiteral("HYPERLINK\\s+\"([^\"]*)\""), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = address.match(text);
        if (match.hasMatch() && !text.contains(QLatin1String("\\l"))) {
            kind.type = FieldKind::Type::Link;
            kind.value = match.captured(1);
        }
    } else if (word == QLatin1String("PAGE") || word == QLatin1String("NUMPAGES") || word == QLatin1String("SECTIONPAGES")) {
        QString format;
        if (text.contains(QLatin1String("\\* ROMAN"))) {
            format = QStringLiteral("I");
        } else if (text.contains(QLatin1String("\\* roman"))) {
            format = QStringLiteral("i");
        }
        kml(word == QLatin1String("PAGE") ? QStringLiteral("page") : QStringLiteral("pages"), format);
    } else if (word == QLatin1String("NUMWORDS")) {
        kml(QStringLiteral("words"));
    } else if (word == QLatin1String("TITLE")) {
        kml(QStringLiteral("book-title"), upper ? QStringLiteral("upper") : QString());
    } else if (word == QLatin1String("AUTHOR")) {
        kml(QStringLiteral("author"), upper ? QStringLiteral("upper") : QString());
    } else if (word == QLatin1String("DATE") || word == QLatin1String("CREATEDATE") ||
               word == QLatin1String("SAVEDATE") || word == QLatin1String("PRINTDATE")) {
        kml(QStringLiteral("date"));
    } else if (word == QLatin1String("TIME")) {
        kml(QStringLiteral("date"), QStringLiteral("time"));
    } else if (word == QLatin1String("SEQ")) {
        const QString label = text.section(QLatin1Char(' '), 1, 1, QString::SectionSkipEmpty);
        if (!label.isEmpty() && !label.startsWith(QLatin1Char('\\'))) {
            kind.type = FieldKind::Type::CaptionNumber;
            kind.value = QString(label).replace(QLatin1Char('_'), QLatin1Char(' '));
        }
    }
    return kind;
}

/// A complex field of Word open at the place being read
struct OpenField {
    QString instruction;
    bool separated = false;  ///< Its result is being read
    FieldKind kind;
    bool written = false;    ///< Its KML field or number is in the text
};

class DocxReader {
public:
    DocxReader(const Package& package, ImportDocument& document) : m_package(package), m_document(document) {}

    bool read(ImportProblem* problem) {
        const QHash<QString, Relationship> packageRels = relationshipsOf(m_package, QString());
        m_mainPart = targetOfType(packageRels, "/officeDocument"_L1);
        if (m_mainPart.isEmpty()) {
            m_mainPart = QStringLiteral("word/document.xml");
        }
        const std::optional<Node> body = m_package.readXml(m_mainPart);
        if (!body || !body->is("document"_L1)) {
            if (problem) {
                problem->kind = m_package.read(m_mainPart) ? ImportProblem::Kind::Damaged : ImportProblem::Kind::NotDocx;
                problem->detail = QStringLiteral("%1 cannot be read").arg(m_mainPart);
            }
            return false;
        }
        m_rels = relationshipsOf(m_package, m_mainPart);
        readProperties(packageRels);
        readStyles();
        readNumbering();
        readNotes();
        readComments();

        if (const Node* content = body->child("body"_L1)) {
            readContent(*content, m_document.blocks);
        }
        finishStyles();
        return true;
    }

private:
    // -------------------------------------------------------------------------
    // The properties, styles, lists, notes and comments
    // -------------------------------------------------------------------------

    void readProperties(const QHash<QString, Relationship>& packageRels) {
        QString corePart = targetOfType(packageRels, "/core-properties"_L1);
        if (corePart.isEmpty()) {
            corePart = QStringLiteral("docProps/core.xml");
        }
        if (const std::optional<Node> core = m_package.readXml(corePart)) {
            for (const Node& node : core->children) {
                if (node.is("title"_L1)) {
                    m_document.title = node.text.trimmed();
                } else if (node.is("creator"_L1)) {
                    m_document.author = node.text.trimmed();
                } else if (node.is("language"_L1)) {
                    m_document.language = node.text.trimmed().section(QLatin1Char('-'), 0, 0).toLower();
                }
            }
        }
        QString appPart = targetOfType(packageRels, "/extended-properties"_L1);
        if (appPart.isEmpty()) {
            appPart = QStringLiteral("docProps/app.xml");
        }
        if (const std::optional<Node> app = m_package.readXml(appPart)) {
            if (const Node* application = app->child("Application"_L1)) {
                m_document.fromKalahari = application->text.trimmed() == QLatin1String("Kalahari");
            }
        }
    }

    void readStyles() {
        const QString part = targetOfType(m_rels, "/styles"_L1);
        m_stylesXml = m_package.readXml(part.isEmpty() ? QStringLiteral("word/styles.xml") : part);
        if (!m_stylesXml) {
            return;
        }
        for (const Node& node : m_stylesXml->children) {
            if (node.is("docDefaults"_L1)) {
                if (const Node* language = node.find("lang"_L1); language && m_document.language.isEmpty()) {
                    m_document.language = language->attr("val"_L1).section(QLatin1Char('-'), 0, 0).toLower();
                }
                continue;
            }
            if (!node.is("style"_L1)) {
                continue;
            }
            const QString type = node.attr("type"_L1);
            const QString id = node.attr("styleId"_L1);
            if (type == QLatin1String("character")) {
                CharacterStyle style;
                style.basedOn = node.value("basedOn"_L1);
                style.properties = node.child("rPr"_L1);
                m_characterStyles.insert(id, style);
                continue;
            }
            if (type != QLatin1String("paragraph")) {
                continue;
            }
            WordStyle style;
            style.name = node.value("name"_L1);
            style.basedOn = node.value("basedOn"_L1);
            if (const Node* properties = node.child("pPr"_L1)) {
                if (const std::optional<double> outline = numberOf(properties->value("outlineLvl"_L1))) {
                    style.outline = *outline >= 0 && *outline < OUTLINE_BODY_TEXT ? static_cast<int>(*outline) + 1 : -1;
                }
                if (const Node* numbering = properties->child("numPr"_L1)) {
                    style.numId = numbering->value("numId"_L1);
                    style.numLevel = numbering->value("ilvl"_L1).toInt();
                }
            }
            if (node.attr("default"_L1) == QLatin1String("1")) {
                m_defaultStyle = id;
            }
            m_styles.insert(id, style);
        }
        if (m_defaultStyle.isEmpty() && m_styles.contains(QStringLiteral("Normal"))) {
            m_defaultStyle = QStringLiteral("Normal");
        }
    }

    /// The heading level of a style, through its base styles
    int styleHeadingLevel(const QString& id) const {
        QSet<QString> seen;
        QString current = id;
        while (!current.isEmpty() && !seen.contains(current) && m_styles.contains(current)) {
            seen.insert(current);
            const WordStyle& style = m_styles[current];
            if (style.outline != 0) {
                return std::max(0, style.outline);
            }
            if (const int level = headingLevelOfName(style.name); level > 0) {
                return level;
            }
            if (current.compare(QLatin1String("Title"), Qt::CaseInsensitive) == 0) {
                return 0;
            }
            current = style.basedOn;
        }
        return 0;
    }

    /// The list of a style, through its base styles
    std::pair<QString, int> styleList(const QString& id) const {
        QSet<QString> seen;
        QString current = id;
        while (!current.isEmpty() && !seen.contains(current) && m_styles.contains(current)) {
            seen.insert(current);
            const WordStyle& style = m_styles[current];
            if (!style.numId.isEmpty()) {
                return {style.numId, style.numLevel};
            }
            current = style.basedOn;
        }
        return {QString(), 0};
    }

    void readNumbering() {
        const QString part = targetOfType(m_rels, "/numbering"_L1);
        m_numberingXml = m_package.readXml(part.isEmpty() ? QStringLiteral("word/numbering.xml") : part);
        if (!m_numberingXml) {
            return;
        }
        for (const Node& node : m_numberingXml->children) {
            if (node.is("abstractNum"_L1)) {
                m_abstractNums.insert(node.attr("abstractNumId"_L1), &node);
            } else if (node.is("num"_L1)) {
                m_nums.insert(node.attr("numId"_L1), &node);
            }
        }
    }

    /// The list of the document of Word's list numId; -1: none
    int listOf(const QString& numId) {
        if (numId.isEmpty() || numId == QLatin1String("0") || !m_nums.contains(numId)) {
            return -1;
        }
        if (const auto it = m_lists.find(numId); it != m_lists.end()) {
            return it.value();
        }
        const Node* num = m_nums.value(numId);
        const Node* abstract = m_abstractNums.value(num->value("abstractNumId"_L1));
        ListDefinition definition = ListDefinition::numbered();
        std::array<bool, LIST_LEVELS> noNumber{};
        const auto readLevel = [&](const Node& level) {
            const int index = level.attr("ilvl"_L1).toInt();
            if (index < 0 || index >= LIST_LEVELS) {
                return;
            }
            ListLevel& format = definition.levels[static_cast<size_t>(index)];
            const QString numberFormat = level.value("numFmt"_L1);
            const QString text = level.value("lvlText"_L1);
            if (const std::optional<double> start = numberOf(level.value("start"_L1))) {
                format.start = std::max(0, static_cast<int>(*start));
            }
            if (numberFormat == QLatin1String("bullet")) {
                format.format = ListNumberFormat::Bullet;
                format.bullet = bulletOf(text);
                return;
            }
            if (numberFormat == QLatin1String("lowerLetter")) {
                format.format = ListNumberFormat::LowerAlpha;
            } else if (numberFormat == QLatin1String("upperLetter")) {
                format.format = ListNumberFormat::UpperAlpha;
            } else if (numberFormat == QLatin1String("lowerRoman")) {
                format.format = ListNumberFormat::LowerRoman;
            } else if (numberFormat == QLatin1String("upperRoman")) {
                format.format = ListNumberFormat::UpperRoman;
            } else {
                format.format = ListNumberFormat::Decimal;
            }
            noNumber[static_cast<size_t>(index)] = numberFormat == QLatin1String("none");
            const qsizetype last = text.lastIndexOf(QLatin1Char('%'));
            format.suffix = last >= 0 && last + 2 <= text.size() ? text.mid(last + 2) : QStringLiteral(".");
            format.bullet.clear();
        };
        if (abstract) {
            for (const Node& level : abstract->children) {
                if (level.is("lvl"_L1)) {
                    readLevel(level);
                }
            }
        }
        for (const Node& override : num->children) {
            if (!override.is("lvlOverride"_L1)) {
                continue;
            }
            const int index = override.attr("ilvl"_L1).toInt();
            if (const Node* level = override.child("lvl"_L1)) {
                readLevel(*level);
            }
            if (const std::optional<double> start = numberOf(override.value("startOverride"_L1));
                start && index >= 0 && index < LIST_LEVELS) {
                definition.levels[static_cast<size_t>(index)].start = std::max(0, static_cast<int>(*start));
            }
        }
        const int list = static_cast<int>(m_document.lists.size());
        m_document.lists.push_back(definition);
        m_noNumber.push_back(noNumber);
        m_lists.insert(numId, list);
        return list;
    }

    void readNotes() {
        for (const bool endnotes : {false, true}) {
            const QString part = targetOfType(m_rels, endnotes ? "/endnotes"_L1 : "/footnotes"_L1);
            if (part.isEmpty()) {
                continue;
            }
            std::optional<Node>& xml = endnotes ? m_endnotesXml : m_footnotesXml;
            xml = m_package.readXml(part);
            if (!xml) {
                continue;
            }
            QHash<QString, const Node*>& notes = endnotes ? m_endnoteNodes : m_footnoteNodes;
            for (const Node& node : xml->children) {
                const QString type = node.attr("type"_L1);
                if ((node.is("footnote"_L1) || node.is("endnote"_L1)) &&
                    (type.isEmpty() || type == QLatin1String("normal"))) {
                    notes.insert(node.attr("id"_L1), &node);
                }
            }
        }
    }

    void readComments() {
        const QString part = targetOfType(m_rels, "/comments"_L1);
        if (part.isEmpty()) {
            return;
        }
        const std::optional<Node> comments = m_package.readXml(part);
        if (!comments) {
            return;
        }
        QHash<QString, int> byParagraph;  // w14:paraId of a comment's last paragraph
        for (const Node& node : comments->children) {
            if (!node.is("comment"_L1)) {
                continue;
            }
            ImportAnnotation annotation;
            annotation.author = node.attr("author"_L1);
            const QString date = node.attr("date"_L1);
            annotation.created = QDateTime::fromString(date, Qt::ISODate);
            static const QRegularExpression zone(QStringLiteral("(?:Z|[+-]\\d\\d:?\\d\\d)$"));
            if (annotation.created.isValid() && !zone.match(date).hasMatch()) {
                // A time without a time zone: UTC, as KML reads it
                annotation.created = QDateTime(annotation.created.date(), annotation.created.time(), QTimeZone::utc());
            }
            QStringList lines;
            QString lastParagraph;
            for (const Node& paragraph : node.children) {
                if (paragraph.is("p"_L1)) {
                    lines << plainTextOf(paragraph);
                    lastParagraph = paragraph.attrIn(W14, "paraId"_L1);
                }
            }
            annotation.text = lines.join(QLatin1Char('\n')).trimmed();
            const int index = static_cast<int>(m_document.annotations.size());
            m_document.annotations.push_back(annotation);
            m_comments.insert(node.attr("id"_L1), index);
            if (!lastParagraph.isEmpty()) {
                byParagraph.insert(lastParagraph, index);
            }
        }

        // Resolved comments and replies (commentsExtended.xml)
        const QString extendedPart = targetOfType(m_rels, "/commentsExtended"_L1);
        const std::optional<Node> extended = extendedPart.isEmpty() ? std::nullopt : m_package.readXml(extendedPart);
        if (!extended) {
            return;
        }
        QHash<int, int> parentOf;
        for (const Node& node : extended->children) {
            if (!node.is("commentEx"_L1)) {
                continue;
            }
            const int index = byParagraph.value(node.attrIn(W15, "paraId"_L1), -1);
            if (index < 0) {
                continue;
            }
            m_document.annotations[static_cast<size_t>(index)].done =
                node.attrIn(W15, "done"_L1) == QLatin1String("1");
            const int parent = byParagraph.value(node.attrIn(W15, "paraIdParent"_L1), -1);
            if (parent >= 0 && parent != index) {
                parentOf.insert(index, parent);
            }
        }
        // A reply goes into the text of the comment it answers, which its places are of
        std::vector<int> remap(m_document.annotations.size());
        for (size_t i = 0; i < remap.size(); ++i) {
            int root = static_cast<int>(i);
            QSet<int> seen;
            while (parentOf.contains(root) && !seen.contains(root)) {
                seen.insert(root);
                root = parentOf.value(root);
            }
            remap[i] = root;
        }
        for (size_t i = 0; i < remap.size(); ++i) {
            if (remap[i] != static_cast<int>(i)) {
                const ImportAnnotation& reply = m_document.annotations[i];
                ImportAnnotation& comment = m_document.annotations[static_cast<size_t>(remap[i])];
                comment.text += QStringLiteral("\n\n") +
                                (reply.author.isEmpty() ? reply.text : reply.author + QStringLiteral(": ") + reply.text);
            }
        }
        for (auto it = m_comments.begin(); it != m_comments.end(); ++it) {
            it.value() = remap[static_cast<size_t>(it.value())];
        }
    }

    /// The text of a paragraph of a comment
    static QString plainTextOf(const Node& node) {
        QString text;
        for (const Node& child : node.children) {
            if (child.is("t"_L1)) {
                text += child.text;
            } else if (child.is("tab"_L1)) {
                text += QLatin1Char('\t');
            } else if (child.is("br"_L1) || child.is("cr"_L1)) {
                text += QLatin1Char('\n');
            } else if (!child.is("rPr"_L1) && !child.is("pPr"_L1) &&
                       !child.is("del"_L1)) {
                text += plainTextOf(child);
            }
        }
        return text;
    }

    /// The styles the text uses, with their paragraphs, in the order of the file's styles
    void finishStyles() {
        // The program's export: the parts' titles over the texts' titles
        const bool parts = m_document.fromKalahari && m_used.contains(QStringLiteral("PartTitle"));
        QSet<QString> keys;
        for (auto it = m_used.begin(); it != m_used.end(); ++it) {
            keys.insert(it.key());
            // Their base styles too, for the suggestions of the book's styles
            QString base = m_styles.value(it.key()).basedOn;
            while (!base.isEmpty() && !keys.contains(base)) {
                keys.insert(base);
                base = m_styles.value(base).basedOn;
            }
        }
        std::vector<QString> order;
        if (m_stylesXml) {
            for (const Node& node : m_stylesXml->children) {
                const QString id = node.attr("styleId"_L1);
                if (node.is("style"_L1) && keys.contains(id)) {
                    order.push_back(id);
                }
            }
        }
        for (const QString& key : keys) {
            if (std::find(order.begin(), order.end(), key) == order.end()) {
                order.push_back(key);
            }
        }
        for (const QString& key : order) {
            ImportStyle style;
            style.key = key;
            style.name = m_styles.contains(key) ? m_styles[key].name : key;
            style.basedOn = m_styles.value(key).basedOn;
            style.headingLevel = styleHeadingLevel(key);
            if (parts) {
                if (key == QLatin1String("PartTitle")) {
                    style.headingLevel = 1;
                } else if (style.headingLevel > 0) {
                    style.headingLevel = std::min(MAX_HEADING_LEVEL, style.headingLevel + 1);
                }
            }
            style.paragraphs = m_used.value(key);
            m_document.styles.push_back(std::move(style));
        }
    }

    // -------------------------------------------------------------------------
    // The content
    // -------------------------------------------------------------------------

    /// The blocks of the body, of a cell, of a note or of a text box
    void readContent(const Node& node, std::vector<ImportBlock>& blocks) {
        for (const Node& child : node.children) {
            if (child.is("p"_L1)) {
                readParagraph(child, blocks);
            } else if (child.is("tbl"_L1)) {
                readTable(child, blocks);
            } else if (child.is("sdt"_L1)) {
                readStructuredBlock(child, blocks);
            } else if (child.is("customXml"_L1) || child.is("ins"_L1) ||
                       child.is("moveTo"_L1)) {
                readContent(child, blocks);
            } else if (child.is("sectPr"_L1)) {
                noteHeadersAndFooters(child);
            } else if (child.is("AlternateContent"_L1)) {
                if (const Node* choice = child.child("Choice"_L1)) {
                    readContent(*choice, blocks);
                }
            }
        }
    }

    /// A content control: Word's table of contents is left out, any other is its content
    void readStructuredBlock(const Node& node, std::vector<ImportBlock>& blocks) {
        if (const Node* properties = node.child("sdtPr"_L1)) {
            if (const Node* gallery = properties->find("docPartGallery"_L1)) {
                if (gallery->attr("val"_L1).contains(QLatin1String("Table of Contents"), Qt::CaseInsensitive)) {
                    ++m_document.omissions[ImportOmission::TableOfContents];
                    return;
                }
            }
        }
        if (const Node* content = node.child("sdtContent"_L1)) {
            readContent(*content, blocks);
        }
    }

    void noteHeadersAndFooters(const Node& section) {
        if (m_headersNoted) {
            return;
        }
        if (section.child("headerReference"_L1) || section.child("footerReference"_L1)) {
            ++m_document.omissions[ImportOmission::HeaderFooter];
            m_headersNoted = true;
        }
    }

    // -------------------------------------------------------------------------
    // Paragraphs
    // -------------------------------------------------------------------------

    /// The paragraph being read: its blocks (a page break in it ends one) and the paragraphs of
    /// its text boxes
    struct ParagraphState {
        ImportBlock* block = nullptr;
        std::vector<ImportBlock>* blocks = nullptr;
        std::vector<ImportBlock> textBoxes;
        ImportBlock model;  ///< The properties of its blocks
        QString link;
        ImportRevision revision = ImportRevision::None;
        bool inNote = false;
        bool horizontalRule = false;  ///< A shape of a horizontal line (Word's, Pandoc's)
    };

    void readParagraph(const Node& node, std::vector<ImportBlock>& blocks, bool inNote = false) {
        const Node* properties = node.child("pPr"_L1);
        ImportBlock model;
        bool pageBreakBefore = false;
        bool bottomBorder = false;
        bool sideIndents = false;
        std::optional<bool> sectionBreak;
        QString numId;
        int numLevel = 0;
        bool hasNumbering = false;

        model.style = m_defaultStyle;
        if (properties) {
            if (const QString style = properties->value("pStyle"_L1); !style.isEmpty()) {
                model.style = style;
            }
            const QString align = properties->value("jc"_L1);
            if (align == QLatin1String("left") || align == QLatin1String("start")) {
                model.align = QStringLiteral("left");
            } else if (align == QLatin1String("center")) {
                model.align = QStringLiteral("center");
            } else if (align == QLatin1String("right") || align == QLatin1String("end")) {
                model.align = QStringLiteral("right");
            } else if (align == QLatin1String("both") || align == QLatin1String("distribute")) {
                model.align = QStringLiteral("justify");
            }
            if (const std::optional<double> outline = numberOf(properties->value("outlineLvl"_L1));
                outline && *outline >= 0 && *outline < OUTLINE_BODY_TEXT) {
                model.headingLevel = static_cast<int>(*outline) + 1;
            }
            if (const Node* numbering = properties->child("numPr"_L1)) {
                hasNumbering = true;
                numId = numbering->value("numId"_L1);
                numLevel = numbering->value("ilvl"_L1).toInt();
            }
            pageBreakBefore = isOn(properties->child("pageBreakBefore"_L1));
            readParagraphFormat(*properties, model.own);
            if (const Node* borders = properties->child("pBdr"_L1)) {
                const Node* bottom = borders->child("bottom"_L1);
                const QString line = bottom ? bottom->attr("val"_L1) : QString();
                bottomBorder = bottom && line != QLatin1String("none") && line != QLatin1String("nil");
                sideIndents = model.own.leftIndent.value_or(0.0) > 0.0 && model.own.rightIndent.value_or(0.0) > 0.0;
            }
            if (const Node* mark = properties->child("rPr"_L1)) {
                if (mark->child("del"_L1) || mark->child("moveFrom"_L1)) {
                    model.revision = ImportRevision::Deleted;
                    ++m_document.trackedChanges;
                } else if (mark->child("ins"_L1) || mark->child("moveTo"_L1)) {
                    model.revision = ImportRevision::Inserted;
                    ++m_document.trackedChanges;
                }
            }
            if (properties->child("pPrChange"_L1)) {
                ++m_document.trackedChanges;
            }
            if (const Node* section = properties->child("sectPr"_L1)) {
                noteHeadersAndFooters(*section);
                const QString type = section->value("type"_L1);
                sectionBreak = type.isEmpty() || type == QLatin1String("nextPage") || type == QLatin1String("oddPage") ||
                               type == QLatin1String("evenPage");
            }
        }
        // Word's table of contents: its heading goes with it
        if (model.style.compare(QLatin1String("TOCHeading"), Qt::CaseInsensitive) == 0 ||
            m_styles.value(model.style).name.compare(QLatin1String("TOC Heading"), Qt::CaseInsensitive) == 0) {
            skipFieldsOf(node);
            return;
        }
        if (!hasNumbering) {
            const auto [styleNumId, styleLevel] = styleList(model.style);
            numId = styleNumId;
            numLevel = styleLevel;
        }
        model.list = listOf(numId);
        if (model.list >= 0) {
            model.listLevel = std::clamp(numLevel, 0, LIST_LEVELS - 1);
            model.numbered = !m_noNumber[static_cast<size_t>(model.list)][static_cast<size_t>(model.listLevel)];
        }

        if (pageBreakBefore && !inNote) {
            addPageBreak(blocks);
        }
        ParagraphState state;
        state.blocks = &blocks;
        state.model = model;
        state.inNote = inNote;
        const size_t first = blocks.size();
        startBlock(state);
        const bool skipping = skippingResult();
        m_skipped = false;
        readInline(node, state);
        // A paragraph of a field's result left out (the table of contents) goes with it
        if ((skipping || m_skipped) &&
            std::none_of(blocks.begin() + static_cast<std::ptrdiff_t>(first), blocks.end(), hasAnyContent)) {
            blocks.resize(first);
            return;
        }

        // A page break at the end of a paragraph takes the paragraph's mark with it, as Word
        // shows it
        if (blocks.size() > first + 1 && blocks.back().kind == ImportBlock::Kind::Paragraph &&
            blocks.back().runs.empty() && blocks[blocks.size() - 2].kind == ImportBlock::Kind::PageBreak) {
            blocks.pop_back();
        }

        // An empty paragraph with only a bottom border: a line (between margins drawn in: the
        // export's scene break of a line); one with the shape of a line alone
        if ((bottomBorder || state.horizontalRule) && blocks.size() == first + 1 &&
            blocks.back().kind == ImportBlock::Kind::Paragraph && !hasAnyContent(blocks.back())) {
            blocks.back() = ImportBlock();
            blocks.back().kind = bottomBorder && sideIndents ? ImportBlock::Kind::SceneBreak : ImportBlock::Kind::HorizontalLine;
        } else {
            for (size_t i = first; i < blocks.size() && !inNote; ++i) {
                if (blocks[i].kind == ImportBlock::Kind::Paragraph) {
                    ++m_used[blocks[i].style];
                }
            }
        }
        for (ImportBlock& box : state.textBoxes) {
            blocks.push_back(std::move(box));
        }
        if (sectionBreak.value_or(false) && !inNote) {
            addPageBreak(blocks);
        }
    }

    static bool hasAnyContent(const ImportBlock& block) {
        return std::any_of(block.runs.begin(), block.runs.end(), [](const ImportRun& run) {
            return run.kind != ImportRun::Kind::Text || !run.text.trimmed().isEmpty();
        });
    }

    static void addPageBreak(std::vector<ImportBlock>& blocks) {
        if (!blocks.empty() && blocks.back().kind == ImportBlock::Kind::PageBreak) {
            return;
        }
        ImportBlock block;
        block.kind = ImportBlock::Kind::PageBreak;
        blocks.push_back(std::move(block));
    }

    void readParagraphFormat(const Node& properties, ParagraphFormat& own) const {
        const auto points = [](const QString& twips) -> std::optional<qreal> {
            const std::optional<double> value = numberOf(twips);
            return value ? std::optional<qreal>(*value / TWIPS_PER_POINT) : std::nullopt;
        };
        if (const Node* indent = properties.child("ind"_L1)) {
            own.leftIndent = points(indent->hasAttr("start"_L1) ? indent->attr("start"_L1)
                                                                               : indent->attr("left"_L1));
            own.rightIndent = points(indent->hasAttr("end"_L1) ? indent->attr("end"_L1)
                                                                              : indent->attr("right"_L1));
            if (const std::optional<qreal> hanging = points(indent->attr("hanging"_L1))) {
                own.firstLineIndent = -*hanging;
            } else {
                own.firstLineIndent = points(indent->attr("firstLine"_L1));
            }
        }
        if (const Node* spacing = properties.child("spacing"_L1)) {
            own.spaceBefore = points(spacing->attr("before"_L1));
            own.spaceAfter = points(spacing->attr("after"_L1));
            if (const std::optional<double> line = numberOf(spacing->attr("line"_L1))) {
                const QString rule = spacing->attr("lineRule"_L1);
                LineSpacing lineSpacing;
                if (rule == QLatin1String("exact")) {
                    lineSpacing = {LineSpacingRule::Exactly, *line / TWIPS_PER_POINT};
                } else if (rule == QLatin1String("atLeast")) {
                    lineSpacing = {LineSpacingRule::AtLeast, *line / TWIPS_PER_POINT};
                } else {
                    lineSpacing = {LineSpacingRule::Multiple, *line / SINGLE_LINE};
                }
                own.lineSpacing = lineSpacing;
            }
        }
        own.keepWithNext = switchOf(properties.child("keepNext"_L1));
        own.keepTogether = switchOf(properties.child("keepLines"_L1));
        if (const Node* widows = properties.child("widowControl"_L1); widows && !isOn(widows)) {
            own.widowControl = false;
        }
    }

    void startBlock(ParagraphState& state) {
        ImportBlock block = state.model;
        state.blocks->push_back(std::move(block));
        state.block = &state.blocks->back();
    }

    /// A page break in a paragraph: the paragraph goes on after it, without its number; the
    /// paragraph before it is left out when it has nothing in it
    void breakPage(ParagraphState& state) {
        if (state.inNote) {
            return;
        }
        if (state.block == &state.blocks->back() && state.block->kind == ImportBlock::Kind::Paragraph &&
            state.block->runs.empty()) {
            state.blocks->pop_back();
            state.block = nullptr;
        }
        addPageBreak(*state.blocks);
        state.model.numbered = false;
        state.model.value.reset();
        startBlock(state);
    }

    // -------------------------------------------------------------------------
    // Fields
    // -------------------------------------------------------------------------

    bool skippingResult() const {
        return std::any_of(m_fields.begin(), m_fields.end(), [](const OpenField& field) {
            return field.separated && (field.kind.type == FieldKind::Type::Skip ||
                                       field.kind.type == FieldKind::Type::KmlField ||
                                       field.kind.type == FieldKind::Type::CaptionNumber);
        });
    }

    bool inInstruction() const {
        return std::any_of(m_fields.begin(), m_fields.end(), [](const OpenField& field) { return !field.separated; });
    }

    QString fieldLink() const {
        for (auto it = m_fields.rbegin(); it != m_fields.rend(); ++it) {
            if (it->separated && it->kind.type == FieldKind::Type::Link) {
                return it->kind.value;
            }
        }
        return QString();
    }

    void fieldCharacter(const Node& node, ParagraphState& state, const ImportCharFormat& format) {
        const QString type = node.attr("fldCharType"_L1);
        if (type == QLatin1String("begin")) {
            m_fields.emplace_back();
        } else if (type == QLatin1String("separate") && !m_fields.empty()) {
            OpenField& field = m_fields.back();
            field.separated = true;
            field.kind = fieldKindOf(field.instruction);
            if (field.kind.type == FieldKind::Type::Skip && !m_tocNoted) {
                ++m_document.omissions[ImportOmission::TableOfContents];
                m_tocNoted = true;
            }
            writeField(field, state, format);
        } else if (type == QLatin1String("end") && !m_fields.empty()) {
            OpenField& field = m_fields.back();
            if (!field.separated) {
                field.kind = fieldKindOf(field.instruction);
                field.separated = true;
                writeField(field, state, format);
            }
            m_fields.pop_back();
            if (m_fields.empty()) {
                m_tocNoted = false;
            }
        }
    }

    void writeField(OpenField& field, ParagraphState& state, const ImportCharFormat& format) {
        if (field.written || !state.block) {
            return;
        }
        // Only of a field outside the skipped results of others
        const bool outer = std::none_of(m_fields.begin(), m_fields.end() - 1, [](const OpenField& other) {
            return other.separated && (other.kind.type == FieldKind::Type::Skip ||
                                       other.kind.type == FieldKind::Type::KmlField ||
                                       other.kind.type == FieldKind::Type::CaptionNumber);
        });
        if (!outer) {
            return;
        }
        ImportRun run;
        run.format = format;
        run.revision = state.revision;
        if (field.kind.type == FieldKind::Type::KmlField && !state.inNote) {
            run.kind = ImportRun::Kind::Field;
            run.text = field.kind.value;
            run.field = field.kind.format;
        } else if (field.kind.type == FieldKind::Type::CaptionNumber && !state.inNote) {
            run.kind = ImportRun::Kind::CaptionNumber;
            run.text = field.kind.value;
        } else {
            return;
        }
        addRun(state, std::move(run));
        field.written = true;
    }

    /// The fields of a paragraph left out with it, kept in step
    void skipFieldsOf(const Node& node) {
        ParagraphState state;
        std::vector<ImportBlock> scratch;
        state.blocks = &scratch;
        startBlock(state);
        readInline(node, state);
    }

    // -------------------------------------------------------------------------
    // Runs
    // -------------------------------------------------------------------------

    void addRun(ParagraphState& state, ImportRun run) {
        if (!state.block) {
            return;
        }
        if (!state.inNote) {
            run.annotations = m_activeAnnotations;
            for (const int index : m_activeAnnotations) {
                m_annotationsUsed.insert(index);
            }
        }
        if (run.link.isEmpty()) {
            run.link = !state.link.isEmpty() ? state.link : fieldLink();
        }
        if (!run.link.isEmpty() && !isOpenableLinkAddress(run.link)) {
            run.link.clear();
        }
        state.block->runs.push_back(std::move(run));
    }

    void addText(ParagraphState& state, const QString& text, const ImportCharFormat& format) {
        if (text.isEmpty() || inInstruction()) {
            return;
        }
        if (skippingResult()) {
            m_skipped = true;
            return;
        }
        ImportRun run;
        run.text = text;
        run.format = format;
        run.revision = state.revision;
        addRun(state, std::move(run));
    }

    void addObject(ParagraphState& state, ImportRun::Kind kind, const ImportCharFormat& format, int index = -1) {
        if (inInstruction()) {
            return;
        }
        if (skippingResult()) {
            m_skipped = true;
            return;
        }
        ImportRun run;
        run.kind = kind;
        run.format = format;
        run.index = index;
        run.revision = state.revision;
        addRun(state, std::move(run));
    }

    /// The emphasis and formatting of a character style, through its base styles
    void applyCharacterStyle(const QString& id, ImportCharFormat& format, QSet<QString>& seen) const {
        if (id.isEmpty() || seen.contains(id) || !m_characterStyles.contains(id)) {
            return;
        }
        seen.insert(id);
        const CharacterStyle& style = m_characterStyles[id];
        applyCharacterStyle(style.basedOn, format, seen);
        if (style.properties) {
            applyRunProperties(*style.properties, format, false);
        }
    }

    /// The formatting of w:rPr over @p format
    void applyRunProperties(const Node& properties, ImportCharFormat& format, bool direct) const {
        if (direct) {
            QSet<QString> seen;
            applyCharacterStyle(properties.value("rStyle"_L1), format, seen);
        }
        for (const Node& node : properties.children) {
            const QString& name = node.name;
            if (name == QLatin1String("b")) {
                format.bold = isOn(&node);
            } else if (name == QLatin1String("i")) {
                format.italic = isOn(&node);
            } else if (name == QLatin1String("u")) {
                format.underline = isOn(&node);
            } else if (name == QLatin1String("strike") || name == QLatin1String("dstrike")) {
                format.strike = isOn(&node);
            } else if (name == QLatin1String("vertAlign")) {
                const QString value = node.attr("val"_L1);
                format.superscript = value == QLatin1String("superscript");
                format.subscript = value == QLatin1String("subscript");
            } else if (name == QLatin1String("rFonts")) {
                QString font = node.attr("ascii"_L1);
                if (font.isEmpty()) {
                    font = node.attr("hAnsi"_L1);
                }
                if (!font.isEmpty()) {
                    format.font = font;
                }
            } else if (name == QLatin1String("sz")) {
                if (const std::optional<double> halfPoints = numberOf(node.attr("val"_L1))) {
                    format.size = *halfPoints / 2.0;
                }
            } else if (name == QLatin1String("color")) {
                format.color = colorOf(node.attr("val"_L1));
                if (format.color == QLatin1String("#000000")) {
                    format.color.clear();
                }
            } else if (name == QLatin1String("highlight")) {
                format.background = highlightColor(node.attr("val"_L1));
            } else if (name == QLatin1String("shd")) {
                const QString fill = colorOf(node.attr("fill"_L1));
                if (!fill.isEmpty() && fill != QLatin1String("#ffffff")) {
                    format.background = fill;
                }
            } else if (name == QLatin1String("smallCaps")) {
                format.caps = isOn(&node) ? QStringLiteral("small") : QString();
            } else if (name == QLatin1String("caps")) {
                format.caps = isOn(&node) ? QStringLiteral("all") : QString();
            } else if (name == QLatin1String("spacing")) {
                if (const std::optional<double> twips = numberOf(node.attr("val"_L1))) {
                    format.spacing = *twips / TWIPS_PER_POINT;
                }
            }
        }
    }

    /// The inline content of a paragraph, a link, a field, a change...
    void readInline(const Node& node, ParagraphState& state) {
        for (const Node& child : node.children) {
            const QString& name = child.name;
            if (name == QLatin1String("r")) {
                readRun(child, state);
            } else if (name == QLatin1String("hyperlink")) {
                const QString saved = state.link;
                const QString id = child.relationship("id"_L1);
                const Relationship target = m_rels.value(id);
                if (!id.isEmpty() && target.external) {
                    state.link = target.target;
                }
                readInline(child, state);
                state.link = saved;
            } else if (name == QLatin1String("ins") || name == QLatin1String("moveTo") ||
                       name == QLatin1String("del") || name == QLatin1String("moveFrom")) {
                ++m_document.trackedChanges;
                const ImportRevision saved = state.revision;
                state.revision = (name == QLatin1String("ins") || name == QLatin1String("moveTo"))
                                     ? ImportRevision::Inserted
                                     : ImportRevision::Deleted;
                readInline(child, state);
                state.revision = saved;
            } else if (name == QLatin1String("fldSimple")) {
                OpenField field;
                field.instruction = child.attr("instr"_L1);
                field.kind = fieldKindOf(field.instruction);
                field.separated = true;
                if (field.kind.type == FieldKind::Type::Skip) {
                    continue;
                }
                m_fields.push_back(field);
                writeField(m_fields.back(), state, ImportCharFormat());
                readInline(child, state);
                m_fields.pop_back();
            } else if (name == QLatin1String("smartTag") || name == QLatin1String("customXml") ||
                       name == QLatin1String("dir") || name == QLatin1String("bdo")) {
                readInline(child, state);
            } else if (name == QLatin1String("sdt")) {
                if (const Node* content = child.child("sdtContent"_L1)) {
                    readInline(*content, state);
                }
            } else if (name == QLatin1String("commentRangeStart")) {
                if (!state.inNote) {
                    const int index = m_comments.value(child.attr("id"_L1), -1);
                    if (index >= 0 && std::find(m_activeAnnotations.begin(), m_activeAnnotations.end(), index) ==
                                          m_activeAnnotations.end()) {
                        m_activeAnnotations.push_back(index);
                    }
                }
            } else if (name == QLatin1String("commentRangeEnd")) {
                if (!state.inNote) {
                    endComment(m_comments.value(child.attr("id"_L1), -1), state);
                }
            } else if (name == QLatin1String("oMath") || name == QLatin1String("oMathPara")) {
                ++m_document.omissions[ImportOmission::Equation];
            } else if (name == QLatin1String("AlternateContent")) {
                if (const Node* choice = child.child("Choice"_L1)) {
                    readInline(*choice, state);
                }
            }
        }
    }

    void endComment(int index, ParagraphState& state) {
        if (index < 0) {
            return;
        }
        std::erase(m_activeAnnotations, index);
        if (!m_annotationsUsed.contains(index) && !m_pointed.contains(index)) {
            // A comment on no text: on its place
            m_pointed.insert(index);
            ImportRun run;
            run.kind = ImportRun::Kind::Annotation;
            run.index = index;
            if (state.block) {
                state.block->runs.push_back(run);
            }
        }
    }

    void readRun(const Node& node, ParagraphState& state) {
        ImportCharFormat format;
        bool hidden = false;
        if (const Node* properties = node.child("rPr"_L1)) {
            applyRunProperties(*properties, format, true);
            hidden = isOn(properties->child("vanish"_L1)) ||
                     isOn(properties->child("specVanish"_L1));
            if (properties->child("rPrChange"_L1)) {
                ++m_document.trackedChanges;
            }
        }
        for (const Node& child : node.children) {
            const QString& name = child.name;
            if (name == QLatin1String("fldChar")) {
                fieldCharacter(child, state, format);
            } else if (name == QLatin1String("instrText")) {
                if (!m_fields.empty() && !m_fields.back().separated) {
                    m_fields.back().instruction += child.text;
                }
            } else if (hidden) {
                continue;
            } else if (name == QLatin1String("t") || name == QLatin1String("delText")) {
                addText(state, child.text, format);
            } else if (name == QLatin1String("tab") || name == QLatin1String("ptab")) {
                addObject(state, ImportRun::Kind::Tab, format);
            } else if (name == QLatin1String("br")) {
                const QString type = child.attr("type"_L1);
                if (type == QLatin1String("page")) {
                    if (!skippingResult()) {
                        breakPage(state);
                    }
                } else if (type != QLatin1String("column")) {
                    addObject(state, ImportRun::Kind::LineBreak, format);
                }
            } else if (name == QLatin1String("cr")) {
                addObject(state, ImportRun::Kind::LineBreak, format);
            } else if (name == QLatin1String("noBreakHyphen")) {
                addText(state, QString(QChar(0x2011)), format);
            } else if (name == QLatin1String("softHyphen")) {
                addText(state, QString(QChar(0x00AD)), format);
            } else if (name == QLatin1String("sym")) {
                bool ok = false;
                const uint code = child.attr("char"_L1).toUInt(&ok, 16);
                if (ok && code > 0 && (code < 0xE000 || code > 0xF8FF) && code <= 0xFFFF) {
                    addText(state, QString(QChar(static_cast<char16_t>(code))), format);
                }
            } else if (name == QLatin1String("footnoteReference") || name == QLatin1String("endnoteReference")) {
                if (!state.inNote) {
                    const int note = noteOf(name == QLatin1String("endnoteReference"), child.attr("id"_L1));
                    if (note >= 0) {
                        ImportCharFormat noteFormat = format;
                        noteFormat.superscript = false;  // the program raises its numbers
                        addObject(state, ImportRun::Kind::Note, noteFormat, note);
                    }
                }
            } else if (name == QLatin1String("commentReference")) {
                if (!state.inNote) {
                    const int index = m_comments.value(child.attr("id"_L1), -1);
                    if (index >= 0 && !m_annotationsUsed.contains(index) && !m_pointed.contains(index) &&
                        std::find(m_activeAnnotations.begin(), m_activeAnnotations.end(), index) == m_activeAnnotations.end()) {
                        endComment(index, state);
                    }
                }
            } else if (name == QLatin1String("drawing")) {
                readDrawing(child, state, format);
            } else if (name == QLatin1String("pict") || name == QLatin1String("object")) {
                readPicture(child, state, format);
            } else if (name == QLatin1String("AlternateContent")) {
                if (const Node* choice = child.child("Choice"_L1)) {
                    Node run;
                    run.name = QStringLiteral("r");
                    run.children = choice->children;
                    if (const Node* properties = node.child("rPr"_L1)) {
                        run.children.insert(run.children.begin(), *properties);
                    }
                    readRun(run, state);
                }
            } else if (name == QLatin1String("ruby")) {
                if (const Node* base = child.child("rubyBase"_L1)) {
                    readInline(*base, state);
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Notes
    // -------------------------------------------------------------------------

    int noteOf(bool endnote, const QString& id) {
        const QHash<QString, const Node*>& nodes = endnote ? m_endnoteNodes : m_footnoteNodes;
        const Node* node = nodes.value(id);
        if (!node) {
            return -1;
        }
        ImportNote note;
        note.endnote = endnote;
        // The images of a note are not read: the relationships are those of its part
        const auto savedFields = std::move(m_fields);
        m_fields.clear();
        for (const Node& child : node->children) {
            if (child.is("p"_L1)) {
                readParagraph(child, note.paragraphs, true);
            }
        }
        m_fields = savedFields;
        const int index = static_cast<int>(m_document.notes.size());
        m_document.notes.push_back(std::move(note));
        return index;
    }

    // -------------------------------------------------------------------------
    // Images and drawings
    // -------------------------------------------------------------------------

    void readDrawing(const Node& node, ParagraphState& state, const ImportCharFormat& format) {
        if (skippingResult() || inInstruction()) {
            return;
        }
        for (const Node& frame : node.children) {
            if (!frame.is("inline"_L1) && !frame.is("anchor"_L1)) {
                continue;
            }
            const Node* data = frame.find("graphicData"_L1);
            const QString uri = data ? data->attr("uri"_L1) : QString();
            if (uri.endsWith(QLatin1String("/picture"))) {
                if (state.inNote) {
                    continue;
                }
                const Node* blip = data->find("blip"_L1);
                const QString id = blip ? blip->relationship("embed"_L1) : QString();
                ImageProperties look;
                if (const Node* extent = frame.child("extent"_L1)) {
                    if (const std::optional<double> cx = numberOf(extent->attr("cx"_L1)); cx && *cx > 0) {
                        look.unit = ImageWidthUnit::Centimeters;
                        look.width = *cx / EMU_PER_CENTIMETRE;
                    }
                }
                if (const Node* properties = frame.child("docPr"_L1)) {
                    const QStringList lines = properties->attr("descr"_L1).split(QLatin1Char('\n'));
                    look.alt = lines.value(0).trimmed();
                    look.description = lines.mid(1).join(QLatin1Char('\n')).trimmed();
                    static const QRegularExpression generic(QStringLiteral("^(?:Picture|Image|Obraz|Rysunek|Grafika)\\s*\\d*$"),
                                                            QRegularExpression::CaseInsensitiveOption);
                    const QString imageName = properties->attr("name"_L1).trimmed();
                    if (!generic.match(imageName).hasMatch()) {
                        look.name = imageName;
                    }
                    look.decorative = properties->find("decorative"_L1) != nullptr;
                    if (look.decorative) {
                        look.alt.clear();
                    }
                }
                if (const Node* crop = data->find("srcRect"_L1)) {
                    const auto percent = [crop](QLatin1StringView side) {
                        return std::clamp(numberOf(crop->attr(side)).value_or(0.0) / CROP_UNITS_PER_PERCENT, 0.0, MAX_IMAGE_CROP);
                    };
                    look.crop = {percent("l"_L1), percent("t"_L1),
                                 percent("r"_L1), percent("b"_L1)};
                }
                if (const Node* transform = data->find("xfrm"_L1)) {
                    const int rotation = static_cast<int>(numberOf(transform->attr("rot"_L1)).value_or(0.0));
                    const int degrees = ((rotation / ROTATION_UNITS_PER_DEGREE) % 360 + 360) % 360;
                    if (degrees % 90 == 0) {
                        look.rotation = degrees / 90;
                    }
                    look.flipHorizontal = transform->attr("flipH"_L1) == QLatin1String("1");
                    look.flipVertical = transform->attr("flipV"_L1) == QLatin1String("1");
                }
                addImage(state, format, id, look);
            } else if (uri.endsWith(QLatin1String("/chart")) || uri.contains(QLatin1String("chartex")) ||
                       uri.endsWith(QLatin1String("/diagram"))) {
                ++m_document.omissions[ImportOmission::Chart];  // SmartArt with the charts
            } else {
                readShape(frame, state);
            }
        }
    }

    /// A VML picture: its image, or a shape with its text boxes
    void readPicture(const Node& node, ParagraphState& state, const ImportCharFormat& format) {
        if (skippingResult() || inInstruction() || state.inNote) {
            return;
        }
        if (const Node* image = node.find("imagedata"_L1)) {
            QString id = image->relationship("id"_L1);
            if (id.isEmpty()) {
                id = image->relationship("pict"_L1);
            }
            ImageProperties look;
            if (const Node* shape = node.find("shape"_L1)) {
                static const QRegularExpression width(QStringLiteral("width:\\s*([0-9.]+)pt"));
                const QRegularExpressionMatch match = width.match(shape->attr("style"_L1));
                if (match.hasMatch()) {
                    look.unit = ImageWidthUnit::Centimeters;
                    look.width = match.captured(1).toDouble() * 2.54 / 72.0;
                }
                look.alt = shape->attr("alt"_L1).trimmed();
            }
            addImage(state, format, id, look);
            return;
        }
        for (const QLatin1StringView shape : {"rect"_L1, "line"_L1}) {
            if (const Node* line = node.find(shape); line && line->attrIn(VML_OFFICE, "hr"_L1) == QLatin1String("t")) {
                state.horizontalRule = true;
                return;
            }
        }
        readShape(node, state);
    }

    /// A shape: its text boxes' paragraphs after the paragraph
    void readShape(const Node& node, ParagraphState& state) {
        std::vector<const Node*> boxes;
        collectTextBoxes(node, boxes);
        if (boxes.empty()) {
            ++m_document.omissions[ImportOmission::Shape];
            return;
        }
        for (const Node* box : boxes) {
            ++m_document.omissions[ImportOmission::TextBox];
            const auto savedFields = std::move(m_fields);
            m_fields.clear();
            readContent(*box, state.textBoxes);
            m_fields = savedFields;
        }
    }

    static void collectTextBoxes(const Node& node, std::vector<const Node*>& boxes) {
        for (const Node& child : node.children) {
            if (child.is("txbxContent"_L1)) {
                boxes.push_back(&child);
            } else {
                collectTextBoxes(child, boxes);
            }
        }
    }

    void addImage(ParagraphState& state, const ImportCharFormat& format, const QString& id, const ImageProperties& look) {
        const Relationship target = m_rels.value(id);
        if (id.isEmpty() || target.target.isEmpty()) {
            ++m_document.omissions[ImportOmission::Image];
            return;
        }
        ImportImage image;
        image.look = look;
        image.address = target.target;
        image.fileName = QFileInfo(target.target).fileName();
        if (target.external) {
            image.web = target.target.startsWith(QLatin1String("http"), Qt::CaseInsensitive);
            image.missing = !image.web;
        } else {
            QString extension = QFileInfo(target.target).suffix().toLower();
            if (extension == QLatin1String("jpg")) {
                extension = QStringLiteral("jpeg");
            }
            static const QList<QByteArray> readable = QImageReader::supportedImageFormats();
            const bool known = readable.contains(extension.toLatin1()) && extension != QLatin1String("emf") &&
                               extension != QLatin1String("wmf");
            if (!known) {
                ++m_document.omissions[ImportOmission::Image];
                return;
            }
            if (const std::optional<QByteArray> bytes = m_package.read(target.target)) {
                image.data = *bytes;
            } else {
                image.missing = true;
            }
        }
        const int index = static_cast<int>(m_document.images.size());
        m_document.images.push_back(std::move(image));
        addObject(state, ImportRun::Kind::Image, format, index);
    }

    // -------------------------------------------------------------------------
    // Tables
    // -------------------------------------------------------------------------

    void readTable(const Node& node, std::vector<ImportBlock>& blocks) {
        auto table = std::make_shared<ImportTable>();
        if (const Node* properties = node.child("tblPr"_L1)) {
            const QString align = properties->value("jc"_L1);
            if (align == QLatin1String("center")) {
                table->align = QStringLiteral("center");
            } else if (align == QLatin1String("right") || align == QLatin1String("end")) {
                table->align = QStringLiteral("right");
            }
        }
        if (const Node* grid = node.child("tblGrid"_L1)) {
            for (const Node& column : grid->children) {
                if (column.is("gridCol"_L1)) {
                    table->columns.push_back(numberOf(column.attr("w"_L1)).value_or(0.0));
                }
            }
        }
        // The cell that starts a vertical merge in each column of the grid: its row and cell
        std::vector<std::pair<int, int>> merges;
        std::vector<const Node*> rows;
        collectRows(node, rows);
        for (const Node* rowNode : rows) {
            ImportRow row;
            if (const Node* properties = rowNode->child("trPr"_L1)) {
                row.header = isOn(properties->child("tblHeader"_L1));
                if (const Node* height = properties->child("trHeight"_L1)) {
                    row.height = numberOf(height->attr("val"_L1)).value_or(0.0) / TWIPS_PER_POINT;
                }
            }
            const int rowIndex = static_cast<int>(table->rows.size());
            std::vector<const Node*> cells;
            collectCells(*rowNode, cells);
            int column = 0;
            for (const Node* cellNode : cells) {
                ImportCell cell;
                QString merge;
                bool merged = false;
                if (const Node* properties = cellNode->child("tcPr"_L1)) {
                    cell.columnSpan = std::max(1, properties->value("gridSpan"_L1).toInt());
                    if (const Node* vertical = properties->child("vMerge"_L1)) {
                        merged = true;
                        merge = vertical->attr("val"_L1);
                    }
                    if (const Node* shading = properties->child("shd"_L1)) {
                        const QString fill = colorOf(shading->attr("fill"_L1));
                        if (fill != QLatin1String("#ffffff")) {
                            cell.background = fill;
                        }
                    }
                    const QString align = properties->value("vAlign"_L1);
                    if (align == QLatin1String("center")) {
                        cell.valign = QStringLiteral("middle");
                    } else if (align == QLatin1String("bottom")) {
                        cell.valign = QStringLiteral("bottom");
                    }
                }
                if (const size_t span = static_cast<size_t>(column) + static_cast<size_t>(cell.columnSpan);
                    merges.size() < span) {
                    merges.resize(span, {-1, -1});
                }
                if (merged && merge != QLatin1String("restart")) {
                    // A cell that goes on the merge above it
                    const auto [startRow, startCell] = merges[static_cast<size_t>(column)];
                    if (startRow >= 0) {
                        ++table->rows[static_cast<size_t>(startRow)].cells[static_cast<size_t>(startCell)].rowSpan;
                        column += cell.columnSpan;
                        continue;
                    }
                }
                readContent(*cellNode, cell.blocks);
                const int cellIndex = static_cast<int>(row.cells.size());
                for (int c = column; c < column + cell.columnSpan; ++c) {
                    merges[static_cast<size_t>(c)] = merged ? std::make_pair(rowIndex, cellIndex) : std::make_pair(-1, -1);
                }
                column += cell.columnSpan;
                row.cells.push_back(std::move(cell));
            }
            table->rows.push_back(std::move(row));
        }
        // Only the first rows are header rows
        bool header = true;
        for (ImportRow& row : table->rows) {
            header = header && row.header;
            row.header = header;
        }
        ImportBlock block;
        block.kind = ImportBlock::Kind::Table;
        block.table = std::move(table);
        blocks.push_back(std::move(block));
    }

    static void collectRows(const Node& node, std::vector<const Node*>& rows) {
        for (const Node& child : node.children) {
            if (child.is("tr"_L1)) {
                rows.push_back(&child);
            } else if (child.is("sdt"_L1) || child.is("sdtContent"_L1) ||
                       child.is("customXml"_L1)) {
                collectRows(child, rows);
            }
        }
    }

    static void collectCells(const Node& node, std::vector<const Node*>& cells) {
        for (const Node& child : node.children) {
            if (child.is("tc"_L1)) {
                cells.push_back(&child);
            } else if (child.is("sdt"_L1) || child.is("sdtContent"_L1) ||
                       child.is("customXml"_L1)) {
                collectCells(child, cells);
            }
        }
    }

    const Package& m_package;
    ImportDocument& m_document;
    QString m_mainPart;
    QHash<QString, Relationship> m_rels;
    std::optional<Node> m_stylesXml;
    std::optional<Node> m_numberingXml;
    std::optional<Node> m_footnotesXml;
    std::optional<Node> m_endnotesXml;
    QHash<QString, WordStyle> m_styles;
    QHash<QString, CharacterStyle> m_characterStyles;
    QString m_defaultStyle;
    QHash<QString, const Node*> m_abstractNums;
    QHash<QString, const Node*> m_nums;
    QHash<QString, int> m_lists;  ///< The document's list of each numId
    std::vector<std::array<bool, LIST_LEVELS>> m_noNumber;  ///< The levels without numbers
    QHash<QString, const Node*> m_footnoteNodes;
    QHash<QString, const Node*> m_endnoteNodes;
    QHash<QString, int> m_comments;  ///< The annotation of each comment's id
    std::vector<int> m_activeAnnotations;
    QSet<int> m_annotationsUsed;
    QSet<int> m_pointed;
    std::vector<OpenField> m_fields;
    bool m_tocNoted = false;
    bool m_skipped = false;  ///< Text of the paragraph being read was left out with a field
    bool m_headersNoted = false;
    QHash<QString, int> m_used;  ///< The paragraphs of each style
};

}  // namespace

bool readDocx(const QString& path, ImportDocument* document, ImportProblem* problem) {
    if (problem) {
        *problem = ImportProblem();
    }
    Package package;
    if (!package.open(path)) {
        if (problem) {
            problem->kind = QFileInfo::exists(path) ? ImportProblem::Kind::NotDocx : ImportProblem::Kind::CannotOpen;
            problem->detail = QStringLiteral("%1 cannot be opened as a ZIP package").arg(path);
        }
        return false;
    }
    ImportDocument result;
    DocxReader reader(package, result);
    if (!reader.read(problem)) {
        return false;
    }
    if (document) {
        *document = std::move(result);
    }
    return true;
}

}  // namespace kalahari::editor
