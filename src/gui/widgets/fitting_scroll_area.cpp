/// @file fitting_scroll_area.cpp
/// @brief A scroll area as large as its content, which scrolls only on a small screen

#include "kalahari/gui/widgets/fitting_scroll_area.h"

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QLayout>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

constexpr int DEFAULT_MINIMUM_LINES = 6;  ///< Height of the area in a lowered dialog, in lines

/// @brief The widget of the area, around the content
///
/// QScrollArea makes its widget as high as the widget's height for the width of the area:
/// here the content's minimum at that width, with all the lines of wrapped texts, so that
/// the content gets lower before it scrolls. It asks the content itself: the frame's own
/// layout learns of a change of the content only from the event loop.
class ContentFrame : public QWidget {
public:
    explicit ContentFrame(QWidget* content)
        : m_content(content)
    {
    }

    int heightForWidth(int width) const override
    {
        int height = -1;
        if (const QLayout* layout = m_content->layout()) {
            height = layout->totalMinimumHeightForWidth(width);
        } else if (m_content->hasHeightForWidth()) {
            height = m_content->heightForWidth(width);
        }
        return std::max(height, m_content->minimumSizeHint().height());
    }

private:
    QWidget* m_content;
};

} // anonymous namespace

FittingScrollArea::FittingScrollArea(QWidget* content, QWidget* parent)
    : QScrollArea(parent)
    , m_content(content)
    , m_minimumLines(DEFAULT_MINIMUM_LINES)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::NoFocus);

    auto* frame = new ContentFrame(content);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(content);
    setWidgetResizable(true);
    setWidget(frame);
    frame->installEventFilter(this);
    content->installEventFilter(this);

    // A widget kept in view stays there when the content or the area gets higher or lower:
    // the range changes before the content has its new place, so the area scrolls after it.
    // The area does not leave a widget of the content the writer works in.
    connect(
        verticalScrollBar(), &QScrollBar::rangeChanged, this,
        [this]() {
            const QWidget* focus = QApplication::focusWidget();
            if (focus && m_content->isAncestorOf(focus)) {
                stopKeeping();
            }
            scrollToKept();
        },
        Qt::QueuedConnection);
    // The writer's own scrolling ends it
    connect(verticalScrollBar(), &QScrollBar::actionTriggered, this,
            &FittingScrollArea::stopKeeping);
}

FittingScrollArea::~FittingScrollArea()
{
    // The widgets of the content are deleted after this part of the area, which must not
    // hear that they are gone
    stopKeeping();
}

QSize FittingScrollArea::sizeHint() const
{
    // The content's own hints: the frame's layout learns of a change only from the event loop
    return m_content->sizeHint() + borders();
}

QSize FittingScrollArea::minimumSizeHint() const
{
    // A content shorter than the lines: its height in the widest area the screen allows,
    // where wrapped texts take the fewest lines; the hint at its own, narrower width would
    // leave a gap under them in a wider window
    const int lines = fontMetrics().lineSpacing() * m_minimumLines + borders().height();
    const int widest = screen() ? screen()->availableGeometry().width()
                                : m_content->sizeHint().width() + borders().width();
    return {m_content->minimumSizeHint().width() + verticalScrollBar()->sizeHint().width() +
                borders().width(),
            qMin(contentHeightForWidth(widest), lines)};
}

void FittingScrollArea::setMinimumLines(int lines)
{
    m_minimumLines = qMax(1, lines);
    updateGeometry();
}

int FittingScrollArea::contentHeightForWidth(int width) const
{
    if (!m_content->hasHeightForWidth()) {
        return sizeHint().height();
    }
    return m_content->heightForWidth(width - borders().width()) + borders().height();
}

void FittingScrollArea::keepInView(QWidget* widget, QWidget* first)
{
    stopKeeping();
    if (!widget) {
        return;
    }
    m_kept = widget;
    m_keptFirst = first;
    // A widget deleted meanwhile is kept no longer
    m_keptGone = connect(widget, &QObject::destroyed, this, &FittingScrollArea::stopKeeping);
    if (first) {
        m_keptFirstGone =
            connect(first, &QObject::destroyed, this, &FittingScrollArea::stopKeeping);
    }
    scrollToKept();
}

void FittingScrollArea::scrollToKept()
{
    if (!m_kept || !m_kept->isVisibleTo(this)) {
        return;
    }
    ensureWidgetVisible(m_kept, 0, 0);
    if (m_keptFirst && m_keptFirst->isVisibleTo(this)) {
        ensureWidgetVisible(m_keptFirst, 0, 0);
    }
}

void FittingScrollArea::stopKeeping()
{
    disconnect(m_keptGone);
    disconnect(m_keptFirstGone);
    m_kept = nullptr;
    m_keptFirst = nullptr;
}

bool FittingScrollArea::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == widget() || watched == m_content) && event->type() == QEvent::LayoutRequest) {
        updateGeometry();
    }
    const bool handled = QScrollArea::eventFilter(watched, event);
    if (watched == widget() &&
        (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize)) {
        dropNeedlessScrollBar();
    }
    return handled;
}

void FittingScrollArea::resizeEvent(QResizeEvent* event)
{
    QScrollArea::resizeEvent(event);
    dropNeedlessScrollBar();
}

void FittingScrollArea::dropNeedlessScrollBar()
{
    if (m_droppingBar || widget() == nullptr || !verticalScrollBar()->isVisible() ||
        verticalScrollBarPolicy() != Qt::ScrollBarAsNeeded) {
        return;
    }
    const QSize room = maximumViewportSize();
    if (widget()->heightForWidth(room.width()) > room.height()) {
        return;  // it scrolls also without the bar beside it
    }
    // Without the bar the area measures the content at its whole width, and it fits
    m_droppingBar = true;
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_droppingBar = false;
}

QSize FittingScrollArea::borders() const
{
    return QSize(2 * frameWidth(), 2 * frameWidth());
}

} // namespace gui
} // namespace kalahari
