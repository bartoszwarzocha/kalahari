/// @file grammar_coordinator.h
/// @brief Grammar as you type: the LanguageTool server of the editors, the language and the
///        command

#pragma once

#include <QObject>
#include <QString>

class QStatusBar;
class QTabWidget;

namespace kalahari::editor {
class GrammarCheckService;
}

namespace kalahari::gui {

/// @brief Checks the grammar of the documents as they are written
///
/// Owns the checking the editors send their paragraphs to and gives it to the editors of
/// the documents. The text is checked by the LanguageTool server whose address is the
/// setting editor.grammarCheck.serverUrl, usually LanguageTool running on the same
/// computer; without one nothing is checked or sent. The language is the one the spelling
/// is checked in (SpellingCoordinator::wantedLanguage()). When the server does not check
/// the text, the status bar says so.
///
/// Tools > Check Grammar as You Type turns the checking on and off (the setting
/// editor.grammarCheck.enabled, also in the Settings dialog); it is greyed out while no
/// server is set.
class GrammarCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param centralTabs The tabs of the documents, whose editors are checked
    /// @param statusBar Where short messages about the grammar are shown (may be null)
    /// @param parent Parent object
    GrammarCoordinator(QTabWidget* centralTabs, QStatusBar* statusBar,
                       QObject* parent = nullptr);

    /// @brief Destructor: the command has no callbacks any more
    ~GrammarCoordinator() override;

    GrammarCoordinator(const GrammarCoordinator&) = delete;
    GrammarCoordinator& operator=(const GrammarCoordinator&) = delete;

    /// @brief Give the command Check Grammar as You Type its callbacks and check mark
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief The checking the editors send their paragraphs to
    editor::GrammarCheckService* service() const { return m_service; }

public slots:
    /// @brief Check on the server, and in the language, the settings and the book give now
    void updateChecking();

private:
    /// @brief Give the checking to the editors of the documents
    void attachEditors();

    /// @brief Turn the checking on or off (the command)
    void toggle();

    /// @brief Show a short message in the status bar
    void showMessage(const QString& message, int timeout);

    QTabWidget* m_centralTabs;
    QStatusBar* m_statusBar;
    editor::GrammarCheckService* m_service;
    int m_settingsListener{0};
};

}  // namespace kalahari::gui
