/// @file fake_language_tool.h
/// @brief A LanguageTool server stand-in on localhost for the grammar tests
///
/// Answers POST /v2/check as LanguageTool does: an issue for every place of the text
/// where one of its phrases is. It can hold its answers, answer with an error, or stop
/// listening.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

#include <memory>
#include <utility>
#include <vector>

namespace kalahari::test {

/// @brief An issue the fake server reports wherever its phrase is in a text
struct FakeIssue {
    FakeIssue(QString what, QString rule, QString explanation = QString(),
              QStringList fixes = QStringList(), QString kind = QStringLiteral("GRAMMAR"),
              bool onlyFinishedSentence = false)
        : phrase(std::move(what))
        , ruleId(std::move(rule))
        , message(std::move(explanation))
        , replacements(std::move(fixes))
        , category(std::move(kind))
        , incompleteSentence(onlyFinishedSentence) {}

    QString phrase;            ///< Marked wherever it is
    QString ruleId;            ///< The rule reporting it
    QString message;           ///< What is wrong
    QStringList replacements;  ///< What to put in its place
    QString category;          ///< Its category (TYPOS: spelling)
    bool incompleteSentence;   ///< ignoreForIncompleteSentence: not while it is unfinished
};

/// @brief A LanguageTool stand-in listening on localhost
class FakeLanguageTool {
public:
    FakeLanguageTool() {
        REQUIRE(m_server.listen(QHostAddress::LocalHost));
        QObject::connect(&m_server, &QTcpServer::newConnection, [this]() {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                auto buffer = std::make_shared<QByteArray>();
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer]() {
                    *buffer += socket->readAll();
                    receive(socket, *buffer);
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket,
                                 &QObject::deleteLater);
            }
        });
    }

    FakeLanguageTool(const FakeLanguageTool&) = delete;
    FakeLanguageTool& operator=(const FakeLanguageTool&) = delete;

    /// @brief The address of the server
    QString url() const { return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()); }

    /// @brief Report an issue wherever its phrase is
    void addIssue(const FakeIssue& issue) { m_issues.push_back(issue); }

    /// @brief Answer every request with an HTTP error (0: answer as usual)
    void setFailure(int status, const QByteArray& body = QByteArray()) {
        m_failureStatus = status;
        m_failureBody = body;
    }

    /// @brief Answer every request with this body (empty: with the issues added)
    void setAnswer(const QByteArray& json) { m_answer = json; }

    /// @brief Keep the answers until release()
    void hold() { m_holding = true; }

    /// @brief Send the answers kept, and answer at once from now on
    void release() {
        m_holding = false;
        const auto held = std::exchange(m_held, {});
        for (const auto& [socket, answer] : held) {
            if (socket != nullptr) {  // not dropped by the client
                send(socket, answer);
            }
        }
    }

    /// @brief Stop listening: the server does not answer any more
    void close() { m_server.close(); }

    /// @brief How many requests came
    int requests() const { return static_cast<int>(m_texts.size()); }

    /// @brief How many answers are kept (hold())
    int heldAnswers() const { return static_cast<int>(m_held.size()); }

    /// @brief The texts checked, in the order they came
    QStringList texts() const { return m_texts; }

    /// @brief The language of the last request
    QString lastLanguage() const { return m_lastLanguage; }

private:
    /// @brief Answer a request once it came whole (and take it from what came)
    void receive(QTcpSocket* socket, QByteArray& request) {
        const qsizetype headerEnd = request.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }
        const QByteArray headers = request.left(headerEnd).toLower();
        const QByteArray lengthKey = "content-length:";
        const qsizetype keyPos = headers.indexOf(lengthKey);
        const qsizetype lineEnd = headers.indexOf("\r\n", keyPos);
        const int length =
            keyPos < 0 ? 0
                       : headers.mid(keyPos + lengthKey.size(), lineEnd - keyPos - lengthKey.size())
                             .trimmed()
                             .toInt();
        if (request.size() < headerEnd + 4 + length) {
            return;
        }
        const QUrlQuery form(QString::fromUtf8(request.mid(headerEnd + 4, length)));
        request.remove(0, headerEnd + 4 + length);
        const QString text = form.queryItemValue(QStringLiteral("text"), QUrl::FullyDecoded);
        m_texts.append(text);
        m_lastLanguage = form.queryItemValue(QStringLiteral("language"), QUrl::FullyDecoded);

        QByteArray answer;
        if (m_failureStatus != 0) {
            answer = "HTTP/1.1 " + QByteArray::number(m_failureStatus) +
                     " Error\r\nContent-Type: text/plain\r\nContent-Length: " +
                     QByteArray::number(m_failureBody.size()) + "\r\nConnection: close\r\n\r\n" +
                     m_failureBody;
        } else {
            const QByteArray json =
                m_answer.isEmpty() ? QJsonDocument(answerFor(text)).toJson(QJsonDocument::Compact)
                                   : m_answer;
            answer = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                     QByteArray::number(json.size()) + "\r\nConnection: close\r\n\r\n" + json;
        }
        if (m_holding) {
            m_held.emplace_back(socket, answer);
        } else {
            send(socket, answer);
        }
    }

    /// @brief The issues of a text, as LanguageTool gives them
    QJsonObject answerFor(const QString& text) const {
        QJsonArray matches;
        for (const FakeIssue& issue : m_issues) {
            for (qsizetype at = text.indexOf(issue.phrase); at >= 0;
                 at = text.indexOf(issue.phrase, at + issue.phrase.size())) {
                QJsonArray replacements;
                for (const QString& replacement : issue.replacements) {
                    replacements.append(QJsonObject{{QStringLiteral("value"), replacement}});
                }
                const QJsonObject category{{QStringLiteral("id"), issue.category},
                                           {QStringLiteral("name"), issue.category}};
                const QJsonObject rule{{QStringLiteral("id"), issue.ruleId},
                                       {QStringLiteral("category"), category}};
                matches.append(QJsonObject{
                    {QStringLiteral("message"), issue.message},
                    {QStringLiteral("shortMessage"), QString()},
                    {QStringLiteral("offset"), static_cast<int>(at)},
                    {QStringLiteral("length"), static_cast<int>(issue.phrase.size())},
                    {QStringLiteral("replacements"), replacements},
                    {QStringLiteral("rule"), rule},
                    {QStringLiteral("ignoreForIncompleteSentence"), issue.incompleteSentence}});
            }
        }
        return QJsonObject{
            {QStringLiteral("software"),
             QJsonObject{{QStringLiteral("name"), QStringLiteral("LanguageTool")},
                         {QStringLiteral("version"), QStringLiteral("6.5")}}},
            {QStringLiteral("matches"), matches}};
    }

    static void send(QTcpSocket* socket, const QByteArray& answer) {
        socket->write(answer);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    std::vector<FakeIssue> m_issues;
    int m_failureStatus{0};
    QByteArray m_failureBody;
    QByteArray m_answer;
    bool m_holding{false};
    std::vector<QPair<QPointer<QTcpSocket>, QByteArray>> m_held;
    QStringList m_texts;
    QString m_lastLanguage;
};

}  // namespace kalahari::test
