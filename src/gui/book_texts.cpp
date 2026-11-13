/// @file book_texts.cpp
/// @brief The texts of the open book in reading order, and the saved text of one not open

#include "kalahari/gui/book_texts.h"

#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/project_manager.h"

#include <QFileInfo>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// The text elements of a list and the groups in it, in reading order
void collectTexts(const core::ProjectManager& projects,
                  const QList<core::ProjectElement>& elements, core::BookPlace place,
                  const core::KindRef& mainKind, std::vector<BookTextElement>& texts) {
    for (const core::ProjectElement& element : elements) {
        if (!element.file.isEmpty() && projects.formOf(element) == core::ElementForm::Text) {
            const bool chapter = place == core::BookPlace::Main && mainKind &&
                                 projects.kindOf(element).kind == mainKind.kind;
            texts.push_back({&element, place, chapter});
        }
        collectTexts(projects, element.elements, place, mainKind, texts);
    }
}

}  // namespace

std::vector<BookTextElement> bookTextElements(const core::ProjectManager& projects) {
    std::vector<BookTextElement> texts;
    const core::ProjectBook* book = projects.isProjectOpen() ? projects.book() : nullptr;
    if (book == nullptr) {
        return texts;
    }
    const core::KindRef mainKind = projects.mainTextKind();
    collectTexts(projects, book->frontElements, core::BookPlace::Front, mainKind, texts);
    collectTexts(projects, book->mainElements, core::BookPlace::Main, mainKind, texts);
    collectTexts(projects, book->backElements, core::BookPlace::Back, mainKind, texts);
    return texts;
}

std::optional<QString> savedTextIfChanged(const core::ProjectManager& projects,
                                          const QString& elementId, QString& signature) {
    // The unsaved text the program keeps, else its file: read again only when it changed
    const std::vector<QString> dirty = projects.getDirtyElements();
    const bool unsaved = std::find(dirty.cbegin(), dirty.cend(), elementId) != dirty.cend();
    QString now;
    QString kml;
    if (unsaved) {
        kml = projects.chapterText(elementId);
        now = QStringLiteral("text:%1:%2").arg(qHash(kml)).arg(kml.size());
    } else if (const core::ProjectElement* element = projects.findElement(elementId)) {
        const QFileInfo file(projects.filePathOf(*element));
        now = QStringLiteral("file:%1:%2")
                  .arg(file.lastModified().toMSecsSinceEpoch())
                  .arg(file.size());
    }
    if (!now.isEmpty() && now == signature) {
        return std::nullopt;
    }
    signature = now;
    return unsaved ? kml : projects.chapterText(elementId);
}

}  // namespace kalahari::gui
