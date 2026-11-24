// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file file_associations_mac.h
/// @brief The program of the files of Kalahari on macOS (file_associations_mac.mm)

#ifndef KALAHARI_GUI_UTILS_FILE_ASSOCIATIONS_MAC_H
#define KALAHARI_GUI_UTILS_FILE_ASSOCIATIONS_MAC_H

#include "kalahari/gui/utils/file_associations.h"

#include <memory>

namespace kalahari::gui {

/// @brief macOS 12 and later: NSWorkspace tells and sets the default application of a type
[[nodiscard]] std::unique_ptr<FileAssociationSystem> macFileAssociationSystem();

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_UTILS_FILE_ASSOCIATIONS_MAC_H
