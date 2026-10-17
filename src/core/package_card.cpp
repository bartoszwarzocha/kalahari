/// @file package_card.cpp
/// @brief What a book keeps of each book type package it uses: the name, the version and the
/// icons of the type and of its kinds

#include <kalahari/core/package_card.h>

#include <kalahari/core/icon_registry.h>
#include <kalahari/core/logger.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QVersionNumber>

#include <algorithm>

namespace kalahari::core {

namespace {

/// Whether icon id @p iconId can name a file: letters, digits, dots, - and _, not first a dot
bool isIconFileName(const QString& iconId) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-][A-Za-z0-9._-]*$"));
    return pattern.match(iconId).hasMatch();
}

QJsonObject textToJson(const LocalizedText& text) {
    QJsonObject json;
    for (auto it = text.values.constBegin(); it != text.values.constEnd(); ++it) {
        json.insert(it.key(), it.value());
    }
    return json;
}

LocalizedText textFromJson(const QJsonValue& value) {
    LocalizedText text;
    const QJsonObject json = value.toObject();
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
        if (it.value().isString() && !it.value().toString().isEmpty()) {
            text.values.insert(it.key(), it.value().toString());
        }
    }
    return text;
}

QString genderName(Gender gender) {
    switch (gender) {
    case Gender::Feminine:
        return QStringLiteral("feminine");
    case Gender::Neuter:
        return QStringLiteral("neuter");
    case Gender::Plural:
        return QStringLiteral("plural");
    case Gender::Masculine:
        break;
    }
    return QStringLiteral("masculine");
}

QJsonObject wordsToJson(const QMap<QString, KindWords>& words) {
    QJsonObject json;
    for (auto language = words.constBegin(); language != words.constEnd(); ++language) {
        QJsonObject forms;
        forms.insert(QStringLiteral("gender"), genderName(language.value().gender));
        for (auto form = language.value().forms.constBegin();
             form != language.value().forms.constEnd(); ++form) {
            forms.insert(form.key(), form.value());
        }
        json.insert(language.key(), forms);
    }
    return json;
}

QMap<QString, KindWords> wordsFromJson(const QJsonValue& value) {
    QMap<QString, KindWords> words;
    const QJsonObject json = value.toObject();
    for (auto language = json.constBegin(); language != json.constEnd(); ++language) {
        KindWords forms;
        const QJsonObject given = language.value().toObject();
        for (auto it = given.constBegin(); it != given.constEnd(); ++it) {
            const QString text = it.value().toString();
            if (it.key() == QLatin1String("gender")) {
                KindWords::genderFromName(text, forms.gender);
            } else if (KindWords::formNames().contains(it.key()) && !text.isEmpty()) {
                forms.forms.insert(it.key(), text);
            }
        }
        words.insert(language.key(), forms);
    }
    return words;
}

/// The icon ids of @p card that can name files: of the package and of its kinds, each once
QStringList iconsOf(const PackageCard& card) {
    QStringList icons;
    const auto add = [&icons](const QString& icon) {
        if (isIconFileName(icon) && !icons.contains(icon)) {
            icons.append(icon);
        }
    };
    add(card.icon);
    for (const ElementKind& kind : card.kinds) {
        add(kind.icon);
    }
    return icons;
}

/// Whether folder @p from is now named @p to; a folder named @p to goes first
bool renameFolder(const QString& from, const QString& to) {
    if (QFileInfo::exists(to) && !QDir(to).removeRecursively()) {
        return false;
    }
    return QDir().rename(from, to);
}

}  // namespace

// =============================================================================
// PackageCard
// =============================================================================

const ElementKind* PackageCard::kind(const QString& kindId) const {
    const auto it = std::find_if(kinds.cbegin(), kinds.cend(),
                                 [&kindId](const ElementKind& kind) { return kind.id == kindId; });
    return it != kinds.cend() ? &*it : nullptr;
}

QString PackageCard::iconFile(const QString& iconId, const QString& theme) const {
    if (directory.isEmpty() || !isIconFileName(iconId) || !isIconFileName(theme)) {
        return QString();
    }
    const QString path = QDir(directory).filePath(QStringLiteral("%1/%2/%3.svg")
                                                      .arg(QLatin1String(ICONS_FOLDER), theme,
                                                           iconId));
    return QFileInfo(path).isFile() ? path : QString();
}

PackageCard PackageCard::of(const BookTypePackage& package) {
    PackageCard card;
    card.id = package.id;
    card.version = package.version;
    card.role = package.role;
    card.name = package.name;
    card.icon = package.icon;
    for (const ElementKind& own : package.kinds) {
        ElementKind kind;
        kind.id = own.id;
        kind.name = own.name;
        kind.plural = own.plural;
        kind.words = own.words;
        kind.icon = own.icon;
        card.kinds.append(kind);
    }
    return card;
}

QJsonObject PackageCard::toJson() const {
    QJsonObject kindsJson;
    for (const ElementKind& kind : kinds) {
        QJsonObject json;
        json.insert(QStringLiteral("name"), textToJson(kind.name));
        if (!kind.plural.isEmpty()) {
            json.insert(QStringLiteral("plural"), textToJson(kind.plural));
        }
        if (!kind.words.isEmpty()) {
            json.insert(QStringLiteral("words"), wordsToJson(kind.words));
        }
        if (!kind.icon.isEmpty()) {
            json.insert(QStringLiteral("icon"), kind.icon);
        }
        kindsJson.insert(kind.id, json);
    }

    QJsonObject json;
    json.insert(QStringLiteral("format"), FORMAT);
    json.insert(QStringLiteral("id"), id);
    json.insert(QStringLiteral("version"), version);
    json.insert(QStringLiteral("role"), role == PackageRole::Shared ? QStringLiteral("shared")
                                                                    : QStringLiteral("type"));
    json.insert(QStringLiteral("name"), textToJson(name));
    if (!icon.isEmpty()) {
        json.insert(QStringLiteral("icon"), icon);
    }
    json.insert(QStringLiteral("kinds"), kindsJson);
    return json;
}

std::optional<PackageCard> PackageCard::fromJson(const QJsonObject& json) {
    PackageCard card;
    card.id = json.value(QStringLiteral("id")).toString();
    card.version = json.value(QStringLiteral("version")).toString();
    if (json.value(QStringLiteral("format")).toInt() != FORMAT ||
        !BookTypePackage::isId(card.id) || QVersionNumber::fromString(card.version).isNull()) {
        return std::nullopt;
    }
    card.role = json.value(QStringLiteral("role")).toString() == QLatin1String("shared")
                    ? PackageRole::Shared
                    : PackageRole::Type;
    card.name = textFromJson(json.value(QStringLiteral("name")));
    card.icon = json.value(QStringLiteral("icon")).toString();

    const QJsonObject kindsJson = json.value(QStringLiteral("kinds")).toObject();
    for (auto it = kindsJson.constBegin(); it != kindsJson.constEnd(); ++it) {
        const QJsonObject given = it.value().toObject();
        ElementKind kind;
        kind.id = it.key();
        kind.name = textFromJson(given.value(QStringLiteral("name")));
        kind.plural = textFromJson(given.value(QStringLiteral("plural")));
        kind.words = wordsFromJson(given.value(QStringLiteral("words")));
        kind.icon = given.value(QStringLiteral("icon")).toString();
        if (!kind.id.isEmpty()) {
            card.kinds.append(kind);
        }
    }
    return card;
}

std::optional<PackageCard> PackageCard::read(const QString& directory) {
    QFile file(QDir(directory).filePath(QString::fromLatin1(FILE_NAME)));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return std::nullopt;
    }
    std::optional<PackageCard> card = fromJson(document.object());
    if (card) {
        card->directory = QFileInfo(directory).absoluteFilePath();
    }
    return card;
}

// =============================================================================
// PackageCards
// =============================================================================

QString PackageCards::folderOf(const QString& projectDir) {
    return QDir(projectDir).filePath(QString::fromLatin1(BookProject::TYPES_FOLDER));
}

QString PackageCards::folderOf(const QString& projectDir, const QString& packageId) {
    return QDir(folderOf(projectDir)).filePath(packageId);
}

std::optional<PackageCard> PackageCards::read(const QString& projectDir,
                                              const QString& packageId) {
    if (!BookTypePackage::isId(packageId)) {
        return std::nullopt;
    }
    std::optional<PackageCard> card = PackageCard::read(folderOf(projectDir, packageId));
    if (card && card->id != packageId) {
        return std::nullopt;  // the folder of another package's card
    }
    return card;
}

QHash<QString, PackageCard> PackageCards::readAll(const QString& projectDir,
                                                  const BookProject& project) {
    QHash<QString, PackageCard> cards;
    for (const QString& id : project.packageIds()) {
        if (const std::optional<PackageCard> card = read(projectDir, id)) {
            cards.insert(id, *card);
        }
    }
    return cards;
}

bool PackageCards::isNewer(const QString& version, const QString& than) {
    // "1.0" and "1.0.0" are one version
    return QVersionNumber::compare(QVersionNumber::fromString(version).normalized(),
                                   QVersionNumber::fromString(than).normalized()) > 0;
}

bool PackageCards::write(const BookTypePackage& package, const QString& destination,
                         QString* problem) {
    const auto fail = [problem](const QString& what) {
        if (problem) {
            *problem = what;
        }
        return false;
    };

    // The new card is complete before it takes the place of the old one
    const QFileInfo target(destination);
    const QDir parent = target.dir();
    const QString fresh = parent.filePath(QStringLiteral(".%1.new").arg(target.fileName()));
    const QString old = parent.filePath(QStringLiteral(".%1.old").arg(target.fileName()));
    if (!QDir().mkpath(parent.absolutePath())) {
        return fail(QStringLiteral("%1: the folder cannot be made")
                        .arg(QDir::toNativeSeparators(parent.absolutePath())));
    }
    if (QFileInfo::exists(fresh) && !QDir(fresh).removeRecursively()) {
        return fail(QStringLiteral("%1: cannot be removed").arg(QDir::toNativeSeparators(fresh)));
    }
    if (!QDir().mkpath(fresh)) {
        return fail(QStringLiteral("%1: the folder cannot be made")
                        .arg(QDir::toNativeSeparators(fresh)));
    }

    const PackageCard card = PackageCard::of(package);
    const QString cardFile = QDir(fresh).filePath(QString::fromLatin1(PackageCard::FILE_NAME));
    QSaveFile file(cardFile);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(card.toJson()).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit()) {
        QDir(fresh).removeRecursively();
        return fail(QStringLiteral("%1: cannot be written")
                        .arg(QDir::toNativeSeparators(cardFile)));
    }

    // The icons in each theme of the program, as the program has them
    const IconRegistry& icons = IconRegistry::getInstance();
    for (const QString& iconId : iconsOf(card)) {
        for (const QString& theme : IconRegistry::iconThemes()) {
            const QString source = icons.svgFileOf(iconId, theme);
            if (source.isEmpty()) {
                continue;  // the program has no such icon in this theme
            }
            const QString to =
                QDir(fresh).filePath(QStringLiteral("%1/%2/%3.svg")
                                         .arg(QLatin1String(PackageCard::ICONS_FOLDER), theme,
                                              iconId));
            if (!QDir().mkpath(QFileInfo(to).absolutePath()) || !QFile::copy(source, to)) {
                QDir(fresh).removeRecursively();
                return fail(QStringLiteral("%1: cannot be copied to %2")
                                .arg(QDir::toNativeSeparators(source),
                                     QDir::toNativeSeparators(to)));
            }
            // A file of the program's folder can be read-only; its copy belongs to the book
            QFile::setPermissions(to, QFile::permissions(to) | QFileDevice::WriteOwner);
        }
    }

    // The card is read as the book will read it
    const std::optional<PackageCard> written = PackageCard::read(fresh);
    if (!written || written->id != package.id || written->version != package.version) {
        QDir(fresh).removeRecursively();
        return fail(QStringLiteral("%1: the card cannot be read")
                        .arg(QDir::toNativeSeparators(cardFile)));
    }

    // The old card steps aside until the new one is in its place
    const bool hadCard = target.exists();
    if (hadCard && !renameFolder(destination, old)) {
        QDir(fresh).removeRecursively();
        return fail(QStringLiteral("%1: cannot be replaced")
                        .arg(QDir::toNativeSeparators(destination)));
    }
    if (!QDir().rename(fresh, destination)) {
        if (hadCard) {
            QDir().rename(old, destination);
        }
        QDir(fresh).removeRecursively();
        return fail(QStringLiteral("%1: cannot be written")
                        .arg(QDir::toNativeSeparators(destination)));
    }
    if (hadCard) {
        QDir(old).removeRecursively();
    }
    return true;
}

PackageCardsUpdate PackageCards::update(const QString& projectDir, const BookProject& project,
                                        const BookTypeRegistry& installed) {
    auto& logger = Logger::getInstance();
    PackageCardsUpdate result;

    // The packages the book names, and the packages they use, whose kinds it can take later
    QStringList ids = project.packageIds();
    for (qsizetype i = 0; i < ids.size(); ++i) {
        if (const BookTypePackage* package = installed.package(ids.at(i))) {
            for (const QString& used : package->uses) {
                if (!ids.contains(used)) {
                    ids.append(used);
                }
            }
        }
    }

    for (const QString& id : ids) {
        const BookTypePackage* package = installed.package(id);
        if (package == nullptr) {
            continue;  // the book keeps the card it has
        }
        const QString folder = folderOf(projectDir, id);
        const std::optional<PackageCard> card = read(projectDir, id);
        if (card && !isNewer(package->version, card->version)) {
            continue;
        }
        const bool hadCard = card.has_value() || QFileInfo::exists(folder);
        QString problem;
        if (!write(*package, folder, &problem)) {
            result.problems.append(QStringLiteral("%1: %2").arg(id, problem));
        } else if (hadCard) {
            result.refreshed.append(id);
            logger.info("The book's card of {} is of version {} now", id.toStdString(),
                        package->version.toStdString());
        } else {
            result.made.append(id);
            logger.debug("The card of {} {} is written into the book", id.toStdString(),
                         package->version.toStdString());
        }
    }
    return result;
}

}  // namespace kalahari::core
