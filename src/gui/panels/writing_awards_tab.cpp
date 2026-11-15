// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_awards_tab.cpp
/// @brief The tab Challenges and Badges

#include "kalahari/gui/panels/writing_awards_tab.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/widgets/badge_painter.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/writing_awards_tracker.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QBoxLayout>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTabWidget>

#include <algorithm>
#include <cmath>

namespace kalahari::gui {

namespace {

/// @brief The settings that turn the badges and the challenges on
constexpr const char* BADGES_SETTING = "stats.badges";
constexpr const char* CHALLENGES_SETTING = "stats.challenges";

/// @brief What an item of a list keeps
enum Role {
    KindRole = Qt::UserRole + 1,  ///< The kind of a badge, or the number of a challenge
    LevelRole,                    ///< The level of a badge
    WonRole,                      ///< It is won, or the challenge is done
    ShareRole,                    ///< How far it is, 0 to 1
    NameRole,                     ///< The name of a badge, the title of a challenge
    SmallRole,                    ///< The threshold of a badge, the period of a challenge
    ValueRole,                    ///< How far a challenge is: "3,200 of 5,000"
    StateRole,                    ///< What is left of a challenge
    OverRole,                     ///< The time of a challenge is up
};

/// @brief The muted color of the notes, of the badges not won and of the small texts
QColor mutedColor()
{
    return SettingsPage::noteColor();
}

/// @brief A font a little smaller than @p font
QFont smallFont(QFont font)
{
    font.setPointSizeF(font.pointSizeF() * 0.88);
    return font;
}

/// @brief A thin bar of @p share in @p rect
void drawBar(QPainter& painter, const QRectF& rect, double share, const QColor& track,
             const QColor& fill)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    const double radius = rect.height() / 2.0;
    painter.setBrush(track);
    painter.drawRoundedRect(rect, radius, radius);
    const double width = rect.width() * std::clamp(share, 0.0, 1.0);
    if (width > 0.5) {
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(rect.left(), rect.top(), std::max(width, rect.height()),
                                       rect.height()),
                                radius, radius);
    }
    painter.restore();
}

/// @brief The frame of the item chosen, and of the one with the keys
void drawChoice(QPainter& painter, const QStyleOptionViewItem& option, const QRectF& rect)
{
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool focused = option.state.testFlag(QStyle::State_HasFocus);
    if (!selected && !focused) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QColor fill = option.palette.color(QPalette::Highlight);
    fill.setAlphaF(selected ? 0.16F : 0.0F);
    QColor edge = option.palette.color(QPalette::Highlight);
    edge.setAlphaF(focused ? 1.0F : 0.5F);
    painter.setBrush(fill);
    painter.setPen(QPen(edge, 1.0));
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
    painter.restore();
}

}  // namespace

namespace awards_tab {

// =============================================================================
// The badges
// =============================================================================

/// @brief A badge: its ring, its name, its threshold and a bar of how far it is
class BadgeDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    /// @brief The size of a badge in @p font
    static QSize cellSize(const QFont& font)
    {
        const QFontMetrics metrics(font);
        const QFontMetrics small(smallFont(font));
        const int width = std::max(qRound(metrics.height() * 6.4), 84);
        const int height = 4 + circleSize(font) + 4 + 2 * metrics.height() + small.height() + 4 +
                           BAR_HEIGHT + 6;
        return {width, height};
    }

    /// @brief The size of the ring
    static int circleSize(const QFont& font)
    {
        return std::max(28, qRound(QFontMetrics(font).height() * 2.4));
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        return cellSize(option.font);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const StatsColors colors = StatsColors::current();
        const QRectF cell = QRectF(option.rect).adjusted(2, 1, -2, -1);
        drawChoice(*painter, option, cell);

        const bool won = index.data(WonRole).toBool();
        const int level = index.data(LevelRole).toInt();
        const auto kind = core::BadgeKind(index.data(KindRole).toInt());
        const QColor muted = mutedColor();
        const QColor color = badge_painter::colorOf(colors, level, won, muted);
        const int circle = circleSize(option.font);
        const QRectF ring(cell.center().x() - circle / 2.0, cell.top() + 4, circle, circle);
        badge_painter::drawBadge(*painter, ring, kind, color, won, won ? 2.0 : 1.5);

        painter->save();
        const QFontMetrics metrics(option.font);
        const QRectF text(cell.left() + 4, ring.bottom() + 4, cell.width() - 8,
                          2 * metrics.height());
        painter->setFont(option.font);
        painter->setPen(won ? option.palette.color(QPalette::Text) : muted);
        // Two lines at most, the second cut with "..."
        const QString name = index.data(NameRole).toString();
        QString shown = name;
        if (metrics.boundingRect(text.toRect(), Qt::AlignHCenter | Qt::TextWordWrap, name)
                .height() > text.height()) {
            shown = metrics.elidedText(name, Qt::ElideRight, int(text.width() * 2 - 8));
        }
        painter->drawText(text, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, shown);
        const QFont small = smallFont(option.font);
        const QFontMetrics smallMetrics(small);
        painter->setFont(small);
        painter->setPen(muted);
        const QRectF threshold(cell.left() + 2, text.bottom(), cell.width() - 4,
                               smallMetrics.height());
        painter->drawText(threshold, Qt::AlignHCenter | Qt::AlignTop,
                          smallMetrics.elidedText(index.data(SmallRole).toString(),
                                                  Qt::ElideRight, int(threshold.width())));
        painter->restore();

        const QRectF bar(cell.left() + 8, threshold.bottom() + 4, cell.width() - 16, BAR_HEIGHT);
        drawBar(*painter, bar, index.data(ShareRole).toDouble(), colors.grid,
                won ? color : muted);
    }

private:
    static constexpr int BAR_HEIGHT = 4;
};

/// @brief The badges of a part, in rows as the width allows; as high as its rows, it does
/// not scroll itself
class BadgeGrid : public QListWidget {
public:
    explicit BadgeGrid(QWidget* parent)
        : QListWidget(parent)
    {
        setViewMode(QListView::IconMode);
        setFlow(QListView::LeftToRight);
        setWrapping(true);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setUniformItemSizes(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setItemDelegate(new BadgeDelegate(this));
        setSpacing(2);
        viewport()->setAutoFillBackground(false);
        setAutoFillBackground(false);
        updateCell();
    }

    /// @brief The badges of @p kinds, as the tracker has them now
    void showBadges(const WritingAwardsTracker& tracker, core::BadgeGroup group)
    {
        const QModelIndex chosen = currentIndex();
        const int chosenRow = chosen.isValid() ? chosen.row() : -1;
        int row = 0;
        for (const core::BadgeKind kind : core::BADGE_KINDS) {
            if (core::badgeGroupOf(kind) != group) {
                continue;
            }
            const core::BadgeProgress progress = tracker.progressOf(kind);
            for (int level = 0; level < core::BADGE_LEVELS; ++level, ++row) {
                QListWidgetItem* item = this->item(row);
                if (item == nullptr) {
                    item = new QListWidgetItem(this);
                }
                const std::optional<core::EarnedBadge> won = tracker.earned(kind, level);
                const int times = group == core::BadgeGroup::Month ? tracker.monthsWon(kind, level)
                                                                    : 0;
                const QString name = badge_painter::name(kind);
                const QString threshold = badge_painter::thresholdText(progress, level);
                const QString tip = badge_painter::toolTip(progress, level, won, times);
                // The text is what the screen readers say and what typing looks for
                item->setText(name + QStringLiteral(" – ") + threshold);
                item->setData(KindRole, int(kind));
                item->setData(LevelRole, level);
                item->setData(WonRole, won.has_value());
                item->setData(ShareRole, won ? 1.0 : progress.share(level));
                item->setData(NameRole, name);
                item->setData(SmallRole, threshold);
                item->setToolTip(tip);
                item->setData(Qt::AccessibleDescriptionRole, tip);
            }
        }
        while (count() > row) {
            delete takeItem(count() - 1);
        }
        if (chosenRow >= 0 && chosenRow < count()) {
            setCurrentRow(chosenRow, QItemSelectionModel::NoUpdate);
        }
        fitHeight();
        viewport()->update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QListWidget::resizeEvent(event);
        fitHeight();
    }

    void changeEvent(QEvent* event) override
    {
        QListWidget::changeEvent(event);
        if (event->type() == QEvent::FontChange) {
            updateCell();
        }
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        QListWidget::focusOutEvent(event);
        viewport()->update();
    }

private:
    void updateCell()
    {
        setGridSize(BadgeDelegate::cellSize(font()) + QSize(2 * spacing(), 2 * spacing()));
        fitHeight();
    }

    /// @brief As high as its rows
    void fitHeight()
    {
        const QSize grid = gridSize();
        const int columns = std::max(1, viewport()->width() / std::max(1, grid.width()));
        const int rows = std::max(1, (count() + columns - 1) / columns);
        const int height = rows * grid.height() + 2 * frameWidth() + 4;
        if (minimumHeight() != height || maximumHeight() != height) {
            setFixedHeight(height);
        }
    }
};

// =============================================================================
// The challenges
// =============================================================================

/// @brief A challenge: its title and last day, a bar, how far it is and what is left
class ChallengeDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    static int rowHeight(const QFont& font)
    {
        const QFontMetrics metrics(font);
        const QFontMetrics small(smallFont(font));
        return 6 + metrics.height() + 4 + BAR_HEIGHT + 4 + small.height() + 6 + GAP;
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        return {200, rowHeight(option.font)};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const StatsColors colors = StatsColors::current();
        const QRectF box = QRectF(option.rect).adjusted(0, 0, 0, -GAP);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(option.palette.color(QPalette::Mid), 1.0));
        painter->setBrush(option.palette.color(QPalette::Base));
        painter->drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
        painter->restore();
        drawChoice(*painter, option, box);

        const QRectF inner = box.adjusted(8, 6, -8, -6);
        const QFontMetrics metrics(option.font);
        const QFont small = smallFont(option.font);
        const QFontMetrics smallMetrics(small);
        const QColor muted = mutedColor();
        painter->save();
        // The title, bold, and the last day on the right
        QFont bold = option.font;
        bold.setBold(true);
        painter->setFont(small);
        painter->setPen(muted);
        const QString period = index.data(SmallRole).toString();
        // A pixel or two over the advance: the eliding rounds the widths its own way
        const int periodWidth =
            std::min(smallMetrics.horizontalAdvance(period) + 2, int(inner.width() / 2));
        const QRectF top(inner.left(), inner.top(), inner.width(), metrics.height());
        painter->drawText(top, Qt::AlignRight | Qt::AlignVCenter,
                          smallMetrics.elidedText(period, Qt::ElideRight, periodWidth));
        painter->setFont(bold);
        painter->setPen(option.palette.color(QPalette::Text));
        const QFontMetrics boldMetrics(bold);
        painter->drawText(top.adjusted(0, 0, -periodWidth - 8, 0), Qt::AlignLeft | Qt::AlignVCenter,
                          boldMetrics.elidedText(index.data(NameRole).toString(), Qt::ElideRight,
                                                 int(top.width()) - periodWidth - 8));
        painter->restore();

        const bool done = index.data(WonRole).toBool();
        const bool over = index.data(OverRole).toBool();
        const QRectF bar(inner.left(), top.bottom() + 4, inner.width(), BAR_HEIGHT);
        drawBar(*painter, bar, index.data(ShareRole).toDouble(), colors.grid,
                done ? colors.done : over ? muted : colors.line);

        painter->save();
        painter->setFont(small);
        painter->setPen(muted);
        const QRectF bottom(inner.left(), bar.bottom() + 4, inner.width(), smallMetrics.height());
        const QString state = index.data(StateRole).toString();
        const int stateWidth =
            std::min(smallMetrics.horizontalAdvance(state) + 2, int(inner.width() / 2));
        painter->drawText(bottom, Qt::AlignRight | Qt::AlignVCenter,
                          smallMetrics.elidedText(state, Qt::ElideRight, stateWidth));
        painter->drawText(bottom.adjusted(0, 0, -stateWidth - 8, 0), Qt::AlignLeft | Qt::AlignVCenter,
                          smallMetrics.elidedText(index.data(ValueRole).toString(), Qt::ElideRight,
                                                  int(bottom.width()) - stateWidth - 8));
        painter->restore();
    }

private:
    static constexpr int BAR_HEIGHT = 7;
    static constexpr int GAP = 4;
};

/// @brief The writer's challenges, as high as they are; Delete removes the chosen one
class ChallengeList : public QListWidget {
public:
    explicit ChallengeList(QWidget* parent)
        : QListWidget(parent)
    {
        setSelectionMode(QAbstractItemView::SingleSelection);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setItemDelegate(new ChallengeDelegate(this));
        setUniformItemSizes(true);
        viewport()->setAutoFillBackground(false);
        setAutoFillBackground(false);
    }

    /// @brief The challenges of the tracker, the open ones first
    void showChallenges(const WritingAwardsTracker& tracker)
    {
        const int chosenId = currentItem() != nullptr ? currentItem()->data(KindRole).toInt() : 0;
        clear();
        QList<core::WritingChallenge> challenges = tracker.challenges();
        std::stable_sort(challenges.begin(), challenges.end(),
                         [](const core::WritingChallenge& a, const core::WritingChallenge& b) {
                             return !a.done.isValid() && b.done.isValid();
                         });
        for (const core::WritingChallenge& challenge : std::as_const(challenges)) {
            const core::ChallengeProgress progress = tracker.challengeProgress(challenge);
            auto* item = new QListWidgetItem(this);
            const QString title = badge_painter::challengeTitle(challenge);
            const QString period = badge_painter::challengePeriod(challenge);
            const QString value = badge_painter::challengeValue(challenge, progress.value);
            const QString state = badge_painter::challengeState(challenge, progress);
            item->setText(QStringLiteral("%1, %2: %3, %4").arg(title, period, value, state));
            item->setData(KindRole, challenge.id);
            item->setData(WonRole, progress.done);
            item->setData(OverRole, progress.over);
            item->setData(ShareRole, progress.done ? 1.0
                                                   : double(progress.value) /
                                                         std::max(1, challenge.target));
            item->setData(NameRole, title);
            item->setData(SmallRole, period);
            item->setData(ValueRole, value);
            item->setData(StateRole, state);
            item->setToolTip(item->text());
            if (challenge.id == chosenId) {
                setCurrentItem(item);
            }
        }
        const int rows = count();
        setFixedHeight(rows * ChallengeDelegate::rowHeight(font()) + 2 * frameWidth());
        setVisible(rows > 0);
    }

    /// @brief The number of the chosen challenge; 0: none
    int chosenId() const
    {
        return currentItem() != nullptr ? currentItem()->data(KindRole).toInt() : 0;
    }
};

// =============================================================================
// The key of the colors
// =============================================================================

/// @brief The colors of the levels and the look of a badge not won yet
class LevelKey : public QWidget {
public:
    explicit LevelKey(QWidget* parent)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override { return {200, rowHeight()}; }
    QSize minimumSizeHint() const override { return {80, rowHeight()}; }

    /// @brief The names, as the screen readers say them
    QStringList names() const
    {
        QStringList names;
        for (int level = 0; level < core::BADGE_LEVELS; ++level) {
            names << badge_painter::levelName(level);
        }
        //: The badges not won yet, in the key of the colors. In Polish: jeszcze nie
        names << WritingAwardsTab::tr("not yet");
        return names;
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        const int height = layoutEntries(nullptr);
        if (height != this->height()) {
            setFixedHeight(height);
        }
    }

    void changeEvent(QEvent* event) override
    {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::LanguageChange || event->type() == QEvent::FontChange) {
            setFixedHeight(layoutEntries(nullptr));
            update();
        }
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        layoutEntries(&painter);
    }

private:
    int rowHeight() const { return fontMetrics().height() + 4; }

    /// @brief Lay the entries out in rows; with @p painter draw them too
    /// @return The height they need
    int layoutEntries(QPainter* painter) const
    {
        const StatsColors colors = StatsColors::current();
        const QFontMetrics metrics(font());
        const int dot = std::max(9, metrics.height() * 3 / 4);
        const int gap = metrics.height();
        const QStringList entries = names();
        int x = 0;
        int y = 0;
        for (int i = 0; i < entries.size(); ++i) {
            const int width = dot + 4 + metrics.horizontalAdvance(entries[i]);
            if (x > 0 && x + width > this->width()) {
                x = 0;
                y += rowHeight();
            }
            if (painter != nullptr) {
                painter->setRenderHint(QPainter::Antialiasing, true);
                const QRectF circle(x + 0.75, y + (rowHeight() - dot) / 2.0 + 0.75, dot - 1.5,
                                    dot - 1.5);
                const bool won = i < core::BADGE_LEVELS;
                if (won) {
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(colors.badge(i));
                } else {
                    QPen pen(mutedColor(), 1.5);
                    pen.setStyle(Qt::DashLine);
                    pen.setDashPattern({2.0, 1.6});
                    painter->setPen(pen);
                    painter->setBrush(Qt::NoBrush);
                }
                painter->drawEllipse(circle);
                painter->setPen(mutedColor());
                painter->drawText(QRect(x + dot + 4, y, width - dot - 4, rowHeight()),
                                  Qt::AlignLeft | Qt::AlignVCenter, entries[i]);
            }
            x += width + gap;
        }
        return y + rowHeight();
    }
};

}  // namespace awards_tab

// =============================================================================
// The tab
// =============================================================================

namespace {

/// @brief A row of a group: its widgets from the left
QHBoxLayout* rowIn(QVBoxLayout* layout, const QList<QWidget*>& widgets)
{
    auto* row = new QHBoxLayout();
    for (QWidget* widget : widgets) {
        row->addWidget(widget);
    }
    row->addStretch(1);
    layout->addLayout(row);
    return row;
}

/// @brief A note: wrapped, muted
QLabel* noteIn(QWidget* parent, QList<QLabel*>& notes)
{
    auto* note = new QLabel(parent);
    note->setWordWrap(true);
    note->setTextFormat(Qt::PlainText);
    notes.append(note);
    return note;
}

}  // namespace

WritingAwardsTab::WritingAwardsTab(WritingAwardsTracker* tracker, WritingStatsModel* model,
                                   QWidget* parent)
    : QWidget(parent)
    , m_tracker(tracker)
    , m_model(model)
{
    setObjectName(QStringLiteral("writingAwardsTab"));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto* page = new QWidget();
    page->setBackgroundRole(QPalette::Window);
    page->setAutoFillBackground(true);
    scroll->setWidget(page);
    // The page fills the whole width of the window
    auto* pageRow = new QHBoxLayout(page);
    auto* content = new QWidget(page);
    pageRow->addWidget(content, 1);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);

    // The heading and the badges won
    m_title = new QLabel(content);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.2);
    m_title->setFont(titleFont);
    m_count = noteIn(content, m_notes);
    m_count->setObjectName(QStringLiteral("badgesWon"));
    auto* heading = new QHBoxLayout();
    heading->addWidget(m_title);
    heading->addStretch(1);
    heading->addWidget(m_count);
    m_count->setWordWrap(false);
    layout->addLayout(heading);

    // The challenges
    m_challengesGroup = new QGroupBox(content);
    auto* challenges = new QVBoxLayout(m_challengesGroup);
    m_addChallenge = new QPushButton(m_challengesGroup);
    m_addChallenge->setObjectName(QStringLiteral("addChallenge"));
    m_removeChallenge = new QPushButton(m_challengesGroup);
    m_removeChallenge->setObjectName(QStringLiteral("removeChallenge"));
    rowIn(challenges, {m_addChallenge, m_removeChallenge});
    m_challenges = new awards_tab::ChallengeList(m_challengesGroup);
    m_challenges->setObjectName(QStringLiteral("challenges"));
    m_challenges->installEventFilter(this);
    challenges->addWidget(m_challenges);
    m_challengesNote = noteIn(m_challengesGroup, m_notes);
    m_challengesNote->setObjectName(QStringLiteral("challengesNote"));
    challenges->addWidget(m_challengesNote);
    layout->addWidget(m_challengesGroup);

    // The key of the colors, and a note when the badges are off
    m_key = new awards_tab::LevelKey(content);
    m_key->setObjectName(QStringLiteral("badgeKey"));
    layout->addWidget(m_key);
    m_badgesNote = noteIn(content, m_notes);
    m_badgesNote->setObjectName(QStringLiteral("badgesNote"));
    layout->addWidget(m_badgesNote);

    // The badges: the writer's on the left, the book's and the month's on the right
    auto* columns = new QWidget(content);
    m_columns = new QBoxLayout(QBoxLayout::LeftToRight, columns);
    m_columns->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(columns);
    m_left = new QWidget(columns);
    auto* left = new QVBoxLayout(m_left);
    left->setContentsMargins(0, 0, 0, 0);
    m_right = new QWidget(columns);
    auto* right = new QVBoxLayout(m_right);
    right->setContentsMargins(0, 0, 0, 0);
    m_columns->addWidget(m_left, 1);
    m_columns->addWidget(m_right, 1);

    m_yoursGroup = new QGroupBox(m_left);
    auto* yours = new QVBoxLayout(m_yoursGroup);
    m_yours = new awards_tab::BadgeGrid(m_yoursGroup);
    m_yours->setObjectName(QStringLiteral("yourBadges"));
    yours->addWidget(m_yours);
    left->addWidget(m_yoursGroup);
    left->addStretch(1);

    m_bookGroup = new QGroupBox(m_right);
    auto* book = new QVBoxLayout(m_bookGroup);
    m_book = new awards_tab::BadgeGrid(m_bookGroup);
    m_book->setObjectName(QStringLiteral("bookBadges"));
    book->addWidget(m_book);
    m_bookNote = noteIn(m_bookGroup, m_notes);
    m_bookNote->setObjectName(QStringLiteral("bookBadgesNote"));
    book->addWidget(m_bookNote);
    m_startOver = new QPushButton(m_bookGroup);
    m_startOver->setObjectName(QStringLiteral("startBookOver"));
    rowIn(book, {m_startOver});
    right->addWidget(m_bookGroup);

    m_monthGroup = new QGroupBox(m_right);
    auto* month = new QVBoxLayout(m_monthGroup);
    m_month = new awards_tab::BadgeGrid(m_monthGroup);
    m_month->setObjectName(QStringLiteral("monthBadges"));
    month->addWidget(m_month);
    right->addWidget(m_monthGroup);
    right->addStretch(1);
    layout->addStretch(1);

    connect(m_addChallenge, &QPushButton::clicked, this, &WritingAwardsTab::addChallengeRequested);
    connect(m_removeChallenge, &QPushButton::clicked, this, &WritingAwardsTab::removeChallenge);
    connect(m_challenges, &QListWidget::currentItemChanged, this,
            &WritingAwardsTab::updateChallengeButtons);
    connect(m_startOver, &QPushButton::clicked, this, &WritingAwardsTab::startBookOver);

    // What changes in another place shows here at once
    connect(m_tracker, &WritingAwardsTracker::changed, this, &WritingAwardsTab::refresh);
    connect(m_model, &WritingStatsModel::goalsChanged, this, &WritingAwardsTab::refresh);
    // The colors of the theme, and the writer's colors of the statistics
    const auto showColors = [this]() {
        updateColors();
        update();
    };
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            showColors);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            showColors);

    // The keys go to Add Challenge when the tab comes to the front
    setFocusProxy(m_addChallenge);
    retranslate();
    updateColors();
    m_tracker->countIfNeeded();
    refresh();
}

void WritingAwardsTab::setBookTitleSource(std::function<QString()> title)
{
    m_bookTitle = std::move(title);
    refresh();
}

QString WritingAwardsTab::tabTitle()
{
    //: The tab in the middle of the window. In Polish: Wyzwania i odznaki
    return tr("Challenges and Badges");
}

void WritingAwardsTab::retranslate()
{
    m_title->setText(tabTitle());
    //: In Polish: Wyzwania
    m_challengesGroup->setTitle(tr("Challenges"));
    //: In Polish: &Dodaj wyzwanie...
    m_addChallenge->setText(tr("&Add Challenge..."));
    //: In Polish: &Usuń
    m_removeChallenge->setText(tr("&Remove"));
    //: In Polish: Wyzwania
    m_challenges->setAccessibleName(tr("Challenges"));
    //: The badges of the writer, of all the books. In Polish: Twoje – wszystkie książki
    m_yoursGroup->setTitle(tr("Yours – All the Books"));
    //: In Polish: Twoje odznaki
    m_yours->setAccessibleName(tr("Your badges"));
    //: In Polish: Odznaki książki
    m_book->setAccessibleName(tr("Badges of the book"));
    //: In Polish: Odznaki miesiąca
    m_month->setAccessibleName(tr("Badges of the month"));
    //: In Polish: &Zacznij od nowa...
    m_startOver->setText(tr("&Start Over..."));
    //: In Polish: Np. przed nową wersją książki; zdobyte odznaki zostają w jej historii.
    m_startOver->setToolTip(tr("E.g. before a new version of the book; the badges won stay in "
                               "its history."));
    m_key->setAccessibleName(m_key->names().join(QStringLiteral(", ")));
    m_key->setMinimumHeight(0);
    m_key->update();
}

void WritingAwardsTab::updateColors()
{
    const QString style = QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name());
    for (QLabel* note : m_notes) {
        note->setStyleSheet(style);
    }
}

void WritingAwardsTab::updateColumns()
{
    // Side by side when the writer's badges have four in a row
    const int cell = m_yours->gridSize().width();
    const int wanted = 2 * (4 * cell + 40) + m_columns->spacing();
    const auto direction = width() >= wanted ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom;
    if (m_columns->direction() != direction) {
        m_columns->setDirection(direction);
    }
}

void WritingAwardsTab::updateChallengeButtons()
{
    m_removeChallenge->setEnabled(m_challenges->chosenId() > 0);
}

void WritingAwardsTab::refresh()
{
    auto& settings = core::SettingsManager::getInstance();
    const QLocale locale;

    // The badges won, of all of them
    const auto won = m_tracker->shownBadges().size();
    const int all = int(core::BADGE_KINDS.size()) * core::BADGE_LEVELS;
    //: In Polish: Zdobyte odznaki: %1 z %2
    m_count->setText(tr("Badges won: %1 of %2").arg(won).arg(all));

    // The challenges
    m_challenges->showChallenges(*m_tracker);
    if (!settings.get<bool>(CHALLENGES_SETTING, false)) {
        //: In Polish: Wyzwania są wyłączone i nie są oznaczane jako ukończone. Dodanie wyzwania
        //: je włącza; włącza je też Ustawienia > Statystyki i cele > Powiadomienia.
        m_challengesNote->setText(
            tr("The challenges are off and are not marked as done. Adding a challenge turns them "
               "on; so does Settings > Statistics and Goals > Notifications."));
        m_challengesNote->show();
    } else if (m_tracker->challenges().isEmpty()) {
        //: In Polish: Nie ma jeszcze wyzwań. Wyzwanie to Twój własny cel na określony czas, np.
        //: 50 000 słów w miesiąc.
        m_challengesNote->setText(tr("No challenges yet. A challenge is a goal of your own for a "
                                     "time, e.g. 50,000 words in a month."));
        m_challengesNote->show();
    } else {
        m_challengesNote->hide();
    }
    updateChallengeButtons();

    // The badges, and a note when they are off
    if (!settings.get<bool>(BADGES_SETTING, true)) {
        //: In Polish: Odznaki są wyłączone: nowych się nie zdobywa. Włącza je Ustawienia >
        //: Statystyki i cele > Powiadomienia.
        m_badgesNote->setText(tr("The badges are off: no new ones are won. They are turned on in "
                                 "Settings > Statistics and Goals > Notifications."));
        m_badgesNote->show();
    } else {
        m_badgesNote->hide();
    }
    m_yours->showBadges(*m_tracker, core::BadgeGroup::Yours);
    m_month->showBadges(*m_tracker, core::BadgeGroup::Month);
    //: %1: the month and its year. In Polish: Miesiąc – %1
    m_monthGroup->setTitle(tr("Month – %1")
                               .arg(locale.standaloneMonthName(m_model->today().month()) +
                                    QLatin1Char(' ') + QString::number(m_model->today().year())));

    // The book's, with an open book
    const bool hasBook = m_model->hasBook();
    const QString title = hasBook && m_bookTitle ? m_bookTitle() : QString();
    m_bookGroup->setTitle(title.isEmpty()
                              //: In Polish: Książka
                              ? tr("Book")
                              //: %1: the title of the book. In Polish: Książka „%1”
                              : tr("Book “%1”").arg(title));
    m_book->setVisible(hasBook);
    m_startOver->setVisible(hasBook);
    m_startOver->setEnabled(hasBook && !m_model->isBookReadOnly());
    if (hasBook) {
        m_book->showBadges(*m_tracker, core::BadgeGroup::Book);
        const QDate since = m_tracker->bookBadges().since;
        if (since.isValid()) {
            //: In Polish: Liczone od %1. Wcześniej zdobyte odznaki zostają w historii książki.
            m_bookNote->setText(tr("Counted since %1. The badges won before stay in the history "
                                   "of the book.")
                                    .arg(locale.toString(since, QStringLiteral("d MMMM yyyy"))));
            m_bookNote->show();
        } else {
            m_bookNote->hide();
        }
    } else {
        //: In Polish: Odznaki książki pokazują się, gdy książka jest otwarta.
        m_bookNote->setText(tr("The badges of the book show when a book is open."));
        m_bookNote->show();
    }
    updateColumns();
}

void WritingAwardsTab::removeChallenge()
{
    const int id = m_challenges->chosenId();
    if (id <= 0) {
        return;
    }
    const auto& challenges = m_tracker->challenges();
    const auto found = std::find_if(challenges.cbegin(), challenges.cend(),
                                    [id](const core::WritingChallenge& c) { return c.id == id; });
    if (found == challenges.cend()) {
        return;
    }
    const int row = m_challenges->currentRow();
    if (!dialogs::MessageDialog::confirm(
            //: In Polish: Usuń wyzwanie
            this, tr("Remove Challenge"),
            //: %1: the challenge, %2: its last day. In Polish: Usunąć wyzwanie „%1” (%2)?
            tr("Remove the challenge “%1” (%2)?")
                .arg(badge_painter::challengeTitle(*found), badge_painter::challengePeriod(*found)),
            //: In Polish: &Usuń
            tr("&Remove"))) {
        return;
    }
    m_tracker->removeChallenge(id);
    if (m_challenges->count() > 0) {
        m_challenges->setCurrentRow(std::min(row, m_challenges->count() - 1));
        m_challenges->setFocus();
    } else {
        m_addChallenge->setFocus();
    }
}

void WritingAwardsTab::startBookOver()
{
    if (!m_model->hasBook()) {
        return;
    }
    const QString title = m_bookTitle ? m_bookTitle() : QString();
    if (!dialogs::MessageDialog::confirm(
            //: In Polish: Zacznij od nowa
            this, tr("Start Over"),
            //: %1: the title of the book. In Polish: Liczyć odznaki książki „%1” od nowa? Od
            //: dziś liczą się od zera, np. przed nową wersją książki. Zdobyte odznaki zostają w
            //: historii książki.
            tr("Count the badges of the book “%1” over? From today they count from nothing, e.g. "
               "before a new version of the book. The badges won stay in the history of the book.")
                .arg(title),
            //: In Polish: &Zacznij od nowa
            tr("&Start Over"))) {
        return;
    }
    m_tracker->startBookOver();
    core::Logger::getInstance().info("WritingAwardsTab: The badges of the book start over");
}

void WritingAwardsTab::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
        refresh();
        // The title of its tab, in the middle of the window
        for (QWidget* up = parentWidget(); up != nullptr; up = up->parentWidget()) {
            if (auto* tabs = qobject_cast<QTabWidget*>(up)) {
                if (const int index = tabs->indexOf(this); index >= 0) {
                    tabs->setTabText(index, tabTitle());
                    tabs->setTabToolTip(index, tabTitle());
                }
                break;
            }
        }
    }
}

void WritingAwardsTab::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateColumns();
}

bool WritingAwardsTab::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_challenges && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Delete && key->modifiers() == Qt::NoModifier) {
            removeChallenge();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace kalahari::gui
