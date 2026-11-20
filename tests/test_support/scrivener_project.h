// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file scrivener_project.h
/// @brief A project of Scrivener the tests of its import read
///
/// Karawana.scriv: in its draft "Manuscript" a part "Część pierwsza" with a text of its own, a
/// chapter "Rozdział 1" of two scenes and one left out of the compile, and a chapter
/// "Rozdział 2" with two snapshots; then "Epilog" and an empty folder. Its research: a picture
/// "Mapa" with a synopsis and notes, a text "Notatki o oazie", and the folder "Characters" with
/// the text "Anna"; one document in its Trash.

#pragma once

#include <QString>

namespace kalahari::test {

/// @brief Write the project Karawana.scriv into @p folder
/// @return Its folder
QString makeScrivenerProject(const QString& folder);

}  // namespace kalahari::test
