/// @file path_label.h
/// @brief A label with a file path, which wraps where its line ends, whatever the path's names

#pragma once

#include <QLabel>
#include <QSizeF>
#include <QString>

class QPaintEvent;
class QTextLayout;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief A label with a file path, which wraps after the separators of the path, and inside
///        a name of a folder or a file that is longer than the line
///
/// A word-wrapped QLabel breaks its lines only between words, so a long name in a path (a
/// folder named with a long number, a file name without spaces) would make it as wide as
/// that name and push a narrow panel to scroll sideways. This one is as narrow as it is made.
class PathLabel : public QLabel {
public:
    /// @brief Constructor
    /// @param path The path, with either separator (shown with the system's)
    /// @param parent Parent widget
    explicit PathLabel(const QString& path, QWidget* parent = nullptr);

    /// @brief The path in one line
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief One character in a line
    [[nodiscard]] QSize minimumSizeHint() const override;

    [[nodiscard]] int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief Lay the path out in lines @p width wide
    /// @return The width of the longest line and the height of all of them
    QSizeF layOut(QTextLayout& layout, qreal width) const;

    /// @brief The room around the text: the margins and the frame
    [[nodiscard]] QSize room() const;
};

} // namespace gui
} // namespace kalahari
