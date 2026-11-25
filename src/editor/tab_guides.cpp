// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file tab_guides.cpp
/// @brief The tab guides of a text: vertical dashed lines across its pages that a Tab aligns
///        the text to

#include "kalahari/editor/tab_guides.h"

#include <QTextDocument>
#include <QVariant>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <vector>

namespace kalahari::editor {

std::vector<double> normalizedTabGuides(std::vector<double> guides) {
    std::erase_if(guides, [](double guide) { return !std::isfinite(guide); });
    for (double& guide : guides) {
        guide = std::clamp(std::round(guide / TAB_GUIDE_STEP_MM) * TAB_GUIDE_STEP_MM, 0.0,
                           TAB_GUIDE_MAX_MM);
    }
    std::sort(guides.begin(), guides.end());
    std::vector<double> kept;
    for (const double guide : guides) {
        if (guide <= 0.0 || (!kept.empty() && guide - kept.back() < TAB_GUIDE_MIN_GAP_MM)) {
            continue;  // at the edge of the text, or too close to the one before it
        }
        if (static_cast<int>(kept.size()) == MAX_TAB_GUIDES) {
            break;
        }
        kept.push_back(guide);
    }
    return kept;
}

std::vector<double> tabGuidesOf(const QTextDocument& document) {
    std::vector<double> guides;
    for (const QVariant& guide : document.property(TAB_GUIDES_PROPERTY).toList()) {
        guides.push_back(guide.toDouble());
    }
    return guides;
}

void setTabGuidesOf(QTextDocument& document, const std::vector<double>& guides) {
    const std::vector<double> kept = normalizedTabGuides(guides);
    QVariantList list;
    for (const double guide : kept) {
        list.append(guide);
    }
    // Its layout follows the property (KalahariTextDocumentLayout::eventFilter())
    if (normalizedTabGuides(tabGuidesOf(document)) != kept) {
        document.setProperty(TAB_GUIDES_PROPERTY, list.isEmpty() ? QVariant() : QVariant(list));
    }
}

}  // namespace kalahari::editor
