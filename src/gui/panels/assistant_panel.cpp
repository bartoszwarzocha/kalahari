// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file assistant_panel.cpp
/// @brief The Assistant panel: the notifications of the session

#include "kalahari/gui/panels/assistant_panel.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/theme.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/notifications.h"
#include "kalahari/gui/widgets/wrapping_label.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <memory>

namespace kalahari::gui {

namespace {

constexpr int TIME_ROLE = Qt::UserRole;
constexpr int TITLE_ROLE = Qt::UserRole + 1;
constexpr int TEXT_ROLE = Qt::UserRole + 2;
constexpr int WARNING_ROLE = Qt::UserRole + 3;

/// The bubbles: their room around them and inside, the round corners and the icon
constexpr int BUBBLE_MARGIN = 4;
constexpr int BUBBLE_PADDING = 7;
constexpr int BUBBLE_GAP = 3;  ///< Between the time and the title, and the text
constexpr double BUBBLE_RADIUS = 6.0;
constexpr int ICON_SIZE = 16;
constexpr int MINIMUM_TEXT_WIDTH = 40;
/// The outline on whole pixels: half a pixel inside the bubble's rect
constexpr double HALF_PIXEL = 0.5;
/// The outline of the chosen bubble while the keys are in the list
constexpr double FOCUS_OUTLINE = 2.0;
/// The room around the panel's list and buttons, and between them
constexpr int PANEL_MARGIN = 6;

/// @brief Lay out a text in lines of a width
/// @return Its height
double layOut(QTextLayout& layout, double width)
{
    double height = 0.0;
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(width);
        line.setPosition(QPointF(0.0, height));
        height += line.height();
    }
    layout.endLayout();
    return height;
}

/// @brief Paints a notification in a bubble of the theme's colors of the notifications: the
///        icon of its kind, its time (muted) and its title (bold), and its text under them
class BubbleDelegate : public QStyledItemDelegate {
public:
    explicit BubbleDelegate(QListWidget* view)
        : QStyledItemDelegate(view), m_view(view) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const Bubble bubble = bubbleOf(option, index);
        const QRectF frame = QRectF(option.rect).adjusted(BUBBLE_MARGIN, BUBBLE_MARGIN / 2.0,
                                                          -BUBBLE_MARGIN, -BUBBLE_MARGIN / 2.0);
        const bool selected = (option.state & QStyle::State_Selected) != 0;
        const auto& themes = core::ThemeManager::getInstance();
        const core::Theme& theme = themes.getCurrentTheme();
        const QColor background = themes.editorColor(
            NotificationCoordinator::BACKGROUND_COLOR_KEY, theme.colors.infoBarBackground);
        const QColor border = themes.editorColor(NotificationCoordinator::BORDER_COLOR_KEY,
                                                 theme.colors.infoBarBorder);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QPainterPath shape;
        shape.addRoundedRect(frame.adjusted(HALF_PIXEL, HALF_PIXEL, -HALF_PIXEL, -HALF_PIXEL),
                             BUBBLE_RADIUS, BUBBLE_RADIUS);
        painter->fillPath(shape, background);
        // The chosen one framed in the color of a selection, thicker while the keys are here
        painter->setPen(QPen(selected ? option.palette.color(QPalette::Highlight) : border,
                             selected && m_view->hasFocus() ? FOCUS_OUTLINE : 1.0));
        painter->drawPath(shape);

        const QString iconId = index.data(WARNING_ROLE).toBool() ? QStringLiteral("common.warning")
                                                                 : QStringLiteral("help.about");
        painter->drawPixmap(bubble.iconAt, core::ArtProvider::getInstance()
                                               .getThemedIcon(iconId)
                                               .pixmap(ICON_SIZE, ICON_SIZE));
        painter->setPen(theme.palette.windowText);
        bubble.heading->draw(painter, bubble.headingAt);
        bubble.text->draw(painter, bubble.textAt);
        painter->restore();
    }

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override
    {
        QStyleOptionViewItem atWidth = option;
        atWidth.rect = QRect(0, 0, m_view->viewport()->width(), 0);
        return {atWidth.rect.width(), bubbleOf(atWidth, index).height};
    }

private:
    /// @brief A bubble laid out in a rect
    struct Bubble {
        QPoint iconAt;
        std::shared_ptr<QTextLayout> heading;
        QPointF headingAt;
        std::shared_ptr<QTextLayout> text;
        QPointF textAt;
        int height = 0;
    };

    [[nodiscard]] static Bubble bubbleOf(const QStyleOptionViewItem& option,
                                         const QModelIndex& index)
    {
        Bubble bubble;
        const QRect frame = option.rect.adjusted(BUBBLE_MARGIN, BUBBLE_MARGIN / 2, -BUBBLE_MARGIN,
                                                 -BUBBLE_MARGIN / 2);
        const int left = frame.left() + BUBBLE_PADDING;
        const int headingLeft = left + ICON_SIZE + BUBBLE_GAP * 2;
        const QFont font = option.font;
        QFont bold = font;
        bold.setBold(true);
        const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();

        // The time, muted, and the title, bold, beside the icon
        const QString time = index.data(TIME_ROLE).toString();
        const QString title = index.data(TITLE_ROLE).toString();
        bubble.heading = std::make_shared<QTextLayout>(time + QStringLiteral("  ") + title, font);
        QTextCharFormat timeFormat;
        timeFormat.setForeground(theme.palette.placeholderText);
        QTextCharFormat titleFormat;
        titleFormat.setFont(bold);
        bubble.heading->setFormats(
            {{0, static_cast<int>(time.size()), timeFormat},
             {static_cast<int>(time.size()) + 2, static_cast<int>(title.size()), titleFormat}});
        double y = frame.top() + BUBBLE_PADDING;
        bubble.iconAt = QPoint(left, static_cast<int>(y));
        bubble.headingAt = QPointF(headingLeft, y);
        const double headingHeight = layOut(
            *bubble.heading,
            std::max(frame.right() - BUBBLE_PADDING - headingLeft, MINIMUM_TEXT_WIDTH));
        y += std::max(headingHeight, static_cast<double>(ICON_SIZE)) + BUBBLE_GAP;

        // The text under them, as wide as the bubble
        bubble.text = std::make_shared<QTextLayout>(index.data(TEXT_ROLE).toString(), font);
        bubble.textAt = QPointF(left, y);
        y += layOut(*bubble.text, std::max(frame.right() - BUBBLE_PADDING - left,
                                           MINIMUM_TEXT_WIDTH));
        y += BUBBLE_PADDING;
        bubble.height = static_cast<int>(std::ceil(y)) - frame.top() + BUBBLE_MARGIN;
        return bubble;
    }

    QListWidget* m_view;
};

} // anonymous namespace

AssistantPanel::AssistantPanel(QWidget* parent)
    : QWidget(parent)
    , m_list(new QListWidget(this))
{
    setObjectName(QStringLiteral("assistantPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN);
    layout->setSpacing(PANEL_MARGIN);

    // An empty panel says what it is for; it wraps on a narrow panel
    //: The Assistant panel without notifications. In Polish: Tu pojawiają się powiadomienia
    //: programu z tej sesji: osiągnięte cele, zbliżające się terminy, wiadomości dodatków i
    //: inne. Gdzie pokazują się najpierw – wybiera się w Ustawieniach > Powiadomienia.
    m_emptyLabel = new WrappingLabel(
        tr("The program's notifications of this session show here: goals reached, deadlines "
           "coming, messages of plugins and more. Settings > Notifications chooses where they "
           "show first."),
        this);
    m_emptyLabel->setObjectName(QStringLiteral("assistantEmpty"));
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_emptyLabel->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_emptyLabel);

    m_list->setObjectName(QStringLiteral("assistantNotifications"));
    //: In Polish: Powiadomienia
    m_list->setAccessibleName(tr("Notifications"));
    m_list->setItemDelegate(new BubbleDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setUniformItemSizes(false);
    m_list->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_list, 1);

    // Ctrl+C in the list copies the chosen one (the key written out, the same on every system)
    auto* copyAction = new QAction(m_list);
    copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
    copyAction->setShortcutContext(Qt::WidgetShortcut);
    m_list->addAction(copyAction);
    connect(copyAction, &QAction::triggered, this, &AssistantPanel::copyNotification);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    // No mnemonics: Alt and a letter in a panel are the menus' (Ctrl+C copies in the list)
    //: Copies the chosen notification of the Assistant panel. In Polish: Kopiuj
    m_copyButton = new QPushButton(tr("Copy"), this);
    m_copyButton->setObjectName(QStringLiteral("assistantCopy"));
    //: In Polish: Kopiuj wybrane powiadomienie, a bez wyboru – ostatnie
    m_copyButton->setToolTip(tr("Copy the chosen notification, or the latest without a choice"));
    //: Takes all the notifications away from the Assistant panel. In Polish: Wyczyść
    m_clearButton = new QPushButton(tr("Clear"), this);
    m_clearButton->setObjectName(QStringLiteral("assistantClear"));
    //: In Polish: Usuń wszystkie powiadomienia z panelu
    m_clearButton->setToolTip(tr("Take all the notifications away from the panel"));
    buttons->addWidget(m_copyButton);
    buttons->addWidget(m_clearButton);
    layout->addLayout(buttons);
    connect(m_copyButton, &QPushButton::clicked, this, &AssistantPanel::copyNotification);
    connect(m_clearButton, &QPushButton::clicked, this, &AssistantPanel::clearNotifications);

    // The bubbles in the colors of the theme in use, and of those the writer changes
    const auto repaint = [this]() { m_list->viewport()->update(); };
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this, repaint);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            repaint);
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            repaint);

    updateState();
    core::Logger::getInstance().debug("AssistantPanel initialized");
}

void AssistantPanel::addNotification(const Notification& notification, const QDateTime& time)
{
    if (notification.text.isEmpty()) {
        return;
    }
    auto* item = new QListWidgetItem();
    const QString shownTime = QLocale().toString(time.time(), QLocale::ShortFormat);
    item->setData(TIME_ROLE, shownTime);
    item->setData(TITLE_ROLE, notification.title);
    item->setData(TEXT_ROLE, notification.text);
    item->setData(WARNING_ROLE, notification.kind == Notification::Kind::Warning);
    // What screen readers say
    item->setData(Qt::AccessibleTextRole,
                  QStringList{shownTime, notification.title, notification.text}.join(
                      QStringLiteral(", ")));
    item->setToolTip(time.toString(QLocale().dateTimeFormat(QLocale::ShortFormat)));
    m_list->addItem(item);
    while (m_list->count() > MAX_NOTIFICATIONS) {
        delete m_list->takeItem(0);
    }
    updateState();
    showLatest();
}

int AssistantPanel::notificationCount() const
{
    return m_list->count();
}

QString AssistantPanel::notificationText(int row) const
{
    const QListWidgetItem* item = m_list->item(row);
    if (item == nullptr) {
        return {};
    }
    QStringList parts{item->data(TITLE_ROLE).toString(), item->data(TEXT_ROLE).toString()};
    parts.removeAll(QString());
    return parts.join(QLatin1Char('\n'));
}

void AssistantPanel::showLatest()
{
    if (m_list->count() > 0) {
        m_list->scrollToBottom();
    }
}

void AssistantPanel::copyNotification()
{
    if (m_list->count() == 0) {
        return;
    }
    const int row = m_list->currentRow() >= 0 && m_list->currentItem()->isSelected()
                        ? m_list->currentRow()
                        : m_list->count() - 1;
    QApplication::clipboard()->setText(notificationText(row));
}

void AssistantPanel::clearNotifications()
{
    m_list->clear();
    updateState();
}

void AssistantPanel::updateState()
{
    const bool empty = m_list->count() == 0;
    m_emptyLabel->setVisible(empty);
    m_list->setVisible(!empty);
    m_copyButton->setEnabled(!empty);
    m_clearButton->setEnabled(!empty);
}

} // namespace kalahari::gui
