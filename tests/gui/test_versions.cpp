/// @file test_versions.cpp
/// @brief The versions of the texts as the writer uses them: Save Version with its small window
/// or without it, the window of Version History, restoring a version and inserting a fragment
/// of it, the Navigator, the Properties panel and Settings > Editor > Versions

#include "document_window.h"

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_versions.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/version_comparison.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/dialogs/version_history_dialog.h>
#include <kalahari/gui/dialogs/version_name_dialog.h>
#include <kalahari/gui/kind_names.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/gui/panels/properties_panel.h>
#include <kalahari/gui/settings/settings_pages.h>
#include <kalahari/gui/version_texts.h>
#include <kalahari/gui/versions_coordinator.h>
#include <kalahari/gui/widgets/standalone_info_bar.h>
#include "context_menu_reader.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <functional>

using namespace kalahari;
using gui::dialogs::VersionHistoryDialog;
using gui::dialogs::VersionNameDialog;
using test::OpenNovel;
using test::Window;

namespace {

const QString READ_ONLY = QStringLiteral("The book is open read-only");

/// KML of @p text, one paragraph a line
QString kmlOf(const QString& text) {
    return editor::ClipboardHandler::textToKml(text);
}

/// The plain text of @p kml, one paragraph a line
QString plainOf(const QString& kml) {
    editor::BookEditor editor;
    editor.fromKml(kml);
    return editor.textDocument()->toPlainText();
}

/// The settings of the versions as they were, after the test
class KeptVersionSettings {
public:
    KeptVersionSettings() {
        auto& settings = core::SettingsManager::getInstance();
        m_ask = settings.get<bool>(core::ChapterVersions::ASK_NAME_SETTING);
        m_onSave = settings.get<bool>(core::ChapterVersions::ON_SAVE_SETTING);
        // Saving the chapter keeps no version of its own in these tests
        settings.set<bool>(core::ChapterVersions::ON_SAVE_SETTING, false);
    }
    ~KeptVersionSettings() {
        auto& settings = core::SettingsManager::getInstance();
        settings.set<bool>(core::ChapterVersions::ASK_NAME_SETTING, m_ask);
        settings.set<bool>(core::ChapterVersions::ON_SAVE_SETTING, m_onSave);
    }
    KeptVersionSettings(const KeptVersionSettings&) = delete;
    KeptVersionSettings& operator=(const KeptVersionSettings&) = delete;

private:
    bool m_ask = true;
    bool m_onSave = true;
};

/// Run @p command, answering each window it shows with @p answer once, as the writer does
/// @return The number of windows the command showed
int answeringWith(const std::function<void()>& command,
                  const std::function<void(QDialog&)>& answer) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        QDialog* dialog = test::shownDialog();
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        ++shown;
        answer(*dialog);
    });
    timer.start();
    command();
    return shown;
}

/// Keep @p text of chapter @p elementId as a version of @p origin
core::ChapterVersion kept(const QString& elementId, const QString& text,
                          core::VersionOrigin origin = core::VersionOrigin::Manual,
                          const QString& name = QString()) {
    core::ChapterVersion version;
    REQUIRE(core::ProjectManager::getInstance().keepVersion(
                elementId, kmlOf(text), {.origin = origin, .name = name}, &version) ==
            core::ProjectManager::VersionKept::Added);
    return version;
}

/// The window of the versions of chapter @p novel over an editor with text @p current
struct HistoryWindow {
    editor::BookEditor editor;
    std::unique_ptr<VersionHistoryDialog> dialog;
    int saved = 0;  ///< Save the Current Text as a Version was pressed

    HistoryWindow(const OpenNovel& novel, const QString& current, bool readOnly = false) {
        editor.fromKml(kmlOf(current));
        const core::ProjectElement* element = novel.pm.findElement(novel.chapterId);
        REQUIRE(element != nullptr);
        VersionHistoryDialog::Text text;
        text.elementId = novel.chapterId;
        text.title = element->title;
        text.bookTitle = QStringLiteral("Rain");
        text.kind = gui::wordsOf(*element);
        text.kml = editor.toKml();
        text.editor = &editor;
        text.readOnly = readOnly;
        dialog = std::make_unique<VersionHistoryDialog>(
            text, [this](QWidget*) { ++saved; });
    }

    /// The files of the versions the list shows, from the top
    [[nodiscard]] QStringList listed() const {
        QStringList files;
        for (int i = 0; i < dialog->versionList()->count(); ++i) {
            files << dialog->versionList()->item(i)->data(Qt::UserRole).toString();
        }
        return files;
    }

    /// The text of the preview
    [[nodiscard]] QString preview() const {
        return dialog->preview()->textDocument()->toPlainText();
    }
};

}  // namespace

// =============================================================================
// The small window of Save Version
// =============================================================================

TEST_CASE("Versions: Save Version asks for a description, which may stay empty",
          "[gui][versions][dialogs]") {
    core::KindWords chapter;
    chapter.forms.insert(QStringLiteral("singular"), QStringLiteral("chapter"));
    chapter.gender = core::Gender::Masculine;
    VersionNameDialog dialog(QStringLiteral("The Storm"), chapter);
    auto* name = dialog.findChild<QLineEdit*>(QStringLiteral("versionNameEdit"));
    REQUIRE(name != nullptr);
    CHECK(name->text().isEmpty());
    CHECK(name->placeholderText() == QStringLiteral("e.g. Before the ending was changed"));
    CHECK(test::textsOf(dialog).contains(
        QStringLiteral("The version keeps the text of the chapter \"The Storm\" as it is in the "
                       "editor now: with its formatting and annotations, also the changes not "
                       "saved yet.")));
    // Enter saves, also without a description
    CHECK(dialog.acceptButton()->isDefault());
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Save Version"));
    CHECK(dialog.name().isEmpty());
    name->setText(QStringLiteral("  Before the dialogue was cut  "));
    CHECK(dialog.name() == QStringLiteral("Before the dialogue was cut"));
    CHECK(name->maxLength() == 200);
}

TEST_CASE("Versions: Change Description shows the description of the version",
          "[gui][versions][dialogs]") {
    VersionNameDialog dialog(QStringLiteral("today, 14:52"), QStringLiteral("Saved by you"),
                             QStringLiteral("Before the storm"));
    auto* name = dialog.findChild<QLineEdit*>(QStringLiteral("versionNameEdit"));
    REQUIRE(name != nullptr);
    CHECK(name->text() == QStringLiteral("Before the storm"));
    CHECK(test::textsOf(dialog).contains(QStringLiteral("The version of today, 14:52. Saved by "
                                                        "you.")));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Change Description"));
}

// =============================================================================
// The window of Version History
// =============================================================================

TEST_CASE("Versions: the history lists the versions, newest first, and hides the program's",
          "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    const core::ChapterVersion first =
        kept(novel.chapterId, QStringLiteral("Dark night."), core::VersionOrigin::Save);
    const core::ChapterVersion second = kept(novel.chapterId, QStringLiteral("Dark storm night."),
                                             core::VersionOrigin::Manual,
                                             QStringLiteral("Before the storm"));
    HistoryWindow window(novel, QStringLiteral("Dark storm."));

    CHECK(window.listed() == QStringList{second.file, first.file});
    QListWidgetItem* top = window.dialog->versionList()->item(0);
    CHECK(top->text().startsWith(QStringLiteral("\"Before the storm\", today, ")));
    CHECK(top->text().endsWith(QStringLiteral(" · 3 word(s) · saved by you")));
    CHECK(window.dialog->versionList()->item(1)->text().startsWith(
        QStringLiteral("When the chapter was saved, today, ")));
    // The newest is chosen; Close is the default button: Enter brings nothing back by mistake
    CHECK(window.dialog->selectedVersion() == second);
    CHECK(window.dialog->cancelButton()->isDefault());
    CHECK(window.dialog->cancelButton()->text() == QStringLiteral("Close"));
    CHECK_FALSE(window.dialog->acceptButton()->isVisibleTo(window.dialog.get()));

    // The versions saved by the program can be hidden
    window.dialog->automaticBox()->setChecked(false);
    CHECK(window.listed() == QStringList{second.file});
    window.dialog->automaticBox()->setChecked(true);
    CHECK(window.listed() == QStringList{second.file, first.file});
    CHECK(window.dialog->selectedVersion() == second);
}

TEST_CASE("Versions: the history compares a version with the current text, change by change",
          "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    kept(novel.chapterId, QStringLiteral("Dark night came.\nTau waited."));
    HistoryWindow window(novel, QStringLiteral("Dark storm came.\nTau waited long."));

    // The changes at first: what only the version has and what was written later
    REQUIRE(window.dialog->showsChanges());
    CHECK(window.preview().contains(QStringLiteral("night")));
    CHECK(window.preview().contains(QStringLiteral("storm")));
    CHECK(window.preview().contains(QStringLiteral("long")));
    CHECK(window.dialog->currentChange() == 0);
    CHECK(window.dialog->summary() == QStringLiteral("Change 1 of 2 · deleted words: 1 · added: 2"));
    CHECK(window.dialog->preview()->isReadOnly());

    // From change to change, from the last to the first again
    window.dialog->goToChange(true);
    CHECK(window.dialog->currentChange() == 1);
    CHECK(window.dialog->summary() == QStringLiteral("Change 2 of 2 · deleted words: 1 · added: 2"));
    window.dialog->goToChange(true);
    CHECK(window.dialog->currentChange() == 0);
    window.dialog->goToChange(false);
    CHECK(window.dialog->currentChange() == 1);
    // Alt+Down and Alt+Up go there too
    QStringList keys;
    for (const QShortcut* shortcut : window.dialog->findChildren<QShortcut*>()) {
        keys << shortcut->key().toString(QKeySequence::PortableText);
    }
#ifndef Q_OS_MACOS
    CHECK(keys.contains(QStringLiteral("Alt+Down")));
    CHECK(keys.contains(QStringLiteral("Alt+Up")));
#endif

    // The text of the version, with its words and those of the current text
    window.dialog->showChanges(false);
    CHECK(window.preview() == QStringLiteral("Dark night came.\nTau waited."));
    CHECK(window.dialog->summary() ==
          QStringLiteral("Version text: 5 word(s) · current text: 6 word(s)"));
    CHECK(window.dialog->currentChange() == -1);
}

TEST_CASE("Versions: a version the same as the current text says so",
          "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    kept(novel.chapterId, QStringLiteral("Dark night."));
    HistoryWindow window(novel, QStringLiteral("Dark night."));
    CHECK(window.dialog->summary() ==
          QStringLiteral("The current text is the same as the text of this version."));
    CHECK(window.dialog->currentChange() == -1);
}

TEST_CASE("Versions: a book without versions says how to save one", "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    HistoryWindow window(novel, QStringLiteral("Dark night."));
    CHECK(window.listed().isEmpty());
    CHECK(window.dialog->summary() ==
          QStringLiteral("The chapter has no versions yet. Save the current text as a version "
                         "with the button under the list, or with %1 in the editor.")
              .arg(gui::saveVersionKeys()));
    CHECK_FALSE(window.dialog->restoreButton()->isEnabled());
    CHECK_FALSE(window.dialog->insertButton()->isEnabled());
    CHECK_FALSE(window.dialog->renameButton()->isEnabled());
    CHECK_FALSE(window.dialog->deleteButton()->isEnabled());
    // Save the Current Text as a Version saves it as Save Version does
    window.dialog->saveButton()->click();
    CHECK(window.saved == 1);
}

TEST_CASE("Versions: Restore This Version brings back the whole version",
          "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    const core::ChapterVersion version = kept(novel.chapterId, QStringLiteral("Dark night."));
    HistoryWindow window(novel, QStringLiteral("Bright day."));
    REQUIRE(window.dialog->restoreButton()->isEnabled());
    window.dialog->restoreButton()->click();
    CHECK(window.dialog->result() == QDialog::Accepted);
    CHECK(window.dialog->choice() == VersionHistoryDialog::Choice::Restore);
    CHECK(window.dialog->chosenVersion() == version);
    CHECK(plainOf(window.dialog->chosenKml()) == QStringLiteral("Dark night."));
}

TEST_CASE("Versions: Insert Selected Fragment takes the text of the version selected",
          "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    kept(novel.chapterId, QStringLiteral("Dark night came."));
    HistoryWindow window(novel, QStringLiteral("Dark storm came."));
    CHECK_FALSE(window.dialog->insertButton()->isEnabled());
    CHECK(window.dialog->insertButton()->toolTip() ==
          QStringLiteral("Select a fragment in the preview"));

    SECTION("In the text of the version") {
        window.dialog->showChanges(false);
        window.dialog->preview()->selectAll();
        REQUIRE(window.dialog->insertButton()->isEnabled());
        window.dialog->insertButton()->click();
        CHECK(window.dialog->choice() == VersionHistoryDialog::Choice::Insert);
        CHECK(plainOf(window.dialog->chosenKml()) == QStringLiteral("Dark night came."));
    }

    SECTION("In the changes: without the words written later") {
        window.dialog->preview()->selectAll();
        REQUIRE(window.dialog->insertButton()->isEnabled());
        window.dialog->insertButton()->click();
        CHECK(window.dialog->choice() == VersionHistoryDialog::Choice::Insert);
        CHECK(plainOf(window.dialog->chosenKml()) == QStringLiteral("Dark night came."));
    }
}

TEST_CASE("Versions: Ctrl+C in the changes copies the text of the version",
          "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    kept(novel.chapterId, QStringLiteral("Dark night came."));
    HistoryWindow window(novel, QStringLiteral("Dark storm came."));
    window.dialog->preview()->selectAll();
    QAction* copy = nullptr;
    for (QAction* action : window.dialog->preview()->actions()) {
        if (action->shortcuts().contains(QKeySequence(QKeySequence::Copy))) {
            copy = action;
        }
    }
    REQUIRE(copy != nullptr);
    copy->trigger();
    const QMimeData* copied = QApplication::clipboard()->mimeData();
    REQUIRE(copied != nullptr);
    CHECK(copied->text() == QStringLiteral("Dark night came."));
}

TEST_CASE("Versions: a description is changed and a version deleted in the history",
          "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    const core::ChapterVersion first = kept(novel.chapterId, QStringLiteral("Dark night."));
    const core::ChapterVersion second = kept(novel.chapterId, QStringLiteral("Dark storm."));
    HistoryWindow window(novel, QStringLiteral("Bright day."));
    REQUIRE(window.dialog->selectVersion(first.file));

    // Change Description... (F2): the window of the description, then the list
    QStringList said;
    CHECK(answeringWith([&]() { window.dialog->renameButton()->click(); },
                        [&said](QDialog& shown) {
                            said << test::textsOf(shown);
                            auto* name = shown.findChild<QLineEdit*>(
                                QStringLiteral("versionNameEdit"));
                            REQUIRE(name != nullptr);
                            name->setText(QStringLiteral("The first night"));
                            qobject_cast<gui::dialogs::KalahariDialog*>(&shown)
                                ->acceptButton()
                                ->click();
                        }) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("Change Description of the Version")));
    std::optional<core::ChapterVersion> renamed =
        novel.pm.versionsOf(novel.chapterId)->find(first.file);
    REQUIRE(renamed.has_value());
    CHECK(renamed->name == QStringLiteral("The first night"));
    CHECK(window.dialog->selectedVersion() == renamed);
    CHECK(window.dialog->versionList()->item(1)->text().startsWith(
        QStringLiteral("\"The first night\", ")));

    // Delete... asks first: deleting cannot be undone
    said.clear();
    CHECK(test::answering([&]() { window.dialog->deleteButton()->click(); },
                          gui::dialogs::MessageDialog::Answer::Accept, said) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("Deleting it cannot be undone.")));
    CHECK(window.listed() == QStringList{second.file});
    CHECK_FALSE(novel.pm.versionsOf(novel.chapterId)->find(first.file).has_value());
}

TEST_CASE("Versions: in a book open read-only the history only reads and compares",
          "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    kept(novel.chapterId, QStringLiteral("Dark night."));
    HistoryWindow window(novel, QStringLiteral("Bright day."), true);
    window.dialog->preview()->selectAll();
    for (QPushButton* button :
         {window.dialog->restoreButton(), window.dialog->insertButton(),
          window.dialog->renameButton(), window.dialog->deleteButton(),
          window.dialog->saveButton()}) {
        INFO(button->text().toStdString());
        CHECK_FALSE(button->isEnabled());
        CHECK(button->toolTip() == READ_ONLY);
    }
    CHECK(window.dialog->summary().startsWith(QStringLiteral("Change 1 of 1")));
}

TEST_CASE("Versions: the history fits a small screen with its buttons in view",
          "[gui][versions][dialogs]") {
    KeptVersionSettings settings;
    OpenNovel novel(QStringLiteral("Rain"));
    kept(novel.chapterId, QStringLiteral("Dark night."));
    HistoryWindow window(novel, QStringLiteral("Bright day."));
    // 1366x768 at 150%: about 910x512, without the taskbar
    window.dialog->setAttribute(Qt::WA_DontShowOnScreen);
    window.dialog->show();
    window.dialog->resize(880, 460);
    QApplication::processEvents();
    QApplication::processEvents();
    CHECK(window.dialog->minimumSizeHint().width() <= 880);
    for (const QPushButton* button :
         {window.dialog->restoreButton(), window.dialog->insertButton(),
          window.dialog->renameButton(), window.dialog->deleteButton(),
          window.dialog->cancelButton()}) {
        INFO(button->text().toStdString());
        CHECK(button->isVisible());
        CHECK(window.dialog->rect().contains(
            QRect(button->mapTo(window.dialog.get(), QPoint(0, 0)), button->size())));
    }
    window.dialog->hide();
}

// =============================================================================
// Save Version, Version History and what they do in the text
// =============================================================================

namespace {

/// The main window's tabs with a novel open, and the versions of its texts
struct VersionsWindow {
    KeptVersionSettings settings;
    OpenNovel novel{QStringLiteral("Rain")};
    Window window;
    gui::VersionsCoordinator versions{
        window.tabs,
        [this](const QString& elementId) {
            window.navigator.onElementSelected(elementId, QStringLiteral("The Storm"));
        },
        [this](gui::EditorPanel* editor, const QString& message, const QString& actionText,
               const QString& actionToolTip, std::function<void()> action) {
            window.documents.showNotice(editor, message, actionText, actionToolTip,
                                        std::move(action));
        },
        &window.statusBar, &window.window};

    /// The chapter open in its tab with @p text
    gui::EditorPanel* chapter(const QString& text) {
        window.writeChapter(novel.chapterId, text);
        return window.editor(window.tabs->currentIndex());
    }

    /// The versions of the chapter, newest first
    [[nodiscard]] QList<core::ChapterVersion> list() const {
        return novel.pm.versionsOf(novel.chapterId)->list();
    }
};

/// The text of the editor of @p panel
QString textOf(gui::EditorPanel* panel) {
    return panel->getBookEditor()->textDocument()->toPlainText();
}

}  // namespace

TEST_CASE("Versions: the commands work for the texts of the book, and say why not for others",
          "[gui][versions]") {
    VersionsWindow w;
    CHECK(w.versions.whyNotSave() == QStringLiteral("No text of the book is in front"));
    CHECK(w.versions.whyNotShowHistory() == QStringLiteral("No text of the book is in front"));

    w.chapter(QStringLiteral("Dark night."));
    CHECK(w.versions.whyNotSave().isEmpty());
    CHECK(w.versions.whyNotShowHistory().isEmpty());

    QTemporaryDir files;
    const QString path = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("Notes");
    }
    w.window.writeFile(path, QStringLiteral("More notes"));
    const QString why = QStringLiteral(
        "Only the texts of a book have versions, and this file is not part of the open book");
    CHECK(w.versions.whyNotSave() == why);
    CHECK(w.versions.whyNotShowHistory() == why);
    CHECK_FALSE(w.versions.saveVersion());
    CHECK(w.window.statusBar.currentMessage() == why);
}

TEST_CASE("Versions: Save Version keeps the text in the editor, with its description or at once",
          "[gui][versions]") {
    VersionsWindow w;
    auto& settings = core::SettingsManager::getInstance();
    gui::EditorPanel* panel = w.chapter(QStringLiteral("Dark night."));

    SECTION("The small window, with a description") {
        settings.set<bool>(core::ChapterVersions::ASK_NAME_SETTING, true);
        QStringList said;
        CHECK(answeringWith([&]() { CHECK(w.versions.saveVersion()); },
                            [&said](QDialog& shown) {
                                said << test::textsOf(shown);
                                shown.findChild<QLineEdit*>(QStringLiteral("versionNameEdit"))
                                    ->setText(QStringLiteral("Before the storm"));
                                qobject_cast<gui::dialogs::KalahariDialog*>(&shown)
                                    ->acceptButton()
                                    ->click();
                            }) == 1);
        REQUIRE(w.list().size() == 1);
        const core::ChapterVersion version = w.list().first();
        CHECK(version.origin == core::VersionOrigin::Manual);
        CHECK(version.name == QStringLiteral("Before the storm"));
        CHECK(plainOf(*w.novel.pm.versionsOf(w.novel.chapterId)->text(version.file)) ==
              QStringLiteral("Dark night."));
        CHECK(w.window.statusBar.currentMessage() ==
              QStringLiteral("Saved a version of the chapter \"The Storm\" (%1).")
                  .arg(gui::versionTime(version.created)));
        // Saving a version does not save the chapter
        CHECK(w.window.navigator.isChapterDirty(w.novel.chapterId));
    }

    SECTION("The small window closed: no version") {
        settings.set<bool>(core::ChapterVersions::ASK_NAME_SETTING, true);
        CHECK(answeringWith([&]() { CHECK_FALSE(w.versions.saveVersion()); },
                            [](QDialog& shown) { shown.reject(); }) == 1);
        CHECK(w.list().isEmpty());
    }

    SECTION("At once, as the settings say") {
        settings.set<bool>(core::ChapterVersions::ASK_NAME_SETTING, false);
        CHECK(answeringWith([&]() { CHECK(w.versions.saveVersion()); },
                            [](QDialog& shown) { shown.reject(); }) == 0);
        REQUIRE(w.list().size() == 1);
        CHECK(w.list().first().name.isEmpty());
        CHECK(w.window.statusBar.currentMessage() ==
              QStringLiteral("Saved a version of the chapter \"The Storm\" (%1). The description "
                             "can be added in the version history (F2).")
                  .arg(gui::versionTime(w.list().first().created)));
    }
    CHECK(textOf(panel) == QStringLiteral("Dark night."));
}

TEST_CASE("Versions: a version restored takes the place of the text, which is kept first",
          "[gui][versions]") {
    VersionsWindow w;
    const core::ChapterVersion old = kept(w.novel.chapterId, QStringLiteral("Dark night."));
    gui::EditorPanel* panel = w.chapter(QStringLiteral("Bright day."));

    REQUIRE(w.versions.restoreVersion(panel, old, kmlOf(QStringLiteral("Dark night."))));
    CHECK(textOf(panel) == QStringLiteral("Dark night."));
    // The text before it is the newest version
    REQUIRE(w.list().size() == 2);
    const core::ChapterVersion before = w.list().first();
    CHECK(before.origin == core::VersionOrigin::Restore);
    CHECK(before.restored == old.created);
    CHECK(plainOf(*w.novel.pm.versionsOf(w.novel.chapterId)->text(before.file)) ==
          QStringLiteral("Bright day."));

    // The bar under the text says so, with Undo
    REQUIRE(w.window.infoBar->showsNotice());
    CHECK_FALSE(w.window.infoBar->isHidden());
    CHECK(w.window.infoBar->message() ==
          QStringLiteral("Restored the version of %1. The text from before the restoring is "
                         "kept as a version, and Ctrl+Z undoes the restoring.")
              .arg(gui::versionTime(old.created)));
    auto* undo = w.window.infoBar->findChild<QPushButton*>();
    REQUIRE(undo != nullptr);
    CHECK(undo->text() == QStringLiteral("&Undo"));

    SECTION("Undo brings the text back, and the bar goes") {
        undo->click();
        CHECK(textOf(panel) == QStringLiteral("Bright day."));
        CHECK(w.window.infoBar->isHidden());
        CHECK_FALSE(w.window.documents.hasNotice(panel));
    }

    SECTION("Ctrl+Z, as Undo") {
        panel->getBookEditor()->undo();
        CHECK(textOf(panel) == QStringLiteral("Bright day."));
        CHECK(w.window.infoBar->isHidden());
    }

    SECTION("The bar goes at the first change of the text") {
        test::write(*panel, QStringLiteral("Dark night came."));
        CHECK(w.window.infoBar->isHidden());
        CHECK_FALSE(w.window.documents.hasNotice(panel));
    }

    SECTION("The bar goes with its X") {
        auto* close = w.window.infoBar->findChild<QToolButton*>();
        REQUIRE(close != nullptr);
        close->click();
        CHECK(w.window.infoBar->isHidden());
        CHECK(textOf(panel) == QStringLiteral("Dark night."));
    }
}

TEST_CASE("Versions: a fragment of a version is inserted at the cursor and selected",
          "[gui][versions]") {
    VersionsWindow w;
    const core::ChapterVersion old = kept(w.novel.chapterId, QStringLiteral("Dark night."));
    gui::EditorPanel* panel = w.chapter(QStringLiteral("Bright day."));
    editor::BookEditor* text = panel->getBookEditor();
    text->setCursorPosition({0, 7});  // after "Bright "

    REQUIRE(w.versions.insertFragment(panel, old, kmlOf(QStringLiteral("dark "))));
    CHECK(textOf(panel) == QStringLiteral("Bright dark day."));
    CHECK(text->selectionCursor().selectedText() == QStringLiteral("dark "));
    CHECK(w.window.statusBar.currentMessage() ==
          QStringLiteral("Inserted a fragment of the version of %1. Ctrl+Z undoes the "
                         "inserting.")
              .arg(gui::versionTime(old.created)));
    // One step of the undo history
    text->undo();
    CHECK(textOf(panel) == QStringLiteral("Bright day."));
}

TEST_CASE("Versions: Version History of a text not open opens it first", "[gui][versions]") {
    VersionsWindow w;
    kept(w.novel.chapterId, QStringLiteral("Dark night."));
    REQUIRE(w.window.tabs->count() == 0);
    QStringList said;
    CHECK(answeringWith([&]() { CHECK(w.versions.showHistory(w.novel.chapterId)); },
                        [&said](QDialog& shown) {
                            said << test::textsOf(shown);
                            shown.reject();
                        }) == 1);
    CHECK(w.window.tabs->count() == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("The Storm from the book \"Rain\"")));
}

TEST_CASE("Versions: Restore in the history goes to the text's editor", "[gui][versions]") {
    VersionsWindow w;
    kept(w.novel.chapterId, QStringLiteral("Dark night."));
    gui::EditorPanel* panel = w.chapter(QStringLiteral("Bright day."));
    CHECK(answeringWith([&]() { CHECK(w.versions.showHistory()); },
                        [](QDialog& shown) {
                            auto* history = qobject_cast<VersionHistoryDialog*>(&shown);
                            REQUIRE(history != nullptr);
                            history->restoreButton()->click();
                        }) == 1);
    CHECK(textOf(panel) == QStringLiteral("Dark night."));
    CHECK(w.window.infoBar->showsNotice());
}

// =============================================================================
// The Navigator and the Properties panel
// =============================================================================

TEST_CASE("Versions: the Navigator has Version History for a text, and its keys",
          "[gui][versions][navigator]") {
    VersionsWindow w;
    w.window.navigator.refreshNavigator();
    auto* tree = w.window.navigatorPanel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    QTreeWidgetItem* chapter = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == w.novel.chapterId) {
            chapter = *it;
        }
    }
    REQUIRE(chapter != nullptr);
    tree->setCurrentItem(chapter);
    QStringList requested;
    QObject::connect(w.window.navigatorPanel, &gui::NavigatorPanel::requestVersionHistory,
                     [&requested](const QString& elementId) { requested.append(elementId); });

    // In its context menu
    {
        w.window.window.setAttribute(Qt::WA_DontShowOnScreen);
        w.window.window.show();
        tree->expandAll();
        test::ContextMenuReader reader(w.window.navigatorPanel);
        reader.chooseWith(QLatin1Char('H'));
        emit tree->customContextMenuRequested(tree->visualItemRect(chapter).center());
        REQUIRE(reader.menusRead() == 1);
        bool listed = false;
        for (const test::MenuItem& item : reader.items()) {
            listed = listed || item.text.startsWith(QStringLiteral("Version &History..."));
        }
        CHECK(listed);
    }
    REQUIRE(requested.size() == 1);
    CHECK(requested.at(0) == w.novel.chapterId);

    // Its keys in the tree
#ifndef Q_OS_MACOS
    QKeyEvent press(QEvent::KeyPress, Qt::Key_H, Qt::AltModifier | Qt::ShiftModifier);
    QApplication::sendEvent(tree, &press);
    REQUIRE(requested.size() == 2);
    CHECK(requested.at(1) == w.novel.chapterId);
#endif
}

TEST_CASE("Versions: the Properties panel of a text shows its versions",
          "[gui][versions][properties]") {
    VersionsWindow w;
    gui::PropertiesPanel* panel = w.window.properties;
    panel->showChapterProperties(w.novel.chapterId);
    auto* group = panel->findChild<QGroupBox*>(QStringLiteral("chapterVersionsGroup"));
    auto* count = panel->findChild<QLabel*>(QStringLiteral("versionsCountLabel"));
    auto* last = panel->findChild<QLabel*>(QStringLiteral("versionsLastLabel"));
    REQUIRE(group != nullptr);
    REQUIRE(count != nullptr);
    REQUIRE(last != nullptr);
    CHECK_FALSE(group->isHidden());
    CHECK(count->text() == QStringLiteral("No versions yet"));
    CHECK(last->text() ==
          QStringLiteral("A version keeps the text of the chapter as it is in the editor now."));

    // A version kept shows at once
    const core::ChapterVersion version = kept(w.novel.chapterId, QStringLiteral("Dark night."),
                                              core::VersionOrigin::Manual,
                                              QStringLiteral("Before the storm"));
    CHECK(count->text() == QStringLiteral("1 version(s), the last one: %1")
                               .arg(gui::versionTime(version.created)));
    CHECK(last->text() == QStringLiteral("\"Before the storm\""));

    // Its buttons ask for the commands of the File menu, for this text
    QStringList save;
    QStringList history;
    QObject::connect(panel, &gui::PropertiesPanel::requestSaveVersion,
                     [&save](const QString& elementId) { save.append(elementId); });
    QObject::connect(panel, &gui::PropertiesPanel::requestVersionHistory,
                     [&history](const QString& elementId) { history.append(elementId); });
    panel->findChild<QPushButton*>(QStringLiteral("saveVersionButton"))->click();
    panel->findChild<QPushButton*>(QStringLiteral("versionHistoryButton"))->click();
    CHECK(save == QStringList{w.novel.chapterId});
    CHECK(history == QStringList{w.novel.chapterId});
}

// =============================================================================
// Settings > Editor > Versions
// =============================================================================

TEST_CASE("Versions: their settings page has the settings of the versions",
          "[gui][versions][settings]") {
    KeptVersionSettings settings;
    auto& manager = core::SettingsManager::getInstance();
    manager.set<bool>(core::ChapterVersions::ASK_NAME_SETTING, true);
    gui::EditorVersionsPage page;
    page.load();
    auto* ask = page.findChild<QCheckBox*>(QStringLiteral("versionsAskName"));
    auto* onSave = page.findChild<QCheckBox*>(QStringLiteral("versionsOnSave"));
    auto* minutes = page.findChild<QComboBox*>(QStringLiteral("versionsOnSaveMinutes"));
    auto* keep = page.findChild<QSpinBox*>(QStringLiteral("versionsKeepAutomatic"));
    REQUIRE(ask != nullptr);
    REQUIRE(onSave != nullptr);
    REQUIRE(minutes != nullptr);
    REQUIRE(keep != nullptr);
    CHECK(ask->isChecked());

    // Off, the versions kept on saving have no minutes to choose
    onSave->setChecked(false);
    CHECK_FALSE(minutes->isEnabled());
    onSave->setChecked(true);
    CHECK(minutes->isEnabled());

    // Saved without the window
    ask->setChecked(false);
    page.apply();
    CHECK_FALSE(manager.get<bool>(core::ChapterVersions::ASK_NAME_SETTING));
}

// =============================================================================
// The bar under the text
// =============================================================================

TEST_CASE("Versions: the bar under the text says what was done, with its button",
          "[gui][versions]") {
    gui::StandaloneInfoBar bar;
    int notice = 0;
    int add = 0;
    QObject::connect(&bar, &gui::StandaloneInfoBar::noticeActionClicked, [&notice]() { ++notice; });
    QObject::connect(&bar, &gui::StandaloneInfoBar::addToProjectClicked, [&add]() { ++add; });
    bar.showNotice(QStringLiteral("Restored."), QStringLiteral("&Undo"), QStringLiteral("Undo it"));
    CHECK(bar.showsNotice());
    CHECK(bar.message() == QStringLiteral("Restored."));
    auto* button = bar.findChild<QPushButton*>();
    REQUIRE(button != nullptr);
    CHECK(button->text() == QStringLiteral("&Undo"));
    CHECK(button->toolTip() == QStringLiteral("Undo it"));
    button->click();
    CHECK(notice == 1);
    CHECK(add == 0);

    // A file shows a file again
    bar.setFilePath(QStringLiteral("/tmp/notes.txt"));
    CHECK_FALSE(bar.showsNotice());
    button->click();
    CHECK(notice == 1);
}
