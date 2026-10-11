/// @file kind_names.cpp
/// @brief The names of kinds of elements in the program's sentences, in the program's language

#include "kalahari/gui/kind_names.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"

#include <QCoreApplication>

namespace kalahari {
namespace gui {

namespace {

QString& language() {
    static QString code = QStringLiteral("en");
    return code;
}

} // namespace

QString programLanguage() {
    return language();
}

void setProgramLanguage(const QString& code) {
    language() = code.isEmpty() ? QStringLiteral("en") : code;
}

QString typeNameOf(const core::BookTypeRegistry& registry, const core::BookProject& project) {
    if (!project.type) {
        //: The type of a book whose author chose its kinds. In Polish: Projekt użytkownika
        return QCoreApplication::translate("KindNames", "User Project");
    }
    if (const core::BookTypePackage* package = registry.package(project.type->id)) {
        return package->name.text(programLanguage());
    }
    //: The type of a book whose package is not installed; %1: the id of the type. In Polish:
    //: %1 (typ niezainstalowany)
    return QCoreApplication::translate("KindNames", "%1 (not installed)").arg(project.type->id);
}

core::KindWords wordsOf(const core::KindRef& kind) {
    return kind ? kind.kind->wordsIn(programLanguage()) : core::KindWords{};
}

core::KindWords wordsOf(const core::BookTypeRegistry& registry,
                        const core::ProjectElement& element) {
    if (const core::KindRef kind = core::BookProject::kindOf(registry, element)) {
        return wordsOf(kind);
    }
    // A kind of a package that is not installed: its id stands for its name
    core::KindWords words;
    words.forms.insert(QStringLiteral("singular"), element.kind.kindId);
    words.forms.insert(QStringLiteral("plural"), element.kind.kindId);
    return words;
}

core::KindWords wordsOf(const core::ProjectElement& element) {
    return wordsOf(core::ProjectManager::getInstance().bookTypes(), element);
}

bool sameKind(const core::BookTypeRegistry& registry, const core::ProjectElement& a,
              const core::ProjectElement& b) {
    const core::KindRef kindA = core::BookProject::kindOf(registry, a);
    const core::KindRef kindB = core::BookProject::kindOf(registry, b);
    if (kindA || kindB) {
        return kindA && kindB && kindA.kind == kindB.kind;
    }
    return a.kind.kindId == b.kind.kindId;
}

core::KindWords mainWords() {
    return wordsOf(core::ProjectManager::getInstance().mainTextKind());
}

core::KindWords elementWords() {
    // Each form is a text of its own, so that a translation gives the forms of its language,
    // as the packages give the forms of the names of kinds
    core::KindWords words;
    //: The word "element" in sentences about elements of different kinds: "Together with
    //: it, 3 elements will be deleted from the book". In Polish: element
    words.forms.insert(QStringLiteral("singular"),
                       QCoreApplication::translate("KindNames", "element", "singular"));
    //: In Polish: elementy
    words.forms.insert(QStringLiteral("plural"),
                       QCoreApplication::translate("KindNames", "elements", "plural"));
    //: In Polish: elementu
    words.forms.insert(QStringLiteral("genitive"),
                       QCoreApplication::translate("KindNames", "element", "genitive"));
    //: In Polish: elementowi
    words.forms.insert(QStringLiteral("dative"),
                       QCoreApplication::translate("KindNames", "element", "dative"));
    //: In Polish: element
    words.forms.insert(QStringLiteral("accusative"),
                       QCoreApplication::translate("KindNames", "element", "accusative"));
    //: In Polish: elementem
    words.forms.insert(QStringLiteral("instrumental"),
                       QCoreApplication::translate("KindNames", "element", "instrumental"));
    //: In Polish: elemencie
    words.forms.insert(QStringLiteral("locative"),
                       QCoreApplication::translate("KindNames", "element", "locative"));
    //: In Polish: elementów
    words.forms.insert(QStringLiteral("genitivePlural"),
                       QCoreApplication::translate("KindNames", "elements", "genitive plural"));
    //: With the indefinite article. In Polish: element
    words.forms.insert(QStringLiteral("indefinite"),
                       QCoreApplication::translate("KindNames", "an element", "indefinite"));
    //: The grammatical gender of the word "element" in the translation, for the words that
    //: agree with it: one of the English words masculine, feminine, neuter or plural, not
    //: translated. In Polish: masculine
    const QString gender =
        QCoreApplication::translate("KindNames", "neuter", "gender of the word element");
    if (!core::KindWords::genderFromName(gender, words.gender)) {
        words.gender = core::Gender::Neuter;
    }
    return words;
}

} // namespace gui
} // namespace kalahari
