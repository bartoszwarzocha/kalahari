// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file link_roles.cpp
/// @brief How a material is linked with an element: the roles of links, and the paragraph a
/// link names

#include "kalahari/core/link_roles.h"

#include "kalahari/core/book_type_package.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/settings_manager.h"

#include <QRegularExpression>

#include <cstdlib>

namespace kalahari::core {

const QStringList& LinkRoles::all() {
    static const QStringList roles{
        QString::fromLatin1(QUOTED), QString::fromLatin1(SOURCE),
        QString::fromLatin1(INSPIRATION), QString::fromLatin1(ILLUSTRATION),
        QString::fromLatin1(TO_CHECK)};
    return roles;
}

const QStringList& LinkRoles::ofCards() {
    static const QStringList roles{
        QString::fromLatin1(POINT_OF_VIEW), QString::fromLatin1(APPEARS),
        QString::fromLatin1(MENTIONED), QString::fromLatin1(SETTING)};
    return roles;
}

QStringList LinkRoles::ofCard(const ElementKind& kind) {
    if (!kind.chapterRoles.isEmpty()) {
        return kind.chapterRoles;
    }
    return {QString::fromLatin1(APPEARS), QString::fromLatin1(MENTIONED)};
}

QString LinkRoles::suggestedForCard(const ElementKind& kind) {
    const QStringList roles = ofCard(kind);
    return roles.contains(QLatin1String(APPEARS)) ? QString::fromLatin1(APPEARS) : roles.first();
}

QString LinkRoles::suggestedFor(const QString& kindId) {
    if (kindId == QLatin1String(MaterialKinds::QUOTE)) {
        return QString::fromLatin1(QUOTED);
    }
    if (kindId == QLatin1String(MaterialKinds::PICTURE)) {
        return QString::fromLatin1(ILLUSTRATION);
    }
    if (kindId == QLatin1String(MaterialKinds::WEB_PAGE) ||
        kindId == QLatin1String(MaterialKinds::DOCUMENT) ||
        kindId == QLatin1String(MaterialKinds::PUBLICATION)) {
        return QString::fromLatin1(SOURCE);
    }
    return {};
}

bool LinkRoles::paragraphsShown() {
    return SettingsManager::getInstance().get<bool>(PARAGRAPHS_SETTING, true);
}

QString LinkRoles::excerptOf(const QString& paragraph) {
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    QStringList words = paragraph.split(spaces, Qt::SkipEmptyParts);
    if (words.size() > EXCERPT_WORDS) {
        words.resize(EXCERPT_WORDS);
    }
    return words.join(QLatin1Char(' '));
}

int LinkRoles::paragraphOf(const QStringList& paragraphs, const ElementLink& link) {
    const int count = static_cast<int>(paragraphs.size());
    if (link.paragraph <= 0) {
        return 0;
    }
    const auto startsWithExcerpt = [&paragraphs, &link](int number) {
        return excerptOf(paragraphs.at(number - 1)).startsWith(link.excerpt);
    };
    if (link.excerpt.isEmpty() || (link.paragraph <= count && startsWithExcerpt(link.paragraph))) {
        return link.paragraph <= count ? link.paragraph : 0;
    }
    // The nearest one with the excerpt, the earlier of two as near
    int found = 0;
    for (int number = 1; number <= count; ++number) {
        if (startsWithExcerpt(number) &&
            (found == 0 || std::abs(number - link.paragraph) < std::abs(found - link.paragraph))) {
            found = number;
        }
    }
    if (found > 0) {
        return found;
    }
    return link.paragraph <= count ? link.paragraph : 0;
}

} // namespace kalahari::core
