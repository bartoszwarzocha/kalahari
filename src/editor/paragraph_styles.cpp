/// @file paragraph_styles.cpp
/// @brief The paragraph styles of a book and the objects of its text

#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/core/book_type_package.h>
#include <QHash>
#include <QSet>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>

namespace kalahari::editor {

// =============================================================================
// The style of a paragraph
// =============================================================================

QString headingStyleId(int level) {
    return QStringLiteral("heading_%1").arg(level);
}

int headingLevelOf(const QString& styleId) {
    for (int level = 1; level <= HEADING_LEVELS; ++level) {
        if (styleId == headingStyleId(level)) {
            return level;
        }
    }
    return 0;
}

int headingLevelOfElement(QStringView element) {
    if (element.size() != 2 || element.at(0) != QLatin1Char('h')) {
        return 0;
    }
    const int level = element.at(1).digitValue();
    return level >= 1 && level <= HEADING_LEVELS ? level : 0;
}

QString paragraphStyleOf(const QTextBlockFormat& format) {
    const QString id = format.stringProperty(PARAGRAPH_STYLE_PROPERTY);
    return id.isEmpty() ? QString::fromLatin1(NORMAL_STYLE_ID) : id;
}

void setParagraphStyle(QTextBlockFormat& format, const QString& styleId) {
    if (styleId.isEmpty() || styleId == QLatin1String(NORMAL_STYLE_ID)) {
        format.clearProperty(PARAGRAPH_STYLE_PROPERTY);
    } else {
        format.setProperty(PARAGRAPH_STYLE_PROPERTY, styleId);
    }
}

// =============================================================================
// Objects of the text
// =============================================================================

TextObject textObjectOf(const QTextFormat& format) {
    switch (format.objectType()) {
        case HORIZONTAL_RULE_OBJECT: return TextObject::HorizontalRule;
        case SCENE_BREAK_OBJECT: return TextObject::SceneBreak;
        case PAGE_BREAK_OBJECT: return TextObject::PageBreak;
        default: return TextObject::None;
    }
}

QTextCharFormat objectCharFormat(TextObject object) {
    QTextCharFormat format;
    switch (object) {
        case TextObject::HorizontalRule: format.setObjectType(HORIZONTAL_RULE_OBJECT); break;
        case TextObject::SceneBreak: format.setObjectType(SCENE_BREAK_OBJECT); break;
        case TextObject::PageBreak: format.setObjectType(PAGE_BREAK_OBJECT); break;
        case TextObject::None: break;
    }
    return format;
}

TextObject blockObject(const QTextBlock& block) {
    // The object's character and the paragraph separator
    if (!block.isValid() || block.length() != 2) {
        return TextObject::None;
    }
    const QTextBlock::iterator it = block.begin();
    if (it.atEnd()) {
        return TextObject::None;
    }
    const QTextFragment fragment = it.fragment();
    if (fragment.length() != 1 || fragment.text().at(0) != QChar::ObjectReplacementCharacter) {
        return TextObject::None;
    }
    return textObjectOf(fragment.charFormat());
}

QString sceneBreakLookName(SceneBreakLook look) {
    switch (look) {
        case SceneBreakLook::Asterisks: return QStringLiteral("asterisks");
        case SceneBreakLook::Asterism: return QStringLiteral("asterism");
        case SceneBreakLook::Fleuron: return QStringLiteral("fleuron");
        case SceneBreakLook::Line: return QStringLiteral("line");
        case SceneBreakLook::Blank: return QStringLiteral("blank");
    }
    return QStringLiteral("asterisks");
}

SceneBreakLook sceneBreakLookFromName(const QString& name) {
    for (const SceneBreakLook look : {SceneBreakLook::Asterisks, SceneBreakLook::Asterism,
                                      SceneBreakLook::Fleuron, SceneBreakLook::Line,
                                      SceneBreakLook::Blank}) {
        if (name == sceneBreakLookName(look)) {
            return look;
        }
    }
    return SceneBreakLook::Asterisks;
}

QString sceneBreakText(SceneBreakLook look) {
    switch (look) {
        case SceneBreakLook::Asterisks: return QStringLiteral("*\u2002*\u2002*");
        case SceneBreakLook::Asterism: return QString(QChar(0x2042));
        case SceneBreakLook::Fleuron: return QString(QChar(0x2766));
        case SceneBreakLook::Line:
        case SceneBreakLook::Blank: break;
    }
    return {};
}

QString kmlElementOf(TextObject object) {
    switch (object) {
        case TextObject::HorizontalRule: return QStringLiteral("hr");
        case TextObject::SceneBreak: return QStringLiteral("scene-break");
        case TextObject::PageBreak: return QStringLiteral("page-break");
        case TextObject::None: break;
    }
    return {};
}

TextObject textObjectOfElement(const QString& element) {
    if (element == QLatin1String("hr")) {
        return TextObject::HorizontalRule;
    }
    if (element == QLatin1String("scene-break")) {
        return TextObject::SceneBreak;
    }
    if (element == QLatin1String("page-break")) {
        return TextObject::PageBreak;
    }
    return TextObject::None;
}

// =============================================================================
// The styles of a book
// =============================================================================

ParagraphStyleSet::ParagraphStyleSet() {
    ParagraphStyle normal;
    normal.id = QString::fromLatin1(NORMAL_STYLE_ID);
    m_styles.append(normal);
}

ParagraphStyleSet ParagraphStyleSet::fromPackage(
    const QList<core::PackageParagraphStyle>& styles, const QString& language) {
    QHash<QString, const core::PackageParagraphStyle*> byId;
    for (const core::PackageParagraphStyle& style : styles) {
        byId.insert(style.id, &style);
    }

    // A style's properties: those of its base styles, the nearest last (a circle, which the
    // registry does not load, ends where it closes)
    const auto resolved = [&byId](const core::PackageParagraphStyle& style) {
        QList<const core::PackageParagraphStyle*> chain;
        QSet<QString> seen;
        for (const core::PackageParagraphStyle* current = &style;
             current != nullptr && !seen.contains(current->id);
             current = byId.value(current->baseStyle, nullptr)) {
            seen.insert(current->id);
            chain.prepend(current);
        }
        QVariantMap properties;
        for (const core::PackageParagraphStyle* link : chain) {
            for (auto it = link->properties.cbegin(); it != link->properties.cend(); ++it) {
                properties.insert(it.key(), it.value());
            }
        }
        return properties;
    };

    ParagraphStyleSet set;
    set.m_styles.clear();
    for (const core::PackageParagraphStyle& style : styles) {
        ParagraphStyle resolvedStyle;
        resolvedStyle.id = style.id;
        resolvedStyle.name = style.name.text(language);
        if (resolvedStyle.name.isEmpty()) {
            resolvedStyle.name = style.id;
        }
        resolvedStyle.nextStyle = style.nextStyle;
        resolvedStyle.properties = resolved(style);
        set.m_styles.append(resolvedStyle);
    }
    if (!set.contains(QString::fromLatin1(NORMAL_STYLE_ID))) {
        set.m_styles.prepend(ParagraphStyleSet().normal());
    }
    return set;
}

const ParagraphStyle* ParagraphStyleSet::find(const QString& id) const {
    for (const ParagraphStyle& style : m_styles) {
        if (style.id == id) {
            return &style;
        }
    }
    return nullptr;
}

const ParagraphStyle& ParagraphStyleSet::normal() const {
    // The constructor and fromPackage() always give the set a style Normal
    return *find(QString::fromLatin1(NORMAL_STYLE_ID));
}

const ParagraphStyle& ParagraphStyleSet::shownStyle(const QString& id) const {
    const ParagraphStyle* style = find(id);
    return style != nullptr ? *style : normal();
}

QString ParagraphStyleSet::nextStyleOf(const QString& id) const {
    const ParagraphStyle* style = find(id);
    if (style == nullptr) {
        return id;  // a style the book does not have stays
    }
    return style->nextStyle.isEmpty() || !contains(style->nextStyle) ? style->id
                                                                      : style->nextStyle;
}

QVariant ParagraphStyleSet::property(const QString& id, const QString& name) const {
    return shownStyle(id).properties.value(name);
}

bool ParagraphStyleSet::isChecked(const QString& id) const {
    const QVariant check = property(id, QStringLiteral("spellCheck"));
    return !check.isValid() || check.toBool();
}

}  // namespace kalahari::editor
