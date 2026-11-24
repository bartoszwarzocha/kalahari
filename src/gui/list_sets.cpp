// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file list_sets.cpp
/// @brief The sets of list formats: built-in, of the book and of the program

#include "kalahari/gui/list_sets.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <nlohmann/json.hpp>

#include <array>
#include <string>

namespace kalahari::gui {

using editor::ListDefinition;
using editor::ListKind;
using editor::ListLevel;
using editor::ListNumberFormat;

namespace {

/// The mark of the files of exported sets, and its version
constexpr const char* EXPORT_MARK = "kalahariListSets";
constexpr int EXPORT_VERSION = 1;

/// More room for the text after a number with the numbers of the levels above: a point
/// for each of them
constexpr qreal SHOWN_LEVEL_ROOM = 9.0;

/// A numbered level with the indents of a new list's
ListLevel numberLevel(int level, ListNumberFormat format, const QString& suffix,
                      const QString& prefix = QString(), int shownLevels = 0)
{
    ListLevel result = ListDefinition::defaultLevel(ListKind::Numbered, level);
    result.format = format;
    result.suffix = suffix;
    result.prefix = prefix;
    result.shownLevels = shownLevels;
    result.textIndent += SHOWN_LEVEL_ROOM * shownLevels + (prefix.isEmpty() ? 0.0 : SHOWN_LEVEL_ROOM);
    return result;
}

/// A bulleted level with the indents of a new list's
ListLevel bulletLevel(int level, char16_t bullet)
{
    ListLevel result = ListDefinition::defaultLevel(ListKind::Bulleted, level);
    result.bullet = QString(QChar(bullet));
    return result;
}

/// A level of the formats a cycle of three repeats: the first, the second and the third
/// level, then again
template <typename Make>
ListDefinition cycled(Make make)
{
    ListDefinition definition;
    for (int level = 0; level < editor::LIST_LEVELS; ++level) {
        definition.levels[static_cast<size_t>(level)] = make(level, level % 3);
    }
    return definition;
}

/// 1) – a) – i)
ListDefinition parenthesesSet()
{
    return cycled([](int level, int cycle) {
        constexpr std::array<ListNumberFormat, 3> FORMATS = {
            ListNumberFormat::Decimal, ListNumberFormat::LowerAlpha, ListNumberFormat::LowerRoman};
        return numberLevel(level, FORMATS.at(static_cast<size_t>(cycle)), QStringLiteral(")"));
    });
}

/// (1) – (a) – (i)
ListDefinition bracketsSet()
{
    return cycled([](int level, int cycle) {
        constexpr std::array<ListNumberFormat, 3> FORMATS = {
            ListNumberFormat::Decimal, ListNumberFormat::LowerAlpha, ListNumberFormat::LowerRoman};
        return numberLevel(level, FORMATS.at(static_cast<size_t>(cycle)), QStringLiteral(")"),
                           QStringLiteral("("));
    });
}

/// 1. – 1.1. – 1.1.1.: every level with the numbers of all the levels above
ListDefinition multilevelSet()
{
    return cycled([](int level, int /*cycle*/) {
        return numberLevel(level, ListNumberFormat::Decimal, QStringLiteral("."), QString(), level);
    });
}

/// I. – A. – 1. – a) – (1) – (a) – i. – a. – i.: an outline
ListDefinition outlineSet()
{
    struct Format {
        ListNumberFormat format;
        const char* prefix;
        const char* suffix;
    };
    constexpr std::array<Format, editor::LIST_LEVELS> FORMATS = {{
        {ListNumberFormat::UpperRoman, "", "."},
        {ListNumberFormat::UpperAlpha, "", "."},
        {ListNumberFormat::Decimal, "", "."},
        {ListNumberFormat::LowerAlpha, "", ")"},
        {ListNumberFormat::Decimal, "(", ")"},
        {ListNumberFormat::LowerAlpha, "(", ")"},
        {ListNumberFormat::LowerRoman, "", "."},
        {ListNumberFormat::LowerAlpha, "", "."},
        {ListNumberFormat::LowerRoman, "", "."},
    }};
    ListDefinition definition;
    for (int level = 0; level < editor::LIST_LEVELS; ++level) {
        const Format& format = FORMATS.at(static_cast<size_t>(level));
        definition.levels[static_cast<size_t>(level)] =
            numberLevel(level, format.format, QString::fromLatin1(format.suffix),
                        QString::fromLatin1(format.prefix));
    }
    return definition;
}

/// § 1. – 1) – a): as in rules and contracts, then the levels of a numbered list
ListDefinition paragraphsSet()
{
    ListDefinition definition = ListDefinition::numbered();
    definition.levels[0] = numberLevel(0, ListNumberFormat::Decimal, QStringLiteral("."),
                                       QStringLiteral("§ "));
    definition.levels[1] = numberLevel(1, ListNumberFormat::Decimal, QStringLiteral(")"));
    definition.levels[2] = numberLevel(2, ListNumberFormat::LowerAlpha, QStringLiteral(")"));
    return definition;
}

/// – – –: en dashes alone, as Polish texts often have them
ListDefinition dashesSet()
{
    return cycled([](int level, int /*cycle*/) { return bulletLevel(level, editor::BULLET_DASH); });
}

/// ✓ – • – ◦
ListDefinition checksSet()
{
    return cycled([](int level, int cycle) {
        constexpr std::array<char16_t, 3> BULLETS = {editor::BULLET_CHECK, editor::BULLET_DISC,
                                                    editor::BULLET_CIRCLE};
        return bulletLevel(level, BULLETS.at(static_cast<size_t>(cycle)));
    });
}

ListSet builtInSet(const char* id, ListSetGroup group, const ListDefinition& definition)
{
    ListSet set;
    set.id = QString::fromLatin1(id);
    set.definition = definition;
    set.place = ListSetPlace::BuiltIn;
    set.group = group;
    return set;
}

/// The setting of the default set of a kind
const char* defaultKey(ListKind kind)
{
    return kind == ListKind::Bulleted ? list_sets::BULLETED_DEFAULT_KEY
                                      : list_sets::NUMBERED_DEFAULT_KEY;
}

}  // namespace

QString ListSet::displayName() const
{
    return name.trimmed().isEmpty() ? editor::listDefinitionSummary(definition) : name;
}

namespace list_sets {

QList<ListSet> builtIn()
{
    return {
        builtInSet(NUMBERED_ID, ListSetGroup::Numbered, ListDefinition::numbered()),
        builtInSet("numbered-parentheses", ListSetGroup::Numbered, parenthesesSet()),
        builtInSet("numbered-brackets", ListSetGroup::Numbered, bracketsSet()),
        builtInSet("multilevel", ListSetGroup::Multilevel, multilevelSet()),
        builtInSet("outline", ListSetGroup::Multilevel, outlineSet()),
        builtInSet("paragraphs", ListSetGroup::Multilevel, paragraphsSet()),
        builtInSet(BULLETED_ID, ListSetGroup::Bulleted, ListDefinition::bulleted()),
        builtInSet("dashes", ListSetGroup::Bulleted, dashesSet()),
        builtInSet("checks", ListSetGroup::Bulleted, checksSet()),
    };
}

QList<ListSet> program()
{
    const nlohmann::json stored =
        core::SettingsManager::getInstance().get<nlohmann::json>(PROGRAM_SETS_KEY, nlohmann::json::array());
    if (!stored.is_array()) {
        return {};
    }
    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromStdString(stored.dump()));
    return fromJson(document.array(), ListSetPlace::Program);
}

void setProgram(const QList<ListSet>& sets)
{
    const QByteArray json = QJsonDocument(toJson(sets)).toJson(QJsonDocument::Compact);
    core::SettingsManager::getInstance().set<nlohmann::json>(
        PROGRAM_SETS_KEY, nlohmann::json::parse(json.toStdString(), nullptr, false));
}

QList<ListSet> ofBook(const core::ProjectManager& book)
{
    QList<ListSet> sets;
    for (const core::BookListSet& own : book.listSets()) {
        if (own.definition.isEmpty()) {
            continue;
        }
        ListSet set;
        set.id = own.id;
        set.name = own.name;
        set.definition = ListDefinition::fromString(own.definition);
        set.place = ListSetPlace::Book;
        sets << set;
    }
    return sets;
}

bool setOfBook(core::ProjectManager& book, const QList<ListSet>& sets)
{
    // The fields this version does not know stay with the sets that stay
    const QList<core::BookListSet> before = book.listSets();
    QList<core::BookListSet> own;
    for (const ListSet& set : sets) {
        core::BookListSet entry;
        for (const core::BookListSet& old : before) {
            if (old.id == set.id) {
                entry = old;
            }
        }
        entry.id = set.id;
        entry.name = set.name;
        entry.definition = set.definition.toString();
        own << entry;
    }
    return book.setListSets(own);
}

QList<ListSet> all(const core::ProjectManager* book)
{
    QList<ListSet> sets = builtIn();
    if (book != nullptr && book->isProjectOpen()) {
        sets << ofBook(*book);
    }
    sets << program();
    return sets;
}

std::optional<ListSet> find(const QString& id, const core::ProjectManager* book)
{
    for (const ListSet& set : all(book)) {
        if (set.id == id) {
            return set;
        }
    }
    return std::nullopt;
}

std::optional<ListSet> matching(const ListDefinition& definition, const QList<ListSet>& sets)
{
    for (const ListSet& set : sets) {
        if (set.definition == definition) {
            return set;
        }
    }
    return std::nullopt;
}

QString defaultId(ListKind kind)
{
    const std::string fallback = kind == ListKind::Bulleted ? BULLETED_ID : NUMBERED_ID;
    return QString::fromStdString(
        core::SettingsManager::getInstance().get<std::string>(defaultKey(kind), fallback));
}

void setDefault(ListKind kind, const QString& id)
{
    core::SettingsManager::getInstance().set<std::string>(defaultKey(kind), id.toStdString());
}

ListDefinition defaultDefinition(ListKind kind, const core::ProjectManager* book)
{
    if (const std::optional<ListSet> set = find(defaultId(kind), book); set && set->kind() == kind) {
        return set->definition;
    }
    return ListDefinition::ofKind(kind);
}

QString newId()
{
    return QStringLiteral("set-") + QUuid::createUuid().toString(QUuid::Id128).left(12);
}

QString groupTitle(const ListSet& set)
{
    if (set.place == ListSetPlace::Book) {
        //: The group of the writer's sets of list formats kept in the book. In Polish: W książce
        return QCoreApplication::translate("ListSets", "In This Book");
    }
    if (set.place == ListSetPlace::Program) {
        //: The group of the writer's sets of list formats kept in the program, for every book.
        //: In Polish: W programie
        return QCoreApplication::translate("ListSets", "In the Program");
    }
    switch (set.group) {
        case ListSetGroup::Numbered:
            //: A group of sets of list formats. In Polish: Numerowane
            return QCoreApplication::translate("ListSets", "Numbered");
        case ListSetGroup::Multilevel:
            //: A group of sets of list formats: 1.1.1., outlines. In Polish: Wielopoziomowe
            return QCoreApplication::translate("ListSets", "Multilevel");
        case ListSetGroup::Bulleted:
            //: A group of sets of list formats. In Polish: Punktowane
            return QCoreApplication::translate("ListSets", "Bulleted");
        case ListSetGroup::Own:
            break;
    }
    //: A group of sets of list formats: the writer's own. In Polish: Własne
    return QCoreApplication::translate("ListSets", "Own");
}

QJsonArray toJson(const QList<ListSet>& sets)
{
    QJsonArray array;
    for (const ListSet& set : sets) {
        QJsonObject entry;
        entry.insert(QStringLiteral("id"), set.id);
        entry.insert(QStringLiteral("name"), set.name);
        entry.insert(QStringLiteral("definition"), set.definition.toString());
        array.append(entry);
    }
    return array;
}

QList<ListSet> fromJson(const QJsonArray& array, ListSetPlace place)
{
    QList<ListSet> sets;
    QSet<QString> ids;
    for (const auto& value : array) {
        const QJsonObject entry = value.toObject();
        const QString definition = entry.value(QStringLiteral("definition")).toString();
        if (definition.isEmpty()) {
            continue;
        }
        ListSet set;
        set.id = entry.value(QStringLiteral("id")).toString();
        if (set.id.isEmpty() || ids.contains(set.id)) {
            set.id = newId();
        }
        ids.insert(set.id);
        set.name = entry.value(QStringLiteral("name")).toString();
        set.definition = ListDefinition::fromString(definition);
        set.place = place;
        sets << set;
    }
    return sets;
}

bool exportTo(const QString& path, const QList<ListSet>& sets)
{
    QJsonObject root;
    root.insert(QLatin1String(EXPORT_MARK), EXPORT_VERSION);
    root.insert(QStringLiteral("sets"), toJson(sets));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        core::Logger::getInstance().warn("list_sets::exportTo: cannot write {}", path.toStdString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

std::optional<QList<ListSet>> importFrom(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() ||
        !document.object().contains(QLatin1String(EXPORT_MARK))) {
        return std::nullopt;
    }
    return fromJson(document.object().value(QStringLiteral("sets")).toArray(), ListSetPlace::Program);
}

}  // namespace list_sets

}  // namespace kalahari::gui
