/// @file auto_correct_coordinator.h
/// @brief Auto-correct as you type turned on and off as a whole: the command, its check mark
///        and its tooltip

#pragma once

#include <QObject>
#include <QString>

class QStatusBar;

namespace kalahari::gui {

/// @brief Turns auto-correct on and off as a whole: Tools > Auto-correct as You Type, also a
///        button on the Tools toolbar
///
/// The command is the setting editor.autoCorrect.enabled, also on the page Editor >
/// Auto-correct of the Settings dialog. Off, the editors replace nothing, and each
/// replacement keeps its own choice for when auto-correct is on again. The command is
/// checked while auto-correct is on; its tooltip names the replacements chosen and where they
/// are chosen, as the settings are now.
class AutoCorrectCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The setting that turns auto-correct on and off as a whole
    static constexpr const char* ENABLED_KEY = "editor.autoCorrect.enabled";

    /// @brief Constructor
    /// @param statusBar Where the command says what it did (may be null)
    /// @param parent Parent object
    explicit AutoCorrectCoordinator(QStatusBar* statusBar, QObject* parent = nullptr);

    /// @brief Destructor: the command has no callback any more
    ~AutoCorrectCoordinator() override;

    AutoCorrectCoordinator(const AutoCorrectCoordinator&) = delete;
    AutoCorrectCoordinator& operator=(const AutoCorrectCoordinator&) = delete;

    /// @brief Give the command its callback, its check mark and its tooltip
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief Turn auto-correct on or off as a whole (the command); remembered
    void toggle();

    /// @brief The command's tooltip as the settings are now, a line each: its name, the
    ///        replacements chosen (while auto-correct is off: those it makes when on) and
    ///        where they are chosen
    static QString toolTip();

private slots:
    /// @brief Give the command the tooltip of the settings as they are now
    void updateToolTip();

private:
    QStatusBar* m_statusBar;
    int m_settingsListener = 0;  ///< The subscription to the settings
};

}  // namespace kalahari::gui
