/// @file spelling_coordinator.h
/// @brief Spelling as you type: the dictionaries of the editors, their languages and the
///        commands

#pragma once

#include <QObject>
#include <QString>

class QStatusBar;
class QTabWidget;
class QTimer;

namespace kalahari::editor {
class BookEditor;
class SpellCheckService;
}

namespace kalahari::gui {

/// @brief Checks the spelling of the documents as they are written
///
/// Owns the dictionaries the editors check their text with and gives them to the editors of
/// the documents. The chapters of the book are checked in the language of the setting
/// editor.spellCheck.language; when the setting is empty, in the language of the book, and
/// without a book in the language of the program. A document outside the book, such as a
/// file opened from the disk, is checked in the language of the setting, else of the
/// program, as a new document is in Word and LibreOffice: with a dictionary of its own when
/// the book's language has another one, loaded when the first such document is open. A
/// language without a dictionary is not checked, and the status bar says so. The writer's
/// own words, the same in every language, are kept in the file user_dictionary.txt next to
/// the settings.
///
/// Tools > Check Spelling as You Type (Shift+F7) turns the checking on and off: the
/// setting editor.spellCheck.enabled, also in the Settings dialog. Tools > Next Spelling or
/// Grammar Issue (F7) selects the next misspelled word or grammar issue of the document in
/// front and opens its context menu, so it is put right from the keyboard.
class SpellingCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor: the dictionary starts loading in the background
    /// @param centralTabs The tabs of the documents, whose editors check with the dictionary
    /// @param statusBar Where short messages about the spelling are shown (may be null)
    /// @param parent Parent object
    SpellingCoordinator(QTabWidget* centralTabs, QStatusBar* statusBar,
                        QObject* parent = nullptr);

    /// @brief Destructor: the commands have no callbacks any more
    ~SpellingCoordinator() override;

    SpellingCoordinator(const SpellingCoordinator&) = delete;
    SpellingCoordinator& operator=(const SpellingCoordinator&) = delete;

    /// @brief Give the commands Check Spelling as You Type (with its check mark) and Next
    ///        Spelling or Grammar Issue their callbacks
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief Select the next misspelled word or grammar issue of the document in front and
    ///        open its context menu (the command Next Spelling or Grammar Issue); the status
    ///        bar says when there is none
    ///
    /// When the grammar of the paragraphs further on is not known yet, the status bar says
    /// it is being checked and the issue is gone to once it is found.
    /// @return false when there is none (true also while the grammar is being checked)
    bool goToNextIssue();

    /// @brief The dictionary of the chapters of the book, and of every document when their
    ///        languages have the same dictionary; it keeps the writer's own words
    editor::SpellCheckService* service() const { return m_service; }

    /// @brief The language the chapters of the book are checked in: the setting, else the
    ///        book's language, else the program's
    static QString wantedLanguage();

    /// @brief The language a document outside the book is checked in: the setting, else the
    ///        program's
    static QString defaultLanguage();

    /// @brief The file the writer's own words are kept in, next to the settings
    static QString userDictionaryFile();

public slots:
    /// @brief Check with the dictionaries of the languages wanted now, or turn the checking
    ///        off as the setting says (after the settings or the book's language change)
    void updateDictionary();

private:
    /// @brief Give the editors of the documents their dictionaries, and load the one of the
    ///        documents outside the book when the first of them needs it
    void updateEditors();

    /// @brief Check with a dictionary, or without one when its language has none: then the
    ///        status bar says so, once for the language
    /// @param service The dictionary of the editors
    /// @param language The language wanted
    /// @param dictionary Its dictionary (empty: none)
    /// @param missingLanguage The language without a dictionary the writer was told of
    void useDictionary(editor::SpellCheckService& service, const QString& language,
                       const QString& dictionary, QString& missingLanguage);

    /// @brief The editor of the document in front, or nullptr
    editor::BookEditor* currentEditor() const;

    /// @brief Turn the checking on or off (the command)
    void toggle();

    /// @brief Show a short message in the status bar
    void showMessage(const QString& message, int timeout);

    QTabWidget* m_centralTabs;
    QStatusBar* m_statusBar;
    editor::SpellCheckService* m_service;       ///< The dictionary of the chapters
    editor::SpellCheckService* m_otherService;  ///< The one of the documents outside the book
    QTimer* m_editorsTimer;  ///< Gives the editors their dictionaries after a tab is added
    int m_settingsListener{0};

    /// The language of the documents outside the book when its dictionary is not the
    /// book's (empty: the book's dictionary checks them), and its dictionary (empty: none)
    QString m_otherLanguage;
    QString m_otherDictionary;

    /// The language of the chapters without a dictionary the writer was told of, and the
    /// same for the documents outside the book
    QString m_missingLanguage;
    QString m_otherMissingLanguage;
};

}  // namespace kalahari::gui
