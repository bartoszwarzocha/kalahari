/// @file package_copies.cpp
/// @brief Copies of the book type packages in the folder of a book, so that the book opens and
/// works the same on every computer

#include <kalahari/core/package_copies.h>

#include <kalahari/core/logger.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QVersionNumber>

namespace kalahari::core {

namespace {

/// Files of the data of @p package, relative to its folder: the manifest, the styles file and
/// the templates of its kinds
QStringList dataFilesOf(const BookTypePackage& package) {
    QStringList files{QString::fromLatin1(BookTypePackage::MANIFEST_FILE)};
    const auto add = [&files](const QString& file) {
        const QString path = QDir::cleanPath(file);
        if (!file.isEmpty() && !files.contains(path)) {
            files.append(path);
        }
    };
    add(package.stylesFile);
    for (const ElementKind& kind : package.kinds) {
        add(kind.templateFile);
    }
    return files;
}

/// Whether folder @p from is now named @p to; a folder named @p to goes first
bool renameFolder(const QString& from, const QString& to) {
    if (QFileInfo::exists(to) && !QDir(to).removeRecursively()) {
        return false;
    }
    return QDir().rename(from, to);
}

}  // namespace

QString PackageCopies::folderOf(const QString& projectDir) {
    return QDir(projectDir).filePath(QString::fromLatin1(BookProject::TYPES_FOLDER));
}

QString PackageCopies::folderOf(const QString& projectDir, const QString& packageId) {
    return QDir(folderOf(projectDir)).filePath(packageId);
}

std::optional<BookTypePackage> PackageCopies::read(const QString& projectDir,
                                                   const QString& packageId) {
    const QString folder = folderOf(projectDir, packageId);
    if (!BookTypePackage::isId(packageId) ||
        !QFileInfo(QDir(folder).filePath(QString::fromLatin1(BookTypePackage::MANIFEST_FILE)))
             .isFile()) {
        return std::nullopt;
    }
    QStringList problems;
    std::optional<BookTypePackage> copy = BookTypePackage::read(folder, problems);
    if (copy && copy->id != packageId) {
        return std::nullopt;  // the folder of another package's copy
    }
    return copy;
}

bool PackageCopies::isNewer(const QString& version, const QString& than) {
    // "1.0" and "1.0.0" are one version
    return QVersionNumber::compare(QVersionNumber::fromString(version).normalized(),
                                   QVersionNumber::fromString(than).normalized()) > 0;
}

bool PackageCopies::write(const BookTypePackage& package, const QString& destination,
                          QString* problem) {
    const auto fail = [problem](const QString& what) {
        if (problem) {
            *problem = what;
        }
        return false;
    };

    // The new copy is complete before it takes the place of the old one
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

    const QDir source(package.directory);
    for (const QString& file : dataFilesOf(package)) {
        const QString to = QDir(fresh).filePath(file);
        if (!QDir().mkpath(QFileInfo(to).absolutePath()) ||
            !QFile::copy(source.filePath(file), to)) {
            QDir(fresh).removeRecursively();
            return fail(QStringLiteral("%1: cannot be copied to %2")
                            .arg(QDir::toNativeSeparators(source.filePath(file)),
                                 QDir::toNativeSeparators(to)));
        }
        // A file of the program's folder can be read-only; its copy belongs to the book
        QFile::setPermissions(to, QFile::permissions(to) | QFileDevice::WriteOwner);
    }

    // The copy is read as the book will read it
    QStringList problems;
    const std::optional<BookTypePackage> written = BookTypePackage::read(fresh, problems);
    if (!written || written->id != package.id || written->version != package.version) {
        QDir(fresh).removeRecursively();
        return fail(QStringLiteral("%1: the copy cannot be read (%2)")
                        .arg(QDir::toNativeSeparators(fresh), problems.join(QStringLiteral("; "))));
    }

    // The old copy steps aside until the new one is in its place
    const bool hadCopy = target.exists();
    if (hadCopy && !renameFolder(destination, old)) {
        QDir(fresh).removeRecursively();
        return fail(QStringLiteral("%1: cannot be replaced")
                        .arg(QDir::toNativeSeparators(destination)));
    }
    if (!QDir().rename(fresh, destination)) {
        if (hadCopy) {
            QDir().rename(old, destination);
        }
        QDir(fresh).removeRecursively();
        return fail(QStringLiteral("%1: cannot be written")
                        .arg(QDir::toNativeSeparators(destination)));
    }
    if (hadCopy) {
        QDir(old).removeRecursively();
    }
    return true;
}

PackageCopiesUpdate PackageCopies::update(const QString& projectDir, const BookProject& project,
                                          const BookTypeRegistry& installed) {
    return walk(projectDir, project, installed, true);
}

QStringList PackageCopies::folders(const QString& projectDir, const BookProject& project,
                                   const BookTypeRegistry& installed) {
    return walk(projectDir, project, installed, false).folders;
}

PackageCopiesUpdate PackageCopies::walk(const QString& projectDir, const BookProject& project,
                                        const BookTypeRegistry& installed, bool writeCopies) {
    auto& logger = Logger::getInstance();
    PackageCopiesUpdate result;

    // The packages the project names, then the packages each of them needs
    QStringList queue = project.packageIds();
    QSet<QString> seen;
    while (!queue.isEmpty()) {
        const QString id = queue.takeFirst();
        if (seen.contains(id) || !BookTypePackage::isId(id)) {
            continue;
        }
        seen.insert(id);

        const BookTypePackage* package = installed.package(id);
        const QString folder = folderOf(projectDir, id);
        std::optional<BookTypePackage> copy = read(projectDir, id);
        const bool unreadable = !copy && QFileInfo::exists(folder);
        if (unreadable && !package) {
            result.problems.append(QStringLiteral("%1: the copy in %2 cannot be read")
                                       .arg(id, QDir::toNativeSeparators(folder)));
        }

        // An installed package newer than the copy, or one the book has no copy of, is copied;
        // without writing, the installed package takes the place of the copy
        if (package && (!copy || isNewer(package->version, copy->version))) {
            const bool hadCopy = copy.has_value() || unreadable;
            copy.reset();
            if (writeCopies) {
                QString problem;
                if (PackageCopies::write(*package, folder, &problem)) {
                    copy = read(projectDir, id);
                    if (!copy) {
                        problem = QStringLiteral("the copy in %1 cannot be read")
                                      .arg(QDir::toNativeSeparators(folder));
                    }
                }
                if (!copy) {
                    // The installed package serves the book this time
                    result.problems.append(QStringLiteral("%1: %2").arg(id, problem));
                } else if (hadCopy) {
                    result.refreshed.append(id);
                    logger.info("The project's copy of {} is version {} now", id.toStdString(),
                                copy->version.toStdString());
                } else {
                    result.made.append(id);
                    logger.debug("The package {} {} is copied into the book", id.toStdString(),
                                 copy->version.toStdString());
                }
            }
        }

        const BookTypePackage* used = copy ? &*copy : package;
        if (!used) {
            continue;  // neither installed nor copied: its elements stay in the book
        }
        result.folders.append(copy ? folder : package->directory);
        queue.append(used->requiredPackages());
    }
    return result;
}

}  // namespace kalahari::core
