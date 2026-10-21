/// @file link_names.cpp
/// @brief What the links of a material name in the open book

#include "kalahari/gui/link_names.h"

#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/project_manager.h"

namespace kalahari {
namespace gui {

namespace {

/// The icons of the sections, as the Navigator has them
QString sectionIconId(core::BookPlace place) {
    switch (place) {
        case core::BookPlace::Front:
            return QStringLiteral("structure.frontmatter");
        case core::BookPlace::Back:
            return QStringLiteral("structure.backmatter");
        default:
            return QStringLiteral("structure.body");
    }
}

} // namespace

QString linkTitle(const QString& link) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (project == nullptr) {
        return {};
    }
    if (const core::ProjectElement* element = project->findElement(link)) {
        return element->title;
    }
    for (qsizetype book = 0; book < project->books.size(); ++book) {
        if (const std::optional<core::BookPlace> place = project->linkedSection(link, book)) {
            return project->books.at(book).sectionName(*place);
        }
    }
    return {};
}

QIcon linkIcon(const QString& link, core::IconContext context) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (project == nullptr) {
        return {};
    }
    if (const core::ProjectElement* element = project->findElement(link)) {
        return NavigatorPanel::iconOf(pm.bookTypes(), *element, context);
    }
    for (qsizetype book = 0; book < project->books.size(); ++book) {
        if (const std::optional<core::BookPlace> place = project->linkedSection(link, book)) {
            return core::ArtProvider::getInstance().getIcon(sectionIconId(*place), context);
        }
    }
    return {};
}

QString linkPlace(const QString& link) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (project == nullptr) {
        return {};
    }
    for (qsizetype book = 0; book < project->books.size(); ++book) {
        if (project->linkedSection(link, book)) {
            return NavigatorPanel::bookName(project->books.at(book));
        }
    }
    const core::ProjectElement* element = project->findElement(link);
    const std::optional<core::ElementPlace> at = project->placeOf(link);
    if (element == nullptr || !at) {
        return {};
    }
    if (at->place == core::BookPlace::Workshop) {
        return core::fillWords(QStringLiteral("{Kind:plural}"),
                               {{QStringLiteral("kind"), wordsOf(*element)}});
    }
    if (!at->groupId.isEmpty()) {
        if (const core::ProjectElement* group = project->findElement(at->groupId)) {
            return group->title;
        }
    }
    return project->books.isEmpty() ? QString()
                                    : project->books.first().sectionName(at->place);
}

} // namespace gui
} // namespace kalahari
