/// @file kind_names.cpp
/// @brief The names of kinds of elements in the program's sentences, in the program's language

#include "kalahari/gui/kind_names.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"

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

core::KindWords mainWords() {
    return wordsOf(core::ProjectManager::getInstance().mainTextKind());
}

} // namespace gui
} // namespace kalahari
