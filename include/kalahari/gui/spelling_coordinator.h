/// @file spelling_coordinator.h
/// @brief Spelling as you type: the dictionary of the editors, its language and its command

#pragma once

#include <QObject>
#include <QString>

class QStatusBar;
class QTabWidget;

namespace kalahari::editor {
class SpellCheckService;
}

namespace kalahari::gui {

/// @brief Checks the spelling of the documents as they are written
///
/// Owns the dictionary every editor checks its text with and gives it to the editors of
/// the documents. Its language is the setting editor.spellCheck.language; when the setting
/// is empty, the language of the book, and without a book the language of the program. A
/// language without a dictionary is not checked, and the status bar says so. The writer's
/// own words are kept in the file user_dictionary.txt next to the settings.
///
/// Tools > Check Spelling as You Type (Shift+F7) turns the checking on and off: the
/// setting editor.spellCheck.enabled, also in the Settings dialog.
class SpellingCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor: the dictionary starts loading in the background
    /// @param centralTabs The tabs of the documents, whose editors check with the dictionary
    /// @param statusBar Where short messages about the spelling are shown (may be null)
    /// @param parent Parent object
    SpellingCoordinator(QTabWidget* centralTabs, QStatusBar* statusBar,
                        QObject* parent = nullptr);

    /// @brief Destructor
    ~SpellingCoordinator() override;

    SpellingCoordinator(const SpellingCoordinator&) = delete;
    SpellingCoordinator& operator=(const SpellingCoordinator&) = delete;

    /// @brief Give the command Check Spelling as You Type its callback and check mark
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief The dictionary of the editors
    editor::SpellCheckService* service() const { return m_service; }

    /// @brief The language the text is checked in: the setting, else the book's language,
    ///        else the program's
    static QString wantedLanguage();

    /// @brief The file the writer's own words are kept in, next to the settings
    static QString userDictionaryFile();

public slots:
    /// @brief Check with the dictionary of the language wanted now, or turn the checking
    ///        off as the setting says (after the settings or the book's language change)
    void updateDictionary();

private:
    /// @brief Give the dictionary to the editors of the documents
    void attachEditors();

    /// @brief Turn the checking on or off (the command)
    void toggle();

    /// @brief Show a short message in the status bar
    void showMessage(const QString& message, int timeout);

    QTabWidget* m_centralTabs;
    QStatusBar* m_statusBar;
    editor::SpellCheckService* m_service;
    int m_settingsListener{0};
    QString m_missingLanguage;  ///< The language without a dictionary the writer was told of
};

}  // namespace kalahari::gui
