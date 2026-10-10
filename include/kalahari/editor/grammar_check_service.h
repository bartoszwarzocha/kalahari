/// @file grammar_check_service.h
/// @brief Grammar as you type: texts checked by the LanguageTool server the writer runs

#pragma once

#include <kalahari/editor/grammar_error.h>

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <deque>
#include <map>
#include <utility>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace kalahari::editor {

/// @brief Checks texts on the writer's LanguageTool server
///
/// One service serves the editors of all documents: each sends the paragraphs it has to
/// check and takes the answers to its own requests. There is no built-in server, so the
/// text of the book leaves the program only for the address the writer gives (usually
/// LanguageTool running on the same computer). A few texts are checked at a time, the
/// others wait their turn.
///
/// When the server does not answer, the service stops sending, says so once
/// (serverError()) and tries again after a while (available()). Spelling issues are left
/// to the dictionary (SpellCheckService).
class GrammarCheckService : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor: no server, nothing is checked until one is given
    explicit GrammarCheckService(QObject* parent = nullptr);

    /// @brief Destructor: the requests sent are dropped
    ~GrammarCheckService() override;

    GrammarCheckService(const GrammarCheckService&) = delete;
    GrammarCheckService& operator=(const GrammarCheckService&) = delete;

    /// @brief The address the texts go to for a server the writer gives
    ///
    /// A server alone ("http://localhost:8081", also without http://) gets the check
    /// endpoint of LanguageTool (/v2/check).
    /// @return The address, or empty for an empty one
    static QString endpointFor(const QString& server);

    /// @brief The language LanguageTool checks a text in (pl for pl_PL or pl-PL)
    ///
    /// The regional variants LanguageTool knows (en-GB, de-CH, pt-BR...) are kept; for
    /// the other languages it is given the language alone.
    static QString languageFor(const QString& language);

    // =========================================================================
    // Setup
    // =========================================================================

    /// @brief Set the server the texts are checked on (see endpointFor()); empty: none
    void setServer(const QString& server);

    /// @brief The address the texts go to, or empty
    QString endpoint() const;

    /// @brief Whether a server is set: an http or https address
    bool isConfigured() const;

    /// @brief Turn the checking on or off
    void setEnabled(bool enabled);

    /// @brief Whether the checking is on (whether or not a server is set)
    bool isEnabled() const;

    /// @brief Whether texts are sent now: on, with a server, not waiting after a failure
    bool isActive() const;

    /// @brief Set the language the texts are in (see languageFor())
    void setLanguage(const QString& language);

    /// @brief The language the texts are checked in
    QString language() const;

    /// @brief How long the server is not asked after it did not answer (30 s at first)
    void setRetryDelay(int ms);

    // =========================================================================
    // Checking
    // =========================================================================

    /// @brief Ask for a text to be checked
    /// @return The request, answered with textChecked() or textNotChecked(); 0 when nothing
    ///         is sent (the checking is not active)
    quint64 check(const QString& text);

    /// @brief Drop a request: it is not answered
    void cancel(quint64 request);

    /// @brief How many requests wait or are being checked
    int pendingChecks() const;

    /// @brief Do not report the issues of a rule any more (until the program closes)
    void ignoreRule(const QString& ruleId);

    /// @brief Whether the issues of a rule are not reported
    bool isRuleIgnored(const QString& ruleId) const;

signals:
    /// @brief A text was checked
    /// @param request The request (check())
    /// @param errors Its issues, without spelling and ignored rules
    void textChecked(quint64 request, const QList<GrammarError>& errors);

    /// @brief A text was not checked: the server did not answer (it is asked again later)
    void textNotChecked(quint64 request);

    /// @brief The issues found so far do not apply: another server or language, or the
    ///        checking was turned on or off. The requests sent are dropped.
    void checkingChanged();

    /// @brief The server is asked again after it did not answer
    void available();

    /// @brief The issues of a rule are not reported any more
    void ruleIgnored(const QString& ruleId);

    /// @brief The server did not answer (told once until it answers again)
    /// @param message What went wrong
    void serverError(const QString& message);

private slots:
    /// @brief An answer of the server (or a request dropped)
    void onReply(QNetworkReply* reply);

private:
    /// @brief Send the requests that wait, while fewer than the limit are being checked
    void sendWaiting();

    /// @brief The server did not answer: stop sending until it is asked again
    void fail(const QString& message);

    /// @brief Drop every request, the checking starts anew
    void restart();

    /// @brief The issues in an answer of the server, for the text checked
    QList<GrammarError> parse(const QByteArray& json, const QString& text) const;

    QNetworkAccessManager* m_network;  ///< Sends the texts
    QTimer* m_retryTimer;              ///< Asks the server again after a failure
    QString m_endpoint;                ///< Empty: no server, nothing is sent
    QString m_language{QStringLiteral("en")};
    bool m_enabled{true};
    bool m_failing{false};     ///< The server did not answer: waiting to ask it again
    bool m_errorTold{false};   ///< serverError() was emitted since the last answer
    quint64 m_lastRequest{0};  ///< The number of the last request

    /// @brief Requests waiting to be sent: number and text
    std::deque<std::pair<quint64, QString>> m_waiting;

    /// @brief Requests being checked: number and text, by their reply
    std::map<QNetworkReply*, std::pair<quint64, QString>> m_sent;

    QSet<QString> m_ignoredRules;  ///< Rules whose issues are not reported
};

}  // namespace kalahari::editor
