/// @file fitting_scroll_area.h
/// @brief A scroll area as large as its content, which scrolls only on a small screen

#pragma once

#include <QScrollArea>
#include <QSize>

class QEvent;
class QObject;
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

protected:
    /// @brief A change of the content (a list shown, a longer text) changes the hints
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The frame around the content, both sides together
    QSize borders() const;

    QWidget* m_content;  ///< The content, inside the area's widget
    int m_minimumLines;  ///< Height of the area in a lowered window, in lines
};

} // namespace gui
} // namespace kalahari
