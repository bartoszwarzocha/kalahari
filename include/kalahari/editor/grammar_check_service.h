/// @file grammar_check_service.h
/// @brief Grammar checking service using LanguageTool API (OpenSpec #00042 Phase 6.14-6.17)
///
/// GrammarCheckService provides:
/// - Real-time grammar checking using LanguageTool REST API
/// - Multi-language support (Polish, English, etc.)
/// - Background checking with debounce and rate limiting
/// - Error descriptions and suggestions
/// - Distinguishes grammar errors from spelling/style issues
///
/// The service uses Qt Network for REST API calls and integrates with
/// the document observer pattern for real-time checking.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QTimer>
#include <QList>
#include <QHash>
#include <QMap>
#include <QQueue>
#include <QPair>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Forward declarations
namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::editor {

/// @brief Type of grammar issue
///
/// Distinguishes different types of writing issues to allow
/// appropriate visual styling (underline colors) in the editor.
enum class GrammarIssueType {
    Grammar,        ///< Grammar error (blue underline)
    Style,          ///< Style suggestion (green underline)
    Typography,     ///< Typography issue (gray underline)
    Spelling,       ///< Spelling from LanguageTool (handled by SpellCheckService)
    Other           ///< Other issues
};

/// @brief Information about a grammar error
///
/// Contains position, error text, message, suggestions and categorization
/// for a single grammar issue found by LanguageTool.
struct GrammarError {
    int startPos{0};            ///< Start position in text
    int length{0};              ///< Length of problematic text
    QString text;               ///< The problematic text
    QString message;            ///< Full error message
    QString shortMessage;       ///< Short description
    QString ruleId;             ///< Rule ID (e.g., "COMMA_BEFORE_AND")
    QString category;           ///< Category (e.g., "Punctuation")
    GrammarIssueType type{GrammarIssueType::Grammar};
    QStringList suggestions;    ///< Suggested replacements (max 5)

    GrammarError() = default;
    GrammarError(int start, int len, const QString& txt)
        : startPos(start), length(len), text(txt) {}

    bool operator==(const GrammarError& other) const {
        return startPos == other.startPos &&
               length == other.length &&
               text == other.text &&
               ruleId == other.ruleId;
    }
};

/// @brief Grammar checking service using LanguageTool API
///
/// Provides asynchronous grammar checking for KML documents using the
/// LanguageTool REST API (public or local server). Integrates with the
/// document observer pattern for real-time checking as the user types.
///
/// Usage:
/// @code
/// auto service = new GrammarCheckService(this);
/// service->setLanguage("en-US");
/// service->setDocument(document);
///
/// // Connect to grammar check results
/// connect(service, &GrammarCheckService::paragraphChecked,
///         this, &MyEditor::onGrammarErrors);
/// @endcode
///
/// Rate Limiting:
/// The public LanguageTool API has rate limits. This service implements
/// request queuing and rate limiting to avoid API abuse. For high-volume
/// usage, consider setting up a local LanguageTool server.
class GrammarCheckService : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a grammar check service
    /// @param parent Parent QObject for ownership
    explicit GrammarCheckService(QObject* parent = nullptr);

    /// @brief Destructor - cancels pending requests
    ~GrammarCheckService() override;

    // Non-copyable
    GrammarCheckService(const GrammarCheckService&) = delete;
    GrammarCheckService& operator=(const GrammarCheckService&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /// @brief Set the BookEditor to check
    /// @param editor BookEditor to monitor (nullptr to disconnect)
    /// @note Previous editor is automatically disconnected
    void setBookEditor(BookEditor* editor);

    /// @brief Set language code (e.g., "en-US", "pl-PL")
    /// @param language Language code for LanguageTool
    void setLanguage(const QString& language);

    /// @brief Get currently set language
    /// @return Language code
    QString language() const;

    /// @brief Set custom API endpoint (for local LanguageTool server)
    /// @param url API endpoint URL (e.g., "http://localhost:8081/v2/check")
    /// @note Default is "https://api.languagetool.org/v2/check"
    void setApiEndpoint(const QString& url);

    /// @brief Get current API endpoint
    /// @return API endpoint URL
    QString apiEndpoint() const;

    /// @brief Enable or disable grammar checking
    /// @param enabled true to enable, false to disable
    void setEnabled(bool enabled);

    /// @brief Check if grammar checking is enabled
    /// @return true if enabled
    bool isEnabled() const;

    // =========================================================================
    // Checking
    // =========================================================================

    /// @brief Check text asynchronously
    /// @param text The text to check
    /// @param paragraphIndex Paragraph index for result association (-1 for standalone)
    /// @note Results are emitted via paragraphChecked signal
    void checkTextAsync(const QString& text, int paragraphIndex = -1);

    /// @brief Check entire document asynchronously
    /// @note Emits paragraphChecked for each paragraph and documentCheckComplete when done
    void checkDocumentAsync();

    /// @brief Cancel all pending checks
    void cancelPendingChecks();

    /// @brief Get errors for a paragraph (from cache)
    /// @param index Paragraph index
    /// @return List of grammar errors, or empty list if not cached
    QList<GrammarError> errorsForParagraph(int index) const;

    /// @brief Check if any requests are currently pending
    /// @return true if requests are in progress or queued
    bool hasPendingRequests() const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Set which rule categories to enable
    /// @param categories List of category IDs to enable
    /// @note If not set, all categories are enabled
    void setEnabledCategories(const QStringList& categories);

    /// @brief Get enabled categories
    /// @return List of enabled category IDs
    QStringList enabledCategories() const;

    /// @brief Set which rule categories to disable
    /// @param categories List of category IDs to disable
    void setDisabledCategories(const QStringList& categories);

    /// @brief Get disabled categories
    /// @return List of disabled category IDs
    QStringList disabledCategories() const;

    /// @brief Ignore a specific rule (session only)
    /// @param ruleId Rule ID to ignore (e.g., "COMMA_BEFORE_AND")
    void ignoreRule(const QString& ruleId);

    /// @brief Check if a rule is ignored
    /// @param ruleId Rule ID to check
    /// @return true if rule is ignored
    bool isRuleIgnored(const QString& ruleId) const;

    /// @brief Get all ignored rules
    /// @return Set of ignored rule IDs
    QSet<QString> ignoredRules() const;

    /// @brief Clear all ignored rules
    void clearIgnoredRules();

    // =========================================================================
    // Rate Limiting Configuration
    // =========================================================================

    /// @brief Set minimum time between API requests (rate limiting)
    /// @param ms Milliseconds between requests (default 500ms)
    void setRateLimitMs(int ms);

    /// @brief Get current rate limit setting
    /// @return Milliseconds between requests
    int rateLimitMs() const;

    /// @brief Set debounce time for input changes
    /// @param ms Milliseconds to wait after last change (default 1000ms)
    void setDebounceMs(int ms);

    /// @brief Get current debounce setting
    /// @return Milliseconds to wait
    int debounceMs() const;

signals:
    /// @brief Emitted when checking is complete for a paragraph
    /// @param paragraphIndex The paragraph index (-1 for standalone checks)
    /// @param errors List of grammar errors found
    void paragraphChecked(int paragraphIndex, const QList<GrammarError>& errors);

    /// @brief Emitted when full document check is complete
    void documentCheckComplete();

    /// @brief Emitted on API error
    /// @param error Error message
    void apiError(const QString& error);

    /// @brief Emitted when checking starts
    void checkingStarted();

    /// @brief Emitted when all checks are done
    void checkingFinished();

private slots:
    /// @brief Handle network reply
    void onNetworkReply(QNetworkReply* reply);

    /// @brief Handle debounce timer timeout
    void onDebounceTimeout();

    /// @brief Process next request in queue
    void processQueue();

    /// @brief Handle paragraph modified signal from BookEditor
    /// @param paragraphIndex Index of the modified paragraph
    void onParagraphModified(int paragraphIndex);

    /// @brief Handle paragraph inserted signal from BookEditor
    /// @param paragraphIndex Index of the newly inserted paragraph
    void onParagraphInserted(int paragraphIndex);

    /// @brief Handle paragraph removed signal from BookEditor
    /// @param paragraphIndex Index of the removed paragraph
    void onParagraphRemoved(int paragraphIndex);

private:

    // =========================================================================
    // API Request Methods
    // =========================================================================

    /// @brief Send API request to LanguageTool
    /// @param text Text to check
    /// @param paragraphIndex Associated paragraph index
    void sendApiRequest(const QString& text, int paragraphIndex);

    /// @brief Parse LanguageTool JSON response
    /// @param json Raw JSON response
    /// @return List of grammar errors
    QList<GrammarError> parseResponse(const QByteArray& json);

    /// @brief Convert LanguageTool category to GrammarIssueType
    /// @param category Category ID from LanguageTool
    /// @return Corresponding issue type
    GrammarIssueType categoryToType(const QString& category) const;

    // =========================================================================
    // Members
    // =========================================================================

    BookEditor* m_editor{nullptr};
    QNetworkAccessManager* m_networkManager{nullptr};

    QString m_language{"en-US"};
    QString m_apiEndpoint{"https://api.languagetool.org/v2/check"};
    bool m_enabled{true};

    // Debounce timer for background checking
    QTimer* m_debounceTimer{nullptr};
    int m_debounceMs{1000};  // Default 1 second (longer for API calls)

    // Rate limit timer for API requests
    QTimer* m_rateLimitTimer{nullptr};
    int m_rateLimitMs{500};  // Default 500ms between requests
    bool m_requestInProgress{false};

    // Request queue (text, paragraphIndex)
    QQueue<QPair<QString, int>> m_requestQueue;

    // Pending network requests (reply -> paragraphIndex)
    QMap<QNetworkReply*, int> m_pendingRequests;

    // Cache of results (paragraphIndex -> errors)
    QHash<int, QList<GrammarError>> m_paragraphErrors;

    // Pending paragraphs to check
    QSet<int> m_pendingParagraphs;

    // Configuration
    QSet<QString> m_enabledCategories;    // Empty = all enabled
    QSet<QString> m_disabledCategories;
    QSet<QString> m_ignoredRules;
};

}  // namespace kalahari::editor
