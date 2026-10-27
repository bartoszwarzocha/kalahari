/// @file kind_names.cpp
/// @brief The names of kinds of elements in the program's sentences, in the program's language

#include "kalahari/gui/kind_names.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/package_card.h"
#include "kalahari/core/project_manager.h"

#include <QCoreApplication>
#include <QHash>
#include <QStringList>

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

QString typeNameOf(const core::BookTypeRegistry& registry, const core::BookProject& project,
                   const core::PackageCard* card) {
    if (!project.type) {
        //: The type of a book whose author chose its kinds. In Polish: Projekt użytkownika
        return QCoreApplication::translate("KindNames", "User Project");
    }
    if (const core::BookTypePackage* package = registry.package(project.type->id)) {
        return package->name.text(programLanguage());
    }
    // A type the program does not have: the name on the book's card of it, else its id
    const QString name = card != nullptr && !card->name.isEmpty()
                             ? card->name.text(programLanguage())
                             : project.type->id;
    //: The type of a book that the program does not have; %1: its name. In Polish:
    //: %1 (nie ma w programie)
    return QCoreApplication::translate("KindNames", "%1 (not in the program)").arg(name);
}

core::KindWords wordsOf(const core::KindRef& kind) {
    return kind ? kind.kind->wordsIn(programLanguage()) : core::KindWords{};
}

QString startTitles(const QList<core::StartElement>& elements, const QString& language) {
    QStringList titles;
    QHash<const core::ElementKind*, int> numbers;
    for (const auto form : {core::ElementForm::Group, core::ElementForm::Text}) {
        for (const core::StartElement& element : elements) {
            if (element.kind && element.kind.kind->form == form) {
                //: A title of an element a new book starts with, in a sentence. In Polish: „%1”
                titles << QCoreApplication::translate("KindNames", "\"%1\"")
                              .arg(element.kind.kind->defaultTitle(
                                  language, ++numbers[element.kind.kind]));
            }
        }
    }
    return titles.join(QStringLiteral(", "));
}

core::KindWords menuWords(core::KindWords words) {
    for (QString& form : words.forms) {
        form.replace(QLatin1Char('&'), QStringLiteral("&&"));
    }
    return words;
}

QString newTextMenuText(const core::KindRef& kind) {
    if (!kind) {
        return QCoreApplication::translate("CommandRegistrar", "&New Chapter...");
    }
    //: Book > New Chapter... named after the texts it adds, e.g. New Story... In Polish:
    //: &{kind:m=Nowy|f=Nowa|n=Nowe|p=Nowe} {kind}...
    return core::fillWords(QCoreApplication::translate("KindNames", "&New {Kind}..."),
                           {{QStringLiteral("kind"), menuWords(wordsOf(kind))}});
}

core::KindWords wordsOf(const core::BookTypeRegistry& registry,
                        const core::ProjectElement& element, const core::PackageCard* card) {
    if (const core::KindRef kind = core::BookProject::kindOf(registry, element)) {
        return wordsOf(kind);
    }
    // A file of the book outside its text has no kind
    if (!element.hasKind()) {
        return fileWords();
    }
    // A kind the program does not have: the names on the book's card of its package, else
    // its id stands for its name
    if (const core::ElementKind* kind =
            card != nullptr ? card->kind(element.kind.kindId) : nullptr) {
        return kind->wordsIn(programLanguage());
    }
    core::KindWords words;
    words.forms.insert(QStringLiteral("singular"), element.kind.kindId);
    words.forms.insert(QStringLiteral("plural"), element.kind.kindId);
    return words;
}

core::KindWords wordsOf(const core::ProjectElement& element) {
    const core::ProjectManager& pm = core::ProjectManager::getInstance();
    return wordsOf(pm.bookTypes(), element, pm.packageCard(element.kind.packageId));
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

core::KindWords fileWords() {
    // Each form is a text of its own, as in elementWords()
    core::KindWords words;
    //: The word "file" in sentences about a file of the book outside its text, such as notes:
    //: "Delete the file "Notes" from the book?". In Polish: plik
    words.forms.insert(QStringLiteral("singular"),
                       QCoreApplication::translate("KindNames", "file", "singular"));
    //: In Polish: pliki
    words.forms.insert(QStringLiteral("plural"),
                       QCoreApplication::translate("KindNames", "files", "plural"));
    //: In Polish: pliku
    words.forms.insert(QStringLiteral("genitive"),
                       QCoreApplication::translate("KindNames", "file", "genitive"));
    //: In Polish: plikowi
    words.forms.insert(QStringLiteral("dative"),
                       QCoreApplication::translate("KindNames", "file", "dative"));
    //: In Polish: plik
    words.forms.insert(QStringLiteral("accusative"),
                       QCoreApplication::translate("KindNames", "file", "accusative"));
    //: In Polish: plikiem
    words.forms.insert(QStringLiteral("instrumental"),
                       QCoreApplication::translate("KindNames", "file", "instrumental"));
    //: In Polish: pliku
    words.forms.insert(QStringLiteral("locative"),
                       QCoreApplication::translate("KindNames", "file", "locative"));
    //: In Polish: plików
    words.forms.insert(QStringLiteral("genitivePlural"),
                       QCoreApplication::translate("KindNames", "files", "genitive plural"));
    //: With the indefinite article. In Polish: plik
    words.forms.insert(QStringLiteral("indefinite"),
                       QCoreApplication::translate("KindNames", "a file", "indefinite"));
    //: The grammatical gender of the word "file" in the translation, for the words that agree
    //: with it: one of the English words masculine, feminine, neuter or plural, not
    //: translated. In Polish: masculine
    const QString gender =
        QCoreApplication::translate("KindNames", "neuter", "gender of the word file");
    if (!core::KindWords::genderFromName(gender, words.gender)) {
        words.gender = core::Gender::Neuter;
    }
    return words;
}

} // namespace gui
} // namespace kalahari
