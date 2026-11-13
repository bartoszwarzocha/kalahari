/// @file book_texts.cpp
/// @brief The texts of the open book in reading order, and the saved text of one not open

#include "kalahari/gui/book_texts.h"

#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/chapter_versions.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"

#include <QFileInfo>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// The text elements of a list and the groups in it, in reading order
void collectTexts(const QList<core::ProjectElement>& elements, core::BookPlace place,
                  const core::KindRef& mainKind, std::vector<BookTextElement>& texts) {
    const auto& projects = core::ProjectManager::getInstance();
    for (const core::ProjectElement& element : elements) {
        if (!element.file.isEmpty() && projects.formOf(element) == core::ElementForm::Text) {
            const bool chapter = place == core::BookPlace::Main && mainKind &&
                                 projects.kindOf(element).kind == mainKind.kind;
            texts.push_back({&element, place, chapter});
        }
        collectTexts(element.elements, place, mainKind, texts);
    }
}

}  // namespace

std::vector<BookTextElement> bookTextElements() {
    std::vector<BookTextElement> texts;
    auto& projects = core::ProjectManager::getInstance();
    const core::ProjectBook* book = projects.isProjectOpen() ? projects.book() : nullptr;
    if (book == nullptr) {
        return texts;
    }
    const core::KindRef mainKind = projects.mainTextKind();
    collectTexts(book->frontElements, core::BookPlace::Front, mainKind, texts);
    collectTexts(book->mainElements, core::BookPlace::Main, mainKind, texts);
    collectTexts(book->backElements, core::BookPlace::Back, mainKind, texts);
    return texts;
}

std::optional<QString> savedTextIfChanged(const QString& elementId, QString& signature) {
    const auto& projects = core::ProjectManager::getInstance();
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

void keepVersionBeforeReplaceAll(const QString& elementId, const QString& kml,
                                 const QString& found, const QString& replacement, int count) {
    auto& projects = core::ProjectManager::getInstance();
    const auto& settings = core::SettingsManager::getInstance();
    if (!projects.isProjectOpen() || projects.isReadOnly() ||
        !settings.get<bool>(core::ChapterVersions::ENABLED_SETTING) ||
        !settings.get<bool>(core::ChapterVersions::BEFORE_REPLACE_ALL_SETTING)) {
        return;
    }
    const core::ProjectManager::VersionKept kept =
        projects.keepVersion(elementId, kml,
                             {.origin = core::VersionOrigin::ReplaceAll,
                              .found = found,
                              .replacement = replacement,
                              .replaced = count});
    if (kept == core::ProjectManager::VersionKept::Failed) {
        core::Logger::getInstance().warn("No version of {} was kept before Replace All",
                                         elementId.toStdString());
    }
}

}  // namespace kalahari::gui
