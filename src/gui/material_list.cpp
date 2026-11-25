// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list.cpp
/// @brief The lists of the materials of the Workshop on their tabs

#include "kalahari/gui/material_list.h"

#include "kalahari/core/bibliography.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/clipping.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/source_description.h"
#include "kalahari/core/volume_materials.h"
#include "kalahari/gui/card_list.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/material_source.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/source_names.h"
#include "kalahari/gui/volume_names.h"

#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QUrl>

#include <nlohmann/json.hpp>

#include <algorithm>

namespace kalahari::gui {

namespace {

using json = nlohmann::json;

/// Bytes in a megabyte, from which a size has one decimal place, as MaterialFile shows it
constexpr qint64 MEGABYTE = 1024 * 1024;

/// What a list shows of a file: what MaterialFile reads of it, kept while the file is the same
/// (its time and size), so that a list read again does not read its pictures again
MaterialFile fileOf(const QString& path) {
    struct Kept {
        QDateTime modified;
        qint64 bytes = 0;
        MaterialFile file;
    };
    static QHash<QString, Kept> kept;
    const QFileInfo info(path);
    const auto found = kept.constFind(path);
    if (found != kept.constEnd() && found->modified == info.lastModified() &&
        found->bytes == info.size() && found->file.exists == info.isFile()) {
        return found->file;
    }
    MaterialFile file = MaterialFile::of(path);
    kept.insert(path, {info.lastModified(), info.size(), file});
    return file;
}

/// The column @p id with @p title, shown at first or not
MaterialColumn column(const char* id, const QString& title, bool shown = true) {
    return {QString::fromLatin1(id), title, shown, true};
}

/// The id of kind @p kind of the base package, else empty: a kind of another package has the
/// columns of any kind
QString baseKindOf(const core::KindRef& kind) {
    return kind && kind.package != nullptr &&
                   kind.package->id == QLatin1String(core::ProjectManager::BASE_PACKAGE)
               ? kind.kind->id
               : QString();
}

/// The kind of list @p list, of the packages installed with the program
core::KindRef kindOfList(const QString& list) {
    const std::optional<core::KindReference> reference = core::KindReference::parse(list);
    return reference ? core::ProjectManager::installedBookTypes().findKind(
                           reference->packageId, reference->kindId)
                     : core::KindRef{};
}

/// The kinds of the Materials, in the order of the Navigator: those of the book's type, then
/// those of the elements of the Workshop of other types and of earlier versions
QList<core::KindRef> materialKinds(const core::ProjectManager& book) {
    QList<core::KindRef> kinds;
    const auto& pm = book;
    const core::BookProject* project = pm.project();
    if (project == nullptr) {
        return kinds;
    }
    // The texts for publishers are on the card For Publishers, not in a list of the Materials
    const core::KindRef publishing = pm.publishingKind();
    const auto add = [&kinds, &publishing](const core::KindRef& kind) {
        if (kind && kind.kind->workshopGroup == core::WorkshopGroup::Resources &&
            !(publishing && kind.kind == publishing.kind) &&
            std::none_of(kinds.cbegin(), kinds.cend(),
                         [&kind](const core::KindRef& known) { return known.kind == kind.kind; })) {
            kinds.append(kind);
        }
    };
    for (const core::KindRef& kind : project->kindsIn(pm.bookTypes(), core::BookPlace::Workshop)) {
        // As the Navigator: a kind with no elements only while the program makes them
        if (core::ProjectManager::canAdd(kind)) {
            add(kind);
        }
    }
    for (const core::ProjectElement& element : project->workshop.elements) {
        add(pm.kindOf(element));
    }
    return kinds;
}

/// The elements of the Workshop of the open book that list @p list has, in their order
QList<const core::ProjectElement*> elementsOf(const core::ProjectManager& book,
                                              const QString& list) {
    QList<const core::ProjectElement*> elements;
    const auto& pm = book;
    const core::BookProject* project = pm.project();
    if (project == nullptr) {
        return elements;
    }
    const bool sources = list == QLatin1String(MaterialList::SOURCES);
    // The texts for publishers are on the card For Publishers (materialKinds())
    const core::KindRef publishing = pm.publishingKind();
    for (const core::ProjectElement& element : project->workshop.elements) {
        const core::KindRef kind = pm.kindOf(element);
        if (kind && kind.kind->workshopGroup == core::WorkshopGroup::Resources &&
            !(publishing && kind.kind == publishing.kind) &&
            (sources ? kind.kind->isMaterial() && !element.source.isEmpty()
                     : list.isEmpty() || kind.reference() == list)) {
            elements << &element;
        }
    }
    return elements;
}

/// The text of the links of @p element of @p book: what each names and how, one after another
QString linksText(const core::ProjectManager& book, const core::ProjectElement& element) {
    QStringList links;
    for (const core::ElementLink& link : element.links) {
        const QString title = linkTitle(book, link.to);
        if (title.isEmpty()) {
            continue;
        }
        const QString role = linkRoleName(link.role);
        //: A link in the list of materials: %1 what it names, %2 its role. In Polish: %1 – %2
        links << (role.isEmpty() ? title : MaterialList::tr("%1 – %2").arg(title, role));
    }
    return links.join(QStringLiteral("; "));
}

/// The host of @p url without "www."
QString siteOf(const QString& url) {
    QString host = QUrl(url).host();
    if (host.startsWith(QLatin1String("www."))) {
        host.remove(0, 4);
    }
    return host;
}

/// The size @p bytes of a file in a few characters: "245 KB", "1.2 MB"
QString sizeText(qint64 bytes) {
    return QLocale().formattedDataSize(bytes, bytes < MEGABYTE ? 0 : 1,
                                       QLocale::DataSizeTraditionalFormat);
}

/// The details of a material of All Materials in a few words, by its kind: the words of a
/// quote, the site of a page, the format and the size of a file...
QString detailsOf(const MaterialRow& row, const QString& kindId) {
    const auto text = [&row](const char* id) { return row.texts.value(QString::fromLatin1(id)); };
    const auto joined = [](const QStringList& parts) {
        QStringList shown;
        for (const QString& part : parts) {
            if (!part.isEmpty()) {
                shown << part;
            }
        }
        return shown.join(QStringLiteral(" · "));
    };
    if (row.text) {
        //: The details of a working note in All Materials: %1 its number of words. In Polish:
        //: Słowa: %1
        return MaterialList::tr("Words: %1").arg(text(MaterialList::WORDS));
    }
    if (kindId == QLatin1String(core::MaterialKinds::QUOTE)) {
        return text(MaterialList::QUOTE);
    }
    if (kindId == QLatin1String(core::MaterialKinds::COVER)) {
        return joined({text(MaterialList::COVER_PART), row.body.isEmpty()
                                                          ? text(MaterialList::DIMENSIONS)
                                                          : row.body});
    }
    if (kindId == QLatin1String(core::MaterialKinds::WEB_PAGE)) {
        return text(MaterialList::SITE);
    }
    if (kindId == QLatin1String(core::MaterialKinds::PICTURE)) {
        return joined({text(MaterialList::FORMAT), text(MaterialList::DIMENSIONS),
                       text(MaterialList::ORIGIN)});
    }
    if (!text(MaterialList::FORMAT).isEmpty() || !text(MaterialList::SIZE).isEmpty()) {
        return joined({text(MaterialList::FORMAT), text(MaterialList::SIZE)});
    }
    return text(MaterialList::DESCRIPTION);
}

/// What a material of the cover shows in the columns of its list: its text or title, its part,
/// author, place and state, and the length of its text; the size of a picture is set with the
/// file
void setCoverTexts(MaterialRow& row, const core::ProjectElement& element,
                   const core::CoverParts& parts) {
    using core::CoverMaterial;
    const core::CoverData& cover = element.cover;
    const auto set = [&row](const char* id, const QString& text) {
        if (!text.isEmpty()) {
            row.texts.insert(QString::fromLatin1(id), text);
        }
    };
    row.group = cover.part;
    row.state = cover.state;
    row.place = cover.place;
    const bool text = CoverMaterial::isText(cover.part) ||
                      (!CoverMaterial::isPicture(cover.part) && !element.description.isEmpty());
    if (text) {
        row.body = element.description.simplified();
    }
    set(MaterialList::COVER_TEXT, row.body.isEmpty() ? element.title : row.body);
    row.order.insert(QString::fromLatin1(MaterialList::COVER_PART),
                     CoverMaterial::parts().contains(cover.part)
                         ? CoverMaterial::parts().indexOf(cover.part)
                         : CoverMaterial::parts().size());
    set(MaterialList::COVER_PART, CoverNames::partName(cover.part));
    set(MaterialList::COVER_AUTHOR, cover.author);
    if (CoverMaterial::goesOnCover(cover.part) || !cover.place.isEmpty()) {
        // A place on a part of the cover turned off says so: the material waits for it
        const QString off = CoverNames::partOffText(cover.place, parts);
        set(MaterialList::COVER_PLACE,
            off.isEmpty()
                ? CoverNames::placeName(cover.place)
                //: The place of a material of the cover on a part turned off, in its list: %1
                //: the place, %2 the part turned off. In Polish: %1 (%2)
                : MaterialList::tr("%1 (%2)").arg(CoverNames::placeName(cover.place), off));
        row.order.insert(QString::fromLatin1(MaterialList::COVER_PLACE),
                         CoverMaterial::places().contains(cover.place)
                             ? CoverMaterial::places().indexOf(cover.place)
                             : CoverMaterial::places().size());
    }
    QString state = CoverNames::shortStateName(cover.state);
    if (cover.part == QLatin1String(CoverMaterial::OPINION) && !cover.consent.isEmpty()) {
        //: The state of an opinion with its consent to print in the list of the cover: %1 the
        //: state, %2 the consent. In Polish: %1 · zgoda: %2
        state = MaterialList::tr("%1 · consent: %2")
                    .arg(state, CoverNames::shortConsentName(cover.consent));
    }
    set(MaterialList::COVER_STATE, state);
    // A draft first, then those chosen, then those rejected
    row.order.insert(QString::fromLatin1(MaterialList::COVER_STATE),
                     cover.state == QLatin1String(CoverMaterial::CHOSEN)     ? 1
                     : cover.state == QLatin1String(CoverMaterial::REJECTED) ? 2
                                                                             : 0);
    if (text) {
        const bool words = CoverMaterial::countsWords();
        const int length = CoverMaterial::lengthOf(element.description, words);
        set(MaterialList::COVER_LENGTH, CoverNames::lengthText(length, words));
        row.order.insert(QString::fromLatin1(MaterialList::COVER_LENGTH), length);
    }
    set(MaterialList::COVER_SOURCE, cover.source);
    set(MaterialList::COVER_DATE, cover.date);
    set(MaterialList::COVER_LICENSE, cover.license);
}

/// Where @p quote comes from, as the column of the source of the quotes shows it: the short
/// name of its source card (or its author and work) and its page
QString quoteSourceText(const core::ProjectManager& book, const core::QuoteSource& quote) {
    QString from;
    if (const core::ProjectElement* material =
            quote.source.isEmpty() ? nullptr : book.findElement(quote.source)) {
        from = SourceNames::shortName(material->source, material->title);
    } else {
        QStringList parts;
        for (const QString& part : {quote.author, quote.work}) {
            if (!part.isEmpty()) {
                parts << part;
            }
        }
        from = parts.join(QStringLiteral(", "));
    }
    if (from.isEmpty() || quote.page.isEmpty()) {
        return from;
    }
    //: The source of a quote in the list of the quotes: %1 the source, %2 the page. In
    //: Polish: %1, s. %2
    return MaterialList::tr("%1, p. %2").arg(from, quote.page);
}

/// What a source shows in the columns of the sources, and a quote in that of its source
void setSourceTexts(const core::ProjectManager& book, MaterialRow& row,
                    const core::ProjectElement& element) {
    const auto set = [&row](const char* id, const QString& text) {
        if (!text.isEmpty()) {
            row.texts.insert(QString::fromLatin1(id), text);
        }
    };
    const core::SourceDescription& source = element.source;
    if (!source.isEmpty()) {
        set(MaterialList::SOURCE_KIND, SourceNames::kindName(source.kind()));
        set(MaterialList::SOURCE_AUTHORS, SourceNames::authorsOf(source));
        set(MaterialList::SOURCE_YEAR, source.value(core::SourceDescription::YEAR));
        const int places = book.placesInText(element.id);
        set(MaterialList::SOURCE_INCLUSION, SourceNames::inclusionName(source.bibliography));
        set(MaterialList::SOURCE_PLACES, places > 0 ? QLocale().toString(places)
                                                    : QStringLiteral("–"));
        row.order.insert(QString::fromLatin1(MaterialList::SOURCE_PLACES), places);
    }
    set(MaterialList::QUOTE_SOURCE, quoteSourceText(book, element.quote));
}

/// The row of @p element, of kind @p kind
MaterialRow rowOf(const core::ProjectManager& book, const core::ProjectElement& element,
                  const core::KindRef& kind) {
    const auto& pm = book;
    MaterialRow row;
    row.id = element.id;
    row.kind = kind.reference();
    row.iconId = NavigatorPanel::iconIdOf(pm.bookTypes(), element);
    row.text = !kind.kind->isMaterial();
    row.rotation = element.rotation;
    row.links = element.linkTargets();
    const auto set = [&row](const char* id, const QString& text) {
        if (!text.isEmpty()) {
            row.texts.insert(QString::fromLatin1(id), text);
        }
    };
    const auto order = [&row](const char* id, const QVariant& value) {
        row.order.insert(QString::fromLatin1(id), value);
    };

    set(MaterialList::TITLE, element.title);
    set(MaterialList::TYPE, kind.kind->plural.text(programLanguage()));
    set(MaterialList::DESCRIPTION, element.description.simplified());
    set(MaterialList::LINKS, linksText(book, element));
    for (const core::ElementField& field : element.fields) {
        const QString name = field.name.trimmed();
        if (!name.isEmpty()) {
            row.texts.insert(QString::fromLatin1(MaterialList::FIELD_PREFIX) + name,
                             field.value.simplified());
        }
    }

    // The file: its name, format, size and time, a picture's size and origin
    const QString path = pm.filePathOf(element);
    if (!path.isEmpty()) {
        const QFileInfo info(path);
        if (info.isFile()) {
            const QDateTime modified = info.lastModified();
            set(MaterialList::MODIFIED, QLocale().toString(modified, QLocale::ShortFormat));
            order(MaterialList::MODIFIED, modified);
        }
    }
    if (row.text) {
        const int words = pm.wordCount(element.id);
        set(MaterialList::WORDS, QLocale().toString(words));
        order(MaterialList::WORDS, words);
        set(MaterialList::STATUS, dialogs::AddToBookDialog::statusName(element.status));
    } else {
        const QString kindId = baseKindOf(kind);
        if (kindId == QLatin1String(core::MaterialKinds::QUOTE)) {
            set(MaterialList::QUOTE, element.description.simplified());
        }
        set(MaterialList::ADDRESS, element.url);
        set(MaterialList::SITE, siteOf(element.url));
        if (!path.isEmpty()) {
            const MaterialFile file = fileOf(path);
            set(MaterialList::FILE, file.name);
            set(MaterialList::FORMAT, file.format);
            if (file.exists) {
                set(MaterialList::SIZE, sizeText(file.bytes));
                order(MaterialList::SIZE, file.bytes);
            }
            if (file.readable) {
                row.picture = path;
            }
            if (file.size.isValid()) {
                //: The size of a picture in the list of materials: %1 the width, %2 the
                //: height. In Polish: %1 × %2
                set(MaterialList::DIMENSIONS,
                    MaterialList::tr("%1 × %2").arg(file.size.width()).arg(file.size.height()));
                order(MaterialList::DIMENSIONS,
                      static_cast<qint64>(file.size.width()) * file.size.height());
            }
            if (kindId == QLatin1String(core::MaterialKinds::DOCUMENT)) {
                set(MaterialList::DOCUMENT_KIND, MaterialList::documentKindOf(file.format));
            }
            // The length of a picture of the cover is its size
            if (kindId == QLatin1String(core::MaterialKinds::COVER) && file.size.isValid() &&
                row.body.isEmpty()) {
                set(MaterialList::COVER_LENGTH, row.texts.value(QString::fromLatin1(
                                                    MaterialList::DIMENSIONS)));
                order(MaterialList::COVER_LENGTH,
                      static_cast<qint64>(file.size.width()) * file.size.height());
            }
        }
        if (kindId == QLatin1String(core::MaterialKinds::COVER)) {
            setCoverTexts(row, element, book.coverParts());
        }
        if (kindId == QLatin1String(core::MaterialKinds::PICTURE)) {
            if (element.url.isEmpty()) {
                //: Where a picture comes from: a file from the disk. In Polish: dysk
                set(MaterialList::ORIGIN, MaterialList::tr("disk"));
            } else if (!element.file.isEmpty()) {
                //: Where a picture comes from: the web, and the book has a copy of it. In
                //: Polish: sieć, z kopią
                set(MaterialList::ORIGIN, MaterialList::tr("web, with a copy"));
            } else {
                //: Where a picture comes from: the web, without a copy in the book. In Polish:
                //: sieć, bez kopii
                set(MaterialList::ORIGIN, MaterialList::tr("web, without a copy"));
            }
        }
    }
    MaterialList::setClippingTexts(row, book, element);
    MaterialList::setVolumeTexts(row, book, element);
    if (!row.text) {
        setSourceTexts(book, row, element);
    }
    set(MaterialList::DETAILS, detailsOf(row, row.text ? QString() : baseKindOf(kind)));
    row.copy = MaterialList::copyTextOf(book, element.id);

    QStringList all = row.texts.values();
    for (const core::ElementField& field : element.fields) {
        all << field.name;
    }
    for (const QString* cover : {&element.cover.role, &element.cover.source,
                                 &element.cover.license, &element.cover.about}) {
        if (!cover->isEmpty()) {
            all << *cover;
        }
    }
    if (!element.description.isEmpty()) {
        all << element.description;
    }
    row.search = all.join(QLatin1Char('\n')).toLower();
    return row;
}

/// The settings of @p list (LISTS_SETTING): an object, empty while the writer has not chosen
json listSettings(const QString& list) {
    const json lists = core::SettingsManager::getInstance().get<json>(
        MaterialList::LISTS_SETTING, json::object());
    const std::string key = list.toStdString();
    if (!lists.is_object() || !lists.contains(key) || !lists.at(key).is_object()) {
        return json::object();
    }
    return lists.at(key);
}

/// Keep @p value as @p name of the settings of @p list (LISTS_SETTING)
void setListSetting(const QString& list, const char* name, const json& value) {
    auto& settings = core::SettingsManager::getInstance();
    json lists = settings.get<json>(MaterialList::LISTS_SETTING, json::object());
    if (!lists.is_object()) {
        lists = json::object();
    }
    json& kept = lists[list.toStdString()];
    if (!kept.is_object()) {
        kept = json::object();
    }
    kept[name] = value;
    settings.set<json>(MaterialList::LISTS_SETTING, lists);
}

} // namespace

QStringList MaterialList::kinds(const core::ProjectManager& book) {
    QStringList lists;
    const QStringList hidden = hiddenKinds();
    for (const core::KindRef& kind : materialKinds(book)) {
        if (!hidden.contains(kind.reference())) {
            lists << kind.reference();
        }
    }
    return lists;
}

QStringList MaterialList::allKinds(const core::ProjectManager& book) {
    QStringList lists;
    for (const core::KindRef& kind : materialKinds(book)) {
        lists << kind.reference();
    }
    return lists;
}

QStringList MaterialList::settingsKinds(const core::ProjectManager& book) {
    QStringList lists = allKinds(book);
    const auto& pm = book;
    if (pm.project() == nullptr) {
        for (const core::KindRef& kind : pm.bookTypes().kindsIn(
                 QString::fromLatin1(core::ProjectManager::BASE_PACKAGE),
                 core::BookPlace::Workshop)) {
            if (kind.kind->workshopGroup == core::WorkshopGroup::Resources &&
                core::ProjectManager::canAdd(kind)) {
                lists << kind.reference();
            }
        }
    }
    for (const QString& hidden : hiddenKinds()) {
        if (!lists.contains(hidden)) {
            lists << hidden;
        }
    }
    return lists;
}

int MaterialList::countOf(const core::ProjectManager& book, const QString& list) {
    if (book.project() == nullptr) {
        return -1;
    }
    return static_cast<int>(elementsOf(book, list).size());
}

QStringList MaterialList::hiddenKinds() {
    const json hidden = core::SettingsManager::getInstance().get<json>(HIDDEN_SETTING,
                                                                       json::array());
    QStringList kinds;
    if (!hidden.is_array()) {
        return kinds;
    }
    for (const json& kind : hidden) {
        if (kind.is_string() && !kind.get<std::string>().empty()) {
            kinds << QString::fromStdString(kind.get<std::string>());
        }
    }
    return kinds;
}

bool MaterialList::isHidden(const QString& list) {
    return !list.isEmpty() && hiddenKinds().contains(list);
}

void MaterialList::setHidden(const QString& list, bool hidden) {
    if (list.isEmpty()) {
        return;
    }
    QStringList kinds = hiddenKinds();
    if (kinds.contains(list) == hidden) {
        return;
    }
    if (hidden) {
        kinds << list;
    } else {
        kinds.removeAll(list);
    }
    json value = json::array();
    for (const QString& kind : kinds) {
        value.push_back(kind.toStdString());
    }
    core::SettingsManager::getInstance().set<json>(HIDDEN_SETTING, value);
}

bool MaterialList::isCover(const QString& list) {
    return CoverNames::isCoverKind(kindOfList(list));
}

bool MaterialList::isClippings(const QString& list) {
    return baseKindOf(kindOfList(list)) == QLatin1String(core::Clippings::KIND);
}

bool MaterialList::hasGroups(const QString& list) {
    return isCover(list);
}

QString MaterialList::partColumnOf(const QString& list) {
    if (isCover(list)) {
        return QString::fromLatin1(COVER_PART);
    }
    return {};
}

QStringList MaterialList::addKindsOf(const core::ProjectManager& book, const QString& list) {
    if (list.isEmpty()) {
        return kinds(book);
    }
    if (list != QLatin1String(SOURCES)) {
        return {list};
    }
    QStringList describable;
    for (const QString& each : allKinds(book)) {
        const QString kindId = baseKindOf(kindOfList(each));
        if (core::SourceDescription::canDescribe(kindId)) {
            // A publication first: what a source is most often
            if (kindId == QLatin1String(core::MaterialKinds::PUBLICATION)) {
                describable.prepend(each);
            } else {
                describable << each;
            }
        }
    }
    return describable;
}

QString MaterialList::nameOf(const QString& list) {
    if (list.isEmpty()) {
        //: The list of all the materials of the Workshop, the working notes too, on a tab. In
        //: Polish: Wszystkie materiały
        return tr("All Materials");
    }
    if (list == QLatin1String(SOURCES)) {
        //: The list of the materials with a bibliographic description, on a tab and in the
        //: Navigator. In Polish: Źródła
        return tr("Sources");
    }
    const core::KindRef kind = kindOfList(list);
    return kind ? kind.kind->plural.text(programLanguage()) : list;
}

QString MaterialList::iconIdOf(const QString& list) {
    if (list.isEmpty()) {
        return QStringLiteral("structure.materials");
    }
    if (list == QLatin1String(SOURCES)) {
        return QStringLiteral("common.filePresent");
    }
    return NavigatorPanel::iconIdOf(kindOfList(list));
}

bool MaterialList::hasTiles(const QString& list) {
    const QString kindId = baseKindOf(kindOfList(list));
    return CardList::isCardList(list) || kindId == QLatin1String(core::MaterialKinds::PICTURE) ||
           kindId == QLatin1String(core::MaterialKinds::COVER) ||
           kindId == QLatin1String(core::MaterialKinds::DOCUMENT) ||
           kindId == QLatin1String(core::MaterialKinds::PUBLICATION);
}

QList<MaterialColumn> MaterialList::columnsOf(const core::ProjectManager& book,
                                              const QString& list) {
    if (CardList::isCardList(list)) {
        return CardList::columnsOf(book, list);
    }
    //: A column of the list of materials. In Polish: Tytuł
    const QString title = tr("Title");
    //: In Polish: Powiązania
    const QString links = tr("Links");
    //: In Polish: Opis
    const QString description = tr("Description");
    //: In Polish: Plik
    const QString file = tr("File");
    //: In Polish: Format
    const QString format = tr("Format");
    //: The size of a file. In Polish: Rozmiar
    const QString size = tr("Size");
    //: The time the file was changed. In Polish: Data zmiany
    const QString modified = tr("Modified");
    //: The web address. In Polish: Adres
    const QString address = tr("Address");

    QList<MaterialColumn> columns;
    const core::KindRef kind = kindOfList(list);
    const QString kindId = baseKindOf(kind);
    if (list.isEmpty()) {
        //: The kind of a material in All Materials. In Polish: Typ
        columns << column(TYPE, tr("Type")) << column(TITLE, title)
                //: A summary of a material in All Materials. In Polish: Szczegóły
                << column(DETAILS, tr("Details")) << column(LINKS, links)
                << column(DESCRIPTION, description, false) << column(MODIFIED, modified, false);
    } else if (const QList<MaterialColumn> own = ownColumnsOf(kindId); !own.isEmpty()) {
        columns << own;
    } else if (list == QLatin1String(SOURCES)) {
        //: The kind of the material of a source: publication, web page. In Polish: Typ
        //: materiału
        columns << column(TYPE, tr("Material"))
                //: A book, an article, a web page... In Polish: Rodzaj źródła
                << column(SOURCE_KIND, tr("Kind of source"))
                //: The authors of a source. In Polish: Autor
                << column(SOURCE_AUTHORS, tr("Author")) << column(TITLE, title)
                //: The year of a source. In Polish: Rok
                << column(SOURCE_YEAR, tr("Year"))
                //: Whether the bibliography of the book has a source. In Polish: W bibliografii
                << column(SOURCE_INCLUSION, tr("In the bibliography"))
                //: How many places of the text a source is linked with, its quotes too. In
                //: Polish: W tekście
                << column(SOURCE_PLACES, tr("In the text")) << column(LINKS, links, false)
                << column(FILE, file, false) << column(DESCRIPTION, description, false);
    } else if (kind && !kind.kind->isMaterial()) {
        //: The words of a working note. In Polish: Słowa
        columns << column(TITLE, title) << column(WORDS, tr("Words"))
                //: The status of a working note: draft, revision, final. In Polish: Stan
                << column(STATUS, tr("Status")) << column(MODIFIED, modified)
                << column(LINKS, links);
    } else if (kindId == QLatin1String(core::MaterialKinds::COVER)) {
        columns << MaterialColumn{QString::fromLatin1(THUMBNAIL), QString(), true, false}
                //: What a material of the cover is: a description, a tagline... In Polish:
                //: Rodzaj
                << column(COVER_PART, tr("Kind"))
                //: A column of the list of the cover: the text of a material, else its title.
                //: In Polish: Treść albo tytuł
                << column(COVER_TEXT, tr("Text or title"))
                //: Who wrote an opinion, made a design or a picture. In Polish: Autor
                << column(COVER_AUTHOR, tr("Author"))
                //: Where a material goes on the cover. In Polish: Miejsce
                << column(COVER_PLACE, tr("Place"))
                //: Chosen for the cover, a draft, rejected. In Polish: Stan
                << column(COVER_STATE, tr("State"))
                //: The length of a text, the size of a picture. In Polish: Długość
                << column(COVER_LENGTH, tr("Length")) << column(TITLE, title, false)
                //: Where an opinion comes from. In Polish: Źródło
                << column(COVER_SOURCE, tr("Source"), false)
                //: The date of an opinion or a design. In Polish: Data
                << column(COVER_DATE, tr("Date"), false)
                //: The license of a reference picture. In Polish: Licencja
                << column(COVER_LICENSE, tr("License"), false) << column(LINKS, links, false)
                << column(FILE, file, false);
    } else if (kindId == QLatin1String(core::MaterialKinds::QUOTE)) {
        //: The words of a quote. In Polish: Cytat
        columns << column(QUOTE, tr("Quote"))
                //: Where a quote comes from: a source and its page. In Polish: Źródło
                << column(QUOTE_SOURCE, tr("Source")) << column(LINKS, links)
                << column(TITLE, title, false);
    } else if (kindId == QLatin1String(core::MaterialKinds::WEB_PAGE)) {
        //: The host of the address of a page. In Polish: Witryna
        columns << column(TITLE, title) << column(SITE, tr("Site")) << column(LINKS, links)
                << column(ADDRESS, address, false) << column(DESCRIPTION, description, false);
    } else if (kindId == QLatin1String(core::MaterialKinds::PICTURE)) {
        columns << MaterialColumn{QString::fromLatin1(THUMBNAIL), QString(), true, false}
                << column(TITLE, title) << column(FORMAT, format)
                //: The size of a picture in pixels. In Polish: Wymiary
                << column(DIMENSIONS, tr("Dimensions"))
                //: Where a picture comes from: the disk, the web. In Polish: Pochodzenie
                << column(ORIGIN, tr("Origin")) << column(LINKS, links)
                << column(SIZE, size, false) << column(FILE, file, false)
                << column(ADDRESS, address, false) << column(DESCRIPTION, description, false);
    } else if (kindId == QLatin1String(core::MaterialKinds::DOCUMENT)) {
        //: The kind of a document: PDF, text, spreadsheet... In Polish: Rodzaj
        columns << column(TITLE, title) << column(DOCUMENT_KIND, tr("Kind"))
                << column(FORMAT, format) << column(SIZE, size) << column(MODIFIED, modified)
                << column(LINKS, links) << column(FILE, file, false)
                << column(DESCRIPTION, description, false);
    } else if (kindId == QLatin1String(core::MaterialKinds::PUBLICATION)) {
        columns << column(TITLE, title) << column(FORMAT, format) << column(SIZE, size)
                << column(LINKS, links) << column(FILE, file, false)
                << column(DESCRIPTION, description, false);
    } else if (kindId == QLatin1String(core::MaterialKinds::RECORDING) ||
               kindId == QLatin1String(core::MaterialKinds::OTHER_FILE)) {
        columns << column(TITLE, title) << column(FILE, file) << column(FORMAT, format)
                << column(SIZE, size) << column(LINKS, links)
                << column(DESCRIPTION, description, false) << column(MODIFIED, modified, false);
    } else {
        columns << column(TITLE, title) << column(DESCRIPTION, description)
                << column(LINKS, links) << column(FILE, file, false)
                << column(FORMAT, format, false) << column(SIZE, size, false)
                << column(ADDRESS, address, false);
    }

    addVolumeColumn(book, list, columns);
    appendFieldColumns(columns, elementsOf(book, list));
    return columns;
}

QList<MaterialColumn> MaterialList::ownColumnsOf(const QString& kindId) {
    if (kindId == QLatin1String(core::Clippings::KIND)) {
        return clippingColumns();
    }
    return {};
}

void MaterialList::appendFieldColumns(QList<MaterialColumn>& columns,
                                      const QList<const core::ProjectElement*>& elements) {
    QStringList fields;
    for (const core::ProjectElement* element : elements) {
        for (const core::ElementField& field : element->fields) {
            const QString name = field.name.trimmed();
            if (!name.isEmpty() && !fields.contains(name)) {
                fields << name;
            }
        }
    }
    std::sort(fields.begin(), fields.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    for (const QString& field : fields) {
        columns << MaterialColumn{QString::fromLatin1(FIELD_PREFIX) + field, field, false, true};
    }
}

QString MaterialList::firstSortOf(const QString& list) {
    if (list.isEmpty()) {
        return QString::fromLatin1(TYPE);
    }
    if (list == QLatin1String(SOURCES)) {
        return QString::fromLatin1(SOURCE_AUTHORS);
    }
    const QString kindId = baseKindOf(kindOfList(list));
    if (kindId == QLatin1String(core::MaterialKinds::COVER)) {
        return QString::fromLatin1(COVER_TEXT);
    }
    if (kindId == QLatin1String(core::Clippings::KIND)) {
        return QString::fromLatin1(CLIPPING_CUT);
    }
    return kindId == QLatin1String(core::MaterialKinds::QUOTE) ? QString::fromLatin1(QUOTE)
                                                                : QString::fromLatin1(TITLE);
}

Qt::SortOrder MaterialList::firstSortOrderOf(const QString& list) {
    return isClippings(list) ? Qt::DescendingOrder : Qt::AscendingOrder;
}

QList<MaterialRow> MaterialList::rowsOf(const core::ProjectManager& book, const QString& list) {
    if (CardList::isCardList(list)) {
        return CardList::rowsOf(book, list);
    }
    QList<MaterialRow> rows;
    const auto& pm = book;
    // Copy of a source copies its entry in the bibliography of the book
    const bool sources = list == QLatin1String(SOURCES);
    const QString style = sources ? pm.bibliographyStyle() : QString();
    for (const core::ProjectElement* element : elementsOf(book, list)) {
        MaterialRow row = rowOf(book, *element, pm.kindOf(*element));
        if (sources) {
            row.copy = core::Bibliography::entry(element->source, element->title, style,
                                                 pm.bookLanguage(), false);
        }
        rows << row;
    }
    return rows;
}

QStringList MaterialList::shownColumns(const core::ProjectManager& book, const QString& list) {
    const QList<MaterialColumn> columns = columnsOf(book, list);
    const json kept = listSettings(list);
    QStringList shown;
    if (kept.contains("columns") && kept.at("columns").is_array()) {
        for (const json& id : kept.at("columns")) {
            const QString name = id.is_string() ? QString::fromStdString(id.get<std::string>())
                                                : QString();
            const bool known = std::any_of(columns.cbegin(), columns.cend(),
                                           [&name](const MaterialColumn& c) { return c.id == name; });
            if (known && !shown.contains(name)) {
                shown << name;
            }
        }
        if (!shown.isEmpty()) {
            return shown;
        }
    }
    for (const MaterialColumn& column : columns) {
        if (column.shown) {
            shown << column.id;
        }
    }
    return shown;
}

void MaterialList::setShownColumns(const QString& list, const QStringList& columns) {
    json ids = json::array();
    for (const QString& id : columns) {
        ids.push_back(id.toStdString());
    }
    setListSetting(list, "columns", ids);
}

bool MaterialList::tilesShown(const QString& list) {
    if (!hasTiles(list)) {
        return false;
    }
    const json kept = listSettings(list);
    return kept.contains("tiles") && kept.at("tiles").is_boolean() && kept.at("tiles").get<bool>();
}

void MaterialList::setTilesShown(const QString& list, bool tiles) {
    setListSetting(list, "tiles", tiles);
}

bool MaterialList::planShown(const QString& list) {
    if (!isCover(list)) {
        return false;
    }
    const json kept = listSettings(list);
    return kept.contains("plan") && kept.at("plan").is_boolean() && kept.at("plan").get<bool>();
}

void MaterialList::setPlanShown(const QString& list, bool plan) {
    setListSetting(list, "plan", plan);
}

bool MaterialList::groupsShown(const QString& list) {
    if (!hasGroups(list)) {
        return false;
    }
    const json kept = listSettings(list);
    return !kept.contains("groups") || !kept.at("groups").is_boolean() ||
           kept.at("groups").get<bool>();
}

void MaterialList::setGroupsShown(const QString& list, bool groups) {
    setListSetting(list, "groups", groups);
}

void MaterialList::addVolumeColumn(const core::ProjectManager& book, const QString& list,
                                   QList<MaterialColumn>& columns) {
    if (!hasGroups(list) || !book.isSeries()) {
        return;
    }
    const QString part = partColumnOf(list);
    const auto before =
        std::find_if(columns.cbegin(), columns.cend(),
                     [&part](const MaterialColumn& each) { return each.id == part; });
    //: The volume of a series a material of the cover is for, or the whole series. In Polish:
    //: Tom
    columns.insert(before - columns.cbegin(), MaterialColumn{.id = QString::fromLatin1(VOLUME),
                                                             .title = tr("Volume"),
                                                             .shown = true,
                                                             .sortable = true});
}

void MaterialList::setVolumeTexts(MaterialRow& row, const core::ProjectManager& book,
                                  const core::ProjectElement& element) {
    if (!book.isSeries() || !core::VolumeMaterials::isVolumeMaterial(element)) {
        return;
    }
    row.volume = VolumeNames::nameOf(book, element);
    row.volumeOrder = VolumeNames::orderOf(book, element);
    if (!row.volume.isEmpty()) {
        row.texts.insert(QString::fromLatin1(VOLUME), row.volume);
    }
    row.order.insert(QString::fromLatin1(VOLUME), row.volumeOrder);
}

bool MaterialList::ownTabs() {
    return core::SettingsManager::getInstance().get<std::string>(TABS_SETTING, "own") != "one";
}

QString MaterialList::documentKindOf(const QString& format) {
    const QString upper = format.toUpper();
    if (upper == QLatin1String("PDF")) {
        return QStringLiteral("PDF");
    }
    static const QStringList texts{QStringLiteral("DOC"), QStringLiteral("DOCX"),
                                   QStringLiteral("ODT"), QStringLiteral("RTF"),
                                   QStringLiteral("TXT"), QStringLiteral("MD")};
    static const QStringList sheets{QStringLiteral("XLS"), QStringLiteral("XLSX"),
                                    QStringLiteral("ODS"), QStringLiteral("CSV")};
    static const QStringList slides{QStringLiteral("PPT"), QStringLiteral("PPTX"),
                                    QStringLiteral("ODP")};
    if (texts.contains(upper)) {
        //: The kind of a document: Word, OpenDocument, RTF, plain text. In Polish: tekst
        return tr("text");
    }
    if (sheets.contains(upper)) {
        //: The kind of a document. In Polish: arkusz
        return tr("spreadsheet");
    }
    if (slides.contains(upper)) {
        //: The kind of a document. In Polish: prezentacja
        return tr("presentation");
    }
    if (upper == QLatin1String("EPUB")) {
        //: The kind of a document. In Polish: e-book
        return tr("e-book");
    }
    //: The kind of a document of any other format. In Polish: inny
    return tr("other");
}

QString MaterialList::copyTextOf(const core::ProjectManager& book, const QString& elementId) {
    const auto& pm = book;
    const core::ProjectElement* element = pm.findElement(elementId);
    if (element == nullptr) {
        return {};
    }
    const core::KindRef kind = pm.kindOf(*element);
    if (baseKindOf(kind) == QLatin1String(core::MaterialKinds::QUOTE) &&
        !element->description.trimmed().isEmpty()) {
        return element->description.trimmed();
    }
    // The text of a material of the cover; an opinion as the cover prints it
    if (baseKindOf(kind) == QLatin1String(core::MaterialKinds::COVER) &&
        !core::CoverMaterial::isPicture(element->cover.part) &&
        !element->description.trimmed().isEmpty()) {
        return element->cover.part == QLatin1String(core::CoverMaterial::OPINION)
                   ? CoverNames::printedOpinion(element->description, element->cover)
                   : element->description.trimmed();
    }
    if (kind && kind.kind->isMaterial()) {
        if (!element->file.isEmpty()) {
            return pm.filePathOf(*element);
        }
        if (!element->url.isEmpty()) {
            return element->url;
        }
    }
    return element->title;
}

} // namespace kalahari::gui
