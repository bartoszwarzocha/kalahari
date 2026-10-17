/// @file read_only_book.cpp
/// @brief What the program says of a book open read-only

#include "kalahari/gui/read_only_book.h"
#include "kalahari/core/package_card.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"

#include <QCoreApplication>
#include <QStringList>

namespace kalahari {
namespace gui {

namespace {

/// The name of package @p packageId in the program's language: the program's, when it has the
/// package (a package without all the kinds of the book), else the one on the book's card of
/// it, else its id
QString packageName(const core::ProjectManager& pm, const QString& packageId) {
    if (const core::BookTypePackage* package = pm.installedBookTypes().package(packageId)) {
        return package->name.text(programLanguage());
    }
    const core::PackageCard* card = pm.packageCard(packageId);
    return card != nullptr && !card->name.isEmpty() ? card->name.text(programLanguage())
                                                    : packageId;
}

} // namespace

QString readOnlyReason() {
    //: Why a command, a field or a panel that would change the open book is greyed out. In
    //: Polish: Książka jest otwarta tylko do odczytu
    return QCoreApplication::translate("ReadOnlyBook", "The book is open read-only");
}

QString readOnlyBookMessage() {
    const core::ProjectManager& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (!pm.isProjectOpen() || project == nullptr || !pm.isReadOnly()) {
        return {};
    }
    // The type first: the writer knows the book by it
    if (project->type && pm.installedBookTypes().package(project->type->id) == nullptr) {
        //: The message at the bottom of the window over a book open read-only; %1: the name of
        //: its type. In Polish: Książka otwarta tylko do odczytu: typu „%1” nie ma w programie.
        //: Możesz ją czytać i eksportować.
        return QCoreApplication::translate(
                   "ReadOnlyBook",
                   "The book is open read-only: the program does not have its type \"%1\". You "
                   "can read it and export it.")
            .arg(packageName(pm, project->type->id));
    }
    QStringList names;
    for (const QString& id : pm.missingPackages()) {
        names << packageName(pm, id);
    }
    //: The message at the bottom of the window over a book open read-only whose type the
    //: program has; %1: the names of the book types the program does not have, or does not
    //: have whole (an older version). In Polish: Książka otwarta tylko do odczytu: program nie
    //: ma wszystkiego, czego używa książka (%1). Możesz ją czytać i eksportować.
    return QCoreApplication::translate(
               "ReadOnlyBook",
               "The book is open read-only: the program does not have all the book uses (%1). "
               "You can read it and export it.")
        .arg(names.join(QStringLiteral(", ")));
}

} // namespace gui
} // namespace kalahari
