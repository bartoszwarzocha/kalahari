// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file status_bar_writing.cpp
/// @brief The time of writing today and the days in a row, in the status bar

#include "kalahari/gui/widgets/status_bar_writing.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QStatusBar>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// @brief A label with a small mark before its text, in the color of the text
class GlyphLabel : public QLabel {
public:
    GlyphLabel(stats_painter::Glyph glyph, QWidget* parent)
        : QLabel(parent)
        , m_glyph(glyph)
    {
        setFrameStyle(QFrame::NoFrame);
        updateMargin();
    }

protected:
    void changeEvent(QEvent* event) override
    {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::FontChange) {
            updateMargin();
        }
    }

    void paintEvent(QPaintEvent* event) override
    {
        QLabel::paintEvent(event);
        QPainter painter(this);
        const int side = glyphSize();
        stats_painter::drawGlyph(painter, m_glyph,
                                 QRectF(1, (height() - side) / 2.0, side, side),
                                 palette().color(QPalette::WindowText));
    }

private:
    int glyphSize() const { return std::max(10, fontMetrics().height() * 3 / 4); }

    void updateMargin() { setContentsMargins(glyphSize() + 5, 0, 0, 0); }

    stats_painter::Glyph m_glyph;
};

}  // namespace

StatusBarWriting::StatusBarWriting(QStatusBar* statusBar, WritingStatsModel* model,
                                   QObject* parent)
    : QObject(parent)
    , m_model(model)
{
    m_time = new GlyphLabel(stats_painter::Glyph::Clock, statusBar);
    m_time->setObjectName(QStringLiteral("statusWritingTime"));
    m_streak = new GlyphLabel(stats_painter::Glyph::Flame, statusBar);
    m_streak->setObjectName(QStringLiteral("statusStreak"));
    statusBar->addPermanentWidget(m_time);
    statusBar->addPermanentWidget(m_streak);
    if (m_model != nullptr) {
        connect(m_model, &WritingStatsModel::changed, this, &StatusBarWriting::update);
        connect(m_model, &WritingStatsModel::goalsChanged, this, &StatusBarWriting::update);
        connect(m_model, &QObject::destroyed, this, [this]() { m_model = nullptr; });
    }
    // The setting may change on another thread: the labels change on this one
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key == SETTING) {
                QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
            }
        });
    update();
}

StatusBarWriting::~StatusBarWriting()
{
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

void StatusBarWriting::update()
{
    const bool shown =
        m_model != nullptr && core::SettingsManager::getInstance().get<bool>(SETTING, true);
    m_time->setVisible(shown);
    m_streak->setVisible(shown);
    if (!shown) {
        return;
    }
    const core::WritingStatistics statistics = m_model->goalStatistics();
    const QDate today = m_model->today();
    const QString time = stats_painter::hoursMinutes(statistics.day(today).activeMs);
    const int streak = statistics.streak(today);
    m_time->setText(time);
    //: In the status bar. In Polish: Czas pisania dziś: %1
    m_time->setToolTip(tr("Time of writing today: %1").arg(time));
    m_time->setAccessibleName(m_time->toolTip());
    m_streak->setText(streak == 1
                          //: In the status bar. In Polish: 1 dzień
                          ? tr("1 day")
                          //: In the status bar. In Polish: %1 dni
                          : tr("%1 days", nullptr, streak).arg(stats_painter::number(streak)));
    //: In Polish: Dni z rzędu: %1
    m_streak->setToolTip(tr("Days in a row: %1").arg(stats_painter::number(streak)));
    m_streak->setAccessibleName(m_streak->toolTip());
}

}  // namespace kalahari::gui
