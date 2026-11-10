/// @file style_draft.cpp
/// @brief A paragraph style of the book as the window of a style changes it

#include "kalahari/gui/style_draft.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/editor_types.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QLocale>
#include <QMetaType>

#include <algorithm>
#include <cmath>
#include <string>

namespace kalahari::gui {

namespace {

/// Numbers the same to a hundredth: of a point, of a line
constexpr double SAME_NUMBER = 0.005;

/// Whether a value is a number (a style's JSON gives whole numbers and decimals)
bool isNumber(const QVariant& value) {
    switch (value.typeId()) {
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Double:
        case QMetaType::Float:
            return true;
        default:
            return false;
    }
}

/// A number as the program writes it: "12", "10,5", "1,15" in Polish
QString numberText(double value) {
    return QLocale().toString(std::round(value * 100.0) / 100.0);
}

QString normalId() {
    return QString::fromLatin1(editor::NORMAL_STYLE_ID);
}

}  // namespace

const QStringList& StyleDraft::propertyNames() {
    static const QStringList names = {
        QStringLiteral("fontFamily"),      QStringLiteral("fontSize"),
        QStringLiteral("bold"),            QStringLiteral("italic"),
        QStringLiteral("underline"),       QStringLiteral("alignment"),
        QStringLiteral("firstLineIndent"), QStringLiteral("leftMargin"),
        QStringLiteral("rightMargin"),     QStringLiteral("spaceBefore"),
        QStringLiteral("spaceAfter"),      QStringLiteral("lineHeight"),
        QStringLiteral("widowControl"),    QStringLiteral("keepWithNext"),
        QStringLiteral("keepTogether"),    QStringLiteral("pageBreakBefore"),
        QStringLiteral("spellCheck")};
    return names;
}

QVariant StyleDraft::defaultValue(const QString& name) {
    if (name == QLatin1String("fontFamily")) {
        // Normal without a font of its own is shown in the editor's font
        return QString::fromStdString(
            core::SettingsManager::getInstance().get<std::string>("editor.fontFamily", "Georgia"));
    }
    if (name == QLatin1String("fontSize")) {
        return editor::DEFAULT_NORMAL_POINT_SIZE;
    }
    if (name == QLatin1String("alignment")) {
        return editor::styleAlignmentName(editor::DEFAULT_PARAGRAPH_ALIGNMENT);
    }
    if (name == QLatin1String("lineHeight")) {
        return 1.0;
    }
    if (name == QLatin1String("widowControl") || name == QLatin1String("spellCheck")) {
        return true;
    }
    if (name == QLatin1String("bold") || name == QLatin1String("italic") ||
        name == QLatin1String("underline") || name == QLatin1String("keepWithNext") ||
        name == QLatin1String("keepTogether") || name == QLatin1String("pageBreakBefore")) {
        return false;
    }
    if (propertyNames().contains(name)) {
        return 0.0;  // the indents and the spacing
    }
    return {};
}

StyleDraft StyleDraft::newStyle(const editor::ParagraphStyleSet& styles, const QString& baseId,
                                const QVariantMap& own, const QString& name) {
    StyleDraft draft;
    draft.m_styles = styles;
    draft.m_kind = Kind::New;
    draft.m_base = styles.contains(baseId) ? baseId : normalId();
    if (!name.trimmed().isEmpty()) {
        draft.m_name = name.trimmed();
    } else {
        // "Style 1", "Style 2"...: the first the book does not have
        for (int number = 1;; ++number) {
            //: The name of a new style of the writer's own, until it is named. In Polish: Styl %1
            const QString candidate = tr("Style %1").arg(number);
            const auto& all = styles.styles();
            if (std::none_of(all.cbegin(), all.cend(),
                             [&candidate](const editor::ParagraphStyle& style) {
                                 return style.name.compare(candidate, Qt::CaseInsensitive) == 0;
                             })) {
                draft.m_name = candidate;
                break;
            }
        }
    }
    draft.setValues(own);
    return draft;
}

StyleDraft StyleDraft::forMissing(const editor::ParagraphStyleSet& styles, const QString& id) {
    // The name the id was made of, as near as the id tells it
    QString name = id.trimmed();
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!name.isEmpty()) {
        name[0] = name.at(0).toUpper();
    }
    StyleDraft draft = newStyle(styles, normalId(), {}, name);
    draft.m_id = id;
    return draft;
}

StyleDraft StyleDraft::existing(const editor::ParagraphStyleSet& styles,
                                const QList<core::PackageParagraphStyle>& typeStyles,
                                const QString& id) {
    const editor::ParagraphStyle* style = styles.find(id);
    if (style == nullptr) {
        return newStyle(styles, normalId());
    }
    StyleDraft draft;
    draft.m_styles = styles;
    draft.m_id = id;
    draft.m_kind = id == normalId()                              ? Kind::Normal
                   : style->origin == editor::StyleOrigin::Own ? Kind::Own
                                                               : Kind::Type;
    draft.m_name = style->name;
    draft.m_base = draft.m_kind == Kind::Normal ? QString() : style->baseStyle;
    draft.m_next = style->nextStyle;
    draft.m_own = style->ownProperties;
    if (draft.isFixed()) {
        const auto type = std::find_if(typeStyles.cbegin(), typeStyles.cend(),
                                       [&id](const core::PackageParagraphStyle& each) {
                                           return each.id == id;
                                       });
        if (type != typeStyles.cend()) {
            draft.m_inType = true;
            draft.m_typeOwn = type->properties;
            draft.m_typeNext = type->nextStyle;
        }
    }
    return draft;
}

void StyleDraft::setName(const QString& name) {
    if (!isFixed()) {
        m_name = name;
    }
}

void StyleDraft::setBaseStyle(const QString& id) {
    if (!isFixed() && (id.isEmpty() || m_styles.contains(id))) {
        m_base = id;
    }
}

void StyleDraft::setNextStyle(const QString& id) {
    m_next = id == m_id ? QString() : id;
}

QVariant StyleDraft::inherited(const QString& name) const {
    if (m_kind != Kind::Normal && !m_base.isEmpty()) {
        if (const editor::ParagraphStyle* base = m_styles.find(m_base)) {
            const QVariant value = base->properties.value(name);
            if (value.isValid()) {
                return value;
            }
        }
    }
    return defaultValue(name);
}

QVariant StyleDraft::value(const QString& name) const {
    const QVariant own = m_own.value(name);
    return own.isValid() ? own : inherited(name);
}

QVariantMap StyleDraft::look() const {
    QVariantMap values;
    for (const QString& name : propertyNames()) {
        values.insert(name, value(name));
    }
    return values;
}

bool StyleDraft::isOwn(const QString& name) const {
    return m_kind != Kind::Normal && m_own.contains(name);
}

void StyleDraft::setValue(const QString& name, const QVariant& value) {
    if (!propertyNames().contains(name) || !value.isValid()) {
        return;
    }
    if (same(value, inherited(name))) {
        m_own.remove(name);
    } else {
        m_own.insert(name, value);
    }
}

void StyleDraft::setValues(const QVariantMap& values) {
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        setValue(it.key(), it.value());
    }
}

QVariantMap StyleDraft::typeLook() const {
    QVariantMap values;
    for (const QString& name : propertyNames()) {
        const QVariant own = m_typeOwn.value(name);
        values.insert(name, own.isValid() ? own : inherited(name));
    }
    return values;
}

bool StyleDraft::canReset() const {
    if (!isFixed()) {
        return !m_own.isEmpty();
    }
    if (m_next != m_typeNext) {
        return true;
    }
    const QVariantMap now = look();
    const QVariantMap type = typeLook();
    return std::any_of(propertyNames().cbegin(), propertyNames().cend(),
                       [&now, &type](const QString& name) {
                           return !same(now.value(name), type.value(name));
                       });
}

void StyleDraft::reset() {
    if (isFixed()) {
        m_own = m_typeOwn;
        m_next = m_typeNext;
    } else {
        m_own.clear();
    }
}

QString StyleDraft::nameOf(const QString& id) const {
    const editor::ParagraphStyle* style = m_styles.find(id);
    return style != nullptr && !style->name.isEmpty() ? style->name : id;
}

QString StyleDraft::summary() const {
    const QString separator = tr("; ");
    QStringList changed;  // what the book changed in a style of the type
    if (isFixed() && canReset()) {
        const QVariantMap now = look();
        const QVariantMap type = typeLook();
        for (const QString& name : propertyNames()) {
            if (!same(now.value(name), type.value(name))) {
                changed.append(phrase(name, now.value(name)));
            }
        }
        if (m_next != m_typeNext) {
            changed.append(m_next.isEmpty()
                               //: What a style changed in this book has: Enter goes on in it
                               ? tr("the next paragraph in the same style")
                               : tr("the next paragraph in the style \"%1\"").arg(nameOf(m_next)));
        }
    }
    //: After the summary of a style of the book's type. In Polish: " W tej książce zmienione: %1."
    const QString changes = changed.isEmpty() ? QString()
                                              : tr(" Changed in this book: %1.")
                                                    .arg(changed.join(separator));

    if (m_kind == Kind::Normal) {
        //: The style Normal: its font and size, alignment, first line and line spacing.
        //: In Polish: Styl podstawowy: %1 %2 pt, %3, %4, %5.
        return tr("The base style: %1 %2 pt, %3, %4, %5.")
                   .arg(value(QStringLiteral("fontFamily")).toString(),
                        numberText(value(QStringLiteral("fontSize")).toDouble()),
                        phrase(QStringLiteral("alignment"), value(QStringLiteral("alignment"))),
                        phrase(QStringLiteral("firstLineIndent"),
                               value(QStringLiteral("firstLineIndent"))),
                        phrase(QStringLiteral("lineHeight"), value(QStringLiteral("lineHeight")))) +
               changes;
    }

    QStringList own;
    for (const QString& name : propertyNames()) {
        if (m_own.contains(name)) {
            own.append(phrase(name, m_own.value(name)));
        }
    }
    QString text;
    if (m_base.isEmpty()) {
        text = own.isEmpty()
                   //: In Polish: Bez stylu bazowego i bez własnych ustawień.
                   ? tr("Without a base style and without settings of its own.")
                   //: In Polish: Bez stylu bazowego: %1.
                   : tr("Without a base style: %1.").arg(own.join(separator));
    } else {
        text = own.isEmpty()
                   //: A style without settings of its own. In Polish: Na razie wszystko jak w
                   //: stylu „%1”.
                   ? tr("Everything as in the style \"%1\" for now.").arg(nameOf(m_base))
                   //: In Polish: Jak styl „%1”, a do tego: %2.
                   : tr("Like the style \"%1\", and also: %2.")
                         .arg(nameOf(m_base), own.join(separator));
    }
    return text + changes;
}

QString StyleDraft::problem() const {
    if (isFixed()) {
        return QString();
    }
    const QString name = m_name.trimmed();
    if (name.isEmpty()) {
        //: In Polish: Nadaj stylowi nazwę.
        return tr("Give the style a name.");
    }
    for (const editor::ParagraphStyle& style : m_styles.styles()) {
        if (style.id != m_id && style.name.compare(name, Qt::CaseInsensitive) == 0) {
            //: In Polish: Książka ma już styl „%1”.
            return tr("The book has a style \"%1\" already.").arg(style.name);
        }
    }
    return QString();
}

std::optional<core::BookParagraphStyle> StyleDraft::entry(const QString& id) const {
    core::BookParagraphStyle entry;
    entry.nextStyle = m_next;
    entry.properties = m_own;
    if (isFixed()) {
        // A style of the type as the type has it needs no entry
        if (!canReset()) {
            return std::nullopt;
        }
        entry.id = m_id;
        return entry;
    }
    entry.id = m_kind == Kind::New ? id : m_id;
    entry.name = m_name.trimmed();
    entry.baseStyle = m_base;
    return entry;
}

bool StyleDraft::same(const QVariant& a, const QVariant& b) {
    if (isNumber(a) && isNumber(b)) {
        return std::abs(a.toDouble() - b.toDouble()) < SAME_NUMBER;
    }
    if (a.typeId() == QMetaType::Bool || b.typeId() == QMetaType::Bool) {
        return a.isValid() && b.isValid() && a.toBool() == b.toBool();
    }
    return a == b;
}

QString StyleDraft::phrase(const QString& name, const QVariant& value) {
    const bool on = value.toBool();
    const double number = value.toDouble();
    if (name == QLatin1String("fontFamily")) {
        //: In Polish: czcionka %1
        return tr("font %1").arg(value.toString());
    }
    if (name == QLatin1String("fontSize")) {
        //: In Polish: rozmiar %1 pt
        return tr("size %1 pt").arg(numberText(number));
    }
    if (name == QLatin1String("bold")) {
        //: In Polish: pogrubienie / bez pogrubienia
        return on ? tr("bold") : tr("not bold");
    }
    if (name == QLatin1String("italic")) {
        //: In Polish: kursywa / bez kursywy
        return on ? tr("italic") : tr("not italic");
    }
    if (name == QLatin1String("underline")) {
        //: In Polish: podkreślenie / bez podkreślenia
        return on ? tr("underlined") : tr("not underlined");
    }
    if (name == QLatin1String("alignment")) {
        switch (editor::styleAlignmentOf(value.toString())) {
            case Qt::AlignLeft:
                //: A paragraph aligned left. In Polish: do lewej
                return tr("aligned left");
            case Qt::AlignHCenter:
                //: In Polish: wyśrodkowany
                return tr("centered");
            case Qt::AlignRight:
                //: In Polish: do prawej
                return tr("aligned right");
            default:
                //: In Polish: wyjustowany
                return tr("justified");
        }
    }
    if (name == QLatin1String("firstLineIndent")) {
        if (number > SAME_NUMBER) {
            //: In Polish: pierwszy wiersz %1
            return tr("first line %1").arg(lengthText(number, LengthUnit::Points));
        }
        if (number < -SAME_NUMBER) {
            //: A hanging indent. In Polish: wysunięcie %1
            return tr("hanging %1").arg(lengthText(-number, LengthUnit::Points));
        }
        //: In Polish: bez wcięcia pierwszego wiersza
        return tr("no first line indent");
    }
    if (name == QLatin1String("leftMargin")) {
        //: In Polish: wcięcie lewe %1
        return tr("left indent %1").arg(lengthText(number, LengthUnit::Points));
    }
    if (name == QLatin1String("rightMargin")) {
        //: In Polish: wcięcie prawe %1
        return tr("right indent %1").arg(lengthText(number, LengthUnit::Points));
    }
    if (name == QLatin1String("spaceBefore")) {
        //: In Polish: odstęp przed %1 pt
        return tr("space before %1 pt").arg(numberText(number));
    }
    if (name == QLatin1String("spaceAfter")) {
        //: In Polish: odstęp po %1 pt
        return tr("space after %1 pt").arg(numberText(number));
    }
    if (name == QLatin1String("lineHeight")) {
        if (std::abs(number - 1.0) < SAME_NUMBER) {
            //: In Polish: interlinia pojedyncza
            return tr("single line spacing");
        }
        if (std::abs(number - 1.5) < SAME_NUMBER) {
            //: In Polish: interlinia 1,5 wiersza
            return tr("line spacing 1.5 lines");
        }
        if (std::abs(number - 2.0) < SAME_NUMBER) {
            //: In Polish: interlinia podwójna
            return tr("double line spacing");
        }
        //: In Polish: interlinia wielokrotna %1
        return tr("line spacing multiple %1").arg(numberText(number));
    }
    if (name == QLatin1String("widowControl")) {
        //: In Polish: kontrola bękartów i wdów / bez kontroli bękartów i wdów
        return on ? tr("widow and orphan control") : tr("no widow and orphan control");
    }
    if (name == QLatin1String("keepWithNext")) {
        //: In Polish: razem z następnym / nie razem z następnym
        return on ? tr("kept with the next") : tr("not kept with the next");
    }
    if (name == QLatin1String("keepTogether")) {
        //: In Polish: wiersze razem / wiersze rozdzielane
        return on ? tr("lines kept together") : tr("lines not kept together");
    }
    if (name == QLatin1String("pageBreakBefore")) {
        //: In Polish: od nowej strony / bez podziału strony
        return on ? tr("on a new page") : tr("no page break before");
    }
    if (name == QLatin1String("spellCheck")) {
        //: In Polish: pisownia sprawdzana / bez sprawdzania pisowni
        return on ? tr("spelling checked") : tr("spelling not checked");
    }
    return QString();
}

}  // namespace kalahari::gui
