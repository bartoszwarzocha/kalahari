// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file project_manager_publishing.cpp
/// @brief ProjectManager: the texts for publishers of the open book (a sentence about the book
/// and a synopsis), their templates and the set of the book

#include "kalahari/core/project_manager.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/publishing_texts.h"

#include <QFile>
#include <QList>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <optional>
#include <utility>

namespace kalahari::core {

namespace {

/// The elements of @p elements and of the groups inside them, in reading order
QList<const ProjectElement*> inReadingOrder(const QList<ProjectElement>& elements) {
    QList<const ProjectElement*> all;
    QList<std::pair<const QList<ProjectElement>*, qsizetype>> lists{{&elements, 0}};
    while (!lists.isEmpty()) {
        auto& [list, next] = lists.last();
        if (next >= list->size()) {
            lists.removeLast();
            continue;
        }
        const ProjectElement& element = list->at(next);
        ++next;
        all.append(&element);
        if (!element.elements.isEmpty()) {
            lists.append({&element.elements, 0});
        }
    }
    return all;
}

}  // namespace

KindRef ProjectManager::publishingKind() const {
    return bookTypes().findKind(QString::fromLatin1(BASE_PACKAGE),
                                QString::fromLatin1(PublishingTexts::KIND));
}

bool ProjectManager::isPublishingText(const ProjectElement& element) {
    return !element.publishing.part.isEmpty();
}

QString ProjectManager::addPublishingText(const QString& part, const QString& templateReference,
                                          const QString& title) {
    auto& logger = Logger::getInstance();
    if (!m_project || refusesChange("addPublishingText")) {
        return {};
    }
    if (!PublishingTexts::parts().contains(part)) {
        logger.error("addPublishingText: {} is not a part of a text for publishers",
                     part.toStdString());
        return {};
    }
    const KindRef kind = publishingKind();
    if (!kind) {
        logger.error("addPublishingText: The base package has no texts for publishers");
        return {};
    }

    // A book without the texts among its kinds takes them, saved with the new text
    const BookProject before = *m_project;
    const QList<KindRef> offered = m_project->kindsIn(bookTypes(), BookPlace::Workshop);
    if (std::ranges::none_of(offered,
                             [&kind](const KindRef& each) { return each.kind == kind.kind; })) {
        m_project->kinds.append(
            KindReference{.packageId = kind.package->id, .kindId = kind.kind->id});
    }
    const QString textId =
        addElement(kind, title, BookPlace::Workshop, QString(), -1, {}, QString());
    if (textId.isEmpty()) {
        *m_project = before;
        return {};
    }
    PublishingData data;
    data.part = part;
    data.templateId = templateReference;
    if (!setPublishing(textId, data)) {
        // Without its part it would be no text for publishers: the book goes back as it was
        logger.error("addPublishingText: '{}' could not be saved as a text for publishers",
                     title.toStdString());
        if (const std::optional<ProjectElement> removed = removeElement(textId)) {
            for (const QString& file : filesOf(*removed)) {
                QFile::remove(file);
            }
        }
        return {};
    }
    return textId;
}

bool ProjectManager::setPublishing(const QString& elementId, const PublishingData& data) {
    return changeDetails("setPublishing", elementId,
                         [&data](ProjectElement& element) { element.publishing = data; });
}

bool ProjectManager::setPublishingState(const QString& elementId, const QString& state,
                                        QStringList* drafts) {
    if (drafts != nullptr) {
        drafts->clear();
    }
    if (refusesChange("setPublishingState")) {
        return false;
    }
    const ProjectElement* found = findElement(elementId);
    if (found == nullptr || found->publishing.part.isEmpty()) {
        Logger::getInstance().warn("setPublishingState: Not a text for publishers: {}",
                                   elementId.toStdString());
        return false;
    }
    // The project as it was, to put back when the file cannot be written
    const BookProject before = *m_project;
    const QStringList others = PublishingTexts::setState(*m_project, elementId, state);
    // The set takes the text chosen, not another version it picked
    bool unpicked = false;
    if (ProjectBook* current = book(); state == QLatin1String(PublishingTexts::CHOSEN) &&
                                       current != nullptr && current->publishingSet) {
        unpicked = current->publishingSet->picks.remove(found->publishing.part) > 0;
    }
    if (*m_project == before) {
        return true;
    }
    if (!saveManifest()) {
        *m_project = before;
        return false;
    }
    if (drafts != nullptr) {
        *drafts = others;
    }
    if (unpicked) {
        emit publishingSetChanged();
    }
    return true;
}

PublishingSet ProjectManager::publishingSet() const {
    const ProjectBook* current = book();
    return current != nullptr && current->publishingSet ? *current->publishingSet
                                                        : PublishingSet::defaults();
}

bool ProjectManager::setPublishingSet(const PublishingSet& set) {
    ProjectBook* current = book();
    if (current == nullptr) {
        Logger::getInstance().warn("setPublishingSet: No project is open");
        return false;
    }
    if (refusesChange("setPublishingSet")) {
        return false;
    }
    if (current->publishingSet == set) {
        return true;
    }
    const std::optional<PublishingSet> previous = current->publishingSet;
    current->publishingSet = set;
    if (!saveManifest()) {
        current->publishingSet = previous;
        return false;
    }
    emit publishingSetChanged();
    return true;
}

bool ProjectManager::pickForPublishingSet(const QString& elementId) {
    const ProjectBook* current = book();
    const ProjectElement* element = findElement(elementId);
    if (!m_project || current == nullptr || element == nullptr) {
        return false;
    }
    const QString part =
        isPublishingText(*element) ? element->publishing.part : element->cover.part;
    if (!PublishingTexts::versionsOf(*m_project, part).contains(element)) {
        Logger::getInstance().warn("pickForPublishingSet: Not a version of a part of the set: {}",
                                   elementId.toStdString());
        return false;
    }
    // Without a pick, the set has the version chosen; a pick names another one
    PublishingSet set = publishingSet();
    set.picks.remove(part);
    const QList<PublishingSetPart> parts = PublishingTexts::resolveSet(*m_project, *current, set);
    const bool taken = std::any_of(parts.cbegin(), parts.cend(), [&](const PublishingSetPart& p) {
        return p.part == part && p.elementId == elementId;
    });
    if (!taken) {
        set.picks.insert(part, elementId);
    }
    return setPublishingSet(set);
}

QList<PublishingSetPart> ProjectManager::publishingSetParts() const {
    const ProjectBook* current = book();
    if (!m_project || current == nullptr) {
        return {};
    }
    return PublishingTexts::resolveSet(*m_project, *current, publishingSet());
}

QString ProjectManager::publishingTextOf(const QString& elementId) const {
    const ProjectElement* element = findElement(elementId);
    if (element == nullptr) {
        return {};
    }
    if (!element->file.isEmpty() && formOf(*element) == ElementForm::Text) {
        return paragraphsOf(elementId).join(QLatin1Char('\n'));
    }
    return element->description;
}

QList<PublishingTemplate> ProjectManager::publishingTemplates() const {
    if (!m_project) {
        return {};
    }
    const BookTypeRegistry& registry = bookTypes();
    QList<PublishingTemplate> templates;
    const std::optional<ProjectType>& projectType = m_project->type;
    const QString typeId = projectType.has_value() ? projectType->id : QString();
    if (!typeId.isEmpty()) {
        templates = registry.publishingTemplates(typeId);
    } else {
        for (const BookTypePackage* type : registry.bookTypes()) {
            templates << type->publishingTemplates;
        }
    }
    for (const PublishingTemplate& own : PublishingTexts::ownTemplates()) {
        if (typeId.isEmpty() || own.isFor(typeId)) {
            templates << own;
        }
    }
    return templates;
}

std::optional<PublishingTemplate> ProjectManager::publishingTemplate(
    const QString& reference) const {
    const QString ownPrefix =
        QString::fromLatin1(PublishingTexts::OWN_TEMPLATE_PREFIX) + QLatin1Char(':');
    if (!reference.startsWith(ownPrefix)) {
        return bookTypes().findPublishingTemplate(reference);
    }
    const QString id = reference.mid(ownPrefix.size());
    for (const PublishingTemplate& own : PublishingTexts::ownTemplates()) {
        if (own.id == id) {
            return own;
        }
    }
    return std::nullopt;
}

QList<std::pair<QString, QString>> ProjectManager::mainTexts() const {
    const ProjectBook* current = book();
    if (current == nullptr) {
        return {};
    }
    QList<std::pair<QString, QString>> texts;
    for (const ProjectElement* element : inReadingOrder(current->mainElements)) {
        if (formOf(*element) == ElementForm::Text) {
            texts.append({element->id, element->title});
        }
    }
    return texts;
}

int ProjectManager::publishingTargetOf(const QString& elementId, bool words) const {
    const ProjectElement* element = findElement(elementId);
    if (element == nullptr) {
        return 0;
    }
    const std::optional<PublishingTemplate> templ =
        publishingTemplate(element->publishing.templateId);
    return PublishingTexts::targetOf(element->publishing, templ ? &*templ : nullptr,
                                     static_cast<int>(mainTexts().size()), words);
}

}  // namespace kalahari::core
