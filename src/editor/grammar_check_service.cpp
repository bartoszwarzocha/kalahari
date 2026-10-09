/// @file grammar_check_service.cpp
/// @brief Grammar as you type: texts checked by the LanguageTool server the writer runs

#include <kalahari/editor/grammar_check_service.h>
#include <kalahari/core/logger.h>

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <array>

namespace kalahari::editor {

namespace {

/// @brief How many texts are checked at a time
constexpr std::size_t MAX_SENT = 2;

/// @brief How long the server is not asked after it did not answer (ms)
constexpr int RETRY_MS = 30000;

/// @brief How long an answer may take (ms): the first check in a language loads its rules
constexpr int TRANSFER_TIMEOUT_MS = 30000;

/// @brief How many replacements of an issue are kept
constexpr qsizetype MAX_SUGGESTIONS = 5;

/// @brief How much of the server's own explanation of a failure is told
constexpr qsizetype MAX_REASON_LENGTH = 160;

/// @brief The answer of a server to a text too long for it
constexpr int HTTP_CONTENT_TOO_LARGE = 413;

/// @brief The regional variants LanguageTool checks as such; for the others of their
///        languages it gets the language alone
constexpr std::array<const char*, 13> LANGUAGE_VARIANTS = {
    "en-US", "en-GB", "en-AU", "en-CA", "en-NZ", "en-ZA", "de-DE",
    "de-AT", "de-CH", "pt-PT", "pt-BR", "pt-AO", "pt-MZ"};

/// @brief The kind of an issue, from its rule's category and issue type
GrammarIssueType issueTypeOf(const QString& category, const QString& issueType) {
    if (category == QLatin1String("TYPOS") || issueType == QLatin1String("misspelling")) {
        return GrammarIssueType::Spelling;
    }
    if (category == QLatin1String("STYLE") || category == QLatin1String("REDUNDANCY") ||
        category == QLatin1String("REPETITIONS") ||
        category == QLatin1String("REPETITIONS_STYLE") ||
        category == QLatin1String("PLAIN_ENGLISH") || category == QLatin1String("SEMANTICS") ||
        issueType == QLatin1String("style")) {
        return GrammarIssueType::Style;
    }
    if (category == QLatin1String("TYPOGRAPHY") || category == QLatin1String("PUNCTUATION") ||
        category == QLatin1String("CASING") || category == QLatin1String("COMPOUNDING") ||
        issueType == QLatin1String("typographical") || issueType == QLatin1String("whitespace")) {
        return GrammarIssueType::Typography;
    }
    if (category == QLatin1String("GRAMMAR") || category == QLatin1String("CONFUSED_WORDS") ||
        category == QLatin1String("MISC") || issueType == QLatin1String("grammar")) {
        return GrammarIssueType::Grammar;
    }
    return GrammarIssueType::Other;
}

/// @brief Whether a host is this computer
bool isLocalHost(const QString& host) {
    return host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0 ||
           QHostAddress(host).isLoopback();
}

}  // namespace

GrammarCheckService::GrammarCheckService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_retryTimer(new QTimer(this))
{
    m_retryTimer->setSingleShot(true);
    m_retryTimer->setInterval(RETRY_MS);
    connect(m_retryTimer, &QTimer::timeout, this, [this]() {
        m_failing = false;
        if (isActive()) {
            emit available();
        }
    });
    connect(m_network, &QNetworkAccessManager::finished, this, &GrammarCheckService::onReply);
}

GrammarCheckService::~GrammarCheckService()
{
    // The replies go with the network manager; none is answered any more
    disconnect(m_network, nullptr, this, nullptr);
    for (const auto& [reply, request] : m_sent) {
        reply->abort();
    }
}

QString GrammarCheckService::endpointFor(const QString& server)
{
    QString address = server.trimmed();
    if (address.isEmpty()) {
        return QString();
    }
    if (!address.contains(QLatin1String("://"))) {
        address.prepend(QLatin1String("http://"));
    }
    QUrl url(address);
    const QString path = url.path();
    if (path.isEmpty() || path == QLatin1String("/") || path == QLatin1String("/v2") ||
        path == QLatin1String("/v2/")) {
        url.setPath(QStringLiteral("/v2/check"));
    }
    return url.toString();
}

QString GrammarCheckService::languageFor(const QString& language)
{
    QString code = language.trimmed();
    code.replace(QLatin1Char('_'), QLatin1Char('-'));
    const qsizetype dash = code.indexOf(QLatin1Char('-'));
    QString base = (dash < 0 ? code : code.left(dash)).toLower();
    if (dash < 0) {
        return base;
    }
    const QString variant = base + QLatin1Char('-') + code.mid(dash + 1).toUpper();
    const bool known =
        std::any_of(LANGUAGE_VARIANTS.begin(), LANGUAGE_VARIANTS.end(),
                    [&variant](const char* name) { return variant == QLatin1String(name); });
    return known ? variant : base;
}

// =============================================================================
// Setup
// =============================================================================

void GrammarCheckService::setServer(const QString& server)
{
    const QString endpoint = endpointFor(server);
    if (endpoint == m_endpoint) {
        return;
    }
    m_endpoint = endpoint;

    // A server on this computer is reached directly, whatever proxy the system has
    m_network->setProxy(isLocalHost(QUrl(endpoint).host()) ? QNetworkProxy(QNetworkProxy::NoProxy)
                                                           : QNetworkProxy());
    core::Logger::getInstance().info("GrammarCheckService: LanguageTool server '{}'",
                                     endpoint.toStdString());
    restart();
}

QString GrammarCheckService::endpoint() const
{
    return m_endpoint;
}

bool GrammarCheckService::isConfigured() const
{
    const QUrl url(m_endpoint);
    return !m_endpoint.isEmpty() && url.isValid() && !url.host().isEmpty() &&
           (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
}

void GrammarCheckService::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        restart();
    }
}

bool GrammarCheckService::isEnabled() const
{
    return m_enabled;
}

bool GrammarCheckService::isActive() const
{
    return m_enabled && !m_failing && isConfigured();
}

void GrammarCheckService::setLanguage(const QString& language)
{
    const QString code = languageFor(language);
    if (code == m_language) {
        return;
    }
    m_language = code;
    core::Logger::getInstance().info("GrammarCheckService: Language '{}'", code.toStdString());
    restart();
}

QString GrammarCheckService::language() const
{
    return m_language;
}

void GrammarCheckService::setRetryDelay(int ms)
{
    m_retryTimer->setInterval(ms);
}

// =============================================================================
// Checking
// =============================================================================

quint64 GrammarCheckService::check(const QString& text)
{
    if (!isActive()) {
        return 0;
    }
    const quint64 request = ++m_lastRequest;
    m_waiting.emplace_back(request, text);
    sendWaiting();
    return request;
}

void GrammarCheckService::cancel(quint64 request)
{
    const auto waiting =
        std::find_if(m_waiting.begin(), m_waiting.end(),
                     [request](const auto& item) { return item.first == request; });
    if (waiting != m_waiting.end()) {
        m_waiting.erase(waiting);
        return;
    }
    const auto sent = std::find_if(m_sent.begin(), m_sent.end(), [request](const auto& item) {
        return item.second.first == request;
    });
    if (sent != m_sent.end()) {
        QNetworkReply* reply = sent->first;
        m_sent.erase(sent);
        reply->abort();
        sendWaiting();
    }
}

int GrammarCheckService::pendingChecks() const
{
    return static_cast<int>(m_waiting.size() + m_sent.size());
}

void GrammarCheckService::ignoreRule(const QString& ruleId)
{
    if (ruleId.isEmpty() || m_ignoredRules.contains(ruleId)) {
        return;
    }
    m_ignoredRules.insert(ruleId);
    emit ruleIgnored(ruleId);
}

bool GrammarCheckService::isRuleIgnored(const QString& ruleId) const
{
    return m_ignoredRules.contains(ruleId);
}

// =============================================================================
// Requests and answers
// =============================================================================

void GrammarCheckService::sendWaiting()
{
    while (m_sent.size() < MAX_SENT && !m_waiting.empty()) {
        auto [request, text] = std::move(m_waiting.front());
        m_waiting.pop_front();

        QNetworkRequest httpRequest{QUrl(m_endpoint)};
        httpRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                              QByteArrayLiteral("application/x-www-form-urlencoded"));
        httpRequest.setTransferTimeout(TRANSFER_TIMEOUT_MS);
        const QByteArray form = "text=" + QUrl::toPercentEncoding(text) +
                                "&language=" + QUrl::toPercentEncoding(m_language);
        QNetworkReply* reply = m_network->post(httpRequest, form);
        m_sent.emplace(reply, std::make_pair(request, std::move(text)));
    }
}

void GrammarCheckService::onReply(QNetworkReply* reply)
{
    reply->deleteLater();
    const auto sent = m_sent.find(reply);
    if (sent == m_sent.end()) {
        return;  // dropped
    }
    const auto [request, text] = std::move(sent->second);
    m_sent.erase(sent);

    if (reply->error() == QNetworkReply::NoError) {
        m_errorTold = false;
        emit textChecked(request, parse(reply->readAll(), text));
    } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ==
               HTTP_CONTENT_TOO_LARGE) {
        emit textChecked(request, {});  // too long for the server: nothing found in it
    } else {
        // The server's own explanation (LanguageTool's first sentence), else Qt's
        QString message = QString::fromUtf8(reply->readAll()).trimmed();
        message = message.section(QLatin1Char('\n'), 0, 0);
        const qsizetype sentenceEnd = message.indexOf(QLatin1String(". "));
        if (sentenceEnd > 0) {
            message.truncate(sentenceEnd + 1);
        }
        if (message.isEmpty()) {
            message = reply->errorString();
        }
        emit textNotChecked(request);
        fail(message.left(MAX_REASON_LENGTH));
        return;
    }
    sendWaiting();
}

void GrammarCheckService::fail(const QString& message)
{
    core::Logger::getInstance().warn("GrammarCheckService: The server did not answer: {}",
                                     message.toStdString());
    m_failing = true;

    // Nothing is sent until the server is asked again; the requests are answered then
    const auto waiting = std::exchange(m_waiting, {});
    const auto sent = std::exchange(m_sent, {});
    for (const auto& [reply, request] : sent) {
        reply->abort();
    }
    for (const auto& [request, text] : waiting) {
        emit textNotChecked(request);
    }
    for (const auto& [reply, request] : sent) {
        emit textNotChecked(request.first);
    }
    m_retryTimer->start();

    if (!m_errorTold) {
        m_errorTold = true;
        emit serverError(message);
    }
}

void GrammarCheckService::restart()
{
    m_waiting.clear();
    const auto sent = std::exchange(m_sent, {});
    for (const auto& [reply, request] : sent) {
        reply->abort();
    }
    m_failing = false;
    m_errorTold = false;
    m_retryTimer->stop();
    emit checkingChanged();
}

QList<GrammarError> GrammarCheckService::parse(const QByteArray& json, const QString& text) const
{
    QList<GrammarError> errors;
    const QJsonArray matches =
        QJsonDocument::fromJson(json).object().value(QLatin1String("matches")).toArray();
    for (const auto& value : matches) {
        const QJsonObject match = value.toObject();
        const QJsonObject rule = match.value(QLatin1String("rule")).toObject();
        const QJsonObject category = rule.value(QLatin1String("category")).toObject();

        GrammarError error;
        error.startPos = match.value(QLatin1String("offset")).toInt(-1);
        error.length = match.value(QLatin1String("length")).toInt();
        error.ruleId = rule.value(QLatin1String("id")).toString();
        error.type = issueTypeOf(category.value(QLatin1String("id")).toString(),
                                 rule.value(QLatin1String("issueType")).toString());

        // Spelling is the dictionary's; an issue lies in the text checked
        const bool outside = error.startPos < 0 || error.length <= 0 ||
                             error.startPos + error.length > text.size();
        if (error.type == GrammarIssueType::Spelling || m_ignoredRules.contains(error.ruleId) ||
            outside) {
            continue;
        }
        error.text = text.mid(error.startPos, error.length);
        error.message = match.value(QLatin1String("message")).toString();
        error.shortMessage = match.value(QLatin1String("shortMessage")).toString();
        error.category = category.value(QLatin1String("name")).toString();
        error.ignoreForIncompleteSentence =
            match.value(QLatin1String("ignoreForIncompleteSentence")).toBool();
        const QJsonArray replacements = match.value(QLatin1String("replacements")).toArray();
        for (const auto& replacement : replacements) {
            const QString suggestion =
                replacement.toObject().value(QLatin1String("value")).toString();
            if (!suggestion.isEmpty() && error.suggestions.size() < MAX_SUGGESTIONS) {
                error.suggestions.append(suggestion);
            }
        }
        errors.append(error);
    }
    return errors;
}

}  // namespace kalahari::editor
