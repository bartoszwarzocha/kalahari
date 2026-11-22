// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_settings_search.cpp
/// @brief The search of the Settings dialog: the tree narrowed to the pages of the words, the
///        options found marked, its keys

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/settings/settings_search.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFont>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
#include <QThread>
#include <QTranslator>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

using namespace kalahari::gui;

namespace {

/// A page of the tree by its category ("" for a page at the top) and its name
QTreeWidgetItem* itemOf(const SettingsDialog& dialog, const QString& category,
                        const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        const QTreeWidgetItem* parent = (*it)->parent();
        if ((*it)->text(0) == title && (parent ? parent->text(0) : QString()) == category) {
            return *it;
        }
    }
    FAIL("No page " << title.toStdString());
    return nullptr;
}

/// Whether the tree shows a page: neither it nor its category is left out
bool shown(const QTreeWidgetItem* item) {
    return !item->isHidden() && (item->parent() == nullptr || !item->parent()->isHidden());
}

/// The pages the tree shows, as "Category > Page"
QStringList shownPages(const SettingsDialog& dialog) {
    QStringList pages;
    auto* tree = dialog.findChild<QTreeWidget*>();
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->childCount() == 0 && shown(*it)) {
            pages << ((*it)->parent() ? (*it)->parent()->text(0) + QStringLiteral(" > ") : QString()) +
                         (*it)->text(0);
        }
    }
    return pages;
}

void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent pressed(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &released);
}

/// The program in Polish while it lives: the translation it ships
class Polish {
public:
    Polish() {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~Polish() { QCoreApplication::removeTranslator(&m_translator); }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

}  // namespace

TEST_CASE("Settings search: the words without case, accents and access keys",
          "[gui][settings][search]") {
    CHECK(SettingsSearch::folded(QStringLiteral("Odstęp &górny:")) ==
          QStringLiteral("odstep gorny:"));
    CHECK(SettingsSearch::folded(QStringLiteral("Łódź  &&  Co")) ==
          QStringLiteral("lodz & co"));
    CHECK(SettingsSearch::folded(QStringLiteral("<b>Bold</b> text")) == QStringLiteral("bold text"));

    const SettingsSearch search(QStringLiteral("  GORNY  odst "));
    CHECK(search.matches(QStringLiteral("Odstęp &górny:")));
    CHECK_FALSE(search.matches(QStringLiteral("Odstęp dolny:")));
    CHECK_FALSE(SettingsSearch(QStringLiteral("   ")).matches(QStringLiteral("anything")));
    CHECK(SettingsSearch(QStringLiteral("   ")).isEmpty());
}

TEST_CASE("Settings search: the options of a page are the texts the writer reads",
          "[gui][settings][search]") {
    QWidget page;
    auto* layout = new QVBoxLayout(&page);
    auto* label = new QLabel(QStringLiteral("Space &above:"));
    auto* box = new QCheckBox(QStringLiteral("Keep the space above"));
    auto* field = new QLineEdit();
    field->setText(QStringLiteral("space above, typed by the writer"));
    auto* hidden = new QLabel(QStringLiteral("Space above, hidden"));
    hidden->hide();
    auto* group = new QGroupBox(QStringLiteral("Other options"));
    auto* inGroup = new QVBoxLayout(group);
    inGroup->addWidget(new QCheckBox(QStringLiteral("One")));
    inGroup->addWidget(new QCheckBox(QStringLiteral("Two")));
    layout->addWidget(label);
    layout->addWidget(field);
    layout->addWidget(box);
    layout->addWidget(hidden);
    layout->addWidget(group);
    page.resize(300, 300);
    page.show();
    QApplication::processEvents();

    // The label and the check box, from the top; not the value of the field, not the hidden
    const std::vector<QWidget*> found = optionsFound(&page, SettingsSearch(QStringLiteral("space above")));
    CHECK(found == std::vector<QWidget*>{label, box});
    CHECK(optionsFound(&page, SettingsSearch()).empty());

    // The marks over them, around each option
    SearchMarks marks(&page);
    marks.setOptions(found);
    CHECK(marks.isVisible());
    CHECK(marks.markOf(label).contains(label->geometry()));
    // A group: its title, not its options
    CHECK(optionsFound(&page, SettingsSearch(QStringLiteral("other"))) == std::vector<QWidget*>{group});
    CHECK(marks.markOf(group).height() < group->height() / 2);
    CHECK(marks.markOf(group).top() <= group->geometry().top());
    CHECK(marks.markColor().isValid());
    marks.setOptions({});
    CHECK_FALSE(marks.isVisible());
}

TEST_CASE("Settings search: the marks take the color of the search's hits of the editor",
          "[gui][settings][search][colors]") {
    auto& themes = kalahari::core::ThemeManager::getInstance();
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string setting = kalahari::core::ThemeManager::editorColorSetting(
        themes.getCurrentTheme().name, kalahari::editor::SEARCH_MATCH_COLOR_KEY);
    settings.removeKey(setting);
    QApplication::processEvents();  // the theme follows the settings
    const QColor before = SearchMarks::markColor();
    CHECK(before == themes.editorColor(kalahari::editor::SEARCH_MATCH_COLOR_KEY, QColor()));

    QWidget page;
    SearchMarks marks(&page);
    int repaints = 0;
    QObject::connect(&themes, &kalahari::core::ThemeManager::editorColorsChanged, &marks,
                     [&repaints]() { ++repaints; });
    const QColor chosen(QStringLiteral("#d000d0"));
    settings.set<std::string>(setting, chosen.name().toStdString());
    QApplication::processEvents();
    CHECK(repaints > 0);
    CHECK(SearchMarks::markColor() == chosen);

    settings.removeKey(setting);
    QApplication::processEvents();
    CHECK(SearchMarks::markColor() == before);
}

TEST_CASE("Settings search: the tree narrows to the pages of the words, their options marked",
          "[gui][settings][search]") {
    SettingsDialog dialog(kalahari::test::book(), kalahari::test::commands(), nullptr);
    dialog.resize(900, 700);
    dialog.show();
    QApplication::processEvents();
    QTreeWidgetItem* images = itemOf(dialog, QStringLiteral("Editor"), QStringLiteral("Images"));
    QTreeWidgetItem* tables = itemOf(dialog, QStringLiteral("Editor"), QStringLiteral("Tables"));
    QTreeWidgetItem* cursor = itemOf(dialog, QStringLiteral("Editor"), QStringLiteral("Cursor"));
    auto* tree = dialog.findChild<QTreeWidget*>();
    tree->setCurrentItem(images);
    const QStringList everyPage = shownPages(dialog);

    // Options of two pages; the page shown stays, its options marked from the top
    dialog.findSettings(QStringLiteral("SPACE abov"));
    CHECK(shown(images));
    CHECK(shown(tables));
    CHECK_FALSE(shown(cursor));
    CHECK(images->font(0).bold());
    CHECK(tree->currentItem() == images);
    const std::vector<QWidget*> marked = dialog.markedOptions();
    REQUIRE_FALSE(marked.empty());
    for (QWidget* option : marked) {
        const auto* label = qobject_cast<QLabel*>(option);
        REQUIRE(label != nullptr);
        CHECK(SettingsSearch(QStringLiteral("space abov")).matches(label->text()));
    }
    CHECK(qobject_cast<QLabel*>(marked.front())->text() == QStringLiteral("Space abov&e:"));
    CHECK(shownPages(dialog).size() < everyPage.size());

    // Another page shown: its own marks, none left on the first
    tree->setCurrentItem(tables);
    CHECK_FALSE(dialog.markedOptions().empty());
    CHECK(dialog.markedOptions().front()->window() == &dialog);

    // An option of the advanced settings: found, and they open to show it
    QTreeWidgetItem* pages = itemOf(dialog, QStringLiteral("Editor"), QStringLiteral("Pages and Margins"));
    tree->setCurrentItem(pages);
    dialog.findSettings(QStringLiteral("text frame border"));
    CHECK(shown(pages));
    CHECK(pages->font(0).bold());
    REQUIRE(tree->currentItem() == pages);
    const std::vector<QWidget*> inAdvanced = dialog.markedOptions();
    REQUIRE_FALSE(inAdvanced.empty());
    CHECK(std::all_of(inAdvanced.cbegin(), inAdvanced.cend(),
                      [](const QWidget* option) { return option->isVisible(); }));
    const auto opened = std::find_if(inAdvanced.cbegin(), inAdvanced.cend(), [](QWidget* option) {
        return qobject_cast<QCheckBox*>(option) != nullptr;
    });
    CHECK(opened != inAdvanced.cend());
    QToolButton* advanced = nullptr;
    for (QWidget* each = inAdvanced.front(); each != nullptr && advanced == nullptr;
         each = each->parentWidget()) {
        advanced = each->findChild<QToolButton*>(QStringLiteral("advancedSettings"),
                                                 Qt::FindDirectChildrenOnly);
    }
    REQUIRE(advanced != nullptr);
    CHECK(advanced->isChecked());

    // A category's name shows all its pages; the name of a page shows it
    dialog.findSettings(QStringLiteral("statistics and goals"));
    QStringList statistics = shownPages(dialog).filter(QStringLiteral("Statistics and Goals > "));
    CHECK(statistics.size() == 4);
    dialog.findSettings(QStringLiteral("cursor"));
    CHECK(shown(cursor));

    // Nothing: the tree is empty and says so
    auto* empty = dialog.findChild<QLabel*>(QStringLiteral("settingsSearchEmpty"));
    REQUIRE(empty != nullptr);
    CHECK_FALSE(empty->isVisible());
    dialog.findSettings(QStringLiteral("xyzzy"));
    CHECK(shownPages(dialog).isEmpty());
    CHECK(empty->isVisible());
    CHECK(empty->text().contains(QStringLiteral("xyzzy")));

    // Without words: every page again, in a plain font, without marks
    dialog.findSettings(QString());
    CHECK(shownPages(dialog) == everyPage);
    CHECK_FALSE(empty->isVisible());
    CHECK_FALSE(images->font(0).bold());
    CHECK(dialog.markedOptions().empty());

    // A page opened from outside while the search leaves it out: every page again
    dialog.findSettings(QStringLiteral("space abov"));
    REQUIRE_FALSE(shown(itemOf(dialog, QString(), QStringLiteral("Plot Board"))));
    CHECK(dialog.openPage(QString::fromLatin1(SettingsDialog::PLOT_BOARD_PAGE)));
    CHECK(dialog.searchField()->text().isEmpty());
    CHECK(shownPages(dialog) == everyPage);
}

TEST_CASE("Settings search: the keys of the search", "[gui][settings][search][keys]") {
    SettingsDialog dialog(kalahari::test::book(), kalahari::test::commands(), nullptr);
    dialog.resize(900, 700);
    dialog.show();
    QApplication::processEvents();
    auto* tree = dialog.findChild<QTreeWidget*>();
    QLineEdit* search = dialog.searchField();
    REQUIRE(search != nullptr);
    tree->setCurrentItem(itemOf(dialog, QStringLiteral("Editor"), QStringLiteral("Images")));

    // Ctrl+F: the cursor in the search
    const auto shortcuts = dialog.findChildren<QShortcut*>();
    const auto find = std::find_if(shortcuts.cbegin(), shortcuts.cend(), [](const QShortcut* each) {
        return each->key() == QKeySequence(Qt::ControlModifier | Qt::Key_F);
    });
    REQUIRE(find != shortcuts.cend());
    CHECK(search->placeholderText().contains(
        ShortcutRules::keysText(QKeyCombination(Qt::ControlModifier, Qt::Key_F))));
    tree->setFocus();
    emit (*find)->activated();
    CHECK(dialog.focusWidget() == search);

    // Typed words narrow the tree after a short pause
    search->setText(QStringLiteral("space abov"));
    for (int pass = 0; pass < 20 && dialog.markedOptions().empty(); ++pass) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(20);
    }
    CHECK_FALSE(dialog.markedOptions().empty());

    // Enter: the field of the first option found, not OK of the dialog
    press(search, Qt::Key_Return);
    CHECK(dialog.isVisible());
    CHECK(qobject_cast<QAbstractSpinBox*>(dialog.focusWidget()) != nullptr);

    // Down: the tree
    search->setFocus();
    press(search, Qt::Key_Down);
    CHECK(dialog.focusWidget() == tree);

    // Esc: the words cleared, the dialog stays; with no words it closes the dialog
    search->setFocus();
    press(search, Qt::Key_Escape);
    CHECK(search->text().isEmpty());
    CHECK(dialog.isVisible());
    CHECK(dialog.markedOptions().empty());
    press(search, Qt::Key_Escape);
    CHECK_FALSE(dialog.isVisible());
}

TEST_CASE("Settings search: Polish words without the Polish letters", "[gui][settings][search]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    SettingsDialog dialog(kalahari::test::book(), kalahari::test::commands(), nullptr);
    dialog.resize(900, 700);
    dialog.show();
    QApplication::processEvents();
    auto* tree = dialog.findChild<QTreeWidget*>();
    QTreeWidgetItem* images = itemOf(dialog, QStringLiteral("Edytor"), QStringLiteral("Obrazy"));
    tree->setCurrentItem(images);

    dialog.findSettings(QStringLiteral("odstep gorny"));
    CHECK(shown(images));
    const std::vector<QWidget*> marked = dialog.markedOptions();
    REQUIRE_FALSE(marked.empty());
    CHECK(qobject_cast<QLabel*>(marked.front())->text() == QStringLiteral("Odstęp &górny:"));
    CHECK(dialog.searchField()->placeholderText().startsWith(QStringLiteral("Szukaj")));
}
