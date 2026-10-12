/// @file height_for_width_scroll_area.h
/// @brief A scroll area as high as its content needs at its width, which scrolls only in a
///        panel too low for it

#pragma once

#include <QScrollArea>
#include <QSize>

class QEvent;
class QObject;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief A scroll area as high as its content at the area's width: rows that wrap on a
///        narrow panel take the lines they need, and the content scrolls only where the
///        panel is lower than that (a small screen, 1366x768 at 150%)
///
/// The layouts that wrap (FlowLayout, SegmentedLayout) are one line high at their minimum:
/// in a panel too low for all their lines they would cover each other. In this area they
/// keep their lines and scroll instead.
///
/// The area has no frame and no horizontal scroll bar. Tab goes through it to the fields of
/// the content, and a field it reaches scrolls into view.
class HeightForWidthScrollArea : public QScrollArea {
public:
    /// @brief Create the area with @p content in it
    /// @param content The content, with its layout; the area takes it
    /// @param minimumLines Its height in a panel too low for it, in lines of text
    /// @param parent Parent widget
    HeightForWidthScrollArea(QWidget* content, int minimumLines, QWidget* parent = nullptr);

    [[nodiscard]] bool hasHeightForWidth() const override;

    /// @brief The height the content needs at @p width
    [[nodiscard]] int heightForWidth(int width) const override;

    /// @brief As wide as the content likes, as high as it needs at the area's width
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief The content's minimum width beside the vertical scroll bar, and the lines of
    ///        the constructor (all of a lower content)
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    /// @brief A change of the content (a longer text, a row that wraps) changes the hints
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    int m_minimumLines;
};

} // namespace gui
} // namespace kalahari
