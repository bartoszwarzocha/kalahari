/// @file spell_check_service.h
/// @brief Spell checking service using Hunspell (OpenSpec #00042 Phase 6.4-6.9)
///
/// SpellCheckService provides:
/// - Real-time spell checking using Hunspell library
/// - Multi-language support (Polish, English, etc.)
/// - Background checking with debounce for performance
/// - User dictionary with persistence
/// - Session-only ignore list
///
/// The service uses an observer pattern to track document changes and
/// emits signals when spell errors are found for UI integration.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QTimer>
#include <QList>
#include <QPair>

// Forward declarations
namespace kalahari::editor {
class KmlDocument;
class IDocumentObserver;
}

namespace kalahari::editor {

/// @brief Extended information about a spelling error with suggestions
///
/// Contains position, length, the misspelled word, and suggested corrections.
struct SpellErrorInfo {
    int startPos{0};          ///< Start position in paragraph
    int length{0};            ///< Length of misspelled word
    QString word;             ///< The misspelled word
    QStringList suggestions;  ///< Suggested corrections (max 5)

    SpellErrorInfo() = default;
    SpellErrorInfo(int start, int len, const QString& w)
        : startPos(start), length(len), word(w) {}

    bool operator==(const SpellErrorInfo& other) const {
        return startPos == other.startPos && length == other.length && word == other.word;
    }
};

/// @brief Spell checking service using Hunspell
///
/// Provides asynchronous spell checking for KML documents with support for
/// multiple languages and user dictionaries. Integrates with the document
/// observer pattern for real-time checking as the user types.
///
/// Usage:
/// @code
/// auto service = new SpellCheckService(this);
/// service->loadDictionary("en_US");
/// service->setDocument(document);
///
/// // Connect to spell check results
/// connect(service, &SpellCheckService::paragraphChecked,
///         this, &MyEditor::onSpellErrors);
/// @endcode
class SpellCheckService : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a spell check service
    /// @param parent Parent QObject for ownership
    explicit SpellCheckService(QObject* parent = nullptr);

    /// @brief Destructor - cleans up Hunspell instance
    ~SpellCheckService() override;

    // Non-copyable
    SpellCheckService(const SpellCheckService&) = delete;
    SpellCheckService& operator=(const SpellCheckService&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /// @brief Set the document to check
    /// @param document KML document to monitor (nullptr to disconnect)
    /// @note Previous document is automatically disconnected
    void setDocument(KmlDocument* document);

    /// @brief Load dictionary for language (e.g., "pl_PL", "en_US")
    /// @param language Language code (e.g., "pl_PL", "en_US", "en_GB")
    /// @return true if dictionary loaded successfully
    bool loadDictionary(const QString& language);

    /// @brief Get list of available dictionaries
    /// @return List of language codes (e.g., "pl_PL", "en_US")
    QStringList availableDictionaries() const;

    /// @brief Get currently loaded language
    /// @return Language code or empty string if no dictionary loaded
    QString currentLanguage() const;

    // =========================================================================
    // Checking
    // =========================================================================

    /// @brief Check a single word
    /// @param word The word to check
    /// @return true if word is spelled correctly
    bool isCorrect(const QString& word) const;

    /// @brief Get suggestions for a misspelled word
    /// @param word The misspelled word
    /// @param maxSuggestions Maximum number of suggestions to return
    /// @return List of suggested corrections
    QStringList suggestions(const QString& word, int maxSuggestions = 5) const;

    /// @brief Get all errors in a paragraph
    /// @param text The paragraph text to check
    /// @return List of spelling errors with suggestions
    QList<SpellErrorInfo> checkParagraph(const QString& text) const;

    /// @brief Check entire document asynchronously
    /// @note Emits paragraphChecked for each paragraph and documentCheckComplete when done
    void checkDocumentAsync();

    /// @brief Enable or disable spell checking
    /// @param enabled true to enable, false to disable
    void setEnabled(bool enabled);

    /// @brief Check if spell checking is enabled
    /// @return true if enabled
    bool isEnabled() const;

    /// @brief Check if a dictionary is loaded
    /// @return true if dictionary is ready
    bool isDictionaryLoaded() const;

    // =========================================================================
    // User Dictionary
    // =========================================================================

    /// @brief Add word to user dictionary (persisted across sessions)
    /// @param word The word to add
    void addToUserDictionary(const QString& word);

    /// @brief Add word to ignore list (session only, not persisted)
    /// @param word The word to ignore
    void ignoreWord(const QString& word);

    /// @brief Remove word from user dictionary
    /// @param word The word to remove
    void removeFromUserDictionary(const QString& word);

    /// @brief Check if word is in user dictionary
    /// @param word The word to check
    /// @return true if word is in user dictionary
    bool isInUserDictionary(const QString& word) const;

    /// @brief Get all words in user dictionary
    /// @return List of user dictionary words
    QStringList userDictionaryWords() const;

signals:
    /// @brief Emitted when checking is complete for a paragraph
    /// @param paragraphIndex The paragraph index
    /// @param errors List of spelling errors found
    void paragraphChecked(int paragraphIndex, const QList<SpellErrorInfo>& errors);

    /// @brief Emitted when full document check is complete
    void documentCheckComplete();

    /// @brief Emitted when dictionary is loaded
    /// @param language The language code that was loaded
    void dictionaryLoaded(const QString& language);

    /// @brief Emitted when dictionary loading fails
    /// @param error Error message
    void dictionaryError(const QString& error);

private slots:
    /// @brief Handle debounce timer timeout
    void onDebounceTimeout();

private:
    // Document observer implementation
    class DocumentObserver;
    friend class DocumentObserver;

    // =========================================================================
    // Hunspell Management
    // =========================================================================

    /// @brief Initialize Hunspell with dictionary files
    /// @param affPath Path to .aff file
    /// @param dicPath Path to .dic file
    /// @return true if successful
    bool initHunspell(const QString& affPath, const QString& dicPath);

    /// @brief Clean up Hunspell instance
    void cleanupHunspell();

    // =========================================================================
    // Dictionary Paths
    // =========================================================================

    /// @brief Find dictionary path for a language
    /// @param language Language code (e.g., "pl_PL")
    /// @return Path to dictionary directory, or empty if not found
    QString findDictionaryPath(const QString& language) const;

    /// @brief Get list of system dictionary paths to search
    /// @return List of paths where dictionaries might be located
    QStringList getSystemDictionaryPaths() const;

    // =========================================================================
    // Word Extraction
    // =========================================================================

    /// @brief Extract words with positions from text
    /// @param text The text to extract words from
    /// @return List of (position, word) pairs
    QList<QPair<int, QString>> extractWords(const QString& text) const;

    // =========================================================================
    // User Dictionary Persistence
    // =========================================================================

    /// @brief Load user dictionary from file
    void loadUserDictionary();

    /// @brief Save user dictionary to file
    void saveUserDictionary();

    /// @brief Get path to user dictionary file
    /// @return Path to user_dictionary.txt
    QString userDictionaryPath() const;

    // =========================================================================
    // Document Observer
    // =========================================================================

    /// @brief Called when document content changes
    void onDocumentChanged();

    /// @brief Mark paragraph for checking
    /// @param index Paragraph index to mark
    void markParagraphForCheck(int index);

    // =========================================================================
    // Members
    // =========================================================================

    /// @brief Hunspell instance (opaque pointer to avoid header dependency)
    void* m_hunspell{nullptr};

    KmlDocument* m_document{nullptr};
    QString m_currentLanguage;
    bool m_enabled{true};

    // Document observer
    std::unique_ptr<DocumentObserver> m_observer;

    // Debounce timer for background checking
    QTimer* m_debounceTimer{nullptr};
    static constexpr int DEBOUNCE_MS = 500;

    // User dictionary (persisted)
    QSet<QString> m_userDictionary;

    // Ignored words (session only)
    QSet<QString> m_ignoredWords;

    // Pending paragraphs to check
    QSet<int> m_pendingParagraphs;
};

}  // namespace kalahari::editor
