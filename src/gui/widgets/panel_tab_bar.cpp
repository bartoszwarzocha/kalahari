/// @file panel_tab_bar.cpp
/// @brief The tabs of panels docked one over another

#include "kalahari/gui/widgets/panel_tab_bar.h"

#include <QApplication>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QToolTip>

#include <algorithm>
#include <numeric>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

/// @brief Between the tabs and the left and right of the bar
constexpr int MARGIN_X = 4;

/// @brief Between the tabs and the top and bottom of the bar
constexpr int MARGIN_Y = 3;

/// @brief Between the tabs, and between the rows
constexpr int SPACING = 2;

/// @brief The frame of a tab
constexpr int BORDER = 1;

/// @brief Beside the name of a tab
constexpr int NAME_PADDING = 8;

/// @brief Beside an icon alone
constexpr int ICON_PADDING = 6;

/// @brief Beside the name or the icon in a row too narrow for the usual room
constexpr int NARROW_PADDING = 3;

/// @brief Above and below the name or the icon
constexpr int PADDING_Y = 4;

/// @brief Between the icon and the name
constexpr int ICON_GAP = 5;

/// @brief The line of the tab in front, on its side toward the panel
constexpr int MARK = 2;

/// @brief The corners of a tab
constexpr qreal RADIUS = 3.0;

/// @brief The least of a name a tab shows: two letters and the ellipsis, as the panels' titles
QString leastOf(const QString& name) {
    return name.size() <= 3 ? name : name.left(2) + QChar(0x2026);
}

/// @brief Widen the tabs from @p first to @p last by @p extra pixels together
void spread(QList<int>& widths, int first, int last, int extra) {
    const int tabs = last - first + 1;
    if (extra <= 0 || tabs <= 0) {
        return;
    }
    for (int i = first; i <= last; ++i) {
        widths[i] += extra / tabs + ((i - first) < extra % tabs ? 1 : 0);
    }
}

/// @brief Narrow the tabs of one row to @p room pixels together: the widest give way first,
///        none below its narrowest
void squeeze(QList<int>& widths, const QList<int>& narrowest, int room) {
    QList<int> sorted = widths;
    std::sort(sorted.begin(), sorted.end());
    // The widest width every tab can keep: the narrow tabs keep theirs, the rest share
    int left = static_cast<int>(sorted.size());
    int rest = room;
    int cap = 0;
    for (const int width : sorted) {
        if (width * left <= rest) {
            rest -= width;
            --left;
            continue;
        }
        cap = rest / left;
        break;
    }
    if (left == 0) {
        return;  // all fit
    }
    int extra = rest - cap * left;  // the pixels the division left over
    for (int i = 0; i < widths.size(); ++i) {
        if (widths[i] > cap) {
            widths[i] = cap + (extra > 0 ? 1 : 0);
            extra = std::max(0, extra - 1);
        }
        widths[i] = std::max(widths[i], narrowest[i]);
    }
}

} // anonymous namespace

PanelTabBar::PanelTabBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("panelTabBar"));
    setAccessibleName(tr("Panel tabs"));
    // A tab bar's focus: by the Tab key, not by a click, as Qt's tabs
    setFocusPolicy(Qt::TabFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void PanelTabBar::setTabs(const QList<Tab>& tabs, int current) {
    // A drag goes on over the same tabs in another order (a tab moved, another panel shown)
    m_pressed = -1;
    for (int i = 0; i < tabs.size(); ++i) {
        if (m_pressedKey != nullptr && tabs.at(i).key == m_pressedKey) {
            m_pressed = i;
        }
    }
    if (m_pressed < 0) {
        cancelDrag();
    }
    m_tabs = tabs;
    m_current = tabs.isEmpty() ? -1 : std::clamp(current, 0, static_cast<int>(tabs.size()) - 1);
    m_hover = -1;
    m_dropGap = -1;
    m_dropLine = QRect();
    relayout();
    updateGeometry();
}

void PanelTabBar::setLook(Look look) {
    if (look == m_look) {
        return;
    }
    m_look = look;
    relayout();
    updateGeometry();
}

void PanelTabBar::setRows(Rows rows) {
    if (rows == m_rows) {
        return;
    }
    m_rows = rows;
    relayout();
    updateGeometry();
}

void PanelTabBar::setSide(Side side) {
    if (side == m_side) {
        return;
    }
    m_side = side;
    update();
}

void PanelTabBar::setIconSize(int size) {
    if (size == m_iconSize || size <= 0) {
        return;
    }
    m_iconSize = size;
    relayout();
    updateGeometry();
}

QRect PanelTabBar::tabRect(int index) const {
    return index >= 0 && index < m_placed.size() ? m_placed.at(index).rect : QRect();
}

QString PanelTabBar::shownName(int index) const {
    return index >= 0 && index < m_placed.size() ? m_placed.at(index).shown : QString();
}

QString PanelTabBar::toolTipOf(int index) const {
    if (index < 0 || index >= m_tabs.size()) {
        return {};
    }
    const Tab& tab = m_tabs.at(index);
    QString text = tab.toolTip.isEmpty() ? tab.name : tab.toolTip;
    // A name seen whole says it all, unless the tooltip says more (the panel's keys)
    if (showsNames() && shownName(index) == tab.name && text == tab.name) {
        return {};
    }
    return text;
}

int PanelTabBar::rowCount() const {
    return m_placed.isEmpty() ? 0 : m_placed.constLast().row + 1;
}

int PanelTabBar::tabAt(const QPoint& pos) const {
    for (int i = 0; i < m_placed.size(); ++i) {
        if (m_placed.at(i).rect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

int PanelTabBar::naturalWidth(int index) const {
    const QString& name = m_tabs.at(index).name;
    int width = 2 * BORDER;
    if (showsIcons()) {
        width += m_iconSize;
    }
    if (showsNames()) {
        width += 2 * NAME_PADDING + fontMetrics().horizontalAdvance(name);
        if (showsIcons()) {
            width += ICON_GAP;
        }
    } else {
        width += 2 * ICON_PADDING;
    }
    return width;
}

int PanelTabBar::narrowestWidth(int index) const {
    int width = 2 * BORDER + 2 * NARROW_PADDING;
    if (showsIcons()) {
        width += m_iconSize;  // a name gives way to the icon
    } else {
        width += fontMetrics().horizontalAdvance(leastOf(m_tabs.at(index).name));
    }
    return width;
}

int PanelTabBar::tabHeight() const {
    const int content = std::max(showsIcons() ? m_iconSize : 0,
                                 showsNames() ? fontMetrics().height() : 0);
    return 2 * BORDER + 2 * PADDING_Y + content;
}

QList<PanelTabBar::Placed> PanelTabBar::arrange(int width) const {
    const int tabs = count();
    QList<Placed> placed(tabs);
    if (tabs == 0) {
        return placed;
    }

    const int room = std::max(0, width - 2 * MARGIN_X);
    QList<int> widths(tabs);
    QList<int> narrowest(tabs);
    for (int i = 0; i < tabs; ++i) {
        widths[i] = naturalWidth(i);
        narrowest[i] = narrowestWidth(i);
    }

    // The rows, each from its first tab to its last
    QList<std::pair<int, int>> rows;
    if (m_rows == Rows::Wrap) {
        int first = 0;
        int used = 0;
        for (int i = 0; i < tabs; ++i) {
            // A tab wider than the bar alone shortens its name
            widths[i] = std::min(widths[i], room);
            if (i > first && used + SPACING + widths[i] > room) {
                rows.append({first, i - 1});
                first = i;
                used = 0;
            }
            used += (i > first ? SPACING : 0) + widths[i];
        }
        rows.append({first, tabs - 1});
    } else {
        rows.append({0, tabs - 1});
        const int spacing = SPACING * (tabs - 1);
        const int total = std::accumulate(widths.cbegin(), widths.cend(), 0) + spacing;
        if (total > room) {
            squeeze(widths, narrowest, room - spacing);
        }
    }

    // Names fill their rows; icons alone keep their size
    if (showsNames()) {
        for (const auto& [first, last] : rows) {
            int used = SPACING * (last - first);
            for (int i = first; i <= last; ++i) {
                used += widths[i];
            }
            spread(widths, first, last, room - used);
        }
    }

    const int height = tabHeight();
    const QFontMetrics metrics = fontMetrics();
    for (int row = 0; row < rows.size(); ++row) {
        const auto [first, last] = rows.at(row);
        int x = MARGIN_X;
        const int y = MARGIN_Y + row * (height + SPACING);
        for (int i = first; i <= last; ++i) {
            Placed& tab = placed[i];
            tab.rect = QRect(x, y, widths[i], height);
            tab.row = row;
            if (showsNames()) {
                const QString& name = m_tabs.at(i).name;
                int nameRoom = widths[i] - 2 * BORDER - 2 * NARROW_PADDING;
                if (showsIcons()) {
                    nameRoom -= m_iconSize + ICON_GAP;
                }
                // Next to an icon, a name without room for its least shows not at all
                if (showsIcons() && nameRoom < metrics.horizontalAdvance(leastOf(name))) {
                    tab.shown.clear();
                } else {
                    tab.shown = metrics.elidedText(name, Qt::ElideRight, std::max(0, nameRoom));
                }
            }
            x += widths[i] + SPACING;
        }
    }
    return placed;
}

int PanelTabBar::oneRowWidth() const {
    int width = 2 * MARGIN_X;
    for (int i = 0; i < count(); ++i) {
        width += naturalWidth(i) + (i > 0 ? SPACING : 0);
    }
    return width;
}

int PanelTabBar::placedWidth() const {
    if (testAttribute(Qt::WA_Resized)) {
        return width();
    }
    const QWidget* parent = parentWidget();
    if (parent != nullptr && parent->testAttribute(Qt::WA_Resized)) {
        return parent->width();
    }
    return oneRowWidth();
}

int PanelTabBar::heightForWidth(int width) const {
    if (m_tabs.isEmpty()) {
        return 0;
    }
    const QList<Placed> placed = arrange(width);
    const int rows = placed.constLast().row + 1;
    return 2 * MARGIN_Y + rows * tabHeight() + (rows - 1) * SPACING;
}

QSize PanelTabBar::sizeHint() const {
    return minimumSizeHint();
}

QSize PanelTabBar::minimumSizeHint() const {
    int width = 0;
    for (int i = 0; i < count(); ++i) {
        if (m_rows == Rows::Single) {
            width += narrowestWidth(i) + (i > 0 ? SPACING : 0);
        } else {
            width = std::max(width, narrowestWidth(i));
        }
    }
    return {2 * MARGIN_X + width, heightForWidth(placedWidth())};
}

void PanelTabBar::relayout() {
    m_placed = arrange(placedWidth());
    update();
}

void PanelTabBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const int rows = rowCount();
    relayout();
    // Another number of rows: the panel's title bar gives the tabs another height
    if (rowCount() != rows || heightForWidth(width()) != height()) {
        updateGeometry();
    }
}

void PanelTabBar::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
        relayout();
        updateGeometry();
        break;
    case QEvent::PaletteChange:
    case QEvent::EnabledChange:
        update();
        break;
    default:
        break;
    }
}

bool PanelTabBar::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        const auto* help = static_cast<QHelpEvent*>(event);
        const int index = m_dragging ? -1 : tabAt(help->pos());
        const QString text = toolTipOf(index);
        if (text.isEmpty()) {
            QToolTip::hideText();
            event->ignore();
        } else {
            QToolTip::showText(help->globalPos(), text, this, tabRect(index));
        }
        return true;
    }
    return QWidget::event(event);
}

void PanelTabBar::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    const QPalette& colors = palette();

    // The line between the tabs and the panel
    painter.setPen(colors.color(QPalette::Mid));
    const int lineY = m_side == Side::Above ? height() - 1 : 0;
    painter.drawLine(0, lineY, width() - 1, lineY);

    painter.setRenderHint(QPainter::Antialiasing, true);
    const QFontMetrics metrics = fontMetrics();
    for (int i = 0; i < m_placed.size(); ++i) {
        const QRect rect = m_placed.at(i).rect;
        const bool current = i == m_current;
        const bool hover = i == m_hover && !m_dragging;
        const bool dragged = m_dragging && i == m_pressed;

        QColor fill = colors.color(QPalette::Button);
        if (current) {
            fill = colors.color(QPalette::Light);
        } else if (hover || dragged) {
            fill = colors.color(QPalette::Midlight);
        }
        painter.setPen(colors.color(current || dragged ? QPalette::Highlight : QPalette::Mid));
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), RADIUS, RADIUS);

        if (current) {
            // The line on the side toward the panel joins the tab to it
            const int markY = m_side == Side::Above ? rect.bottom() - BORDER - MARK + 1
                                                    : rect.top() + BORDER;
            painter.fillRect(QRect(rect.left() + BORDER + 1, markY,
                                   rect.width() - 2 * BORDER - 2, MARK),
                             colors.color(QPalette::Highlight));
        }

        // The icon and the name, together in the middle of the tab
        const QString& shown = m_placed.at(i).shown;
        const bool named = !shown.isEmpty();
        int contentWidth = showsIcons() ? m_iconSize : 0;
        if (named) {
            contentWidth += metrics.horizontalAdvance(shown) + (showsIcons() ? ICON_GAP : 0);
        }
        int x = rect.left() + std::max(BORDER + NARROW_PADDING, (rect.width() - contentWidth) / 2);
        if (showsIcons()) {
            const QRect iconRect(x, rect.top() + (rect.height() - m_iconSize) / 2, m_iconSize,
                                 m_iconSize);
            m_tabs.at(i).icon.paint(&painter, iconRect, Qt::AlignCenter,
                                    isEnabled() ? QIcon::Normal : QIcon::Disabled);
            x += m_iconSize + ICON_GAP;
        }
        if (named) {
            painter.setPen(colors.color(isEnabled() ? QPalette::Active : QPalette::Disabled,
                                        QPalette::ButtonText));
            const QRect nameRect(x, rect.top(), rect.right() - BORDER - NARROW_PADDING - x + 1,
                                 rect.height());
            painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                             shown);
        }

        if (current && hasFocus()) {
            QStyleOptionFocusRect focus;
            focus.initFrom(this);
            focus.rect = rect.adjusted(BORDER + 2, BORDER + 2, -BORDER - 2, -BORDER - 2);
            focus.backgroundColor = fill;
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
        }
    }

    // Where a dragged tab goes
    if (m_dragging && m_dropLine.isValid()) {
        painter.fillRect(m_dropLine, colors.color(QPalette::Highlight));
    }
}

void PanelTabBar::setHover(int index) {
    if (index == m_hover) {
        return;
    }
    m_hover = index;
    update();
}

void PanelTabBar::cancelDrag() {
    m_pressed = -1;
    m_pressedKey = nullptr;
    m_dragging = false;
    m_dropGap = -1;
    m_dropLine = QRect();
}

void PanelTabBar::updateDrop(const QPoint& pos) {
    m_dropGap = -1;
    m_dropLine = QRect();
    if (m_placed.isEmpty() || !rect().contains(pos)) {
        update();
        return;
    }

    // The row at the height of the mouse, the nearest one above or below the tabs
    const int height = tabHeight();
    const int row = std::clamp((pos.y() - MARGIN_Y + SPACING / 2) / (height + SPACING), 0,
                               rowCount() - 1);
    int gap = -1;
    int last = -1;
    for (int i = 0; i < m_placed.size(); ++i) {
        if (m_placed.at(i).row != row) {
            continue;
        }
        if (pos.x() < m_placed.at(i).rect.center().x()) {
            gap = i;
            break;
        }
        last = i;
    }

    // Before tab gap, or after the last tab of the row
    QRect line;
    if (gap >= 0) {
        const QRect tab = m_placed.at(gap).rect;
        line = QRect(tab.left() - SPACING / 2 - 1, tab.top(), MARK, tab.height());
    } else {
        const QRect tab = m_placed.at(last).rect;
        line = QRect(tab.right() + SPACING / 2, tab.top(), MARK, tab.height());
        gap = last + 1;
    }

    // Dropped next to itself the tab stays where it is
    if (gap != m_pressed && gap != m_pressed + 1) {
        m_dropGap = gap;
        m_dropLine = line;
    }
    update();
}

void PanelTabBar::mousePressEvent(QMouseEvent* event) {
    // The tabs are no handle to float or move the panel by
    event->accept();
    if (event->button() != Qt::LeftButton) {
        return;
    }
    cancelDrag();
    m_pressPos = event->position().toPoint();
    m_pressed = tabAt(m_pressPos);
    m_pressedKey = m_pressed >= 0 ? m_tabs.at(m_pressed).key : nullptr;
    update();
}

void PanelTabBar::mouseMoveEvent(QMouseEvent* event) {
    event->accept();
    const QPoint pos = event->position().toPoint();
    if (m_pressed < 0 || !(event->buttons() & Qt::LeftButton)) {
        setHover(tabAt(pos));
        return;
    }
    if (!m_dragging && count() > 1 &&
        (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
        m_dragging = true;
        QToolTip::hideText();
    }
    if (m_dragging) {
        updateDrop(pos);
    }
}

void PanelTabBar::mouseReleaseEvent(QMouseEvent* event) {
    event->accept();
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const int pressed = m_pressed;
    const bool dragged = m_dragging;
    const int gap = m_dropGap;
    cancelDrag();
    setHover(tabAt(event->position().toPoint()));
    update();
    if (pressed < 0) {
        return;
    }
    if (dragged) {
        if (gap >= 0) {
            emit tabMoveRequested(pressed, gap > pressed ? gap - 1 : gap);
        }
        return;
    }
    if (tabAt(event->position().toPoint()) == pressed && pressed != m_current) {
        emit tabChosen(pressed);
    }
}

void PanelTabBar::mouseDoubleClickEvent(QMouseEvent* event) {
    // Not the panel's title bar: a double click does not float the panel
    event->accept();
}

void PanelTabBar::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    setHover(-1);
}

void PanelTabBar::keyPressEvent(QKeyEvent* event) {
    int next = -1;
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Up:
        next = m_current - 1;
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
        next = m_current + 1;
        break;
    case Qt::Key_Home:
        next = 0;
        break;
    case Qt::Key_End:
        next = count() - 1;
        break;
    case Qt::Key_Escape:
        if (m_dragging) {
            cancelDrag();
            update();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
    if (next >= 0 && next < count() && next != m_current) {
        emit tabChosen(next);
    }
}

void PanelTabBar::focusInEvent(QFocusEvent* event) {
    QWidget::focusInEvent(event);
    update();
}

void PanelTabBar::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    update();
}

} // namespace gui
} // namespace kalahari
