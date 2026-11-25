// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file card_names.cpp
/// @brief The names of the cards of the Workshop in the book's dictionary

#include <kalahari/core/card_names.h>

#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/name_declension.h>

#include <QLatin1String>
#include <QList>
#include <QStringList>

#include <algorithm>
#include <ranges>
#include <utility>

namespace kalahari::core {

namespace {

/// @brief A name of the dictionary without a card whose name is @p title, in any case; none:
///        nullptr
const DictionaryName* freeNameWithText(const BookDictionary& dictionary, const QString& title) {
    const QString wanted = title.simplified();
    if (wanted.isEmpty()) {
        return nullptr;
    }
    const QList<DictionaryName>& names = dictionary.names();
    const auto found = std::ranges::find_if(names, [&wanted](const DictionaryName& name) {
        return name.card.isEmpty() && name.text().compare(wanted, Qt::CaseInsensitive) == 0;
    });
    return found != names.cend() ? &*found : nullptr;
}

}  // namespace

NameKind nameKindOf(const ElementKind& kind) {
    if (kind.id == QLatin1String("character")) {
        return NameKind::Character;
    }
    if (kind.id == QLatin1String("location")) {
        return NameKind::Place;
    }
    if (kind.id == QLatin1String("item")) {
        return NameKind::Item;
    }
    return NameKind::Other;
}

DictionaryName nameOfCard(const BookDictionary& dictionary, const QString& cardId,
                          const QString& title, NameKind kind, const NameDeclension& declension,
                          const NameDeclension::DictionaryForms& forms) {
    if (const DictionaryName* own = dictionary.findNameOfCard(cardId)) {
        return *own;
    }
    // A name without a card: the card takes it over with the forms it has, those the author
    // wrote among them; another kind gives the others the forms of that kind
    if (const DictionaryName* free = freeNameWithText(dictionary, title)) {
        DictionaryName name = *free;
        name.card = cardId;
        if (name.kind != kind) {
            name.kind = kind;
            for (NameVariant& variant : name.variants) {
                refreshForms(variant, kind, declension, forms);
            }
        }
        return name;
    }
    DictionaryName name = makeName(title, kind, declension, forms);
    name.card = cardId;
    return name;
}

bool retitleName(DictionaryName& name, const QString& title, const NameDeclension& declension,
                 const NameDeclension::DictionaryForms& forms) {
    const QString text = title.simplified();
    if (text.isEmpty() || text == name.text()) {
        return false;
    }
    DictionaryName fresh = makeName(text, name.kind, declension, forms);
    // The other names of the author stay, unless the new ones have their text
    for (const NameVariant& variant : name.variants | std::views::drop(1)) {
        const bool taken =
            std::ranges::any_of(fresh.variants, [&variant](const NameVariant& made) {
                return made.text.compare(variant.text, Qt::CaseInsensitive) == 0;
            });
        if (!variant.automatic && !taken) {
            fresh.variants.append(variant);
        }
    }
    name.variants = std::move(fresh.variants);
    return true;
}

QString formOfName(const DictionaryName& name, GrammaticalCase grammaticalCase) {
    if (name.variants.isEmpty()) {
        return {};
    }
    const QStringList& cell = name.variants.constFirst().forms.cell(grammaticalCase, false);
    return cell.isEmpty() ? QString() : cell.constFirst();
}

bool nameHas(const DictionaryName& name, const QString& text) {
    const QString wanted = text.simplified();
    if (wanted.isEmpty()) {
        return true;
    }
    return std::ranges::any_of(name.variants, [&wanted, &name](const NameVariant& variant) {
        if (variant.text.contains(wanted, Qt::CaseInsensitive)) {
            return true;
        }
        const QStringList forms = variant.forms.allForms(name.plural);
        return std::ranges::any_of(forms, [&wanted](const QString& form) {
            return form.contains(wanted, Qt::CaseInsensitive);
        });
    });
}

QStringList cardForms(const DictionaryName& name) {
    QStringList forms;
    for (const NameVariant& variant : name.variants) {
        if (!variant.text.isEmpty() && !forms.contains(variant.text)) {
            forms << variant.text;
        }
    }
    for (const QString& form : name.allForms()) {
        if (!forms.contains(form)) {
            forms << form;
        }
    }
    return forms;
}

}  // namespace kalahari::core
