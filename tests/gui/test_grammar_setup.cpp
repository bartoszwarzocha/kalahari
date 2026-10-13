/// @file test_grammar_setup.cpp
/// @brief The preparation of grammar checking: Java and LanguageTool downloaded from sites on
///        localhost, checked, unpacked into a folder of the test and started once with the
///        stand-in for Java; its window

#include <catch2/catch_test_macros.hpp>

#include "../editor/editor_test_utils.h"
#include "grammar_test_utils.h"
#include "kalahari/gui/dialogs/grammar_setup_dialog.h"
#include "kalahari/gui/grammar_components.h"
#include "kalahari/gui/grammar_setup.h"
#include "kalahari/gui/language_tool_server.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QLabel>
#include <QPushButton>
#include <QTcpServer>

using namespace kalahari::gui;
using namespace kalahari::test;
using kalahari::gui::dialogs::GrammarSetupDialog;

namespace {

/// How long a preparation may take (ms)
constexpr int PREPARE_MS = 60000;

/// A free port on this computer
int freePort() {
    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost));
    return server.serverPort();
}

/// The parts of the files being downloaded
QStringList partFiles() {
    return QDir(GrammarComponents::archiveFolder()).entryList({QStringLiteral("*.part")});
}

/// Whether a label of a window shows a text
bool shows(const QWidget& window, const QString& text) {
    for (const QLabel* label : window.findChildren<QLabel*>()) {
        if (label->text() == text) {
            return true;
        }
    }
    return false;
}

/// The sites of Java and LanguageTool, a server and its preparation, in a folder of the test
class Preparation {
public:
    Preparation() : java(javaZip()), languageTool(languageToolZip()), setup(&server) {
        site.add(javaFileName(), java);
        site.add(languageToolFileName(), languageTool);
        server.setPort(freePort());
        setup.setJavaDownload(downloadOf(site, javaFileName(), java));
        setup.setLanguageToolDownload(downloadOf(site, languageToolFileName(), languageTool));
        setup.setLanguage(QStringLiteral("en_US"));
        QObject::connect(&setup, &GrammarSetup::progress, &setup, [this]() {
            if (steps.isEmpty() || steps.last() != setup.step()) {
                steps.append(setup.step());
            }
        });
        QObject::connect(&setup, &GrammarSetup::finished, &setup, [this]() { ++finished; });
    }

    /// Start, and wait for the end
    void run() {
        setup.start();
        REQUIRE(waitUntil([this]() { return !setup.isRunning(); }, PREPARE_MS));
    }

    TemporaryGrammarFolder folder;
    FakeJavaLibraries libraries;
    FakeDownloads site;
    QByteArray java;
    QByteArray languageTool;
    LanguageToolServer server;
    GrammarSetup setup;
    QList<GrammarSetup::Step> steps;
    int finished = 0;
};

}  // namespace

TEST_CASE("Grammar setup: Java and LanguageTool downloaded, checked, unpacked and started once",
          "[gui][grammar][setup]") {
    Preparation preparation;
    GrammarSetup& setup = preparation.setup;
    CHECK(setup.state() == GrammarSetup::State::Idle);
    CHECK(GrammarSetup::canWrite());
    CHECK(GrammarSetup::freeBytes() > 0);
    CHECK_FALSE(GrammarSetup::disk().isEmpty());
    CHECK_FALSE(QDir(preparation.folder.path()).exists());  // canWrite() leaves nothing

    preparation.run();
    CHECK(setup.state() == GrammarSetup::State::Finished);
    CHECK(setup.failure() == GrammarSetup::Failure::None);
    CHECK(preparation.finished == 1);
    CHECK(preparation.steps ==
          QList<GrammarSetup::Step>({GrammarSetup::Step::Java, GrammarSetup::Step::LanguageTool,
                                     GrammarSetup::Step::Unpack, GrammarSetup::Step::FirstRun}));
    for (const GrammarSetup::Step step :
         {GrammarSetup::Step::Java, GrammarSetup::Step::LanguageTool, GrammarSetup::Step::Unpack,
          GrammarSetup::Step::FirstRun}) {
        CHECK(setup.isDone(step));
    }
    CHECK(setup.percent() == 100);
    CHECK(setup.received(GrammarSetup::Step::Java) == preparation.java.size());

    // Unpacked into the folder of the program, the files downloaded removed
    const GrammarComponents::JavaInfo java =
        GrammarComponents::inspectJava(GrammarComponents::downloadedJava());
    CHECK(java.feature == 21);
    CHECK(java.vendor == QStringLiteral("Eclipse Temurin"));
    CHECK(GrammarComponents::languageToolDownloaded());
    CHECK(GrammarComponents::javaArchive().isEmpty());
    CHECK(GrammarComponents::languageToolArchive().isEmpty());

    // The server runs with them
    CHECK(preparation.server.state() == LanguageToolServer::State::Running);
    CHECK(preparation.server.java() == GrammarComponents::downloadedJava());
    CHECK(preparation.server.folder() == GrammarComponents::languageToolFolder());
    CHECK_FALSE(preparation.server.isExternal());

    // Again: nothing is downloaded again
    preparation.server.stop();
    preparation.run();
    CHECK(setup.state() == GrammarSetup::State::Finished);
    CHECK(preparation.site.requests(javaFileName()) == 1);
    CHECK(preparation.site.requests(languageToolFileName()) == 1);
    preparation.server.stop();
}

TEST_CASE("Grammar setup: a damaged file is removed, and the next try goes on from there",
          "[gui][grammar][setup]") {
    Preparation preparation;
    GrammarSetup& setup = preparation.setup;
    preparation.site.add(languageToolFileName(), "not the file of LanguageTool");

    preparation.run();
    CHECK(setup.state() == GrammarSetup::State::Failed);
    CHECK(setup.failure() == GrammarSetup::Failure::Damaged);
    CHECK(setup.step() == GrammarSetup::Step::LanguageTool);
    CHECK(setup.errorUrl() == preparation.site.url(languageToolFileName()));
    CHECK(setup.errorText().startsWith(QStringLiteral("SHA-256: ")));
    CHECK(GrammarComponents::languageToolArchive().isEmpty());
    CHECK(partFiles().isEmpty());
    CHECK(GrammarComponents::javaDownloaded());

    // The right file now: only LanguageTool is downloaded again
    preparation.site.add(languageToolFileName(), preparation.languageTool);
    preparation.run();
    CHECK(setup.state() == GrammarSetup::State::Finished);
    CHECK(preparation.site.requests(javaFileName()) == 1);
    CHECK(preparation.site.requests(languageToolFileName()) == 2);
    preparation.server.stop();
}

TEST_CASE("Grammar setup: why it stopped", "[gui][grammar][setup]") {
    Preparation preparation;
    GrammarSetup& setup = preparation.setup;

    SECTION("the server of a file does not answer as it should") {
        preparation.site.setStatus(javaFileName(), 503);
        preparation.run();
        CHECK(setup.failure() == GrammarSetup::Failure::ServerDown);
        CHECK(setup.step() == GrammarSetup::Step::Java);
        CHECK(setup.errorUrl() == preparation.site.url(javaFileName()));
        CHECK(partFiles().isEmpty());
    }

    SECTION("no connection: the name of the server is not found") {
        GrammarDownload java = downloadOf(preparation.site, javaFileName(), preparation.java);
        java.url = QUrl(QStringLiteral("http://kalahari-test.invalid/java.zip"));
        setup.setJavaDownload(java);
        preparation.run();
        CHECK(setup.failure() == GrammarSetup::Failure::NoInternet);
    }

    SECTION("the Adoptium service has no Java for this system") {
        setup.setJavaDownload(GrammarDownload());
        preparation.site.add(QStringLiteral("assets"), "[]");
        setup.setJavaInfoUrl(preparation.site.url(QStringLiteral("assets")));
        preparation.run();
        CHECK(setup.failure() == GrammarSetup::Failure::ServerDown);
        CHECK(setup.errorText() == QStringLiteral("No Java 21 for this system"));
        CHECK(setup.errorUrl() == preparation.site.url(QStringLiteral("assets")));
    }

    SECTION("the folder cannot be written") {
        QFile file(QDir(preparation.folder.root()).filePath(QStringLiteral("a file")));
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.close();
        GrammarComponents::setFolder(QDir(file.fileName()).filePath(QStringLiteral("grammar")));
        CHECK_FALSE(GrammarSetup::canWrite());
        preparation.run();
        CHECK(setup.failure() == GrammarSetup::Failure::NoFolder);
    }

    SECTION("a file that cannot be unpacked") {
        const QByteArray broken = "PK not a zip";
        preparation.site.add(languageToolFileName(), broken);
        setup.setLanguageToolDownload(downloadOf(preparation.site, languageToolFileName(), broken));
        preparation.run();
        CHECK(setup.failure() == GrammarSetup::Failure::NotUnpacked);
        CHECK(setup.step() == GrammarSetup::Step::Unpack);
        CHECK_FALSE(GrammarComponents::languageToolDownloaded());
    }

    SECTION("Java does not start LanguageTool") {
        ScopedEnvironment fail("KALAHARI_FAKE_JAVA_FAIL", "1");
        preparation.run();
        CHECK(setup.failure() == GrammarSetup::Failure::NotStarted);
        CHECK(setup.step() == GrammarSetup::Step::FirstRun);
        CHECK(setup.errorText().contains(QStringLiteral("Could not find or load main class")));
    }
    CHECK(setup.state() == GrammarSetup::State::Failed);
    CHECK(preparation.finished == 1);
}

TEST_CASE("Grammar setup: broken off, the files downloaded whole stay for the next try",
          "[gui][grammar][setup]") {
    Preparation preparation;
    GrammarSetup& setup = preparation.setup;
    preparation.site.setStatus(languageToolFileName(), 500);
    preparation.run();
    REQUIRE(setup.step() == GrammarSetup::Step::LanguageTool);
    REQUIRE_FALSE(GrammarComponents::javaArchive().isEmpty());

    preparation.site.setStatus(languageToolFileName(), 0);
    preparation.site.hold();
    setup.start();
    REQUIRE(waitUntil([&preparation]() { return preparation.site.heldAnswers() == 1; }));
    CHECK(setup.step() == GrammarSetup::Step::LanguageTool);
    setup.cancel();
    CHECK(setup.state() == GrammarSetup::State::Cancelled);
    CHECK(preparation.finished == 2);
    CHECK_FALSE(GrammarComponents::javaArchive().isEmpty());
    CHECK(partFiles().isEmpty());
    preparation.site.release();
    runEventLoop(100);
    CHECK(setup.state() == GrammarSetup::State::Cancelled);
}

TEST_CASE("Grammar setup window: what is downloaded, then the steps, in the background too",
          "[gui][grammar][setup]") {
    Preparation preparation;
    GrammarSetup& setup = preparation.setup;
    GrammarSetupDialog dialog(&setup);
    CHECK(dialog.page() == GrammarSetupDialog::Page::Plan);
    CHECK(dialog.windowTitle() == QStringLiteral("Preparing Grammar Checking"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("&Download and Prepare"));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(shows(dialog, QStringLiteral("about %1").arg(GrammarComponents::sizeText(300'000'000))));
    CHECK(shows(dialog, QStringLiteral("LanguageTool 6.6")));

    // Started: the steps as they go
    preparation.site.hold();
    dialog.acceptButton()->click();
    CHECK(setup.isRunning());
    CHECK(dialog.page() == GrammarSetupDialog::Page::Progress);
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Download in &Background"));
    CHECK_FALSE(dialog.stopButton()->isHidden());
    CHECK(dialog.cancelButton()->isHidden());

    // In the background: the window closes, the preparation goes on
    dialog.acceptButton()->click();
    CHECK(dialog.result() == QDialog::Accepted);
    CHECK(setup.isRunning());
    preparation.site.release();
    REQUIRE(waitUntil([&setup]() { return !setup.isRunning(); }, PREPARE_MS));
    CHECK(setup.state() == GrammarSetup::State::Finished);

    // Shown again: the end
    GrammarSetupDialog again(&setup);
    CHECK(again.page() == GrammarSetupDialog::Page::Done);
    CHECK(again.acceptButton()->text() == QStringLiteral("&Finish"));
    CHECK(shows(again, QStringLiteral("Grammar checking is ready and turned on.")));
    again.acceptButton()->click();
    CHECK(again.result() == QDialog::Accepted);
    preparation.server.stop();
}

TEST_CASE("Grammar setup window: why it stopped, and a new try", "[gui][grammar][setup]") {
    Preparation preparation;
    GrammarSetup& setup = preparation.setup;
    preparation.site.setStatus(javaFileName(), 503);
    GrammarSetupDialog dialog(&setup);
    dialog.acceptButton()->click();
    REQUIRE(waitUntil([&dialog]() { return dialog.page() == GrammarSetupDialog::Page::Failed; },
                      PREPARE_MS));
    CHECK(dialog.message() ==
          QStringLiteral("Java could not be downloaded: the server 127.0.0.1 does not answer. "
                         "Try again later."));
    CHECK(dialog.details().contains(preparation.site.url(javaFileName()).toString()));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("&Try Again"));
    CHECK(dialog.cancelButton()->text() == QStringLiteral("Close"));
    CHECK_FALSE(dialog.copyButton()->isHidden());
    CHECK(dialog.stopButton()->isHidden());

    // Tried again once the server answers
    preparation.site.setStatus(javaFileName(), 0);
    dialog.acceptButton()->click();
    CHECK(dialog.page() == GrammarSetupDialog::Page::Progress);
    REQUIRE(waitUntil([&dialog]() { return dialog.page() == GrammarSetupDialog::Page::Done; },
                      PREPARE_MS));
    preparation.server.stop();
}

TEST_CASE("Grammar setup window: Cancel breaks the preparation off, closing the window does not",
          "[gui][grammar][setup]") {
    Preparation preparation;
    GrammarSetup& setup = preparation.setup;
    preparation.site.hold();
    GrammarSetupDialog dialog(&setup);
    dialog.acceptButton()->click();
    REQUIRE(setup.isRunning());

    // Esc or the window's close button
    dialog.reject();
    CHECK(dialog.result() == QDialog::Rejected);
    CHECK(setup.isRunning());

    // Shown again, at the steps: Cancel breaks it off and closes the window
    GrammarSetupDialog again(&setup);
    CHECK(again.page() == GrammarSetupDialog::Page::Progress);
    again.stopButton()->click();
    CHECK(setup.state() == GrammarSetup::State::Cancelled);
    CHECK(again.result() == QDialog::Rejected);
    preparation.site.release();
}
