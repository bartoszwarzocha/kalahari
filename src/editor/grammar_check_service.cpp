/// @file grammar_check_service.cpp
/// @brief Grammar checking service implementation (OpenSpec #00042 Phase 6.14-6.17)

#include <kalahari/editor/grammar_check_service.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>

#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace kalahari::editor {

// =============================================================================
// GrammarCheckService
// =============================================================================

GrammarCheckService::GrammarCheckService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_debounceTimer(new QTimer(this))
    , m_rateLimitTimer(new QTimer(this))
{
    // Setup debounce timer
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(m_debounceMs);
    connect(m_debounceTimer, &QTimer::timeout,
            this, &GrammarCheckService::onDebounceTimeout);

    // Setup rate limit timer
    m_rateLimitTimer->setSingleShot(true);
    m_rateLimitTimer->setInterval(m_rateLimitMs);
    connect(m_rateLimitTimer, &QTimer::timeout,
            this, &GrammarCheckService::processQueue);

    // Connect network manager
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &GrammarCheckService::onNetworkReply);

    core::Logger::getInstance().debug("GrammarCheckService created");
}

GrammarCheckService::~GrammarCheckService()
{
    // Disconnect from editor
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
    }

    // Cancel pending requests
    cancelPendingChecks();

    core::Logger::getInstance().debug("GrammarCheckService destroyed");
}

// =============================================================================
// Setup
// =============================================================================

void GrammarCheckService::setBookEditor(BookEditor* editor)
{
    if (m_editor == editor) {
        return;
    }

    // Disconnect from previous editor
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
    }

    m_editor = editor;

    // Clear caches
    m_pendingParagraphs.clear();
    m_paragraphErrors.clear();

    // Connect to new editor
    if (m_editor) {
        connect(m_editor, &BookEditor::paragraphModified,
                this, &GrammarCheckService::onParagraphModified);
        connect(m_editor, &BookEditor::paragraphInserted,
                this, &GrammarCheckService::onParagraphInserted);
        connect(m_editor, &BookEditor::paragraphRemoved,
                this, &GrammarCheckService::onParagraphRemoved);

        // Mark all paragraphs for initial check
        if (m_enabled) {
            for (size_t i = 0; i < m_editor->paragraphCount(); ++i) {
                m_pendingParagraphs.insert(static_cast<int>(i));
            }
            m_debounceTimer->start();
        }
    }
}

void GrammarCheckService::setLanguage(const QString& language)
{
    if (m_language == language) {
        return;
    }

    m_language = language;
    core::Logger::getInstance().info("GrammarCheckService: Language set to '{}'",
                                     language.toStdString());

    // Re-check document with new language
    if (m_editor && m_enabled) {
        checkDocumentAsync();
    }
}

QString GrammarCheckService::language() const
{
    return m_language;
}

void GrammarCheckService::setApiEndpoint(const QString& url)
{
    if (m_apiEndpoint == url) {
        return;
    }

    m_apiEndpoint = url;
    core::Logger::getInstance().info("GrammarCheckService: API endpoint set to '{}'",
                                     url.toStdString());
}

QString GrammarCheckService::apiEndpoint() const
{
    return m_apiEndpoint;
}

void GrammarCheckService::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;

    if (enabled && m_editor) {
        // Trigger full document check
        checkDocumentAsync();
    } else if (!enabled) {
        // Clear pending checks and cached errors
        cancelPendingChecks();
        m_paragraphErrors.clear();

        // Emit empty error lists for all paragraphs to clear UI
        if (m_editor) {
            for (size_t i = 0; i < m_editor->paragraphCount(); ++i) {
                emit paragraphChecked(static_cast<int>(i), QList<GrammarError>());
            }
        }
    }
}

bool GrammarCheckService::isEnabled() const
{
    return m_enabled;
}

// =============================================================================
// Checking
// =============================================================================

void GrammarCheckService::checkTextAsync(const QString& text, int paragraphIndex)
{
    if (!m_enabled || text.trimmed().isEmpty()) {
        // No text to check, emit empty result
        emit paragraphChecked(paragraphIndex, QList<GrammarError>());
        return;
    }

    // Add to queue
    m_requestQueue.enqueue({text, paragraphIndex});

    // Process queue if no request in progress
    if (!m_requestInProgress && !m_rateLimitTimer->isActive()) {
        processQueue();
    }
}

void GrammarCheckService::checkDocumentAsync()
{
    if (!m_editor || !m_enabled) {
        emit documentCheckComplete();
        return;
    }

    // Mark all paragraphs for checking
    m_pendingParagraphs.clear();
    for (size_t i = 0; i < m_editor->paragraphCount(); ++i) {
        m_pendingParagraphs.insert(static_cast<int>(i));
    }

    // Start debounce timer
    m_debounceTimer->start();
}

void GrammarCheckService::cancelPendingChecks()
{
    // Stop timers
    m_debounceTimer->stop();
    m_rateLimitTimer->stop();

    // Clear queue
    m_requestQueue.clear();
    m_pendingParagraphs.clear();

    // Cancel pending network requests
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        QNetworkReply* reply = it.key();
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
    m_requestInProgress = false;
}

QList<GrammarError> GrammarCheckService::errorsForParagraph(int index) const
{
    return m_paragraphErrors.value(index);
}

bool GrammarCheckService::hasPendingRequests() const
{
    return m_requestInProgress ||
           !m_requestQueue.isEmpty() ||
           !m_pendingRequests.isEmpty();
}

// =============================================================================
// Configuration
// =============================================================================

void GrammarCheckService::setEnabledCategories(const QStringList& categories)
{
    m_enabledCategories.clear();
    for (const QString& cat : categories) {
        m_enabledCategories.insert(cat);
    }
}

QStringList GrammarCheckService::enabledCategories() const
{
    return QStringList(m_enabledCategories.begin(), m_enabledCategories.end());
}

void GrammarCheckService::setDisabledCategories(const QStringList& categories)
{
    m_disabledCategories.clear();
    for (const QString& cat : categories) {
        m_disabledCategories.insert(cat);
    }
}

QStringList GrammarCheckService::disabledCategories() const
{
    return QStringList(m_disabledCategories.begin(), m_disabledCategories.end());
}

void GrammarCheckService::ignoreRule(const QString& ruleId)
{
    if (m_ignoredRules.contains(ruleId)) {
        return;
    }

    m_ignoredRules.insert(ruleId);
    core::Logger::getInstance().debug("GrammarCheckService: Ignoring rule '{}'",
                                      ruleId.toStdString());

    // Re-check document to update UI
    if (m_editor && m_enabled) {
        checkDocumentAsync();
    }
}

bool GrammarCheckService::isRuleIgnored(const QString& ruleId) const
{
    return m_ignoredRules.contains(ruleId);
}

QSet<QString> GrammarCheckService::ignoredRules() const
{
    return m_ignoredRules;
}

void GrammarCheckService::clearIgnoredRules()
{
    m_ignoredRules.clear();

    // Re-check document to update UI
    if (m_editor && m_enabled) {
        checkDocumentAsync();
    }
}

// =============================================================================
// Rate Limiting Configuration
// =============================================================================

void GrammarCheckService::setRateLimitMs(int ms)
{
    m_rateLimitMs = qMax(100, ms);  // Minimum 100ms
    m_rateLimitTimer->setInterval(m_rateLimitMs);
}

int GrammarCheckService::rateLimitMs() const
{
    return m_rateLimitMs;
}

void GrammarCheckService::setDebounceMs(int ms)
{
    m_debounceMs = qMax(100, ms);  // Minimum 100ms
    m_debounceTimer->setInterval(m_debounceMs);
}

int GrammarCheckService::debounceMs() const
{
    return m_debounceMs;
}

// =============================================================================
// Private Slots
// =============================================================================

void GrammarCheckService::onNetworkReply(QNetworkReply* reply)
{
    m_requestInProgress = false;

    // Get associated paragraph index
    int paragraphIndex = m_pendingRequests.value(reply, -1);
    m_pendingRequests.remove(reply);

    QList<GrammarError> errors;

    if (reply->error() == QNetworkReply::NoError) {
        // Parse response
        QByteArray data = reply->readAll();
        errors = parseResponse(data);

        // Cache results
        if (paragraphIndex >= 0) {
            m_paragraphErrors.insert(paragraphIndex, errors);
        }

        core::Logger::getInstance().debug(
            "GrammarCheckService: Received {} errors for paragraph {}",
            errors.size(), paragraphIndex);
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
        // Request was cancelled, ignore
        core::Logger::getInstance().debug("GrammarCheckService: Request cancelled");
    } else {
        // Network error
        QString errorMsg = reply->errorString();
        core::Logger::getInstance().warn("GrammarCheckService: API error: {}",
                                         errorMsg.toStdString());
        emit apiError(errorMsg);
    }

    reply->deleteLater();

    // Emit results
    emit paragraphChecked(paragraphIndex, errors);

    // Process next in queue after rate limit delay
    if (!m_requestQueue.isEmpty()) {
        m_rateLimitTimer->start();
    } else if (m_pendingRequests.isEmpty()) {
        emit checkingFinished();
    }
}

void GrammarCheckService::onDebounceTimeout()
{
    if (!m_editor || !m_enabled) {
        m_pendingParagraphs.clear();
        return;
    }

    // Queue all pending paragraphs for checking
    emit checkingStarted();

    for (int idx : m_pendingParagraphs) {
        if (idx < 0 || static_cast<size_t>(idx) >= m_editor->paragraphCount()) {
            continue;
        }

        QString text = m_editor->paragraphPlainText(static_cast<size_t>(idx));
        if (!text.trimmed().isEmpty()) {
            m_requestQueue.enqueue({text, idx});
        } else {
            // Empty paragraph - emit empty result
            m_paragraphErrors.insert(idx, QList<GrammarError>());
            emit paragraphChecked(idx, QList<GrammarError>());
        }
    }

    m_pendingParagraphs.clear();

    // Start processing queue
    if (!m_requestInProgress && !m_requestQueue.isEmpty()) {
        processQueue();
    } else if (m_requestQueue.isEmpty()) {
        emit documentCheckComplete();
    }
}

// =============================================================================
// BookEditor Signal Handlers
// =============================================================================

void GrammarCheckService::onParagraphModified(int paragraphIndex)
{
    if (m_enabled) {
        m_pendingParagraphs.insert(paragraphIndex);
        m_debounceTimer->start();
    }
}

void GrammarCheckService::onParagraphInserted(int paragraphIndex)
{
    if (m_enabled) {
        m_pendingParagraphs.insert(paragraphIndex);
        m_debounceTimer->start();
    }
}

void GrammarCheckService::onParagraphRemoved(int paragraphIndex)
{
    // Remove cached errors for removed paragraph
    m_paragraphErrors.remove(paragraphIndex);
    m_pendingParagraphs.remove(paragraphIndex);
}

void GrammarCheckService::processQueue()
{
    if (m_requestInProgress || m_requestQueue.isEmpty()) {
        return;
    }

    m_requestInProgress = true;
    auto [text, index] = m_requestQueue.dequeue();
    sendApiRequest(text, index);
}

// =============================================================================
// API Request Methods
// =============================================================================

void GrammarCheckService::sendApiRequest(const QString& text, int paragraphIndex)
{
    QUrl url(m_apiEndpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Kalahari/1.0 (Grammar Checker)");

    // Build request parameters
    QUrlQuery params;
    params.addQueryItem("text", text);
    params.addQueryItem("language", m_language);

    // Add enabled categories if specified
    if (!m_enabledCategories.isEmpty()) {
        QStringList catList(m_enabledCategories.begin(), m_enabledCategories.end());
        params.addQueryItem("enabledCategories", catList.join(","));
    }

    // Add disabled categories if any
    if (!m_disabledCategories.isEmpty()) {
        QStringList catList(m_disabledCategories.begin(), m_disabledCategories.end());
        params.addQueryItem("disabledCategories", catList.join(","));
    }

    // Send request
    QByteArray postData = params.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply* reply = m_networkManager->post(request, postData);

    // Track pending request
    m_pendingRequests.insert(reply, paragraphIndex);

    core::Logger::getInstance().debug(
        "GrammarCheckService: Sent request for paragraph {} ({} chars)",
        paragraphIndex, text.length());
}

QList<GrammarError> GrammarCheckService::parseResponse(const QByteArray& json)
{
    QList<GrammarError> errors;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        core::Logger::getInstance().warn(
            "GrammarCheckService: JSON parse error: {}",
            parseError.errorString().toStdString());
        return errors;
    }

    if (!doc.isObject()) {
        return errors;
    }

    QJsonObject root = doc.object();
    QJsonArray matches = root["matches"].toArray();

    for (const QJsonValue& matchVal : matches) {
        QJsonObject match = matchVal.toObject();

        GrammarError error;
        error.startPos = match["offset"].toInt();
        error.length = match["length"].toInt();
        error.message = match["message"].toString();
        error.shortMessage = match["shortMessage"].toString();

        // Get rule information
        QJsonObject rule = match["rule"].toObject();
        error.ruleId = rule["id"].toString();

        // Check if rule is ignored
        if (m_ignoredRules.contains(error.ruleId)) {
            continue;
        }

        // Get category information
        QJsonObject category = rule["category"].toObject();
        error.category = category["name"].toString();
        QString categoryId = category["id"].toString();
        error.type = categoryToType(categoryId);

        // Skip spelling errors (handled by SpellCheckService)
        if (error.type == GrammarIssueType::Spelling) {
            continue;
        }

        // Get suggestions (max 5)
        QJsonArray replacements = match["replacements"].toArray();
        for (int i = 0; i < qMin(5, static_cast<int>(replacements.size())); ++i) {
            QJsonObject replacement = replacements[i].toObject();
            error.suggestions.append(replacement["value"].toString());
        }

        // Get problematic text from context
        QJsonObject context = match["context"].toObject();
        int contextOffset = context["offset"].toInt();
        int contextLength = context["length"].toInt();
        QString contextText = context["text"].toString();
        error.text = contextText.mid(contextOffset, contextLength);

        errors.append(error);
    }

    return errors;
}

GrammarIssueType GrammarCheckService::categoryToType(const QString& category) const
{
    // LanguageTool category IDs mapping
    // See: https://languagetool.org/http-api/swagger-ui/#/default/post_check

    // Spelling-related categories (skip these, handled by SpellCheckService)
    if (category == "TYPOS" ||
        category == "SPELLING" ||
        category == "MORFOLOGIK_RULE_EN_US" ||
        category == "MORFOLOGIK_RULE_PL_PL") {
        return GrammarIssueType::Spelling;
    }

    // Style categories
    if (category == "STYLE" ||
        category == "REDUNDANCY" ||
        category == "REPETITIONS" ||
        category == "SEMANTICS" ||
        category == "PLAIN_ENGLISH") {
        return GrammarIssueType::Style;
    }

    // Typography categories
    if (category == "TYPOGRAPHY" ||
        category == "PUNCTUATION" ||
        category == "CASING" ||
        category == "COMPOUNDING") {
        return GrammarIssueType::Typography;
    }

    // Grammar categories
    if (category == "GRAMMAR" ||
        category == "CONFUSED_WORDS" ||
        category == "MISC" ||
        category == "GENDER_NEUTRALITY") {
        return GrammarIssueType::Grammar;
    }

    // Default to Grammar for unknown categories
    return GrammarIssueType::Grammar;
}

}  // namespace kalahari::editor
