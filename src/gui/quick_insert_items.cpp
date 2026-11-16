// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file quick_insert_items.cpp
/// @brief What the list of quick insert offers

#include "kalahari/gui/quick_insert_items.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/quick_insert.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/name_labels.h"

#include <QCoreApplication>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace kalahari::gui {

/// @brief The context of the translations of quick insert
struct QuickInsert {
    Q_DECLARE_TR_FUNCTIONS(QuickInsert)
};

namespace {

/// @brief The kinds of the cards: their elements open in the card editor
constexpr const char* CARD_EDITOR = "card";

/// @brief The kind of an item in the settings: "fragment" or "character"
std::string kindName(editor::QuickInsertSource source) {
    return source == editor::QuickInsertSource::Character ? "character" : "fragment";
}

/// @brief The sources of a character's list in the settings
QStringList sourcesOf(const char* key) {
    QStringList sources;
    for (const std::string& source :
         core::SettingsManager::getInstance().get<std::vector<std::string>>(key)) {
        sources.append(QString::fromStdString(source));
    }
    return sources;
}

void addCards(const QList<core::ProjectElement>& elements, const core::BookTypeRegistry& registry,
              QList<editor::QuickInsertItem>& cards) {
    const QString language = programLanguage();
    for (const core::ProjectElement& element : elements) {
        const core::KindRef kind = core::BookProject::kindOf(registry, element);
        if (kind && kind.kind->form == core::ElementForm::Window &&
            kind.kind->editor == QLatin1String(CARD_EDITOR) && !element.title.trimmed().isEmpty()) {
            const QString title = element.title.trimmed();
            cards.append({editor::QuickInsertSource::Card, title, title,
                          wordsOf(kind).form(QStringLiteral("singular")),
                          kind.kind->plural.text(language), QString(), {}});
        }
        addCards(element.elements, registry, cards);  // the cards in its groups
    }
}

/// @brief Each group in one place, the groups in the order of their first item
void sortByGroup(QList<editor::QuickInsertItem>& items) {
    QStringList groups;
    for (const editor::QuickInsertItem& item : items) {
        if (!groups.contains(item.group)) {
            groups.append(item.group);
        }
    }
    std::ranges::stable_sort(items, std::ranges::less{},
                             [&groups](const editor::QuickInsertItem& item) {
                                 return groups.indexOf(item.group);
                             });
}

/// @brief A form of a name with the cases it is the form of
struct FormCases {
    QString text;
    QStringList cases;
};

/// @brief Add the forms of a cell (named @p label) to @p forms, each once
void addCell(const QStringList& cell, const QString& label, QList<FormCases>& forms) {
    for (const QString& form : cell) {
        if (form.isEmpty()) {
            continue;
        }
        const auto found = std::ranges::find(forms, form, &FormCases::text);
        if (found == forms.end()) {
            forms.append({.text = form, .cases = {label}});
        } else if (!found->cases.contains(label)) {
            found->cases.append(label);
        }
    }
}

/// @brief Whether a table has forms only in the nominative: the forms of a language without
///        cases, named by their number
bool onlyNominative(const core::FormTable& forms) {
    return std::ranges::all_of(core::FormTable::allCases(), [&forms](core::GrammaticalCase cell) {
        return cell == core::GrammaticalCase::Nominative ||
               (forms.cell(cell, false).isEmpty() && forms.cell(cell, true).isEmpty());
    });
}

/// @brief The item of a variant of a name of the dictionary; @p itself: the name itself
editor::QuickInsertItem variantItem(const core::DictionaryName& name,
                                    const core::NameVariant& variant, bool itself) {
    editor::QuickInsertItem item;
    item.source = editor::QuickInsertSource::Name;
    item.name = variant.text.trimmed();
    item.text = item.name;
    item.group = nameKindGroup(name.kind);
    item.owner = itself ? QString() : name.text();
    item.detail = itself ? nameKindLabel(name.kind) : item.owner;
    item.forms = quickInsertForms(variant, name.plural);
    return item;
}

/// @brief The names of the dictionary and the cards of the book open in @p book; a card
///        whose name the dictionary has is left to it
QList<editor::QuickInsertItem> nameItems(const core::ProjectManager& book) {
    const core::BookDictionary& dictionary = book.bookDictionary();
    QList<editor::QuickInsertItem> names = dictionaryItems(dictionary);
    if (const core::BookProject* project = book.project()) {
        for (const editor::QuickInsertItem& card : cardItems(*project, book.bookTypes())) {
            if (dictionary.findNameWithText(card.name) == nullptr) {
                names.append(card);
            }
        }
    }
    sortByGroup(names);
    return names;
}

}  // namespace

editor::QuickInsertItem fragmentItem(editor::QuickInsertSource source, const QString& name,
                                     const QString& text) {
    editor::QuickInsertItem item;
    item.source = source;
    item.name = name;
    item.text = text;
    item.detail = text;
    item.group = source == editor::QuickInsertSource::Character
                     //: Quick insert: the heading of the typographic characters in the list
                     ? QuickInsert::tr("Typographic Characters")
                     //: Quick insert: the heading of the writer's text fragments in the list
                     : QuickInsert::tr("Text Fragments");
    return item;
}

QList<editor::QuickInsertItem> defaultQuickInsertCharacters() {
    using editor::QuickInsertSource;
    const auto character = [](const char* name, const QString& text) {
        return fragmentItem(QuickInsertSource::Character, QuickInsert::tr(name), text);
    };
    const bool polish = programLanguage().startsWith(QLatin1String("pl"));
    return {
        //: Quick insert: the name of a typographic character, typed after # to find it
        character(QT_TRANSLATE_NOOP("QuickInsert", "en dash"), QStringLiteral("–")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "em dash"), QStringLiteral("—")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "ellipsis"), QStringLiteral("…")),
        // The quotation marks of the language of the program: those of Polish, „”, or “”
        character(QT_TRANSLATE_NOOP("QuickInsert", "quotation marks"),
                  polish ? QStringLiteral("„”") : QStringLiteral("“”")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "inner quotation marks"),
                  polish ? QStringLiteral("«»") : QStringLiteral("‘’")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "no-break space"), QStringLiteral(" ")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "non-breaking hyphen"),
                  QStringLiteral("‑")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "section sign"), QStringLiteral("§")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "degree sign"), QStringLiteral("°")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "multiplication sign"), QStringLiteral("×")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "arrow"), QStringLiteral("→")),
        character(QT_TRANSLATE_NOOP("QuickInsert", "copyright sign"), QStringLiteral("©"))};
}

QList<editor::QuickInsertItem> quickInsertFragments() {
    return quickInsertFragmentsOf(
        core::SettingsManager::getInstance().get<nlohmann::json>(quick_insert_keys::ITEMS));
}

QList<editor::QuickInsertItem> quickInsertFragmentsOf(const nlohmann::json& value) {
    if (!value.is_array()) {
        return defaultQuickInsertCharacters();
    }
    QList<editor::QuickInsertItem> items;
    for (const nlohmann::json& entry : value) {
        if (!entry.is_object() || !entry.value("name", nlohmann::json()).is_string() ||
            !entry.value("text", nlohmann::json()).is_string()) {
            continue;
        }
        const QString name = QString::fromStdString(entry["name"].get<std::string>());
        const QString text = QString::fromStdString(entry["text"].get<std::string>());
        if (name.trimmed().isEmpty() || text.isEmpty()) {
            continue;
        }
        const bool character = entry.value("kind", std::string()) == "character";
        items.append(fragmentItem(character ? editor::QuickInsertSource::Character
                                            : editor::QuickInsertSource::Fragment,
                                  name.trimmed(), text));
    }
    return items;
}

nlohmann::json quickInsertFragmentsValue(const QList<editor::QuickInsertItem>& items) {
    nlohmann::json value = nlohmann::json::array();
    for (const editor::QuickInsertItem& item : items) {
        value.push_back({{"name", item.name.toStdString()},
                         {"text", item.text.toStdString()},
                         {"kind", kindName(item.source)}});
    }
    return value;
}

void setQuickInsertFragments(const QList<editor::QuickInsertItem>& items) {
    nlohmann::json value = quickInsertFragmentsValue(items);
    if (value == quickInsertFragmentsValue(defaultQuickInsertCharacters())) {
        value = nullptr;
    }
    core::SettingsManager::getInstance().set<nlohmann::json>(quick_insert_keys::ITEMS, value);
}

QList<editor::QuickInsertItem> cardItems(const core::BookProject& project,
                                         const core::BookTypeRegistry& registry) {
    QList<editor::QuickInsertItem> cards;
    addCards(project.elementsIn(core::BookPlace::Workshop), registry, cards);
    sortByGroup(cards);  // each kind in one group, the kinds in the order of their first card
    return cards;
}

QList<editor::QuickInsertForm> quickInsertForms(const core::NameVariant& variant,
                                                bool withPlural) {
    const bool byNumber = onlyNominative(variant.forms);
    QList<FormCases> found;
    for (const bool inPlural : {false, true}) {
        if (inPlural && !withPlural) {
            break;
        }
        for (const core::GrammaticalCase cell : core::FormTable::allCases()) {
            addCell(variant.forms.cell(cell, inPlural),
                    byNumber ? numberLabel(inPlural) : caseLabel(cell, inPlural), found);
        }
    }
    if (found.size() < 2) {
        return {};  // A single form: nothing to choose
    }
    QList<editor::QuickInsertForm> forms;
    forms.reserve(found.size());
    for (const FormCases& form : found) {
        forms.append({.text = form.text, .cases = form.cases.join(QStringLiteral(", "))});
    }
    return forms;
}

QList<editor::QuickInsertItem> dictionaryItems(const core::BookDictionary& dictionary) {
    QList<editor::QuickInsertItem> items;
    for (const core::DictionaryName& name : dictionary.names()) {
        if (!name.quickInsert) {
            continue;
        }
        bool itself = true;
        for (const core::NameVariant& variant : name.variants) {
            if (!variant.text.trimmed().isEmpty()) {
                items.append(variantItem(name, variant, itself));
            }
            itself = false;
        }
    }
    sortByGroup(items);
    return items;
}

QString quickInsertTriggers() {
    auto& settings = core::SettingsManager::getInstance();
    if (!settings.get<bool>(quick_insert_keys::ENABLED)) {
        return {};
    }
    QString triggers;
    if (!sourcesOf(quick_insert_keys::AT_SOURCES).isEmpty()) {
        triggers.append(QLatin1Char('@'));
    }
    if (!sourcesOf(quick_insert_keys::HASH_SOURCES).isEmpty()) {
        triggers.append(QLatin1Char('#'));
    }
    return triggers;
}

QList<editor::QuickInsertItem> quickInsertItems(const core::ProjectManager& book, QChar trigger) {
    QStringList sources;
    if (trigger == QLatin1Char('@')) {
        sources = sourcesOf(quick_insert_keys::AT_SOURCES);
    } else if (trigger == QLatin1Char('#')) {
        sources = sourcesOf(quick_insert_keys::HASH_SOURCES);
    } else {
        sources = {QString::fromLatin1(quick_insert_keys::SOURCE_CARDS),
                   QString::fromLatin1(quick_insert_keys::SOURCE_FRAGMENTS),
                   QString::fromLatin1(quick_insert_keys::SOURCE_CHARACTERS)};
    }

    QList<editor::QuickInsertItem> items;
    if (sources.contains(QLatin1String(quick_insert_keys::SOURCE_CARDS))) {
        items.append(nameItems(book));
    }
    const bool fragments = sources.contains(QLatin1String(quick_insert_keys::SOURCE_FRAGMENTS));
    const bool characters = sources.contains(QLatin1String(quick_insert_keys::SOURCE_CHARACTERS));
    if (fragments || characters) {
        const QList<editor::QuickInsertItem> stored = quickInsertFragments();
        // The fragments before the characters, each group together
        for (const editor::QuickInsertItem& item : stored) {
            if (fragments && item.source == editor::QuickInsertSource::Fragment) {
                items.append(item);
            }
        }
        for (const editor::QuickInsertItem& item : stored) {
            if (characters && item.source == editor::QuickInsertSource::Character) {
                items.append(item);
            }
        }
    }
    return items;
}

}  // namespace kalahari::gui
