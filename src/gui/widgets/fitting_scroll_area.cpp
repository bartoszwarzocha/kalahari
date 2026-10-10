/// @file fitting_scroll_area.cpp
/// @brief A scroll area as large as its content, which scrolls only on a small screen

#include "kalahari/gui/widgets/fitting_scroll_area.h"

#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QLayout>
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

bool FittingScrollArea::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == widget() || watched == m_content) && event->type() == QEvent::LayoutRequest) {
        updateGeometry();
    }
    return QScrollArea::eventFilter(watched, event);
}

QSize FittingScrollArea::borders() const
{
    return QSize(2 * frameWidth(), 2 * frameWidth());
}

} // namespace gui
} // namespace kalahari
