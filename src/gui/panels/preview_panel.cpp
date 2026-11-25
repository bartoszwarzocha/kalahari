// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file preview_panel.cpp
/// @brief The preview of the file of a material on a tab in the center of the window

#include "kalahari/gui/panels/preview_panel.h"

namespace kalahari::gui {

PreviewPanel::PreviewPanel(const QString& elementId, QWidget* parent)
    : QWidget(parent), m_elementId(elementId) {
    setProperty("elementId", elementId);
}

QString PreviewPanel::pageStatus() const {
    return {};
}

bool PreviewPanel::canFind() const {
    return false;
}

bool PreviewPanel::showPage(const QString& page) {
    Q_UNUSED(page)
    return false;
}

void PreviewPanel::refresh() {
    // A preview without anything of the material to show again
}

void PreviewPanel::find() {
    // A preview without a search of its own
}

}  // namespace kalahari::gui
