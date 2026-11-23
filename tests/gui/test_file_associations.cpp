// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_file_associations.cpp
/// @brief The files of Kalahari and the program the system opens them with: the types the
/// installers give to the program, what the program writes to the registry of Windows and runs
/// on Linux, and the page Settings > Files > File Associations

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/utils/file_associations.h"

#include <QCoreApplication>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTranslator>

#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "kalahari_tests/test_support/window_parts.h"

using namespace kalahari;
using gui::FileAssociation;
using State = gui::FileAssociation::State;

namespace {

QString sourceFile(const QString& path)
{
    QFile file(QStringLiteral(KALAHARI_SOURCE_DIR "/") + path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

/// The value @p name of key @p key that windowsRegistration() gives
QString registryValue(const std::vector<gui::WindowsRegistryValue>& values, const QString& key,
                      const QString& name = {})
{
    for (const auto& value : values) {
        if (value.key == key && value.name == name) {
            return value.value;
        }
    }
    FAIL("No value " << name.toStdString() << " of " << key.toStdString());
    return {};
}

/// A system the page talks to instead of the real one
class FakeSystem final : public gui::FileAssociationSystem {
public:
    std::map<QString, FileAssociation> states;  ///< By extension
    bool settingsWindow = false;
    bool settingsOpen = true;
    int settingsOpened = 0;
    int associations = 0;
    QString problem;     ///< What associate() reports
    bool later = false;  ///< associate() keeps done for the test to call
    std::function<void(const QString&)> pending;

    FileAssociation current(const gui::KalahariFileType& type) const override
    {
        const auto found = states.find(type.extension);
        return found == states.end() ? FileAssociation{State::NoProgram, {}} : found->second;
    }

    void associate(std::function<void(const QString&)> done) override
    {
        ++associations;
        if (problem.isEmpty() && !settingsWindow) {
            for (const auto& type : gui::kalahariFileTypes()) {
                states[type.extension] = {State::Kalahari, {}};
            }
        }
        if (later) {
            pending = std::move(done);
        } else {
            done(problem);
        }
    }

    bool hasSettingsWindow() const override { return settingsWindow; }

    bool openSettingsWindow() override
    {
        ++settingsOpened;
        return settingsOpen;
    }
};

/// The fake system for one test; the real one comes back after it
struct FakeSystemGuard {
    FakeSystem* system;
    FakeSystemGuard()
    {
        auto fake = std::make_unique<FakeSystem>();
        system = fake.get();
        gui::setFileAssociationSystemForTests(std::move(fake));
    }
    ~FakeSystemGuard() { gui::setFileAssociationSystemForTests(nullptr); }
    FakeSystemGuard(const FakeSystemGuard&) = delete;
    FakeSystemGuard& operator=(const FakeSystemGuard&) = delete;
};

QString labelText(const QWidget& page, const QString& name)
{
    const auto* label = page.findChild<QLabel*>(name);
    REQUIRE(label != nullptr);
    return label->text();
}

}  // namespace

TEST_CASE("File associations: the books and the documents of Kalahari", "[gui][associations]")
{
    const auto& types = gui::kalahariFileTypes();
    REQUIRE(types.size() == 2);
    CHECK(types[0].extension == QStringLiteral("klh"));
    CHECK(types[0].mimeType == QStringLiteral("application/x-kalahari-book"));
    CHECK(types[0].progId == QStringLiteral("Kalahari.Book"));
    CHECK(types[0].contentType == QStringLiteral("io.github.bartoszwarzocha.kalahari.book"));
    CHECK(types[1].extension == QStringLiteral("kchapter"));
    CHECK(types[1].mimeType == QStringLiteral("application/x-kalahari-document"));
    CHECK(types[1].progId == QStringLiteral("Kalahari.Document"));
    CHECK(types[1].contentType == QStringLiteral("io.github.bartoszwarzocha.kalahari.document"));
}

TEST_CASE("File associations: the installers give the same files to the program",
          "[gui][associations]")
{
    const QString mime = sourceFile(QStringLiteral("packaging/linux/kalahari-mime.xml"));
    const QString desktop = sourceFile(QStringLiteral("packaging/linux/kalahari.desktop"));
    const QString plist = sourceFile(QStringLiteral("packaging/macos/Info.plist.in"));
    const QString nsi = sourceFile(QStringLiteral("packaging/windows/kalahari.nsi"));
    QStringList mimeTypes;
    for (const auto& type : gui::kalahariFileTypes()) {
        INFO(type.extension.toStdString());
        // Linux: the type with its extension
        const QRegularExpression mimeType(
            QStringLiteral("<mime-type type=\"%1\">.*?<glob pattern=\"\\*\\.%2\"/>.*?</mime-type>")
                .arg(QRegularExpression::escape(type.mimeType), type.extension),
            QRegularExpression::DotMatchesEverythingOption);
        CHECK(mimeType.match(mime).hasMatch());
        mimeTypes.append(type.mimeType);
        // macOS: the exported type with its extension
        const QRegularExpression exported(
            QStringLiteral("<string>%1</string>.*?<string>%2</string>")
                .arg(QRegularExpression::escape(type.contentType), type.extension),
            QRegularExpression::DotMatchesEverythingOption);
        CHECK(exported.match(plist.section(QStringLiteral("UTExportedTypeDeclarations"), 1))
                  .hasMatch());
        CHECK(plist.section(QStringLiteral("UTExportedTypeDeclarations"), 0, 0)
                  .contains(QStringLiteral("<string>%1</string>").arg(type.contentType)));
        // Windows: the extension and its ProgID
        CHECK(nsi.contains(QStringLiteral("\"%1\"").arg(type.progId)));
        CHECK(nsi.contains(QStringLiteral("REGISTER_FILE_TYPE \".%1\"").arg(type.extension)));
    }
    // The menu entry opens all of them (xdg-mime default needs it)
    CHECK(desktop.contains(QStringLiteral("\nMimeType=%1;\n").arg(mimeTypes.join(u';'))));
    CHECK(desktop.contains(QStringLiteral("\nExec=kalahari %f\n")));
    CHECK(nsi.contains(QStringLiteral("!define APP_NAME \"%1\"")
                           .arg(QLatin1String(gui::WINDOWS_REGISTERED_NAME))));
}

TEST_CASE("File associations: what the program writes to the registry of Windows",
          "[gui][associations]")
{
    const QString program = QStringLiteral("C:\\Users\\Writer\\Kalahari\\kalahari.exe");
    const auto values = gui::windowsRegistration(
        program, {QStringLiteral("Kalahari book"), QStringLiteral("Kalahari document")});
    CHECK(registryValue(values, QStringLiteral("Software\\Classes\\Kalahari.Book")) ==
          QStringLiteral("Kalahari book"));
    CHECK(registryValue(values, QStringLiteral("Software\\Classes\\Kalahari.Document")) ==
          QStringLiteral("Kalahari document"));
    CHECK(registryValue(values,
                        QStringLiteral("Software\\Classes\\Kalahari.Book\\shell\\open\\command")) ==
          QStringLiteral("\"C:\\Users\\Writer\\Kalahari\\kalahari.exe\" \"%1\""));
    CHECK(registryValue(values,
                        QStringLiteral("Software\\Classes\\Kalahari.Document\\DefaultIcon")) ==
          QStringLiteral("C:\\Users\\Writer\\Kalahari\\kalahari.exe,0"));
    CHECK(registryValue(values, QStringLiteral("Software\\Classes\\.klh")) ==
          QStringLiteral("Kalahari.Book"));
    CHECK(registryValue(values, QStringLiteral("Software\\Classes\\.kchapter\\OpenWithProgids"),
                        QStringLiteral("Kalahari.Document"))
              .isEmpty());
    CHECK(registryValue(values,
                        QStringLiteral("Software\\Kalahari\\Capabilities\\FileAssociations"),
                        QStringLiteral(".kchapter")) == QStringLiteral("Kalahari.Document"));
    CHECK(registryValue(values, QStringLiteral("Software\\Kalahari\\Capabilities"),
                        QStringLiteral("ApplicationName")) == QStringLiteral("Kalahari"));
    CHECK_FALSE(registryValue(values, QStringLiteral("Software\\Kalahari\\Capabilities"),
                              QStringLiteral("ApplicationDescription"))
                    .isEmpty());
    CHECK(registryValue(values, QStringLiteral("Software\\RegisteredApplications"),
                        QStringLiteral("Kalahari")) ==
          QStringLiteral("Software\\Kalahari\\Capabilities"));
}

TEST_CASE("File associations: the Default Apps page of Windows Settings", "[gui][associations]")
{
    CHECK(gui::windowsSettingsAddress(true, true) ==
          QStringLiteral("ms-settings:defaultapps?registeredAppUser=Kalahari"));
    CHECK(gui::windowsSettingsAddress(false, true) ==
          QStringLiteral("ms-settings:defaultapps?registeredAppMachine=Kalahari"));
    CHECK(gui::windowsSettingsAddress(false, false) == QStringLiteral("ms-settings:defaultapps"));
}

TEST_CASE("File associations: the menu entry of an AppImage", "[gui][associations]")
{
    const QString entry = QStringLiteral(
        "[Desktop Entry]\nName=Kalahari\nExec=kalahari %f\nTryExec=kalahari\nIcon=kalahari\n");
    CHECK(gui::linuxDesktopEntry(entry, QStringLiteral("/opt/apps/Kalahari.AppImage")) ==
          QStringLiteral("[Desktop Entry]\nName=Kalahari\nExec=/opt/apps/Kalahari.AppImage %f\n"
                         "TryExec=/opt/apps/Kalahari.AppImage\nIcon=kalahari\n"));
    // A reserved character: the argument quoted, "$" and "\" escaped twice (the quoting, then
    // the string), "%" as "%%"
    const QString odd = gui::linuxDesktopEntry(QStringLiteral("Exec=kalahari %f"),
                                               QStringLiteral("/home/a b/K$1\\x%.AppImage"));
    CHECK(odd == QStringLiteral("Exec=\"/home/a b/K\\\\$1\\\\\\\\x%%.AppImage\" %f"));
}

TEST_CASE("File associations: the commands of xdg-utils", "[gui][associations]")
{
    const QStringList defaults{QStringLiteral("default"), QStringLiteral("kalahari.desktop"),
                               QStringLiteral("application/x-kalahari-book"),
                               QStringLiteral("application/x-kalahari-document")};
    SECTION("The package installed the types and the menu entry")
    {
        const auto commands = gui::linuxAssociateCommands(false, {}, {}, {});
        REQUIRE(commands.size() == 1);
        CHECK(commands[0].program == QStringLiteral("xdg-mime"));
        CHECK(commands[0].arguments == defaults);
    }
    SECTION("An AppImage installs them for the writer first")
    {
        const QStringList user{QStringLiteral("install"), QStringLiteral("--mode"),
                               QStringLiteral("user"), QStringLiteral("--novendor")};
        const auto commands = gui::linuxAssociateCommands(true, QStringLiteral("/a/kalahari.xml"),
                                                          QStringLiteral("/t/kalahari.desktop"),
                                                          QStringLiteral("/a/kalahari.png"));
        REQUIRE(commands.size() == 6);
        CHECK(commands[0].program == QStringLiteral("xdg-mime"));
        CHECK(commands[0].arguments == user + QStringList{QStringLiteral("/a/kalahari.xml")});
        const QStringList size{QStringLiteral("--size"), QStringLiteral("256"),
                               QStringLiteral("/a/kalahari.png")};
        const QStringList fileIcon =
            user + QStringList{QStringLiteral("--context"), QStringLiteral("mimetypes")} + size;
        CHECK(commands[1].program == QStringLiteral("xdg-icon-resource"));
        CHECK(commands[1].arguments == user + size + QStringList{QStringLiteral("kalahari")});
        // The same picture for the books and the documents in the file managers
        CHECK(commands[2].program == QStringLiteral("xdg-icon-resource"));
        CHECK(commands[2].arguments ==
              fileIcon + QStringList{QStringLiteral("application-x-kalahari-book")});
        CHECK(commands[3].program == QStringLiteral("xdg-icon-resource"));
        CHECK(commands[3].arguments ==
              fileIcon + QStringList{QStringLiteral("application-x-kalahari-document")});
        CHECK(commands[4].program == QStringLiteral("xdg-desktop-menu"));
        CHECK(commands[4].arguments == user + QStringList{QStringLiteral("/t/kalahari.desktop")});
        CHECK(commands[5].arguments == defaults);
        // Without an icon
        CHECK(gui::linuxAssociateCommands(true, QStringLiteral("/a/kalahari.xml"),
                                          QStringLiteral("/t/kalahari.desktop"), {})
                  .size() == 3);
    }
}

TEST_CASE("File associations: the answer of xdg-mime query default", "[gui][associations]")
{
    CHECK(gui::linuxAssociation(QStringLiteral("kalahari.desktop\n"), {}).state == State::Kalahari);
    CHECK(gui::linuxAssociation(QString(), {}).state == State::NoProgram);
    const FileAssociation other = gui::linuxAssociation(
        QStringLiteral("org.gnome.TextEditor.desktop"), QStringLiteral("Text Editor"));
    CHECK(other.state == State::OtherProgram);
    CHECK(other.program == QStringLiteral("Text Editor"));
    CHECK(gui::linuxAssociation(QStringLiteral("code.desktop"), {}).program ==
          QStringLiteral("code.desktop"));
}

TEST_CASE("File associations: the page shows which program opens the files", "[gui][associations]")
{
    const FakeSystemGuard guard;
    FakeSystem& system = *guard.system;
    system.states[QStringLiteral("klh")] = {State::Kalahari, {}};
    system.states[QStringLiteral("kchapter")] = {State::OtherProgram, QStringLiteral("Word")};

    gui::FileAssociationsPage page;
    page.show();
    CHECK(labelText(page, QStringLiteral("fileAssociation_klh")) ==
          QStringLiteral("Kalahari opens them"));
    CHECK(labelText(page, QStringLiteral("fileAssociation_kchapter")) ==
          QStringLiteral("Word opens them"));
    CHECK(page.findChild<QPushButton*>(QStringLiteral("fileAssociationSettings")) == nullptr);
    CHECK_FALSE(page.isChanged());
    CHECK(page.apply().empty());

    system.states[QStringLiteral("klh")] = {State::NoProgram, {}};
    system.states[QStringLiteral("kchapter")] = {State::Unknown, QStringLiteral("No xdg-mime.")};
    CHECK_FALSE(page.refresh());
    CHECK(labelText(page, QStringLiteral("fileAssociation_klh"))
              .startsWith(QStringLiteral("No program opens them")));
    CHECK(labelText(page, QStringLiteral("fileAssociation_kchapter")) ==
          QStringLiteral("The system does not tell: No xdg-mime."));
}

TEST_CASE("File associations: the page makes Kalahari the program of the files",
          "[gui][associations]")
{
    const FakeSystemGuard guard;
    FakeSystem& system = *guard.system;
    system.states[QStringLiteral("kchapter")] = {State::OtherProgram, QStringLiteral("Word")};

    SECTION("The system lets the program do it")
    {
        system.later = true;
        gui::FileAssociationsPage page;
        page.show();
        auto* associate = page.findChild<QPushButton*>(QStringLiteral("associateFiles"));
        REQUIRE(associate != nullptr);
        associate->click();
        CHECK(system.associations == 1);
        CHECK_FALSE(associate->isEnabled());  // until the system answers
        system.pending(QString());
        CHECK(associate->isEnabled());
        CHECK(labelText(page, QStringLiteral("fileAssociation_kchapter")) ==
              QStringLiteral("Kalahari opens them"));
        CHECK(labelText(page, QStringLiteral("fileAssociationResult")) ==
              QStringLiteral("Kalahari opens its books and documents now."));
    }
    SECTION("What went wrong")
    {
        system.problem = QStringLiteral("xdg-mime: no desktop.");
        gui::FileAssociationsPage page;
        page.show();
        page.findChild<QPushButton*>(QStringLiteral("associateFiles"))->click();
        CHECK(labelText(page, QStringLiteral("fileAssociationResult")) ==
              QStringLiteral("Kalahari could not become the program of its files: "
                             "xdg-mime: no desktop."));
        CHECK(labelText(page, QStringLiteral("fileAssociation_kchapter")) ==
              QStringLiteral("Word opens them"));
    }
    SECTION("The page is closed before the system answers")
    {
        system.later = true;
        auto page = std::make_unique<gui::FileAssociationsPage>();
        page->findChild<QPushButton*>(QStringLiteral("associateFiles"))->click();
        page.reset();
        system.pending(QString());  // nothing to show it on
        CHECK(system.associations == 1);
    }
}

TEST_CASE("File associations: Windows lets only the writer choose the program",
          "[gui][associations]")
{
    const FakeSystemGuard guard;
    FakeSystem& system = *guard.system;
    system.settingsWindow = true;  // the fake leaves the files to another program
    system.states[QStringLiteral("klh")] = {State::OtherProgram, QStringLiteral("Notepad")};

    gui::FileAssociationsPage page;
    page.show();
    auto* settings = page.findChild<QPushButton*>(QStringLiteral("fileAssociationSettings"));
    REQUIRE(settings != nullptr);
    page.findChild<QPushButton*>(QStringLiteral("associateFiles"))->click();
    CHECK(labelText(page, QStringLiteral("fileAssociationResult"))
              .contains(QStringLiteral("Default Apps page of Windows Settings")));
    settings->click();
    CHECK(system.settingsOpened == 1);
    system.settingsOpen = false;
    settings->click();
    CHECK(labelText(page, QStringLiteral("fileAssociationResult")) ==
          QStringLiteral("Windows Settings did not open."));
}

TEST_CASE("File associations: the page of the Settings", "[gui][associations]")
{
    const FakeSystemGuard guard;
    gui::SettingsDialog dialog(kalahari::test::book(), kalahari::test::commands(), nullptr);
    REQUIRE(dialog.openPage(QString::fromLatin1(gui::SettingsDialog::FILE_ASSOCIATIONS_PAGE)));
    CHECK(dialog.findChild<gui::FileAssociationsPage*>() != nullptr);
}

TEST_CASE("File associations: the page in Polish", "[gui][associations]")
{
    QTranslator polish;
    REQUIRE(polish.load(QStringLiteral(":/i18n/kalahari_pl.qm")));
    QCoreApplication::installTranslator(&polish);
    const FakeSystemGuard guard;
    FakeSystem& system = *guard.system;
    system.settingsWindow = true;
    system.states[QStringLiteral("klh")] = {State::Kalahari, {}};
    system.states[QStringLiteral("kchapter")] = {State::OtherProgram, QStringLiteral("Word")};
    {
        gui::FileAssociationsPage page;
        page.show();
        CHECK(labelText(page, QStringLiteral("fileAssociation_klh")) ==
              QStringLiteral("Otwiera je Kalahari"));
        CHECK(labelText(page, QStringLiteral("fileAssociation_kchapter")) ==
              QStringLiteral("Otwiera je Word"));
        CHECK(page.findChild<QPushButton*>(QStringLiteral("associateFiles"))->text() ==
              QStringLiteral("&Powiąż z programem Kalahari"));
        CHECK(page.findChild<QPushButton*>(QStringLiteral("fileAssociationSettings"))->text() ==
              QStringLiteral("&Aplikacje domyślne w Ustawieniach systemu Windows…"));
        const auto texts = gui::windowsRegistration(QStringLiteral("C:\\k.exe"), {});
        CHECK(registryValue(texts, QStringLiteral("Software\\Kalahari\\Capabilities"),
                            QStringLiteral("ApplicationDescription")) ==
              QStringLiteral("Pisanie, planowanie i wydawanie książek"));
    }
    QCoreApplication::removeTranslator(&polish);
}
