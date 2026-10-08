/// @file test_grammar_check_service.cpp
/// @brief Unit tests for GrammarCheckService (OpenSpec #00042 Phase 6.14-6.17)

#include <catch2/catch_test_macros.hpp>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/editor/book_editor.h"

using namespace kalahari::editor;

// ============================================================================
// Construction and Basic State
// ============================================================================

TEST_CASE("GrammarCheckService construction", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("initial state") {
        REQUIRE(service.isEnabled());
        REQUIRE(service.language() == "en-US");
        // No built-in server: nothing is sent until the user sets one
        REQUIRE(service.apiEndpoint().isEmpty());
        REQUIRE_FALSE(service.isConfigured());
        REQUIRE_FALSE(service.isActive());
        REQUIRE_FALSE(service.hasPendingRequests());
        REQUIRE(service.ignoredRules().isEmpty());
    }
}

TEST_CASE("GrammarCheckService enable/disable", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("disable") {
        service.setEnabled(false);
        REQUIRE_FALSE(service.isEnabled());
    }

    SECTION("enable") {
        service.setEnabled(false);
        service.setEnabled(true);
        REQUIRE(service.isEnabled());
    }
}

// ============================================================================
// Language Configuration
// ============================================================================

TEST_CASE("GrammarCheckService language configuration", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("set language") {
        service.setLanguage("pl-PL");
        REQUIRE(service.language() == "pl-PL");
    }

    SECTION("set language empty") {
        service.setLanguage("");
        REQUIRE(service.language().isEmpty());
    }

    SECTION("various language codes") {
        service.setLanguage("de-DE");
        REQUIRE(service.language() == "de-DE");

        service.setLanguage("fr-FR");
        REQUIRE(service.language() == "fr-FR");
    }
}

// ============================================================================
// API Endpoint Configuration
// ============================================================================

TEST_CASE("GrammarCheckService API endpoint", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("set custom endpoint") {
        service.setApiEndpoint("http://localhost:8081/v2/check");
        REQUIRE(service.apiEndpoint() == "http://localhost:8081/v2/check");
    }

    SECTION("set empty endpoint") {
        service.setApiEndpoint("");
        REQUIRE(service.apiEndpoint().isEmpty());
    }

    SECTION("a server address alone means its check endpoint") {
        service.setApiEndpoint("http://localhost:8081");
        REQUIRE(service.apiEndpoint() == "http://localhost:8081/v2/check");
        service.setApiEndpoint(" http://127.0.0.1:8010/ ");
        REQUIRE(service.apiEndpoint() == "http://127.0.0.1:8010/v2/check");
        REQUIRE(service.isConfigured());
        REQUIRE(service.isActive());
    }

    SECTION("an address that is not http or https is not a server") {
        service.setApiEndpoint("localhost");
        REQUIRE_FALSE(service.isConfigured());
        service.setApiEndpoint("ftp://localhost:8081");
        REQUIRE_FALSE(service.isConfigured());
    }
}

// ============================================================================
// Checking against a LanguageTool server (a fake one on localhost)
// ============================================================================

namespace {

/// A LanguageTool stand-in: answers every request with one match and keeps the request
class FakeLanguageTool {
public:
    FakeLanguageTool() {
        REQUIRE(m_server.listen(QHostAddress::LocalHost));
        QObject::connect(&m_server, &QTcpServer::newConnection, [this]() {
            QTcpSocket* socket = m_server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                m_request += socket->readAll();
                const qsizetype headerEnd = m_request.indexOf("\r\n\r\n");
                if (headerEnd < 0) {
                    return;
                }
                const QByteArray lengthKey = "content-length:";
                const QByteArray headers = m_request.left(headerEnd).toLower();
                const qsizetype keyPos = headers.indexOf(lengthKey);
                const qsizetype lineEnd = headers.indexOf("\r\n", keyPos);
                const int length =
                    headers.mid(keyPos + lengthKey.size(), lineEnd - keyPos - lengthKey.size())
                        .trimmed()
                        .toInt();
                if (m_request.size() < headerEnd + 4 + length) {
                    return;
                }
                m_body = m_request.mid(headerEnd + 4, length);
                ++m_requests;
                const QByteArray json =
                    R"({"matches":[{"message":"Possible typo","shortMessage":"Typo",)"
                    R"("offset":4,"length":3,"replacements":[{"value":"cat"}],)"
                    R"("rule":{"id":"TEST_RULE","category":{"id":"GRAMMAR","name":"Grammar"}}}]})";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                              "Content-Length: " + QByteArray::number(json.size()) +
                              "\r\nConnection: close\r\n\r\n" + json);
                socket->disconnectFromHost();
            });
        });
    }

    QString url() const { return QString("http://127.0.0.1:%1").arg(m_server.serverPort()); }
    int requests() const { return m_requests; }
    QUrlQuery body() const { return QUrlQuery(QString::fromUtf8(m_body)); }

private:
    QTcpServer m_server;
    QByteArray m_request;
    QByteArray m_body;
    int m_requests{0};
};

/// Runs the event loop until the condition holds or the time is up
template <typename Condition>
bool waitFor(Condition condition, int timeoutMs = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return condition();
}

/// Talks to localhost directly, whatever proxy the environment sets
struct NoProxy {
    QNetworkProxy previous = QNetworkProxy::applicationProxy();
    NoProxy() { QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy); }
    ~NoProxy() { QNetworkProxy::setApplicationProxy(previous); }
};

}  // namespace

TEST_CASE("GrammarCheckService checks text on the server it is given",
          "[editor][grammar_check]") {
    NoProxy noProxy;
    FakeLanguageTool server;
    GrammarCheckService service;
    service.setLanguage("en-US");
    service.setApiEndpoint(server.url());

    QList<GrammarError> result;
    int checkedIndex = -2;
    QObject::connect(&service, &GrammarCheckService::paragraphChecked,
                     [&](int index, const QList<GrammarError>& errors) {
                         checkedIndex = index;
                         result = errors;
                     });

    service.checkTextAsync("The kat sat.", 3);
    REQUIRE(waitFor([&]() { return checkedIndex == 3; }));

    REQUIRE(server.requests() == 1);
    REQUIRE(server.body().queryItemValue("text", QUrl::FullyDecoded) == "The kat sat.");
    REQUIRE(server.body().queryItemValue("language") == "en-US");

    REQUIRE(result.size() == 1);
    REQUIRE(result.first().startPos == 4);
    REQUIRE(result.first().length == 3);
    REQUIRE(result.first().ruleId == "TEST_RULE");
    REQUIRE(result.first().suggestions == QStringList{"cat"});
}

TEST_CASE("GrammarCheckService sends nothing without a server", "[editor][grammar_check]") {
    GrammarCheckService service;
    REQUIRE_FALSE(service.isConfigured());

    bool checked = false;
    QList<GrammarError> result{GrammarError(0, 1, "x")};
    QObject::connect(&service, &GrammarCheckService::paragraphChecked,
                     [&](int, const QList<GrammarError>& errors) {
                         checked = true;
                         result = errors;
                     });

    service.checkTextAsync("The kat sat.", 0);
    REQUIRE(checked);
    REQUIRE(result.isEmpty());
    REQUIRE_FALSE(service.hasPendingRequests());
}

TEST_CASE("GrammarCheckService stops sending when the server is removed",
          "[editor][grammar_check]") {
    NoProxy noProxy;
    FakeLanguageTool server;
    GrammarCheckService service;
    service.setApiEndpoint(server.url());
    service.setApiEndpoint("");

    service.checkTextAsync("The kat sat.", 0);
    QCoreApplication::processEvents();
    REQUIRE_FALSE(service.hasPendingRequests());
    REQUIRE(server.requests() == 0);
}

// ============================================================================
// Rate Limiting Configuration
// ============================================================================

TEST_CASE("GrammarCheckService rate limiting", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("default rate limit") {
        REQUIRE(service.rateLimitMs() == 500);
    }

    SECTION("set rate limit") {
        service.setRateLimitMs(1000);
        REQUIRE(service.rateLimitMs() == 1000);
    }

    SECTION("set low rate limit") {
        // Note: implementation may enforce a minimum rate limit (e.g., 100ms)
        service.setRateLimitMs(100);
        REQUIRE(service.rateLimitMs() == 100);
    }
}

TEST_CASE("GrammarCheckService debounce", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("default debounce") {
        REQUIRE(service.debounceMs() == 1000);
    }

    SECTION("set debounce") {
        service.setDebounceMs(2000);
        REQUIRE(service.debounceMs() == 2000);
    }
}

// ============================================================================
// Category Configuration
// ============================================================================

TEST_CASE("GrammarCheckService category configuration", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("enabled categories initially empty") {
        REQUIRE(service.enabledCategories().isEmpty());
    }

    SECTION("disabled categories initially empty") {
        REQUIRE(service.disabledCategories().isEmpty());
    }

    SECTION("set enabled categories") {
        QStringList cats = {"GRAMMAR", "PUNCTUATION"};
        service.setEnabledCategories(cats);
        auto enabled = service.enabledCategories();
        REQUIRE(enabled.size() == 2);
        REQUIRE(enabled.contains("GRAMMAR"));
        REQUIRE(enabled.contains("PUNCTUATION"));
    }

    SECTION("set disabled categories") {
        QStringList cats = {"STYLE", "REDUNDANCY"};
        service.setDisabledCategories(cats);
        auto disabled = service.disabledCategories();
        REQUIRE(disabled.size() == 2);
        REQUIRE(disabled.contains("STYLE"));
        REQUIRE(disabled.contains("REDUNDANCY"));
    }

    SECTION("clear categories") {
        service.setEnabledCategories({"CAT1", "CAT2"});
        service.setEnabledCategories({});
        REQUIRE(service.enabledCategories().isEmpty());
    }
}

// ============================================================================
// Rule Ignore
// ============================================================================

TEST_CASE("GrammarCheckService ignore rules", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("ignore rule") {
        service.ignoreRule("COMMA_BEFORE_AND");
        REQUIRE(service.isRuleIgnored("COMMA_BEFORE_AND"));
        REQUIRE(service.ignoredRules().contains("COMMA_BEFORE_AND"));
    }

    SECTION("multiple ignored rules") {
        service.ignoreRule("RULE1");
        service.ignoreRule("RULE2");
        service.ignoreRule("RULE3");
        REQUIRE(service.ignoredRules().size() == 3);
    }

    SECTION("clear ignored rules") {
        service.ignoreRule("RULE1");
        service.ignoreRule("RULE2");
        service.clearIgnoredRules();
        REQUIRE(service.ignoredRules().isEmpty());
        REQUIRE_FALSE(service.isRuleIgnored("RULE1"));
    }

    SECTION("duplicate ignore is no-op") {
        service.ignoreRule("SAME_RULE");
        service.ignoreRule("SAME_RULE");
        REQUIRE(service.ignoredRules().size() == 1);
    }
}

// ============================================================================
// Document Integration
// ============================================================================

TEST_CASE("GrammarCheckService BookEditor integration", "[editor][grammar_check]") {
    // Note: Service must be declared AFTER editor to ensure correct destruction order

    SECTION("set editor") {
        BookEditor editor;
        GrammarCheckService service;
        service.setBookEditor(&editor);
        service.setBookEditor(nullptr);  // Disconnect before editor is destroyed
    }

    SECTION("set null editor") {
        BookEditor editor;
        GrammarCheckService service;
        service.setBookEditor(&editor);
        service.setBookEditor(nullptr);
        // Should not crash
    }

    SECTION("change editor") {
        BookEditor editor1;
        BookEditor editor2;
        GrammarCheckService service;
        service.setBookEditor(&editor1);
        service.setBookEditor(&editor2);
        service.setBookEditor(nullptr);  // Disconnect before editors are destroyed
    }
}

// ============================================================================
// Cached Results
// ============================================================================

TEST_CASE("GrammarCheckService cached results", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("errorsForParagraph returns empty for uncached") {
        auto errors = service.errorsForParagraph(0);
        REQUIRE(errors.isEmpty());
    }

    SECTION("errorsForParagraph returns empty for negative index") {
        auto errors = service.errorsForParagraph(-1);
        REQUIRE(errors.isEmpty());
    }
}

// ============================================================================
// Cancel Operations
// ============================================================================

TEST_CASE("GrammarCheckService cancel operations", "[editor][grammar_check]") {
    GrammarCheckService service;

    SECTION("cancel with no pending requests") {
        service.cancelPendingChecks();
        REQUIRE_FALSE(service.hasPendingRequests());
    }
}

// ============================================================================
// GrammarError struct
// ============================================================================

TEST_CASE("GrammarError struct", "[editor][grammar_check]") {
    SECTION("default construction") {
        GrammarError error;
        REQUIRE(error.startPos == 0);
        REQUIRE(error.length == 0);
        REQUIRE(error.text.isEmpty());
        REQUIRE(error.message.isEmpty());
        REQUIRE(error.shortMessage.isEmpty());
        REQUIRE(error.ruleId.isEmpty());
        REQUIRE(error.category.isEmpty());
        REQUIRE(error.type == GrammarIssueType::Grammar);
        REQUIRE(error.suggestions.isEmpty());
    }

    SECTION("parameterized construction") {
        GrammarError error(10, 5, "worng");
        REQUIRE(error.startPos == 10);
        REQUIRE(error.length == 5);
        REQUIRE(error.text == "worng");
    }

    SECTION("equality comparison") {
        GrammarError error1(10, 5, "text");
        error1.ruleId = "RULE1";

        GrammarError error2(10, 5, "text");
        error2.ruleId = "RULE1";

        GrammarError error3(10, 5, "text");
        error3.ruleId = "RULE2";

        REQUIRE(error1 == error2);
        REQUIRE_FALSE(error1 == error3);
    }
}

// ============================================================================
// GrammarIssueType enum
// ============================================================================

TEST_CASE("GrammarIssueType enum values", "[editor][grammar_check]") {
    SECTION("enum values exist") {
        GrammarIssueType grammar = GrammarIssueType::Grammar;
        GrammarIssueType style = GrammarIssueType::Style;
        GrammarIssueType typography = GrammarIssueType::Typography;
        GrammarIssueType spelling = GrammarIssueType::Spelling;
        GrammarIssueType other = GrammarIssueType::Other;

        REQUIRE(grammar != style);
        REQUIRE(style != typography);
        REQUIRE(typography != spelling);
        REQUIRE(spelling != other);
    }
}
