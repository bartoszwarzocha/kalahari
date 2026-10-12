/// @file wrapping_label.h
/// @brief A label in one line where it has room, which wraps its text where it has less

#pragma once

#include <QLabel>
#include <QString>

class QWidget;

namespace kalahari {
namespace gui {

/// @brief A label that wraps its text only where its line does not fit
///
/// A word-wrapped QLabel likes a shape of a few lines even with room for one: its size hint
/// is a guess at a pleasant shape. This one likes its text in its own lines, so layouts give
/// it that where they can, and it wraps on a narrow panel (a small screen) instead of being
/// cut off. For plain text; a rich text keeps the hint of QLabel.
class WrappingLabel : public QLabel {
public:
    /// @brief Constructor
    /// @param text The text (plain)
    /// @param parent Parent widget
    explicit WrappingLabel(const QString& text = QString(), QWidget* parent = nullptr);

    /// @brief The text in its own lines, unwrapped
    [[nodiscard]] QSize sizeHint() const override;
};

} // namespace gui
} // namespace kalahari
