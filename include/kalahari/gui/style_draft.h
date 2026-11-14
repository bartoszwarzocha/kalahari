// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file style_draft.h
/// @brief A paragraph style of the book as the window of a style changes it

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <optional>

namespace kalahari::gui {

/// @brief A paragraph style as the window of a style shows and changes it: New Style and
///        Modify Style (dialogs::StyleDialog), both of its variants on one draft
///
/// The draft keeps what the style sets itself (its own properties); the rest it shows as its
/// base style has it, and what no style sets as the defaults of the program. A value set to
/// the one of the base style is no longer the style's own. Normal has no base: it sets what
/// differs from the defaults. A style of the book's type keeps its name and base; Reset brings
/// back its properties and next style as the type has them, and a style of the writer's own
/// loses its own properties.
class StyleDraft {
    Q_DECLARE_TR_FUNCTIONS(StyleDraft)

public:
    /// @brief What the window makes or changes
    enum class Kind {
        New,     ///< A new style of the writer's own
        Own,     ///< A style of the writer's own
        Type,    ///< A style of the book's type, changed in this book or not
        Normal   ///< The style Normal: the base of the others
    };

    /// @brief The properties of a style the window sets, in the order the summary names them
    static const QStringList& propertyNames();

    /// @brief The value of a property when no style sets it: the editor's font, 12 points,
    ///        justified, single lines, no indents or spacing, widow control and spell checking
    static QVariant defaultValue(const QString& name);

    /// @brief A new style of the writer's own
    /// @param styles The styles of the book
    /// @param baseId The style it is based on (the one selected); Normal when the book has none
    /// @param own Its own properties: the formatting of the paragraph at the cursor
    ///        (editor::BookEditor::ownStyleProperties()); those the base has the same are not
    ///        its own
    /// @param name Its name; empty: "Style 1", "Style 2"... the first the book does not have
    static StyleDraft newStyle(const editor::ParagraphStyleSet& styles, const QString& baseId,
                               const QVariantMap& own = {}, const QString& name = QString());

    /// @brief A new style for the paragraphs of style @p id, which the book does not have
    ///        (pasted from another book): it keeps their id, so they show it at once, and is
    ///        named after it at first ("inner_voice": "Inner voice"); based on Normal
    static StyleDraft forMissing(const editor::ParagraphStyleSet& styles, const QString& id);

    /// @brief A style the book has
    /// @param styles The styles of the book
    /// @param typeStyles The styles of the book's type (for Reset of a style of the type)
    /// @param id The style
    static StyleDraft existing(const editor::ParagraphStyleSet& styles,
                               const QList<core::PackageParagraphStyle>& typeStyles,
                               const QString& id);

    [[nodiscard]] Kind kind() const { return m_kind; }
    /// @brief The id of the style; empty for a new one, whose id the name gives when it is
    ///        saved, except one for paragraphs of a style the book does not have (forMissing())
    [[nodiscard]] const QString& id() const { return m_id; }
    [[nodiscard]] const QString& name() const { return m_name; }
    [[nodiscard]] const QString& baseStyle() const { return m_base; }
    [[nodiscard]] const QString& nextStyle() const { return m_next; }
    [[nodiscard]] const QVariantMap& ownProperties() const { return m_own; }
    [[nodiscard]] const editor::ParagraphStyleSet& styles() const { return m_styles; }

    /// @brief Whether the name and the base can change: only of a style of the writer's own
    [[nodiscard]] bool isFixed() const { return m_kind == Kind::Type || m_kind == Kind::Normal; }

    /// @brief Name a style of the writer's own (a style of the type keeps its name)
    void setName(const QString& name);

    /// @brief Base it on another style of the book (possibleBaseStyles()); its own properties
    ///        stay its own
    void setBaseStyle(const QString& id);

    /// @brief The style Enter starts after a paragraph of it; empty: the same
    void setNextStyle(const QString& id);

    /// @brief A property as the style has it: its own, else its base style's, else the default
    [[nodiscard]] QVariant value(const QString& name) const;

    /// @brief Every property of propertyNames() as the style has it
    [[nodiscard]] QVariantMap look() const;

    /// @brief Whether the style sets a property itself, over its base (Normal: never, it has
    ///        no base)
    [[nodiscard]] bool isOwn(const QString& name) const;

    /// @brief Set a property: the style's own unless its base style gives it the same
    void setValue(const QString& name, const QVariant& value);

    /// @brief Set several properties
    void setValues(const QVariantMap& values);

    /// @brief Whether Reset changes anything: a style of the type looks or goes on otherwise
    ///        than the type has it; a style of the writer's own has properties of its own
    [[nodiscard]] bool canReset() const;

    /// @brief A style of the type as the type has it; a style of the writer's own without its
    ///        own properties
    void reset();

    /// @brief One sentence of what the style is: how it differs from its base style and, for
    ///        a style of the type, what this book changed in it
    [[nodiscard]] QString summary() const;

    /// @brief Why the style cannot be saved as it is: an empty name, a name another style of
    ///        the book has; empty when it can
    [[nodiscard]] QString problem() const;

    /// @brief The entry the book keeps for the style (core::BookProject::paragraphStyles):
    ///        nothing for a style of the type as the type has it
    /// @param id The id of a new style (paragraphStyleIdFor()); ignored for the others
    [[nodiscard]] std::optional<core::BookParagraphStyle> entry(
        const QString& id = QString()) const;

    /// @brief A property of a style in words, as the summary says it: "italic", "left indent
    ///        6.4 mm", "line spacing 1.5 lines"
    [[nodiscard]] static QString phrase(const QString& name, const QVariant& value);

    /// @brief Whether two values of a property are the same (numbers to a hundredth)
    [[nodiscard]] static bool same(const QVariant& a, const QVariant& b);

private:
    StyleDraft() = default;

    /// @brief A property as the base style has it, else the default
    [[nodiscard]] QVariant inherited(const QString& name) const;

    /// @brief The look of the style as the book's type has it (a style of the type)
    [[nodiscard]] QVariantMap typeLook() const;

    /// @brief The name of style @p id in the book
    [[nodiscard]] QString nameOf(const QString& id) const;

    editor::ParagraphStyleSet m_styles;
    Kind m_kind = Kind::New;
    QString m_id;
    QString m_name;
    QString m_base;
    QString m_next;
    QVariantMap m_own;
    QVariantMap m_typeOwn;  ///< The style's own properties in the type (Kind::Type, Normal)
    QString m_typeNext;     ///< Its next style in the type
    bool m_inType = false;  ///< The type has the style (Kind::Type, Normal)
};

}  // namespace kalahari::gui
