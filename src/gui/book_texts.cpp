// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

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

std::vector<BookTextElement> bookTextElements(const core::ProjectManager& book) {
    std::vector<BookTextElement> texts;
    const core::ProjectBook* opened = book.isProjectOpen() ? book.book() : nullptr;
    if (opened == nullptr) {
        return texts;
    }
    const core::KindRef mainKind = book.mainTextKind();
    collectTexts(book, opened->frontElements, core::BookPlace::Front, mainKind, texts);
    collectTexts(book, opened->mainElements, core::BookPlace::Main, mainKind, texts);
    collectTexts(book, opened->backElements, core::BookPlace::Back, mainKind, texts);
    return texts;
}

std::optional<QString> savedTextIfChanged(const core::ProjectManager& book,
                                          const QString& elementId, QString& signature) {
    // The unsaved text the program keeps, else its file: read again only when it changed
    const std::vector<QString> dirty = book.getDirtyElements();
    const bool unsaved = std::find(dirty.cbegin(), dirty.cend(), elementId) != dirty.cend();
    QString now;
    QString kml;
    if (unsaved) {
        kml = book.chapterText(elementId);
        now = QStringLiteral("text:%1:%2").arg(qHash(kml)).arg(kml.size());
    } else if (const core::ProjectElement* element = book.findElement(elementId)) {
        const QFileInfo file(book.filePathOf(*element));
        now = QStringLiteral("file:%1:%2")
                  .arg(file.lastModified().toMSecsSinceEpoch())
                  .arg(file.size());
    }
    if (!now.isEmpty() && now == signature) {
        return std::nullopt;
    }
    signature = now;
    return unsaved ? kml : book.chapterText(elementId);
}

void keepVersionBeforeReplaceAll(core::ProjectManager& book, const QString& elementId,
                                 const QString& kml, const QString& found,
                                 const QString& replacement, int count) {
    const auto& settings = core::SettingsManager::getInstance();
    if (!book.isProjectOpen() || book.isReadOnly() ||
        !settings.get<bool>(core::ChapterVersions::ENABLED_SETTING) ||
        !settings.get<bool>(core::ChapterVersions::BEFORE_REPLACE_ALL_SETTING)) {
        return;
    }
    const core::ProjectManager::VersionKept kept =
        book.keepVersion(elementId, kml,
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
