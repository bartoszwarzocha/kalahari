/// @file info_bar.h
/// @brief A message across the text area: an icon, the message, a button and a close button

#pragma once

#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;
class QToolButton;

namespace kalahari {
namespace gui {

/// @brief A message across the text area, in the theme's colors of an info bar
///
/// +----------------------------------------------------------------------+
/// | [i] The message, in more lines when the bar is narrow.  [Action] [X] |
/// +----------------------------------------------------------------------+
///
/// The button does what the message suggests; without its text the bar has none. The close
/// button only reports that the bar was dismissed: whoever shows the bar hides it.
class InfoBar : public QFrame {
    Q_OBJECT

public:
    /// @brief Create the bar, without a message and without a button
    explicit InfoBar(QWidget* parent = nullptr);

    /// @brief The message
    void setMessage(const QString& message);

    /// @brief The message
    QString message() const;

    /// @brief The text of the button; empty: no button
    void setActionText(const QString& text);

    /// @brief The tooltip of the button
    void setActionToolTip(const QString& toolTip);

signals:
    /// @brief The button was clicked
    void actionClicked();

    /// @brief The close button was clicked
    void dismissed();

private:
    /// @brief The colors of the theme
    void updateStyling();

    /// @brief The icons of the theme
    void updateIcons();

    QLabel* m_iconLabel = nullptr;
    QLabel* m_messageLabel = nullptr;
    QPushButton* m_actionButton = nullptr;
    QToolButton* m_closeButton = nullptr;
};

} // namespace gui
} // namespace kalahari
