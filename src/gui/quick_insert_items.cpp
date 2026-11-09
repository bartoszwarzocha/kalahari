/// @file quick_insert_items.cpp
/// @brief What the list of quick insert offers

#include "kalahari/gui/quick_insert_items.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/kind_names.h"

#include <QCoreApplication>

#include <nlohmann/json.hpp>

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
                          kind.kind->plural.text(language)});
        }
        addCards(element.elements, registry, cards);  // the cards in its groups
    }
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
    // Each kind in one group, the kinds in the order of their first card
    QStringList groups;
    for (const editor::QuickInsertItem& card : cards) {
        if (!groups.contains(card.group)) {
            groups.append(card.group);
        }
    }
    std::stable_sort(cards.begin(), cards.end(),
                     [&groups](const editor::QuickInsertItem& a, const editor::QuickInsertItem& b) {
                         return groups.indexOf(a.group) < groups.indexOf(b.group);
                     });
    return cards;
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

QList<editor::QuickInsertItem> quickInsertItems(QChar trigger) {
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
        auto& projects = core::ProjectManager::getInstance();
        if (const core::BookProject* project = projects.project()) {
            items.append(cardItems(*project, projects.bookTypes()));
        }
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
