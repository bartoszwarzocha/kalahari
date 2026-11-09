/// @file link_names.cpp
/// @brief What the links of a material name in the open book

#include "kalahari/gui/link_names.h"

#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/project_manager.h"

#include <QCoreApplication>

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

QString linkRoleName(const QString& role) {
    using core::LinkRoles;
    if (role == QLatin1String(LinkRoles::QUOTED)) {
        //: A role of a link: a material quoted in the text of a chapter. In Polish: cytowany w
        //: tekście
        return QCoreApplication::translate("LinkRoles", "quoted in the text");
    }
    if (role == QLatin1String(LinkRoles::SOURCE)) {
        //: A role of a link: a material the text draws facts from. In Polish: źródło
        //: informacji
        return QCoreApplication::translate("LinkRoles", "source of information");
    }
    if (role == QLatin1String(LinkRoles::INSPIRATION)) {
        //: A role of a link. In Polish: inspiracja
        return QCoreApplication::translate("LinkRoles", "inspiration");
    }
    if (role == QLatin1String(LinkRoles::ILLUSTRATION)) {
        //: A role of a link: a picture of what the text tells. In Polish: ilustracja
        return QCoreApplication::translate("LinkRoles", "illustration");
    }
    if (role == QLatin1String(LinkRoles::TO_CHECK)) {
        //: A role of a link: something the writer still has to check. In Polish: do
        //: sprawdzenia
        return QCoreApplication::translate("LinkRoles", "to check");
    }
    return role;
}

QString linkDetails(const core::ElementLink& link) {
    QStringList parts;
    if (!link.role.isEmpty()) {
        parts << linkRoleName(link.role);
    }
    if (link.paragraph > 0 && core::LinkRoles::paragraphsShown()) {
        //: The paragraph of a chapter a link names, after its role. In Polish: akapit %1
        parts << QCoreApplication::translate("LinkRoles", "paragraph %1").arg(link.paragraph);
    }
    return parts.join(QStringLiteral(" · "));
}

} // namespace gui
} // namespace kalahari
