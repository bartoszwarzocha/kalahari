// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file wrapping_button.h
/// @brief A button in one line where it has room, which wraps its text where it has less

#pragma once

#include <QPushButton>
#include <QSize>
#include <QString>

class QEvent;
class QResizeEvent;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief A push button that wraps its text only where its line does not fit
///
/// A QPushButton is never narrower than its text, so a long text (or a longer translation)
/// keeps a side panel too wide for a small screen. This one is only as narrow as its longest
/// word: where its line does not fit, it shows its text in more lines and grows taller.
/// Change its text with setFullText(); QPushButton::setText() would not wrap.
class WrappingButton : public QPushButton {
public:
    /// @brief Constructor
    /// @param text The text, with an access key marked by "&" if it has one
    /// @param parent Parent widget
    explicit WrappingButton(const QString& text, QWidget* parent = nullptr);

    /// @brief Set the text, shown in one line or in more as the button's width allows
    void setFullText(const QString& text);

    /// @brief The text in one line, as it was set
    [[nodiscard]] QString fullText() const { return m_fullText; }

    /// @brief The text in one line
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief The longest word in a line
    [[nodiscard]] QSize minimumSizeHint() const override;

    /// @brief True: narrower, the button needs more lines
    [[nodiscard]] bool hasHeightForWidth() const override;

    /// @brief The height of the lines the text needs in @p width
    [[nodiscard]] int heightForWidth(int width) const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    /// @brief The size of the button showing @p shown (lines separated by "\n")
    [[nodiscard]] QSize sizeFor(const QString& shown) const;

    /// @brief The text in the lines that fit a button @p width wide
    [[nodiscard]] QString linesFor(int width) const;

    /// @brief Show the lines that fit the button's width
    void wrap();

    QString m_fullText;
};

} // namespace gui
} // namespace kalahari
