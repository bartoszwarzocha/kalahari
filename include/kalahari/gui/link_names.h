// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file link_names.h
/// @brief What the links of a material name in the open book: elements and sections, with
/// their titles, icons and places

#pragma once

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"

#include <QIcon>
#include <QString>

namespace kalahari {
namespace core {
class ProjectManager;
} // namespace core

namespace gui {

/// @brief Title of link @p link of the book open in @p book: the title of its element, or the
/// name of its section (core::BookProject::sectionLink()); empty when it names nothing of the
/// book
QString linkTitle(const core::ProjectManager& book, const QString& link);

/// @brief Icon of link @p link of the book open in @p book: that of its element, as the
/// Navigator shows it, or of its section
QIcon linkIcon(const core::ProjectManager& book, const QString& link, core::IconContext context);

/// @brief Where link @p link of the book open in @p book is, as a short name: the part or the
/// section of an element of the book, the group of an element of the Workshop ("Materials"), the
/// book of a section
QString linkPlace(const core::ProjectManager& book, const QString& link);

/// @brief Name of role @p role of a link (core::LinkRoles): "quoted in the text"...; the id
/// itself for a role of a later version, empty for none
QString linkRoleName(const QString& role);

/// @brief The role and the paragraph of @p link in a few words: "quoted in the text ·
/// paragraph 4"; the paragraph only while links may name one
/// (core::LinkRoles::paragraphsShown()); empty when it says neither
QString linkDetails(const core::ElementLink& link);

} // namespace gui
} // namespace kalahari
