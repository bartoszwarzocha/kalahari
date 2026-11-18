// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file length_meter.cpp
/// @brief The length of a text against its target: its counts, the target and a bar

#include "kalahari/gui/widgets/length_meter.h"

#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/publishing_names.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QProgressBar>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// The lowest height of the bar; it is half a line high
constexpr int MIN_BAR_HEIGHT = 6;

}  // namespace

LengthMeter::LengthMeter(QWidget* parent)
    : QWidget(parent),
      m_counts(new QLabel(this)),
      m_target(new QLabel(this)),
      m_bar(new QProgressBar(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    auto* row = new QHBoxLayout();
    m_counts->setObjectName(QStringLiteral("lengthCounts"));
    m_counts->setWordWrap(true);
    m_target->setObjectName(QStringLiteral("lengthTarget"));
    m_target->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(m_counts, 1);
    row->addWidget(m_target);
    layout->addLayout(row);
    m_bar->setObjectName(QStringLiteral("lengthBar"));
    m_bar->setTextVisible(false);
    m_bar->setMaximumHeight(std::max(MIN_BAR_HEIGHT, fontMetrics().height() / 2));
    //: In Polish: Długość względem celu
    m_bar->setAccessibleName(tr("Length against the target"));
    layout->addWidget(m_bar);
}

void LengthMeter::setLength(int characters, int words, int target, bool inWords, bool pages) {
    QString counts = CoverNames::countsText(characters, words);
    if (pages) {
        counts += QStringLiteral(" · ") + PublishingNames::pagesText(characters);
    }
    m_counts->setText(counts);
    m_target->setVisible(target > 0);
    m_bar->setVisible(target > 0);
    if (target <= 0) {
        return;
    }
    const int length = inWords ? words : characters;
    const bool over = length > target;
    m_bar->setRange(0, target);
    m_bar->setValue(std::min(length, target));
    // Past the target the bar and its words take the warning color of the theme
    QPalette warning;
    if (over) {
        const QColor color = core::ThemeManager::getInstance().getCurrentTheme().log.warning;
        warning.setColor(QPalette::Highlight, color);
        warning.setColor(QPalette::WindowText, color);
    }
    m_bar->setPalette(warning);
    m_target->setPalette(warning);
    QFont font = m_target->font();
    font.setBold(over);
    m_target->setFont(font);
    m_target->setText(over ? CoverNames::overText(length - target, inWords)
                           : CoverNames::targetText(target, inWords));
    m_bar->setAccessibleDescription(m_target->text());
}

}  // namespace kalahari::gui
