/// @file fitting_scroll_area.h
/// @brief A scroll area as large as its content, which scrolls only on a small screen

#pragma once

#include <QMetaObject>
#include <QScrollArea>
#include <QSize>

class QEvent;
class QObject;
class QResizeEvent;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief A scroll area as large as its content: the content scrolls only when the window is
/// lower than the content can be (a small screen, 1366×768 at 150%)
///
/// Its hints are those of the content, not the few lines of QScrollArea: as wide and as high
/// as the content needs, at least a few lines high, and wide enough for the content beside
/// the vertical scroll bar. In a lower window the content first gets lower itself, as far as
/// its minimum (lists and texts that scroll on their own give up their room), and only then
/// it scrolls; wrapped texts keep all their lines.
///
/// The area has no frame and no horizontal scroll bar. Tab goes through it to the fields of
/// the content, and a field it reaches scrolls into view.
class FittingScrollArea : public QScrollArea {
    Q_OBJECT

public:
    /// @brief Create the area with @p content in it
    /// @param content The content, with its layout; the area takes it
    /// @param parent Parent widget
    explicit FittingScrollArea(QWidget* content, QWidget* parent = nullptr);

    ~FittingScrollArea() override;

    /// @brief The size the content needs
    QSize sizeHint() const override;

    /// @brief The content's minimum width beside the scroll bar, and the height of
    /// setMinimumLines() (all of a lower content, measured as wide as the screen)
    QSize minimumSizeHint() const override;

    /// @brief The height of the area in a lowered window, in lines of text: 6 by default (a
    /// dialog), fewer in a panel, so that more panels fit one under another on a small screen
    /// @param lines Lines of text, at least 1
    void setMinimumLines(int lines);

    /// @brief The height the content needs in the area @p width wide: wrapped texts take the
    /// lines they need at that width
    int contentHeightForWidth(int width) const;

    /// @brief Scroll to @p widget of the content and keep it in view while the window
    /// settles: after a later change of the area's height or the content's (a window the
    /// system or the writer makes lower, a scroll bar that comes or goes) the area scrolls to
    /// it again, until the writer scrolls the area or works in a widget of the content
    /// @param widget The widget; nullptr: none is kept in view
    /// @param first The part of @p widget in view first when @p widget is higher than the
    /// area (the sentence of a result, above its details); nullptr: the middle of @p widget
    void keepInView(QWidget* widget, QWidget* first = nullptr);

protected:
    /// @brief A change of the content (a list shown, a longer text) changes the hints
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// @brief A higher area takes away the scroll bar the content no longer needs
    void resizeEvent(QResizeEvent* event) override;

private:
    /// @brief Take away the vertical scroll bar when the content fits the area without it
    ///
    /// QScrollArea measures the content at the width beside a scroll bar it shows, where
    /// wrapped texts take more lines: a bar that came while the window was still lower than
    /// its new content (numbers or details shown) would stay in the window grown for it.
    void dropNeedlessScrollBar();

    /// @brief The frame around the content, both sides together
    QSize borders() const;

    /// @brief Scroll to the widget kept in view, when it is shown
    void scrollToKept();

    /// @brief Keep no widget in view
    void stopKeeping();

    QWidget* m_content;  ///< The content, inside the area's widget
    int m_minimumLines;  ///< Height of the area in a lowered window, in lines
    bool m_droppingBar = false;               ///< Taking away the scroll bar just now
    QWidget* m_kept = nullptr;                ///< The widget kept in view; nullptr: none
    QWidget* m_keptFirst = nullptr;           ///< Its part in view first, in a lower area
    QMetaObject::Connection m_keptGone;       ///< Keeps no widget once the widget is deleted
    QMetaObject::Connection m_keptFirstGone;  ///< The same for its part
};

} // namespace gui
} // namespace kalahari
