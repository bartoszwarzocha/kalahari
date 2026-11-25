// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file info_bar.h
/// @brief A message across the text area: an icon, the message, a button and a close button

#ifndef KALAHARI_GUI_WIDGETS_INFO_BAR_H
#define KALAHARI_GUI_WIDGETS_INFO_BAR_H

#include <QFrame>
#include <QString>

#include <string>

class QLabel;
class QPushButton;
class QToolButton;

namespace kalahari::gui {

/// @brief A message across the text area, in the theme's colors of an info bar
///
/// +-------------------------------------------------------------------------------------+
/// | [i] The message, in more lines when the bar is narrow.  [Action] [Second] [*] [X]   |
/// +-------------------------------------------------------------------------------------+
///
/// The buttons do what the message suggests; without their texts the bar has none. The gear
/// (only when showSettingsButton() asks for it) reports that the settings of the messages
/// were asked for. The close button only reports that the bar was dismissed: whoever shows
/// the bar hides it.
class InfoBar : public QFrame {
    Q_OBJECT

public:
    /// @brief Create the bar, without a message and without a button
    explicit InfoBar(QWidget* parent = nullptr);

    /// @brief The message
    void setMessage(const QString& message);

    /// @brief The message
    [[nodiscard]] QString message() const;

    /// @brief The text of the button; empty: no button
    void setActionText(const QString& text);

    /// @brief The tooltip of the button
    void setActionToolTip(const QString& toolTip);

    /// @brief Whether the button can be clicked; greyed out when not
    void setActionEnabled(bool enabled);

    /// @brief The text of the second button, after the first one; empty: none
    void setSecondActionText(const QString& text);

    /// @brief The tooltip of the second button
    void setSecondActionToolTip(const QString& toolTip);

    /// @brief The icon before the message
    /// @param iconId ArtProvider id (by default "help.about", the info icon)
    void setIconId(const QString& iconId);

    /// @brief The icon before the message (ArtProvider id)
    [[nodiscard]] QString iconId() const { return m_iconId; }

    /// @brief Show the gear before the close button; a click emits settingsClicked()
    /// @param toolTip What the settings are ("Notification Settings"), also its accessible name
    void showSettingsButton(const QString& toolTip);

    /// @brief The gear (nullptr until showSettingsButton())
    [[nodiscard]] QToolButton* settingsButton() const { return m_settingsButton; }

    /// @brief Take the background and the border from colors of the theme's "editor" section
    ///        (the writer can change them in Settings) instead of the colors of all info bars
    /// @param backgroundKey Key of the background (ThemeManager::editorColor())
    /// @param borderKey Key of the border
    void setColorKeys(const std::string& backgroundKey, const std::string& borderKey);

    /// @brief A bar that stands out: the message and the button in colors of the theme's
    ///        "editor" section too, the button filled with its color and in bold
    /// @param textKey Key of the message's color
    /// @param actionKey Key of the button's color
    /// @param actionTextKey Key of the color of the button's text
    void setEmphasisColorKeys(const std::string& textKey, const std::string& actionKey,
                              const std::string& actionTextKey);

    /// @brief The button (shown when it has a text)
    [[nodiscard]] QPushButton* actionButton() const { return m_actionButton; }

signals:
    /// @brief The button was clicked
    void actionClicked();

    /// @brief The second button was clicked (setSecondActionText())
    void secondActionClicked();

    /// @brief The close button was clicked
    void dismissed();

    /// @brief The gear was clicked (showSettingsButton())
    void settingsClicked();

private:
    /// @brief The colors of the theme
    void updateStyling();

    /// @brief The icons of the theme
    void updateIcons();

    /// @brief The style sheet of the button: of the theme's buttons, or filled with the color
    ///        of setEmphasisColorKeys()
    [[nodiscard]] QString actionButtonStyle() const;

    QLabel* m_iconLabel = nullptr;
    QLabel* m_messageLabel = nullptr;
    QPushButton* m_actionButton = nullptr;
    QPushButton* m_secondButton = nullptr;
    QToolButton* m_closeButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QString m_iconId;
    std::string m_backgroundKey;  ///< Empty: the colors of all info bars
    std::string m_borderKey;
    std::string m_textKey;  ///< Empty: the message and the button as on all info bars
    std::string m_actionKey;
    std::string m_actionTextKey;
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_INFO_BAR_H
