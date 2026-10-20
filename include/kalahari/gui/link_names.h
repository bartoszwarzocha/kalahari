/// @file link_names.h
/// @brief What the links of a material name in the open book: elements and sections, with
/// their titles, icons and places

#pragma once

#include "kalahari/core/art_provider.h"

#include <QIcon>
#include <QString>

namespace kalahari {
namespace gui {

/// @brief Title of link @p link of the open book: the title of its element, or the name of its
/// section (core::BookProject::sectionLink()); empty when it names nothing of the book
QString linkTitle(const QString& link);

/// @brief Icon of link @p link of the open book: that of its element, as the Navigator shows
/// it, or of its section
QIcon linkIcon(const QString& link, core::IconContext context);

/// @brief Where link @p link of the open book is, as a short name: the part or the section of an
/// element of the book, the group of an element of the Workshop ("Materials"), the book of a
/// section
QString linkPlace(const QString& link);

} // namespace gui
} // namespace kalahari
