// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file project_manager_plan.cpp
/// @brief ProjectManager: the texts in the plan of the book, without chapter files, and the
/// elements put back where they were

#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/file_trash.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/project_manager.h>

#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QString>

#include <optional>

namespace kalahari::core {

namespace {

/// @brief Whether @p project has an element with the id of @p element or of an element inside
/// it
bool hasAnyId(const BookProject& project, const ProjectElement& element) {
    if (project.findElement(element.id) != nullptr) {
        return true;
    }
    for (const ProjectElement& inside : element.elements) {
        if (hasAnyId(project, inside)) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool ProjectManager::insertElement(const ProjectElement& element, const ElementPlace& place,
                                   const std::optional<QString>& kml) {
    auto& logger = Logger::getInstance();
    if (!m_project || refusesChange("insertElement")) {
        return false;
    }
    if (element.id.isEmpty() || hasAnyId(*m_project, element)) {
        logger.error("insertElement: The project has an element of the id of '{}' already",
                     element.title.toStdString());
        return false;
    }
    const BookProject previous = *m_project;
    QList<ProjectElement>* list = nullptr;
    if (place.groupId.isEmpty()) {
        list = &m_project->elementsIn(place.place, currentBookIndex());
    } else if (ProjectElement* group = m_project->findElement(place.groupId);
               group != nullptr && formOf(*group) == ElementForm::Group) {
        list = &group->elements;
    }
    if (list == nullptr || place.index < 0 || place.index > list->size()) {
        logger.error("insertElement: No place {} for '{}'", place.index,
                     element.title.toStdString());
        return false;
    }
    // A text that gets its file back writes it first: without it the .klh file is not changed
    const bool writesFile =
        kml.has_value() && !element.file.isEmpty() && formOf(element) == ElementForm::Text;
    if (writesFile) {
        if (QFileInfo::exists(filePathOf(element))) {
            logger.error("insertElement: {} is there already", element.file.toStdString());
            return false;
        }
        if (!writeChapterFile(element, kml)) {
            m_elementStates.remove(element.id);
            return false;
        }
    }
    list->insert(place.index, element);
    if (!saveManifest()) {
        *m_project = previous;
        if (writesFile) {
            QFile::remove(filePathOf(element));
            m_elementStates.remove(element.id);
        }
        return false;
    }
    logger.info("insertElement: Put back '{}' (id: {})", element.title.toStdString(),
                element.id.toStdString());
    return true;
}

bool ProjectManager::isPlannedText(const ProjectElement& element) const {
    return element.file.isEmpty() && element.hasKind() && formOf(element) == ElementForm::Text;
}

bool ProjectManager::isPlannedText(const QString& elementId) const {
    const ProjectElement* element = findElement(elementId);
    return element != nullptr && isPlannedText(*element);
}

QString ProjectManager::addPlannedText(const KindRef& kind, const QString& title,
                                       BookPlace place, const QString& groupId,
                                       qsizetype index) {
    auto& logger = Logger::getInstance();
    if (!m_project || refusesChange("addPlannedText")) {
        return QString();
    }
    if (!kind || kind.kind->form != ElementForm::Text || kind.kind->retired ||
        place == BookPlace::Workshop) {
        logger.error("addPlannedText: Only a text of the book can be in the plan");
        return QString();
    }
    const ElementPlace at =
        index < 0 ? newPlaceOf(kind, place, groupId) : ElementPlace{place, groupId, index};
    QList<ProjectElement>* list = listForNew(kind, place, at.groupId);
    if (list == nullptr) {
        return QString();
    }
    if (at.index < 0 || at.index > list->size() ||
        !BookProject::canBeAt(bookTypes(), *list, kind, at.index)) {
        logger.error("addPlannedText: An element of {} cannot be at place {} of its list",
                     kind.reference().toStdString(), at.index);
        return QString();
    }
    const BookProject previous = *m_project;
    ProjectElement element;
    element.id = m_project->newElementId();
    element.kind = KindReference{kind.package->id, kind.kind->id};
    element.title = title;
    list->insert(at.index, element);
    if (!saveManifest()) {
        *m_project = previous;
        return QString();
    }
    logger.info("addPlannedText: Added '{}' to the plan ({}, id: {})", title.toStdString(),
                kind.reference().toStdString(), element.id.toStdString());
    return element.id;
}

QString ProjectManager::createTextFile(const QString& elementId,
                                       const std::optional<QString>& kml) {
    auto& logger = Logger::getInstance();
    if (!m_project || refusesChange("createTextFile")) {
        return QString();
    }
    ProjectElement* element = m_project->findElement(elementId);
    const std::optional<ElementPlace> at = m_project->placeOf(elementId);
    if (element == nullptr || !at || !isPlannedText(*element)) {
        logger.error("createTextFile: {} is not a text in the plan", elementId.toStdString());
        return QString();
    }
    const KindRef kind = kindOf(*element);
    ProjectElement withFile = *element;
    withFile.file = newChapterFile(kind, at->place);
    withFile.status = QStringLiteral("draft");
    QString text = kml.value_or(QString());
    if (!kml && book()) {
        text = startingText(kind, *book());
    }
    if (!writeChapterFile(withFile, text)) {
        m_elementStates.remove(elementId);
        return QString();
    }
    const ProjectElement before = *element;
    element->file = withFile.file;
    element->status = withFile.status;
    if (!saveManifest()) {
        if (ProjectElement* again = m_project->findElement(elementId)) {
            *again = before;
        }
        QFile::remove(filePathOf(withFile));
        m_elementStates.remove(elementId);
        return QString();
    }
    logger.info("createTextFile: '{}' has its file {}", withFile.title.toStdString(),
                withFile.file.toStdString());
    return filePathOf(withFile);
}

bool ProjectManager::returnToPlan(const QString& elementId, bool trash) {
    auto& logger = Logger::getInstance();
    if (!m_project || refusesChange("returnToPlan")) {
        return false;
    }
    ProjectElement* element = m_project->findElement(elementId);
    const std::optional<ElementPlace> at = m_project->placeOf(elementId);
    if (element == nullptr || !at || at->place == BookPlace::Workshop ||
        formOf(*element) != ElementForm::Text || element->file.isEmpty()) {
        logger.error("returnToPlan: {} is not a text of the book with a file",
                     elementId.toStdString());
        return false;
    }
    const QString path = filePathOf(*element);
    if (trash && QFileInfo::exists(path) && !canMoveToTrash(path)) {
        logger.error("returnToPlan: {} cannot go to the Trash", path.toStdString());
        return false;
    }
    const ProjectElement before = *element;
    element->file.clear();
    element->status.clear();
    if (!saveManifest()) {
        if (ProjectElement* again = m_project->findElement(elementId)) {
            *again = before;
        }
        return false;
    }
    const bool gone =
        !QFileInfo::exists(path) || (trash ? moveToTrash(path) : QFile::remove(path));
    if (!gone) {
        // The file stays, and so does the text with it
        if (ProjectElement* again = m_project->findElement(elementId)) {
            *again = before;
        }
        saveManifest();
        logger.error("returnToPlan: {} could not go", path.toStdString());
        return false;
    }
    m_elementStates.remove(elementId);
    logger.info("returnToPlan: '{}' is in the plan again", before.title.toStdString());
    return true;
}

}  // namespace kalahari::core
