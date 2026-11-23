// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file project_manager_publishing.cpp
/// @brief ProjectManager: the texts for publishers of the open book (a sentence about the book
/// and a synopsis), their templates and the set of the book

#include "kalahari/core/project_manager.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/volume_materials.h"
#include "kalahari/editor/text_skeleton.h"

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
    QString textId = addElement(kind, title, BookPlace::Workshop, QString(), -1, {}, QString());
    if (textId.isEmpty()) {
        *m_project = before;
        return {};
    }
    PublishingData data;
    data.part = part;
    data.templateId = templateReference;
    const bool saved =
        changeDetails("addPublishingText", textId, [this, &data](ProjectElement& text) {
            text.publishing = data;
            text.bookId = volumeOfNew(text);
        });
    if (!saved) {
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
    startWithSkeleton(textId, templateReference);
    return textId;
}

void ProjectManager::startWithSkeleton(const QString& textId, const QString& templateReference) {
    if (!PublishingTexts::pointsInText()) {
        return;
    }
    // The headings of the points of its template, each with its hint, in its text
    const QList<editor::SkeletonPoint> skeleton = templateSkeleton(templateReference);
    if (skeleton.isEmpty()) {
        return;
    }
    setChapterContent(textId, QStringLiteral("<kml>") + editor::skeletonKml(skeleton) +
                                  QStringLiteral("</kml>"));
    if (!saveChapterContent(textId)) {
        Logger::getInstance().warn("startWithSkeleton: The skeleton of {} is not saved yet",
                                   textId.toStdString());
    }
}

bool ProjectManager::setPublishing(const QString& elementId, const PublishingData& data) {
    return changeDetails("setPublishing", elementId,
                         [&data](ProjectElement& element) { element.publishing = data; });
}

bool ProjectManager::setPublishingPoints(const QString& elementId, const QStringList& done,
                                         bool textChanged) {
    const ProjectElement* element = findElement(elementId);
    if (element == nullptr || !isPublishingText(*element)) {
        Logger::getInstance().warn("setPublishingPoints: Not a text for publishers: {}",
                                   elementId.toStdString());
        return false;
    }
    if (refusesChange("setPublishingPoints")) {
        return false;
    }
    ElementState* state = stateOf(elementId);
    // The points wait for the changes of the text, to be saved or dropped with them
    if (state != nullptr && (textChanged || hasUnsavedText(elementId))) {
        if (done == element->publishing.done) {
            state->waitingPoints.reset();
        } else {
            state->waitingPoints = done;
        }
        return true;
    }
    const bool saved = changeDetails("setPublishingPoints", elementId,
                                     [&done](ProjectElement& text) { text.publishing.done = done; });
    if (saved && state != nullptr) {
        state->waitingPoints.reset();
    }
    return saved;
}

QStringList ProjectManager::publishingPointsOf(const QString& elementId) const {
    const ProjectElement* element = findElement(elementId);
    QStringList saved = element != nullptr ? element->publishing.done : QStringList();
    const auto state = m_elementStates.constFind(elementId);
    return state != m_elementStates.cend() ? state->waitingPoints.value_or(std::move(saved))
                                           : saved;
}

bool ProjectManager::hasWaitingPoints(const QString& elementId) const {
    const auto state = m_elementStates.constFind(elementId);
    return state != m_elementStates.cend() && state->waitingPoints.has_value();
}

bool ProjectManager::saveWaitingPoints(const QString& elementId) {
    const auto state = m_elementStates.constFind(elementId);
    if (state == m_elementStates.cend()) {
        return true;
    }
    const std::optional<QStringList> waiting = state->waitingPoints;
    if (!waiting) {
        return true;
    }
    const QStringList& done = *waiting;
    if (!changeDetails("saveWaitingPoints", elementId,
                       [&done](ProjectElement& text) { text.publishing.done = done; })) {
        return false;
    }
    m_elementStates[elementId].waitingPoints.reset();
    return true;
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
    const QString part = found->publishing.part;
    const qsizetype setBook = m_project->bookIndex(setBookOf(elementId)->id);
    const QStringList others = PublishingTexts::setState(*m_project, elementId, state);
    // The set of its book takes the text chosen, not another version it picked
    bool unpicked = false;
    if (ProjectBook& owner = m_project->books[setBook];
        state == QLatin1String(PublishingTexts::CHOSEN) && owner.publishingSet) {
        unpicked = owner.publishingSet->picks.remove(part) > 0;
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
    return setPublishingSetOf(*current, set);
}

bool ProjectManager::setPublishingSetOf(ProjectBook& book, const PublishingSet& set) {
    if (book.publishingSet == set) {
        return true;
    }
    const std::optional<PublishingSet> previous = book.publishingSet;
    book.publishingSet = set;
    if (!saveManifest()) {
        book.publishingSet = previous;
        return false;
    }
    emit publishingSetChanged();
    return true;
}

bool ProjectManager::pickForPublishingSet(const QString& elementId) {
    const ProjectElement* element = findElement(elementId);
    const ProjectBook* owner = setBookOf(elementId);
    if (owner == nullptr || element == nullptr || refusesChange("pickForPublishingSet")) {
        return false;
    }
    const QString part =
        isPublishingText(*element) ? element->publishing.part : element->cover.part;
    if (!PublishingTexts::versionsOf(*m_project, part, owner).contains(element)) {
        Logger::getInstance().warn("pickForPublishingSet: Not a version of a part of the set: {}",
                                   elementId.toStdString());
        return false;
    }
    // Without a pick, the set has the version chosen; a pick names another one
    PublishingSet set = owner->publishingSet.value_or(PublishingSet::defaults());
    set.picks.remove(part);
    const QList<PublishingSetPart> parts = PublishingTexts::resolveSet(*m_project, *owner, set);
    const bool taken = std::any_of(parts.cbegin(), parts.cend(), [&](const PublishingSetPart& p) {
        return p.part == part && p.elementId == elementId;
    });
    if (!taken) {
        set.picks.insert(part, elementId);
    }
    return setPublishingSetOf(m_project->books[m_project->bookIndex(owner->id)], set);
}

QList<PublishingSetPart> ProjectManager::publishingSetParts(const ProjectBook* volume) const {
    const ProjectBook* shown = volume != nullptr ? volume : book();
    if (!m_project || shown == nullptr) {
        return {};
    }
    return PublishingTexts::resolveSet(*m_project, *shown,
                                       shown->publishingSet.value_or(PublishingSet::defaults()));
}

const ProjectBook* ProjectManager::setBookOf(const QString& elementId) const {
    const ProjectElement* material = findElement(elementId);
    if (!m_project || material == nullptr) {
        return book();
    }
    const qsizetype volume =
        m_project->bookIndex(VolumeMaterials::volumeOf(*m_project, *material));
    return volume >= 0 ? &m_project->books.at(volume) : book();
}

QList<PublishingSetPart> ProjectManager::publishingSetPartsOf(const QString& elementId) const {
    const ProjectBook* owner = setBookOf(elementId);
    return owner != nullptr ? publishingSetParts(owner) : QList<PublishingSetPart>();
}

QString ProjectManager::publishingTextOf(const QString& elementId) const {
    const ProjectElement* element = findElement(elementId);
    if (element == nullptr) {
        return {};
    }
    if (!element->file.isEmpty() && formOf(*element) == ElementForm::Text) {
        // As in its editor; without the headings of the points of a skeleton, which are no
        // text of the writer
        const auto state = m_elementStates.constFind(elementId);
        const std::optional<QString> kml = state != m_elementStates.cend() && state->editorText
                                               ? state->editorText
                                               : textKmlOf(elementId);
        return kml ? ChapterDocument::kmlToPlainText(
                         *kml, QString::fromLatin1(editor::TEMPLATE_POINT_STYLE_ID))
                   : QString();
    }
    return element->description;
}

QList<PublishingTemplatePoint> ProjectManager::unlinkedPublishingPointsOf(
    const QString& elementId) const {
    const ProjectElement* element = findElement(elementId);
    if (element == nullptr || !PublishingTexts::hasPoints(element->publishing.part) ||
        element->publishing.templateId.isEmpty() || element->file.isEmpty() ||
        formOf(*element) != ElementForm::Text) {
        return {};
    }
    const std::optional<PublishingTemplate> templ =
        publishingTemplate(element->publishing.templateId);
    if (!templ) {
        return {};
    }
    const auto state = m_elementStates.constFind(elementId);
    const std::optional<QString> kml = state != m_elementStates.cend() && state->editorText
                                           ? state->editorText
                                           : textKmlOf(elementId);
    const QSet<QString> linked = kml ? PublishingTexts::linkedPointsOf(*kml) : QSet<QString>();
    QList<PublishingTemplatePoint> unlinked;
    for (const PublishingTemplatePoint& point : templ->pointsFor(mainTexts())) {
        if (!linked.contains(point.id)) {
            unlinked << point;
        }
    }
    return unlinked;
}

void ProjectManager::setEditorText(const QString& elementId, const QString& kml) {
    const ProjectElement* element = findElement(elementId);
    ElementState* state = element != nullptr && isPublishingText(*element) ? stateOf(elementId)
                                                                           : nullptr;
    if (state == nullptr || state->editorText == kml) {
        return;
    }
    state->editorText = kml;
    emit publishingTextChanged(elementId);
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

QList<editor::SkeletonPoint> ProjectManager::templateSkeleton(const QString& reference) const {
    const std::optional<PublishingTemplate> templ = publishingTemplate(reference);
    if (!templ || !PublishingTexts::hasPoints(templ->part)) {
        return {};
    }
    const QString language = bookLanguage();
    QList<editor::SkeletonPoint> skeleton;
    for (const PublishingTemplatePoint& point : templ->pointsFor(mainTexts())) {
        skeleton.append({point.id, point.name.text(language), point.hint.text(language)});
    }
    return skeleton;
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
