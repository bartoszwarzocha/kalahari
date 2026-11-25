// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_source_lookup.cpp
/// @brief The description of a source from a catalog on the web: the window of From the Web by
/// ISBN or DOI..., its button on the card of a source and its options in the Settings

#include "kalahari_tests/gui/document_window.h"

#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/source_catalogs.h"
#include "kalahari/core/source_description.h"
#include "kalahari/gui/dialogs/source_lookup_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/panels/source_boxes.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/source_lookup.h"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <cstdio>
#include <functional>
#include <string>

#include "kalahari_tests/test_support/window_parts.h"

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::SettingsManager;
using core::SourceCatalogs;
using core::SourceDescription;
using gui::dialogs::SourceLookupDialog;

namespace {

/// An answer of Google Books to "q=isbn:9780306406157"
constexpr const char* GOOGLE_ANSWER = R"({ "kind": "books#volumes", "totalItems": 1,
  "items": [ { "volumeInfo": {
      "title": "Atlas pustyń świata", "authors": [ "Anna Wiśniewska", "Jan Nowak" ],
      "publisher": "Wydawnictwo Piasek", "publishedDate": "2011-05-01",
      "industryIdentifiers": [ { "type": "ISBN_13", "identifier": "9780306406157" } ],
      "pageCount": 212 } } ] })";

/// An answer of Google Books that has no book
constexpr const char* NO_BOOK = R"({ "kind": "books#volumes", "totalItems": 0 })";

/// Allows the lookup with the ISBN catalog @p catalog and gives the settings back at the end
class LookupAllowed {
public:
    explicit LookupAllowed(bool allowed, const char* catalog = SourceCatalogs::GOOGLE_BOOKS)
        : m_lookup(SettingsManager::getInstance().get<bool>(SourceCatalogs::LOOKUP_SETTING,
                                                            false)),
          m_catalog(SettingsManager::getInstance().get<std::string>(
              SourceCatalogs::ISBN_CATALOG_SETTING, SourceCatalogs::GOOGLE_BOOKS)) {
        SettingsManager::getInstance().set<bool>(SourceCatalogs::LOOKUP_SETTING, allowed);
        SettingsManager::getInstance().set<std::string>(SourceCatalogs::ISBN_CATALOG_SETTING,
                                                        catalog);
    }
    ~LookupAllowed() {
        try {
            SettingsManager::getInstance().set<bool>(SourceCatalogs::LOOKUP_SETTING, m_lookup);
            SettingsManager::getInstance().set<std::string>(SourceCatalogs::ISBN_CATALOG_SETTING,
                                                            m_catalog);
        } catch (...) {
            // A destructor may not throw: the later tests then find the settings of this one
            static_cast<void>(std::fputs("LookupAllowed: the settings of the test stay\n", stderr));
        }
    }
    LookupAllowed(const LookupAllowed&) = delete;
    LookupAllowed& operator=(const LookupAllowed&) = delete;
    LookupAllowed(LookupAllowed&&) = delete;
    LookupAllowed& operator=(LookupAllowed&&) = delete;

private:
    bool m_lookup;
    std::string m_catalog;
};

/// A folder with the answers of Google Books: isbn-<number>.json
class Answers {
public:
    Answers() {
        REQUIRE(m_dir.isValid());
        write(QStringLiteral("9780306406157"), GOOGLE_ANSWER);
        write(QStringLiteral("9780131103627"), NO_BOOK);
    }

    /// The address the answer to a number is at (%1: the number)
    [[nodiscard]] QString pattern() const {
        return QUrl::fromLocalFile(m_dir.path()).toString() + QStringLiteral("/isbn-%1.json");
    }

private:
    void write(const QString& number, const QByteArray& answer) const {
        QFile file(QDir(m_dir.path()).filePath(QStringLiteral("isbn-%1.json").arg(number)));
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(answer);
    }

    QTemporaryDir m_dir;
};

/// Run the events until @p done, at most five seconds
bool waitUntil(const std::function<bool()>& done) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 5000) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return done();
}

/// Run @p command, looking up with Look Up in each window of the lookup it shows, with the
/// answers at @p pattern
/// @return The number of windows the command showed
int lookingUp(const std::function<void()>& command, const QString& pattern) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &pattern]() {
        auto* dialog = qobject_cast<SourceLookupDialog*>(test::shownDialog());
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        ++shown;
        dialog->lookup()->setAddressPattern(QString::fromLatin1(SourceCatalogs::GOOGLE_BOOKS),
                                            pattern);
        dialog->acceptButton()->click();
    });
    timer.start();
    command();
    return shown;
}

/// Open page @p title of the Settings
void openPage(gui::SettingsDialog& dialog, const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->text(0) == title) {
            tree->setCurrentItem(*it);
            return;
        }
    }
    FAIL("No page " << title.toStdString());
}

}  // namespace

TEST_CASE("Source lookup: the window asks only for a number, says why nothing came and gives "
          "up with Esc",
          "[gui][materials][sources][lookup]") {
    gui::registerAllIcons();
    const LookupAllowed allowed(true);
    const Answers answers;
    SourceLookupDialog dialog(QString(), nullptr);
    dialog.lookup()->setAddressPattern(QString::fromLatin1(SourceCatalogs::GOOGLE_BOOKS),
                                       answers.pattern());
    QLabel* status = dialog.statusLabel();
    CHECK(status->isHidden());

    // What is neither an ISBN nor a DOI is not asked for
    dialog.numberEdit()->setText(QStringLiteral("978-0-306-40615-8"));
    dialog.accept();
    CHECK_FALSE(dialog.lookup()->isRunning());
    CHECK_FALSE(status->isHidden());
    CHECK(status->text().startsWith(QStringLiteral("This is neither an ISBN nor a DOI.")));
    CHECK(dialog.result() == QDialog::Rejected);

    // A catalog without the book says so, and the window stays
    dialog.numberEdit()->setText(QStringLiteral("978-0-13-110362-7"));
    dialog.accept();
    CHECK(dialog.lookup()->isRunning());
    CHECK(status->text() == QStringLiteral("Asking Google Books…"));
    CHECK(dialog.numberEdit()->isReadOnly());
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    REQUIRE(waitUntil([&dialog]() { return !dialog.lookup()->isRunning(); }));
    CHECK(status->text() == QStringLiteral("Google Books has no book with this ISBN."));
    CHECK_FALSE(dialog.numberEdit()->isReadOnly());
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK_FALSE(dialog.found().has_value());
    CHECK(dialog.result() == QDialog::Rejected);

    // Esc gives up, also while the catalog answers: nothing comes after it
    dialog.numberEdit()->setText(QStringLiteral("9780306406157"));
    dialog.accept();
    CHECK(dialog.lookup()->isRunning());
    int finished = 0;
    QObject::connect(dialog.lookup(), &gui::SourceLookup::finished,
                     [&finished](const gui::SourceLookup::Result&) { ++finished; });
    dialog.reject();
    CHECK_FALSE(dialog.lookup()->isRunning());
    QApplication::processEvents();
    CHECK(finished == 0);
    CHECK_FALSE(dialog.found().has_value());

    // The book found closes the window with its description
    dialog.setResult(QDialog::Rejected);
    dialog.accept();
    REQUIRE(waitUntil([&dialog]() { return !dialog.lookup()->isRunning(); }));
    CHECK(finished == 1);
    CHECK(dialog.result() == QDialog::Accepted);
    REQUIRE(dialog.found().has_value());
    CHECK(dialog.found()->value(SourceDescription::TITLE) ==
          QStringLiteral("Atlas pustyń świata"));
    CHECK(dialog.foundIn() == QLatin1String(SourceCatalogs::GOOGLE_BOOKS));
}

TEST_CASE("Source card: From the Web by ISBN or DOI, shown when the Settings allow it, fills "
          "the description and keeps the fields the catalog has not",
          "[gui][materials][sources][lookup]") {
    gui::registerAllIcons();
    const LookupAllowed allowed(false);
    const Answers answers;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    core::NewMaterial material;
    material.title = QStringLiteral("Atlas");
    const QString id = pm.addMaterial(
        material, pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                          QString::fromLatin1(core::MaterialKinds::PUBLICATION)));
    REQUIRE_FALSE(id.isEmpty());
    SourceDescription source;
    source.type = QString::fromLatin1(SourceDescription::BOOK);
    source.setValue(QString::fromLatin1(SourceDescription::ISBN),
                    QStringLiteral("978-0-306-40615-7"));
    source.setValue(QString::fromLatin1(SourceDescription::PLACE), QStringLiteral("Kraków"));
    source.setValue(QString::fromLatin1(SourceDescription::TITLE), QStringLiteral("Atlas"));
    REQUIRE(pm.setSource(id, source));

    gui::MaterialPage page(kalahari::test::book(), nullptr, true);
    page.showMaterial(id);
    gui::SourceDescriptionBox* box = page.sourceBox();
    // Without the option the card has no button that sends anything
    CHECK(box->lookupButton()->isHidden());
    SettingsManager::getInstance().set<bool>(SourceCatalogs::LOOKUP_SETTING, true);
    REQUIRE(waitUntil([box]() { return !box->lookupButton()->isHidden(); }));

    QStringList said;
    QObject::connect(box, &gui::SourceDescriptionBox::message,
                     [&said](const QString& text) { said << text; });
    CHECK(lookingUp([box]() { box->lookupButton()->click(); }, answers.pattern()) == 1);
    const SourceDescription& filled = pm.findElement(id)->source;
    CHECK(filled.value(SourceDescription::TITLE) == QStringLiteral("Atlas pustyń świata"));
    CHECK(filled.value(SourceDescription::PUBLISHER) == QStringLiteral("Wydawnictwo Piasek"));
    CHECK(filled.value(SourceDescription::YEAR) == QStringLiteral("2011"));
    CHECK(filled.value(SourceDescription::PLACE) == QStringLiteral("Kraków"));
    REQUIRE(filled.people.size() == 2);
    CHECK(filled.people.first().name == QStringLiteral("Anna Wiśniewska"));
    CHECK(box->field(QStringLiteral("title"))->text() == QStringLiteral("Atlas pustyń świata"));
    CHECK(said == QStringList{QStringLiteral("Bibliographic description filled in from Google "
                                             "Books")});

    // The option taken back hides the button again
    SettingsManager::getInstance().set<bool>(SourceCatalogs::LOOKUP_SETTING, false);
    REQUIRE(waitUntil([box]() { return box->lookupButton()->isHidden(); }));
}

TEST_CASE("Settings dialog: the Materials page allows the descriptions from the web and "
          "chooses the catalog of an ISBN",
          "[gui][settings][materials][lookup]") {
    const LookupAllowed allowed(false);
    gui::SettingsDialog dialog(kalahari::test::book(), kalahari::test::commands(), nullptr);
    openPage(dialog, QStringLiteral("Materials"));
    auto* lookup = dialog.findChild<QCheckBox*>(QStringLiteral("materialsSourcesLookup"));
    auto* catalog = dialog.findChild<QComboBox*>(QStringLiteral("materialsSourcesIsbnCatalog"));
    REQUIRE(lookup != nullptr);
    REQUIRE(catalog != nullptr);
    // Off at first: the catalog waits, dimmed
    CHECK_FALSE(lookup->isChecked());
    CHECK_FALSE(catalog->isEnabled());
    CHECK(catalog->currentData().toString() == QLatin1String(SourceCatalogs::GOOGLE_BOOKS));
    CHECK(catalog->currentText() == QStringLiteral("Google Books (recommended)"));

    lookup->click();
    CHECK(catalog->isEnabled());
    catalog->setCurrentIndex(
        catalog->findData(QString::fromLatin1(SourceCatalogs::OPEN_LIBRARY)));
    const QStringList written = dialog.applyChanges();
    CHECK(written.size() == 2);
    CHECK(SourceCatalogs::lookupAllowed());
    CHECK(SourceCatalogs::settingsIsbnCatalog() == QLatin1String(SourceCatalogs::OPEN_LIBRARY));
}
