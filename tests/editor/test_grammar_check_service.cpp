/// @file test_grammar_check_service.cpp
/// @brief Grammar as you type: the texts checked on the writer's LanguageTool server (a
///        stand-in on localhost), a few at a time; a server that does not check them is
///        told once and asked again later

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/grammar_check_service.h>
#include "editor_test_utils.h"
#include "fake_language_tool.h"

#include <QChar>
#include <QList>
#include <QObject>
#include <QStringList>

#include <map>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// What a service told: the answers to its requests and its other signals
class Answers {
public:
    explicit Answers(GrammarCheckService& service) {
        QObject::connect(&service, &GrammarCheckService::textChecked, &m_context,
                         [this](quint64 request, const QList<GrammarError>& errors) {
                             checked[request] = errors;
                         });
        QObject::connect(&service, &GrammarCheckService::textNotChecked, &m_context,
                         [this](quint64 request) { notChecked.append(request); });
        QObject::connect(&service, &GrammarCheckService::serverError, &m_context,
                         [this](const QString& message) { errors.append(message); });
        QObject::connect(&service, &GrammarCheckService::available, &m_context,
                         [this]() { ++available; });
        QObject::connect(&service, &GrammarCheckService::checkingChanged, &m_context,
                         [this]() { ++changes; });
        QObject::connect(&service, &GrammarCheckService::ruleIgnored, &m_context,
                         [this](const QString& ruleId) { ignoredRules.append(ruleId); });
    }

    /// Whether a request was answered with its issues
    bool answered(quint64 request) const { return checked.count(request) == 1; }

    std::map<quint64, QList<GrammarError>> checked;  ///< Issues, by request
    QList<quint64> notChecked;
    QStringList errors;  ///< serverError()
    QStringList ignoredRules;
    int available = 0;
    int changes = 0;  ///< checkingChanged()

private:
    QObject m_context;  ///< The connections go with the answers
};

/// The service checking on the server
std::unique_ptr<GrammarCheckService> serviceOn(const FakeLanguageTool& server) {
    auto service = std::make_unique<GrammarCheckService>();
    service->setServer(server.url());
    return service;
}

}  // namespace

TEST_CASE("Grammar service: the address of the server", "[editor][grammar]") {
    CHECK(GrammarCheckService::endpointFor(QString()).isEmpty());
    CHECK(GrammarCheckService::endpointFor(QStringLiteral("  ")).isEmpty());
    CHECK(GrammarCheckService::endpointFor(QStringLiteral("localhost:8081")) ==
          QStringLiteral("http://localhost:8081/v2/check"));
    CHECK(GrammarCheckService::endpointFor(QStringLiteral(" http://localhost:8081/ ")) ==
          QStringLiteral("http://localhost:8081/v2/check"));
    CHECK(GrammarCheckService::endpointFor(QStringLiteral("http://127.0.0.1:8010/v2")) ==
          QStringLiteral("http://127.0.0.1:8010/v2/check"));
    CHECK(GrammarCheckService::endpointFor(QStringLiteral("https://lt.example.org/v2/check")) ==
          QStringLiteral("https://lt.example.org/v2/check"));
    CHECK(GrammarCheckService::endpointFor(QStringLiteral("http://example.org/lt/check")) ==
          QStringLiteral("http://example.org/lt/check"));

    // No built-in server
    GrammarCheckService service;
    CHECK(service.endpoint().isEmpty());
    CHECK_FALSE(service.isConfigured());
    CHECK(service.isEnabled());
    CHECK_FALSE(service.isActive());

    service.setServer(QStringLiteral("localhost:8081"));
    CHECK(service.endpoint() == QStringLiteral("http://localhost:8081/v2/check"));
    CHECK(service.isConfigured());
    CHECK(service.isActive());
    service.setServer(QStringLiteral("ftp://localhost"));
    CHECK_FALSE(service.isConfigured());
    service.setServer(QStringLiteral("http://"));
    CHECK_FALSE(service.isConfigured());
    service.setServer(QString());
    CHECK_FALSE(service.isConfigured());
}

TEST_CASE("Grammar service: the language LanguageTool checks in", "[editor][grammar]") {
    CHECK(GrammarCheckService::languageFor(QStringLiteral("pl_PL")) == QStringLiteral("pl"));
    CHECK(GrammarCheckService::languageFor(QStringLiteral("pl-PL")) == QStringLiteral("pl"));
    CHECK(GrammarCheckService::languageFor(QStringLiteral("PL")) == QStringLiteral("pl"));
    CHECK(GrammarCheckService::languageFor(QStringLiteral("en_US")) == QStringLiteral("en-US"));
    CHECK(GrammarCheckService::languageFor(QStringLiteral("en-gb")) == QStringLiteral("en-GB"));
    CHECK(GrammarCheckService::languageFor(QStringLiteral("en_IE")) == QStringLiteral("en"));
    CHECK(GrammarCheckService::languageFor(QStringLiteral("de_CH")) == QStringLiteral("de-CH"));
    CHECK(GrammarCheckService::languageFor(QStringLiteral("pt_BR")) == QStringLiteral("pt-BR"));
    CHECK(GrammarCheckService::languageFor(QStringLiteral("fr_FR")) == QStringLiteral("fr"));

    GrammarCheckService service;
    CHECK(service.language() == QStringLiteral("en"));
    service.setLanguage(QStringLiteral("pl_PL"));
    CHECK(service.language() == QStringLiteral("pl"));
}

TEST_CASE("Grammar service: nothing is sent without a server or turned off",
          "[editor][grammar]") {
    FakeLanguageTool server;
    GrammarCheckService service;
    CHECK(service.check(QStringLiteral("The kat sat.")) == 0);

    service.setServer(server.url());
    service.setEnabled(false);
    CHECK_FALSE(service.isEnabled());
    CHECK_FALSE(service.isActive());
    CHECK(service.check(QStringLiteral("The kat sat.")) == 0);
    CHECK(service.pendingChecks() == 0);
    runEventLoop(50);
    CHECK(server.requests() == 0);
}

TEST_CASE("Grammar service: a text is checked on the server", "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("TEST_RULE"),
                     QStringLiteral("Did you mean a cat?"),
                     {QStringLiteral("cat"), QStringLiteral("hat")}});
    auto service = serviceOn(server);
    service->setLanguage(QStringLiteral("en_US"));
    Answers answers(*service);

    // The characters with a meaning in a form arrive as they are
    const QString text = QStringLiteral("The kat & the dog + 50% = ") + QChar(0x0105) +
                         QStringLiteral(".");
    const quint64 request = service->check(text);
    REQUIRE(request != 0);
    CHECK(service->pendingChecks() == 1);
    REQUIRE(waitUntil([&answers, request]() { return answers.answered(request); }, 5000));
    CHECK(service->pendingChecks() == 0);
    CHECK(server.texts() == QStringList{text});
    CHECK(server.lastLanguage() == QStringLiteral("en-US"));

    const QList<GrammarError>& errors = answers.checked[request];
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].startPos == 4);
    CHECK(errors[0].length == 3);
    CHECK(errors[0].text == QStringLiteral("kat"));
    CHECK(errors[0].ruleId == QStringLiteral("TEST_RULE"));
    CHECK(errors[0].message == QStringLiteral("Did you mean a cat?"));
    CHECK(errors[0].suggestions == QStringList({"cat", "hat"}));
    CHECK(errors[0].type == GrammarIssueType::Grammar);
    CHECK(answers.errors.isEmpty());
}

TEST_CASE("Grammar service: spelling and the rules ignored are not reported",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("teh"), QStringLiteral("MORFOLOGIK_RULE_EN_US"),
                     QStringLiteral("Possible spelling mistake found."),
                     {QStringLiteral("the")}, QStringLiteral("TYPOS")});
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    server.addIssue({QStringLiteral("dog"), QStringLiteral("RULE_B")});
    auto service = serviceOn(server);
    Answers answers(*service);

    service->ignoreRule(QStringLiteral("RULE_B"));
    service->ignoreRule(QStringLiteral("RULE_B"));
    service->ignoreRule(QString());
    CHECK(answers.ignoredRules == QStringList{"RULE_B"});
    CHECK(service->isRuleIgnored(QStringLiteral("RULE_B")));
    CHECK_FALSE(service->isRuleIgnored(QStringLiteral("RULE_A")));

    const quint64 request = service->check(QStringLiteral("The kat and teh dog."));
    REQUIRE(waitUntil([&answers, request]() { return answers.answered(request); }, 5000));
    const QList<GrammarError>& errors = answers.checked[request];
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].ruleId == QStringLiteral("RULE_A"));
}

TEST_CASE("Grammar service: an issue is taken as far as it fits the text",
          "[editor][grammar]") {
    FakeLanguageTool server;
    auto service = serviceOn(server);
    Answers answers(*service);
    const QString text = QStringLiteral("The kat sat.");

    SECTION("issues out of the text, empty or without a place are dropped") {
        server.setAnswer(
            R"({"matches":[)"
            R"({"message":"Out","offset":10,"length":3,"rule":{"id":"OUT"}},)"
            R"({"message":"Empty","offset":2,"length":0,"rule":{"id":"EMPTY"}},)"
            R"({"message":"Nowhere","length":2,"rule":{"id":"NOWHERE"}},)"
            R"({"message":"Many","shortMessage":"Short","offset":0,"length":3,)"
            R"("replacements":[{"value":"A"},{"value":"B"},{"value":""},{"value":"C"},)"
            R"({"value":"D"},{"value":"E"},{"value":"F"}],)"
            R"("rule":{"id":"MANY","category":{"id":"STYLE","name":"Style"}}}]})");
        const quint64 request = service->check(text);
        REQUIRE(waitUntil([&answers, request]() { return answers.answered(request); }, 5000));
        const QList<GrammarError>& errors = answers.checked[request];
        REQUIRE(errors.size() == 1);
        CHECK(errors[0].ruleId == QStringLiteral("MANY"));
        CHECK(errors[0].text == QStringLiteral("The"));
        CHECK(errors[0].shortMessage == QStringLiteral("Short"));
        CHECK(errors[0].category == QStringLiteral("Style"));
        CHECK(errors[0].type == GrammarIssueType::Style);
        CHECK(errors[0].suggestions == QStringList({"A", "B", "C", "D", "E"}));
    }

    SECTION("an answer that is not one of LanguageTool has no issues") {
        server.setAnswer("<html>Not here</html>");
        const quint64 request = service->check(text);
        REQUIRE(waitUntil([&answers, request]() { return answers.answered(request); }, 5000));
        CHECK(answers.checked[request].isEmpty());
        CHECK(answers.errors.isEmpty());
    }
}

TEST_CASE("Grammar service: two texts are checked at a time, the others wait",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.hold();
    auto service = serviceOn(server);
    Answers answers(*service);

    const quint64 first = service->check(QStringLiteral("One."));
    const quint64 second = service->check(QStringLiteral("Two."));
    const quint64 third = service->check(QStringLiteral("Three."));
    CHECK(first != 0);
    CHECK(second != first);
    CHECK(third != second);
    REQUIRE(waitUntil([&server]() { return server.heldAnswers() == 2; }, 5000));
    runEventLoop(100);
    CHECK(server.requests() == 2);
    CHECK(service->pendingChecks() == 3);

    // The third is sent when an answer came
    server.release();
    REQUIRE(waitUntil([&answers]() { return answers.checked.size() == 3; }, 5000));
    CHECK(server.requests() == 3);
    CHECK(server.texts().last() == QStringLiteral("Three."));
    CHECK(service->pendingChecks() == 0);
}

TEST_CASE("Grammar service: a request dropped is not answered", "[editor][grammar]") {
    FakeLanguageTool server;
    server.hold();
    auto service = serviceOn(server);
    Answers answers(*service);

    const quint64 sent = service->check(QStringLiteral("Sent."));
    const quint64 kept = service->check(QStringLiteral("Kept."));
    const quint64 waiting = service->check(QStringLiteral("Waiting."));
    REQUIRE(waitUntil([&server]() { return server.heldAnswers() == 2; }, 5000));

    // The one waiting first: dropping one being checked sends the next one waiting
    service->cancel(waiting);
    CHECK(service->pendingChecks() == 2);
    service->cancel(sent);
    CHECK(service->pendingChecks() == 1);
    service->cancel(sent);
    CHECK(service->pendingChecks() == 1);

    server.release();
    REQUIRE(waitUntil([&answers, kept]() { return answers.answered(kept); }, 5000));
    runEventLoop(100);
    CHECK(answers.checked.size() == 1);
    CHECK(answers.notChecked.isEmpty());
    CHECK(answers.errors.isEmpty());
    CHECK(server.requests() == 2);
}

TEST_CASE("Grammar service: a server that does not check is told once and asked again",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.setFailure(500, "Internal error. More about it.\nThe second line.");
    auto service = serviceOn(server);
    service->setRetryDelay(200);
    Answers answers(*service);

    const quint64 first = service->check(QStringLiteral("One."));
    REQUIRE(waitUntil([&answers]() { return !answers.notChecked.isEmpty(); }, 5000));
    CHECK(answers.notChecked == QList<quint64>{first});
    CHECK(answers.errors == QStringList{"Internal error."});
    CHECK(service->isConfigured());
    CHECK_FALSE(service->isActive());
    CHECK(service->check(QStringLiteral("Not sent.")) == 0);

    // Asked again after a while: it still does not check, which is not told again
    REQUIRE(waitUntil([&answers]() { return answers.available == 1; }, 5000));
    CHECK(service->isActive());
    const quint64 second = service->check(QStringLiteral("Two."));
    REQUIRE(waitUntil([&answers, second]() { return answers.notChecked.contains(second); },
                      5000));
    CHECK(answers.errors.size() == 1);

    // It checks again; a failure after that is told again
    REQUIRE(waitUntil([&answers]() { return answers.available == 2; }, 5000));
    server.setFailure(0);
    const quint64 third = service->check(QStringLiteral("Three."));
    REQUIRE(waitUntil([&answers, third]() { return answers.answered(third); }, 5000));
    server.setFailure(503);
    const quint64 fourth = service->check(QStringLiteral("Four."));
    REQUIRE(waitUntil([&answers, fourth]() { return answers.notChecked.contains(fourth); },
                      5000));
    REQUIRE(answers.errors.size() == 2);
    CHECK_FALSE(answers.errors[1].isEmpty());  // Qt's words: the server said nothing
    CHECK(server.requests() == 4);
}

TEST_CASE("Grammar service: a server that is not there is told", "[editor][grammar]") {
    FakeLanguageTool server;
    const QString address = server.url();
    server.close();
    GrammarCheckService service;
    service.setServer(address);
    Answers answers(service);

    const quint64 request = service.check(QStringLiteral("One."));
    REQUIRE(waitUntil([&answers, request]() { return answers.notChecked.contains(request); },
                      10000));
    REQUIRE(answers.errors.size() == 1);
    CHECK_FALSE(answers.errors[0].isEmpty());
}

TEST_CASE("Grammar service: a text too long for the server has no issues",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.setFailure(413);
    auto service = serviceOn(server);
    Answers answers(*service);

    const quint64 request = service->check(QStringLiteral("A very long chapter."));
    REQUIRE(waitUntil([&answers, request]() { return answers.answered(request); }, 5000));
    CHECK(answers.checked[request].isEmpty());
    CHECK(answers.errors.isEmpty());
    CHECK(service->isActive());
}

TEST_CASE("Grammar service: another server, language or switch drops the requests",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.hold();
    auto service = serviceOn(server);
    Answers answers(*service);
    const quint64 request = service->check(QStringLiteral("One."));
    REQUIRE(waitUntil([&server]() { return server.heldAnswers() == 1; }, 5000));

    SECTION("another language") {
        service->setLanguage(QStringLiteral("de_DE"));
        CHECK(answers.changes == 1);
        service->setLanguage(QStringLiteral("de-DE"));  // the same
        CHECK(answers.changes == 1);
    }

    SECTION("turned off") {
        service->setEnabled(false);
        CHECK(answers.changes == 1);
        service->setEnabled(false);
        CHECK(answers.changes == 1);
        CHECK(service->check(QStringLiteral("Two.")) == 0);
    }

    SECTION("another server") {
        service->setServer(QStringLiteral("localhost:1"));
        CHECK(answers.changes == 1);
        service->setServer(QStringLiteral("http://localhost:1/v2/check"));  // the same
        CHECK(answers.changes == 1);
    }

    CHECK(service->pendingChecks() == 0);
    server.release();
    runEventLoop(100);
    CHECK_FALSE(answers.answered(request));
    CHECK(answers.notChecked.isEmpty());
}

TEST_CASE("Grammar service: closed while texts are checked", "[editor][grammar]") {
    FakeLanguageTool server;
    server.hold();
    auto service = serviceOn(server);
    service->check(QStringLiteral("One."));
    service->check(QStringLiteral("Two."));
    service->check(QStringLiteral("Three."));
    REQUIRE(waitUntil([&server]() { return server.heldAnswers() == 2; }, 5000));

    service.reset();
    server.release();
    runEventLoop(50);
    CHECK(server.requests() == 2);
}

TEST_CASE("Grammar service: an issue", "[editor][grammar]") {
    const GrammarError none;
    CHECK(none.startPos == 0);
    CHECK(none.length == 0);
    CHECK(none.type == GrammarIssueType::Grammar);
    CHECK(none.suggestions.isEmpty());
    CHECK_FALSE(none.ignoreForIncompleteSentence);

    GrammarError first(10, 5, QStringLiteral("worng"));
    first.ruleId = QStringLiteral("RULE1");
    GrammarError same(10, 5, QStringLiteral("worng"));
    same.ruleId = QStringLiteral("RULE1");
    same.message = QStringLiteral("Another message");
    GrammarError other(10, 5, QStringLiteral("worng"));
    other.ruleId = QStringLiteral("RULE2");
    CHECK(first == same);
    CHECK_FALSE(first == other);
}
