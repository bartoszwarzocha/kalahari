/// @file test_settings_dialog.cpp
/// @brief The settings dialog: lazily built pages, their layout and what Apply writes

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/annotations_coordinator.h"
#include "kalahari/gui/material_downloader.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari/core/theme_manager.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <optional>
#include <utility>
#include <vector>

using namespace kalahari::gui;

namespace {

/// Brings back the theme the test started with
class ThemeRestorer {
public:
    ThemeRestorer()
        : m_name(QString::fromStdString(
              kalahari::core::ThemeManager::getInstance().getCurrentTheme().name))
    {
    }
    ~ThemeRestorer() { kalahari::core::ThemeManager::getInstance().reloadTheme(m_name); }
    ThemeRestorer(const ThemeRestorer&) = delete;
    ThemeRestorer& operator=(const ThemeRestorer&) = delete;

private:
    QString m_name;
};

/// The color a label's text is drawn in (its style sheet's, or the palette's)
QColor textColorOf(QLabel* label) {
    label->ensurePolished();
    return label->palette().color(label->foregroundRole());
}

/// The color inside a group box on the window, as it is drawn
QColor groupFillOn(const QColor& window) {
    QGroupBox group;
    QPalette palette = group.palette();
    palette.setColor(QPalette::Window, window);
    group.setPalette(palette);
    group.setAutoFillBackground(true);
    group.resize(80, 80);
    return group.grab().toImage().pixelColor(40, 60);
}

/// Open every page of the dialog (pages are built when first opened)
void openAllPages(SettingsDialog& dialog) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        tree->setCurrentItem(*it);
    }
}

/// Open the page with a title under a category ("" for a top-level page)
void openPage(SettingsDialog& dialog, const QString& category, const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        QTreeWidgetItem* parent = (*it)->parent();
        if ((*it)->text(0) == title && (parent ? parent->text(0) : QString()) == category) {
            tree->setCurrentItem(*it);
            return;
        }
    }
    FAIL("No page " << title.toStdString());
}

QPushButton* applyButton(SettingsDialog& dialog) {
    REQUIRE(dialog.applyButton()->isVisibleTo(&dialog));
    return dialog.applyButton();
}

ColorConfigWidget* colorWidget(SettingsDialog& dialog, const QString& toolTip) {
    for (ColorConfigWidget* widget : dialog.findChildren<ColorConfigWidget*>()) {
        if (widget->toolTip() == toolTip) {
            return widget;
        }
    }
    return nullptr;
}

/// The layouts of a shown window settle
void settle() {
    for (int pass = 0; pass < 3; ++pass) {
        QApplication::processEvents();
    }
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

/// The program's style (Fusion, on every system) while it lives, then the system's style again
class ProgramStyle {
public:
    ProgramStyle()
        : m_systemStyle(QApplication::style()->name()) {
        QApplication::setStyle(QStringLiteral("Fusion"));
    }
    ~ProgramStyle() { QApplication::setStyle(m_systemStyle); }
    ProgramStyle(const ProgramStyle&) = delete;
    ProgramStyle& operator=(const ProgramStyle&) = delete;

private:
    QString m_systemStyle;
};

void clearThemeColors(const std::string& theme) {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    settings.clearCustomPaletteColorsForTheme(theme);
    settings.clearCustomLogColorsForTheme(theme);
    settings.clearCustomUiColorsForTheme(theme);
    settings.clearCustomIconColorsForTheme(theme);
}

}  // anonymous namespace

TEST_CASE("Settings dialog: pages are built when first opened", "[gui][settings]") {
    SettingsDialog dialog(nullptr);
    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    CHECK(stack->count() == 1);  // The page shown first

    openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Cursor"));
    CHECK(stack->count() == 2);
    openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Cursor"));
    CHECK(stack->count() == 2);
}

TEST_CASE("Settings dialog: the gears of the panels open it at their pages", "[gui][settings]") {
    SettingsDialog dialog(nullptr);
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);

    // The gear of the Spelling and Grammar panel: Editor > Spelling
    CHECK(dialog.openPage(QString::fromLatin1(SettingsDialog::SPELLING_PAGE)));
    REQUIRE(tree->currentItem() != nullptr);
    CHECK(tree->currentItem()->text(0) == QStringLiteral("Spelling"));
    REQUIRE(tree->currentItem()->parent() != nullptr);
    CHECK(tree->currentItem()->parent()->text(0) == QStringLiteral("Editor"));

    // The gear of the Annotations panel: Annotations
    CHECK(dialog.openPage(QString::fromLatin1(SettingsDialog::ANNOTATIONS_PAGE)));
    REQUIRE(tree->currentItem() != nullptr);
    CHECK(tree->currentItem()->text(0) == QStringLiteral("Annotations"));
    CHECK(tree->currentItem()->parent() == nullptr);

    // The gear of the group of the versions in the Properties panel: Editor > Versions
    CHECK(dialog.openPage(QString::fromLatin1(SettingsDialog::VERSIONS_PAGE)));
    REQUIRE(tree->currentItem() != nullptr);
    CHECK(tree->currentItem()->text(0) == QStringLiteral("Versions"));
    REQUIRE(tree->currentItem()->parent() != nullptr);
    CHECK(tree->currentItem()->parent()->text(0) == QStringLiteral("Editor"));

    // A page the dialog does not have leaves the page shown
    CHECK_FALSE(dialog.openPage(QStringLiteral("nowhere")));
    CHECK(tree->currentItem()->text(0) == QStringLiteral("Versions"));
}

TEST_CASE("Settings dialog: no page squeezes its groups", "[gui][settings]") {
    // Regression: the dialog kept its size whatever a page needed, so the groups of
    // Editor > Pages and Margins were squeezed until their fields overlapped, and the
    // description of Editor > General > Typewriter Scrolling was cut off. In the program's
    // style (Fusion) on every system: in the macOS style a group whose fields go under their
    // labels in a narrow window measures a height that depends on the widths measured before
    ProgramStyle programStyle;
    SettingsDialog dialog(nullptr);
    dialog.resize(dialog.minimumSize());
    dialog.show();
    openAllPages(dialog);
    QApplication::processEvents();

    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    REQUIRE(stack->count() > 10);

    for (int index = 0; index < stack->count(); ++index) {
        stack->setCurrentIndex(index);
        dialog.layout()->activate();
        QApplication::processEvents();

        QWidget* page = stack->widget(index);
        if (auto* scrollArea = qobject_cast<QScrollArea*>(page)) {
            page = scrollArea->widget();
        }
        REQUIRE(page != nullptr);
        REQUIRE(page->layout() != nullptr);

        // The page is as tall as its contents need at its width (a word-wrapped label
        // needs more height in a narrower page)
        const int needed = page->hasHeightForWidth()
            ? page->heightForWidth(page->width())
            : page->minimumSizeHint().height();
        INFO("page " << index << ": " << page->height() << " px high, needs " << needed);
        CHECK(page->height() >= needed);
        CHECK(page->width() >= page->minimumSizeHint().width());
    }
}

TEST_CASE("Settings dialog: every page fits the narrowest window, in English and in Polish",
          "[gui][settings]") {
    // The dialog narrowed as far as it goes, as on a small screen (1366x768 at 125% or 150%):
    // a page may scroll down, never sideways. Regression: Editor > General, Pages and Margins
    // and Keyboard Shortcuts were wider than the window in Polish, Keyboard Shortcuts also in
    // English. In the program's style (Fusion) on every system
    ProgramStyle programStyle;

    for (const bool inPolish : {false, true}) {
        INFO((inPolish ? "in Polish" : "in English"));
        std::optional<Polish> polish;
        if (inPolish) {
            polish.emplace();
            REQUIRE(polish->loaded());
        }
        SettingsDialog dialog(nullptr);
        dialog.resize(dialog.minimumSize());
        dialog.show();
        auto* tree = dialog.findChild<QTreeWidget*>();
        auto* stack = dialog.findChild<QStackedWidget*>();
        REQUIRE(tree != nullptr);
        REQUIRE(stack != nullptr);

        int pages = 0;
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            INFO("page " << (*it)->text(0).toStdString());
            tree->setCurrentItem(*it);
            settle();
            const auto* scrollArea = qobject_cast<QScrollArea*>(stack->currentWidget());
            REQUIRE(scrollArea != nullptr);
            REQUIRE(scrollArea->widget() != nullptr);
            CHECK(scrollArea->widget()->minimumSizeHint().width() <=
                  scrollArea->viewport()->width());
            CHECK_FALSE(scrollArea->horizontalScrollBar()->isVisible());
            ++pages;
        }
        CHECK(pages > 20);
    }
}

TEST_CASE("Settings dialog: Apply writes only the changed options", "[gui][settings]") {
    // Regression: Apply wrote every option, including all theme colors, so the theme's
    // own colors were frozen into the settings file and later theme fixes never showed
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string theme = settings.getTheme();
    clearThemeColors(theme);
    settings.setLanguage("en");

    SettingsDialog dialog(nullptr);
    openAllPages(dialog);
    CHECK_FALSE(dialog.hasChanges());

    QStringList applied;
    int appliedCount = 0;
    QObject::connect(&dialog, &SettingsDialog::settingsApplied, [&](const QStringList& keys) {
        applied = keys;
        ++appliedCount;
    });

    // Change only the language
    QComboBox* language = nullptr;
    for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
        if (combo->findData("pl") >= 0) {
            language = combo;
        }
    }
    REQUIRE(language != nullptr);
    language->setCurrentIndex(language->findData("pl"));
    CHECK(dialog.hasChanges());
    applyButton(dialog)->click();

    CHECK(appliedCount == 1);
    CHECK(applied == QStringList{QStringLiteral("ui.language")});
    CHECK(settings.getLanguage() == "pl");
    CHECK_FALSE(settings.hasCustomPaletteColorsForTheme(theme));
    CHECK_FALSE(settings.hasCustomLogColorsForTheme(theme));
    CHECK_FALSE(settings.hasCustomUiColorsForTheme(theme));
    CHECK_FALSE(settings.hasCustomIconColorsForTheme(theme));

    // Nothing changed since the last Apply: nothing is applied again
    CHECK_FALSE(dialog.hasChanges());
    applyButton(dialog)->click();
    CHECK(appliedCount == 1);

    settings.setLanguage("en");
}

TEST_CASE("Settings dialog: OK writes the changes and closes, Apply keeps it open", "[gui][settings]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    settings.setLanguage("en");
    SettingsDialog dialog(nullptr);
    CHECK(dialog.windowTitle() == QStringLiteral("Settings"));
    openAllPages(dialog);

    QStringList applied;
    QObject::connect(&dialog, &SettingsDialog::settingsApplied,
                     [&applied](const QStringList& keys) { applied += keys; });
    QComboBox* language = nullptr;
    for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
        if (combo->findData("pl") >= 0) {
            language = combo;
        }
    }
    REQUIRE(language != nullptr);

    dialog.show();
    language->setCurrentIndex(language->findData("pl"));
    applyButton(dialog)->click();
    CHECK(dialog.isVisible());
    CHECK(applied == QStringList{QStringLiteral("ui.language")});

    language->setCurrentIndex(language->findData("en"));
    dialog.acceptButton()->click();
    CHECK_FALSE(dialog.isVisible());
    CHECK(dialog.result() == QDialog::Accepted);
    CHECK(applied == (QStringList{QStringLiteral("ui.language"), QStringLiteral("ui.language")}));
    CHECK(settings.getLanguage() == "en");
}

TEST_CASE("Settings dialog: the Annotations page has the author and the size of the marks",
          "[gui][settings]") {
    // The user's test: the size of the marks was lost in Editor > General. The annotations
    // have a page of their own, with the author of new ones as well.
    auto& settings = kalahari::core::SettingsManager::getInstance();
    settings.set<std::string>("annotations.author", "");
    settings.set<int>("editor.annotationMarkSize", 100);

    SettingsDialog dialog(nullptr);
    openPage(dialog, QString(), QStringLiteral("Annotations"));
    auto* author = dialog.findChild<QLineEdit*>(QStringLiteral("annotationsAuthor"));
    auto* markSize = dialog.findChild<QSpinBox*>(QStringLiteral("annotationsMarkSize"));
    REQUIRE(author != nullptr);
    REQUIRE(markSize != nullptr);
    CHECK(author->text().isEmpty());
    // An empty field says what goes in it: no one is put in its place
    CHECK(author->placeholderText() == QStringLiteral("Your name or pen name"));
    CHECK(markSize->value() == 100);
    CHECK_FALSE(dialog.hasChanges());

    QStringList applied;
    QObject::connect(&dialog, &SettingsDialog::settingsApplied,
                     [&applied](const QStringList& keys) { applied = keys; });
    author->setText(QStringLiteral("Anna Nowak"));
    markSize->setValue(150);
    CHECK(dialog.hasChanges());
    applyButton(dialog)->click();
    CHECK(applied.contains(QStringLiteral("annotations.author")));
    CHECK(applied.contains(QStringLiteral("editor.annotationMarkSize")));
    CHECK(settings.get<std::string>("annotations.author") == "Anna Nowak");
    CHECK(settings.get<int>("editor.annotationMarkSize") == 150);
    CHECK(AnnotationsCoordinator::author() == QStringLiteral("Anna Nowak"));

    // The size of the marks is no longer in Editor > General
    openPage(dialog, QStringLiteral("Editor"), QStringLiteral("General"));
    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    for (const QSpinBox* spin : stack->currentWidget()->findChildren<QSpinBox*>()) {
        CHECK(spin->suffix() != QStringLiteral(" %"));
    }
}

TEST_CASE("Settings dialog: the Navigator page chooses the commands that add in the menus",
          "[gui][settings]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string before =
        settings.get<std::string>(NavigatorPanel::ADD_COMMANDS_SETTING, "named");
    settings.set<std::string>(NavigatorPanel::ADD_COMMANDS_SETTING, "named");

    SettingsDialog dialog(nullptr);
    openPage(dialog, QString(), QStringLiteral("Navigator"));
    auto* named = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorAddNamed"));
    auto* single = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorAddSingle"));
    REQUIRE(named != nullptr);
    REQUIRE(single != nullptr);
    CHECK(named->isChecked());
    CHECK_FALSE(single->isChecked());
    CHECK_FALSE(dialog.hasChanges());

    single->click();
    CHECK(dialog.hasChanges());
    CHECK(dialog.applyChanges() ==
          QStringList{QString::fromLatin1(NavigatorPanel::ADD_COMMANDS_SETTING)});
    CHECK(settings.get<std::string>(NavigatorPanel::ADD_COMMANDS_SETTING) == "single");

    // Opened again, the page shows the stored choice
    SettingsDialog again(nullptr);
    openPage(again, QString(), QStringLiteral("Navigator"));
    auto* singleAgain = again.findChild<QRadioButton*>(QStringLiteral("navigatorAddSingle"));
    REQUIRE(singleAgain != nullptr);
    CHECK(singleAgain->isChecked());

    settings.set<std::string>(NavigatorPanel::ADD_COMMANDS_SETTING, before);
}

TEST_CASE("Settings dialog: the Navigator page chooses the top of the Add Element window",
          "[gui][settings]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string before =
        settings.get<std::string>(NavigatorPanel::ADD_WINDOW_SETTING, "sections");
    settings.set<std::string>(NavigatorPanel::ADD_WINDOW_SETTING, "sections");

    SettingsDialog dialog(nullptr);
    openPage(dialog, QString(), QStringLiteral("Navigator"));
    auto* sections = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorAddWindowSections"));
    auto* book = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorAddWindowBook"));
    auto* place = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorAddWindowPlace"));
    REQUIRE(sections != nullptr);
    REQUIRE(book != nullptr);
    REQUIRE(place != nullptr);
    CHECK(sections->isChecked());
    CHECK_FALSE(book->isChecked());
    CHECK_FALSE(place->isChecked());
    CHECK(NavigatorPanel::addChooser() == kalahari::gui::AddChooser::Sections);

    // A choice of its own, apart from the commands of the menus
    auto* named = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorAddNamed"));
    REQUIRE(named != nullptr);
    const bool namedBefore = named->isChecked();
    place->click();
    CHECK(named->isChecked() == namedBefore);
    CHECK(dialog.applyChanges() ==
          QStringList{QString::fromLatin1(NavigatorPanel::ADD_WINDOW_SETTING)});
    CHECK(settings.get<std::string>(NavigatorPanel::ADD_WINDOW_SETTING) == "place");
    CHECK(NavigatorPanel::addChooser() == kalahari::gui::AddChooser::None);
    book->click();
    CHECK(dialog.applyChanges() ==
          QStringList{QString::fromLatin1(NavigatorPanel::ADD_WINDOW_SETTING)});
    CHECK(NavigatorPanel::addChooser() == kalahari::gui::AddChooser::Book);

    // Opened again, the page shows the stored choice
    SettingsDialog again(nullptr);
    openPage(again, QString(), QStringLiteral("Navigator"));
    auto* bookAgain = again.findChild<QRadioButton*>(QStringLiteral("navigatorAddWindowBook"));
    REQUIRE(bookAgain != nullptr);
    CHECK(bookAgain->isChecked());

    settings.set<std::string>(NavigatorPanel::ADD_WINDOW_SETTING, before);
}

TEST_CASE("Settings dialog: the Navigator page chooses how a material opens",
          "[gui][settings]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string before =
        settings.get<std::string>(NavigatorPanel::OPEN_MATERIALS_SETTING, "card");
    settings.set<std::string>(NavigatorPanel::OPEN_MATERIALS_SETTING, "card");

    SettingsDialog dialog(nullptr);
    openPage(dialog, QString(), QStringLiteral("Navigator"));
    auto* card = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorOpenMaterialsCard"));
    auto* system = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorOpenMaterialsSystem"));
    REQUIRE(card != nullptr);
    REQUIRE(system != nullptr);
    CHECK(card->isChecked());
    CHECK_FALSE(system->isChecked());

    // A choice of its own, apart from those of the window and the menus
    auto* sections = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorAddWindowSections"));
    REQUIRE(sections != nullptr);
    const bool sectionsBefore = sections->isChecked();
    system->click();
    CHECK(sections->isChecked() == sectionsBefore);
    CHECK(dialog.applyChanges() ==
          QStringList{QString::fromLatin1(NavigatorPanel::OPEN_MATERIALS_SETTING)});
    CHECK(settings.get<std::string>(NavigatorPanel::OPEN_MATERIALS_SETTING) == "system");

    // Opened again, the page shows the stored choice
    SettingsDialog again(nullptr);
    openPage(again, QString(), QStringLiteral("Navigator"));
    auto* systemAgain =
        again.findChild<QRadioButton*>(QStringLiteral("navigatorOpenMaterialsSystem"));
    REQUIRE(systemAgain != nullptr);
    CHECK(systemAgain->isChecked());

    settings.set<std::string>(NavigatorPanel::OPEN_MATERIALS_SETTING, before);
}

TEST_CASE("Settings dialog: the Navigator page says whether links name paragraphs",
          "[gui][settings][links]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const bool before = settings.get<bool>(kalahari::core::LinkRoles::PARAGRAPHS_SETTING, true);
    settings.set<bool>(kalahari::core::LinkRoles::PARAGRAPHS_SETTING, true);

    // They do, as usual
    SettingsDialog dialog(nullptr);
    openPage(dialog, QString(), QStringLiteral("Navigator"));
    auto* paragraphs = dialog.findChild<QCheckBox*>(QStringLiteral("navigatorLinkParagraphs"));
    REQUIRE(paragraphs != nullptr);
    CHECK(paragraphs->isChecked());

    // Turned off, the links name none
    paragraphs->click();
    CHECK(dialog.applyChanges() ==
          QStringList{QString::fromLatin1(kalahari::core::LinkRoles::PARAGRAPHS_SETTING)});
    CHECK_FALSE(kalahari::core::LinkRoles::paragraphsShown());

    // Opened again, the page shows it
    SettingsDialog again(nullptr);
    openPage(again, QString(), QStringLiteral("Navigator"));
    auto* paragraphsAgain =
        again.findChild<QCheckBox*>(QStringLiteral("navigatorLinkParagraphs"));
    REQUIRE(paragraphsAgain != nullptr);
    CHECK_FALSE(paragraphsAgain->isChecked());

    settings.set<bool>(kalahari::core::LinkRoles::PARAGRAPHS_SETTING, before);
}

TEST_CASE("Settings dialog: the Navigator page chooses what the Materials show and where a "
          "list of materials opens", "[gui][settings][materials]") {
    using kalahari::gui::MaterialList;
    auto& settings = kalahari::core::SettingsManager::getInstance();

    // The usual ones: the kinds with their numbers, each on a tab of its own
    SettingsDialog dialog(nullptr);
    openPage(dialog, QString(), QStringLiteral("Navigator"));
    auto* types = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorMaterialsTypes"));
    auto* expand = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorMaterialsExpand"));
    auto* own = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorMaterialsOwnTabs"));
    auto* one = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorMaterialsOneTab"));
    REQUIRE(types != nullptr);
    REQUIRE(expand != nullptr);
    REQUIRE(own != nullptr);
    REQUIRE(one != nullptr);
    CHECK(types->isChecked());
    CHECK(own->isChecked());
    CHECK_FALSE(NavigatorPanel::materialsExpandable());
    CHECK(MaterialList::ownTabs());

    // Each row is a choice of its own
    expand->click();
    CHECK(expand->isChecked());
    CHECK(own->isChecked());
    one->click();
    CHECK(expand->isChecked());
    const QStringList written = dialog.applyChanges();
    CHECK(written.size() == 2);
    CHECK(written.contains(QString::fromLatin1(NavigatorPanel::MATERIALS_TREE_SETTING)));
    CHECK(written.contains(QString::fromLatin1(MaterialList::TABS_SETTING)));
    CHECK(settings.get<std::string>(NavigatorPanel::MATERIALS_TREE_SETTING) == "expand");
    CHECK(settings.get<std::string>(MaterialList::TABS_SETTING) == "one");
    CHECK(NavigatorPanel::materialsExpandable());
    CHECK_FALSE(MaterialList::ownTabs());

    // Opened again, the page shows the stored choices
    SettingsDialog again(nullptr);
    openPage(again, QString(), QStringLiteral("Navigator"));
    auto* expandAgain = again.findChild<QRadioButton*>(QStringLiteral("navigatorMaterialsExpand"));
    auto* oneAgain = again.findChild<QRadioButton*>(QStringLiteral("navigatorMaterialsOneTab"));
    REQUIRE(expandAgain != nullptr);
    REQUIRE(oneAgain != nullptr);
    CHECK(expandAgain->isChecked());
    CHECK(oneAgain->isChecked());
}

TEST_CASE("Settings dialog: the settings of the Materials on the Navigator page keep apart",
          "[gui][settings][materials]") {
    // Regression: the notes of a setting ran into the next setting, with no more room between
    // two settings than between the choices of one
    ProgramStyle programStyle;
    SettingsDialog dialog(nullptr);
    dialog.resize(900, 700);
    dialog.show();
    openPage(dialog, QString(), QStringLiteral("Navigator"));
    settle();

    auto* types = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorMaterialsTypes"));
    auto* expand = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorMaterialsExpand"));
    auto* own = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorMaterialsOwnTabs"));
    auto* card = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorOpenMaterialsCard"));
    auto* browser = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorOpenPagesBrowser"));
    auto* link = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorQuickMaterialsLink"));
    auto* paragraphs = dialog.findChild<QCheckBox*>(QStringLiteral("navigatorLinkParagraphs"));
    REQUIRE(types != nullptr);
    REQUIRE(expand != nullptr);
    REQUIRE(own != nullptr);
    REQUIRE(card != nullptr);
    REQUIRE(browser != nullptr);
    REQUIRE(link != nullptr);
    REQUIRE(paragraphs != nullptr);

    // The choices of a setting with their notes, in the group of the Materials
    QWidget* const tree = types->parentWidget();
    QWidget* const group = tree->parentWidget();
    REQUIRE(qobject_cast<QGroupBox*>(group) != nullptr);
    QLabel* dropNote = nullptr;
    for (QLabel* label : group->findChildren<QLabel*>()) {
        if (label->text().startsWith(QStringLiteral("Applies to a file dragged"))) {
            dropNote = label;
        }
    }
    REQUIRE(dropNote != nullptr);
    auto* typesNote = qobject_cast<QLabel*>(tree->layout()->itemAt(1)->widget());
    REQUIRE(typesNote != nullptr);

    const auto top = [group](const QWidget* widget) { return widget->mapTo(group, QPoint()).y(); };
    const auto gap = [group, &top](const QWidget* above, const QWidget* below) {
        return top(below) - above->mapTo(group, QPoint(0, above->height())).y();
    };
    const int line = group->fontMetrics().height();
    const int within = gap(typesNote, expand);
    INFO("a line is " << line << " px; the choices of one setting are " << within
                      << " px apart");

    const std::vector<std::pair<const QWidget*, const QWidget*>> settings{
        {tree, own->parentWidget()},
        {own->parentWidget(), card->parentWidget()},
        {card->parentWidget(), browser->parentWidget()},
        {browser->parentWidget(), link->parentWidget()},
        {dropNote, paragraphs}};
    for (const auto& [above, below] : settings) {
        INFO("two settings " << gap(above, below) << " px apart");
        CHECK(gap(above, below) >= line);
        CHECK(gap(above, below) > within);
    }
}

TEST_CASE("Settings dialog: the Navigator page chooses how pages open and what a drop gives",
          "[gui][settings]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string pagesBefore =
        settings.get<std::string>(NavigatorPanel::OPEN_PAGES_SETTING, "browser");
    const std::string quickBefore =
        settings.get<std::string>(NavigatorPanel::QUICK_MATERIALS_SETTING, "link");
    const std::string materialsBefore =
        settings.get<std::string>(NavigatorPanel::OPEN_MATERIALS_SETTING, "card");
    settings.set<std::string>(NavigatorPanel::OPEN_PAGES_SETTING, "browser");
    settings.set<std::string>(NavigatorPanel::QUICK_MATERIALS_SETTING, "link");
    settings.set<std::string>(NavigatorPanel::OPEN_MATERIALS_SETTING, "card");

    SettingsDialog dialog(nullptr);
    openPage(dialog, QString(), QStringLiteral("Navigator"));
    auto* browser = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorOpenPagesBrowser"));
    auto* onCard = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorOpenPagesCard"));
    auto* link = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorQuickMaterialsLink"));
    auto* add = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorQuickMaterialsAdd"));
    auto* window = dialog.findChild<QRadioButton*>(QStringLiteral("navigatorQuickMaterialsWindow"));
    auto* materialsCard =
        dialog.findChild<QRadioButton*>(QStringLiteral("navigatorOpenMaterialsCard"));
    REQUIRE(browser != nullptr);
    REQUIRE(onCard != nullptr);
    REQUIRE(link != nullptr);
    REQUIRE(add != nullptr);
    REQUIRE(window != nullptr);
    REQUIRE(materialsCard != nullptr);
    // The usual ones: the browser, and at once with the link
    CHECK(browser->isChecked());
    CHECK(link->isChecked());
    CHECK_FALSE(add->isChecked());
    CHECK_FALSE(window->isChecked());

    // Each row is a choice of its own
    onCard->click();
    window->click();
    CHECK(onCard->isChecked());
    CHECK_FALSE(browser->isChecked());
    CHECK(window->isChecked());
    CHECK_FALSE(link->isChecked());
    CHECK(materialsCard->isChecked());
    const QStringList written = dialog.applyChanges();
    CHECK(written.size() == 2);
    CHECK(written.contains(QString::fromLatin1(NavigatorPanel::OPEN_PAGES_SETTING)));
    CHECK(written.contains(QString::fromLatin1(NavigatorPanel::QUICK_MATERIALS_SETTING)));
    CHECK(settings.get<std::string>(NavigatorPanel::OPEN_PAGES_SETTING) == "card");
    CHECK(settings.get<std::string>(NavigatorPanel::QUICK_MATERIALS_SETTING) == "window");
    CHECK(NavigatorPanel::quickMaterials() == QStringLiteral("window"));

    add->click();
    CHECK(dialog.applyChanges() ==
          QStringList{QString::fromLatin1(NavigatorPanel::QUICK_MATERIALS_SETTING)});
    CHECK(settings.get<std::string>(NavigatorPanel::QUICK_MATERIALS_SETTING) == "add");

    // Opened again, the page shows the stored choices
    SettingsDialog again(nullptr);
    openPage(again, QString(), QStringLiteral("Navigator"));
    auto* onCardAgain = again.findChild<QRadioButton*>(QStringLiteral("navigatorOpenPagesCard"));
    auto* addAgain = again.findChild<QRadioButton*>(QStringLiteral("navigatorQuickMaterialsAdd"));
    REQUIRE(onCardAgain != nullptr);
    REQUIRE(addAgain != nullptr);
    CHECK(onCardAgain->isChecked());
    CHECK(addAgain->isChecked());

    settings.set<std::string>(NavigatorPanel::OPEN_PAGES_SETTING, pagesBefore);
    settings.set<std::string>(NavigatorPanel::QUICK_MATERIALS_SETTING, quickBefore);
    settings.set<std::string>(NavigatorPanel::OPEN_MATERIALS_SETTING, materialsBefore);
}

TEST_CASE("Settings dialog: Network > Materials chooses the downloads of the materials",
          "[gui][settings]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string before = settings.get<std::string>(MaterialDownloader::SETTING, "ask");
    settings.set<std::string>(MaterialDownloader::SETTING, "ask");

    SettingsDialog dialog(nullptr);
    openPage(dialog, QStringLiteral("Network"), QStringLiteral("Materials"));
    auto* ask = dialog.findChild<QRadioButton*>(QStringLiteral("networkDownloadsAsk"));
    auto* always = dialog.findChild<QRadioButton*>(QStringLiteral("networkDownloadsAlways"));
    auto* never = dialog.findChild<QRadioButton*>(QStringLiteral("networkDownloadsNever"));
    REQUIRE(ask != nullptr);
    REQUIRE(always != nullptr);
    REQUIRE(never != nullptr);
    CHECK(ask->isChecked());
    CHECK(MaterialDownloader::policy() == MaterialDownloader::Policy::Ask);

    never->click();
    CHECK(dialog.applyChanges() == QStringList{QString::fromLatin1(MaterialDownloader::SETTING)});
    CHECK(settings.get<std::string>(MaterialDownloader::SETTING) == "never");
    CHECK(MaterialDownloader::policy() == MaterialDownloader::Policy::Never);

    always->click();
    CHECK(dialog.applyChanges() == QStringList{QString::fromLatin1(MaterialDownloader::SETTING)});
    CHECK(MaterialDownloader::policy() == MaterialDownloader::Policy::Always);

    // Opened again, the page shows the stored choice
    SettingsDialog again(nullptr);
    openPage(again, QStringLiteral("Network"), QStringLiteral("Materials"));
    auto* alwaysAgain = again.findChild<QRadioButton*>(QStringLiteral("networkDownloadsAlways"));
    REQUIRE(alwaysAgain != nullptr);
    CHECK(alwaysAgain->isChecked());

    settings.set<std::string>(MaterialDownloader::SETTING, before);
}

TEST_CASE("Settings dialog: theme colors come from the theme file", "[gui][settings]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string theme = settings.getTheme();
    clearThemeColors(theme);
    const kalahari::core::Theme themeFile =
        kalahari::core::ThemeManager::getInstance().loadTheme(QString::fromStdString(theme));

    SettingsDialog dialog(nullptr);
    openPage(dialog, QStringLiteral("Appearance"), QStringLiteral("Theme"));
    ColorConfigWidget* window = colorWidget(dialog, QStringLiteral("General background color for windows and panels"));
    ColorConfigWidget* logInfo = colorWidget(dialog, QStringLiteral("Color for INFO level log messages"));
    REQUIRE(window != nullptr);
    REQUIRE(logInfo != nullptr);
    CHECK(window->color() == themeFile.palette.window);
    CHECK(logInfo->color() == themeFile.log.info);

    // An edited color is stored for this theme only, the others still follow the file
    const QColor edited(QStringLiteral("#123456"));
    window->setColor(edited);
    CHECK(dialog.applyChanges() ==
          QStringList{QString::fromStdString("themes." + theme + ".palette.window")});
    CHECK(settings.getPaletteColorForTheme(theme, "window", "") == "#123456");
    CHECK_FALSE(settings.hasKey("themes." + theme + ".log.info"));

    clearThemeColors(theme);
    kalahari::core::ThemeManager::getInstance().reloadTheme(QString::fromStdString(theme));
}

TEST_CASE("Settings dialog: options that need a restart are marked", "[gui][settings]") {
    SettingsDialog dialog(nullptr);
    openPage(dialog, QStringLiteral("Appearance"), QStringLiteral("General"));

    QComboBox* language = nullptr;
    for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
        if (combo->findData("pl") >= 0) {
            language = combo;
        }
    }
    REQUIRE(language != nullptr);
    CHECK(language->toolTip().contains(QStringLiteral("restarting")));

    bool noteShown = false;
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        noteShown = noteShown || label->text() == QStringLiteral("Takes effect after restarting Kalahari.");
    }
    CHECK(noteShown);
}

TEST_CASE("Settings dialog: the notes can be read in every theme", "[gui][settings]") {
    // Regression: the notes had the theme's placeholder color, 2.3:1 with the window in the
    // light theme and 3.6:1 in the dark one, where normal text needs 4.5:1 (WCAG AA). So had
    // the descriptions of the planned pages, whose titles kept the color of the theme they
    // were opened in.
    auto& themes = kalahari::core::ThemeManager::getInstance();
    const ThemeRestorer restorer;
    REQUIRE(themes.reloadTheme(QStringLiteral("Light")));
    SettingsDialog dialog(nullptr);
    dialog.show();  // Open while the theme changes
    openAllPages(dialog);

    std::vector<QLabel*> notes;
    for (const SettingsPage* page : dialog.findChildren<SettingsPage*>()) {
        notes.insert(notes.end(), page->notes().begin(), page->notes().end());
    }
    const QString planned = QStringLiteral("These settings will be available in a future version.");
    std::vector<QLabel*> plannedTitles;
    for (QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->text().startsWith(planned)) {
            notes.push_back(label);
            // The title is the first text of the planned page
            plannedTitles.push_back(
                label->parentWidget()->findChildren<QLabel*>(Qt::FindDirectChildrenOnly).front());
        }
    }
    REQUIRE(notes.size() > plannedTitles.size());
    REQUIRE_FALSE(plannedTitles.empty());

    for (const QString& theme : {QStringLiteral("Dark"), QStringLiteral("Light")}) {
        INFO(theme.toStdString());
        REQUIRE(themes.reloadTheme(theme));
        QApplication::processEvents();  // The windows get the palette of the theme
        // A note lies on the window or in a group, which Fusion fills a shade darker
        const QColor window = themes.getCurrentTheme().palette.window;
        const QColor group = groupFillOn(window);
        for (QLabel* note : notes) {
            INFO(note->text().left(60).toStdString());
            CHECK(contrastRatio(textColorOf(note), window) >= MIN_TEXT_CONTRAST);
            CHECK(contrastRatio(textColorOf(note), group) >= MIN_TEXT_CONTRAST);
        }
        for (QLabel* title : plannedTitles) {
            INFO(title->text().toStdString());
            CHECK(textColorOf(title) == themes.getCurrentTheme().palette.windowText);
        }
    }
}

TEST_CASE("Settings dialog: a missing editor font keeps its name", "[gui][settings]") {
    // Regression: a font missing on this system was shown as its substitute, and the
    // substitute's name was saved on the next Apply as if the user had chosen it
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string family = settings.get<std::string>("editor.fontFamily");
    const int size = settings.get<int>("editor.fontSize");
    settings.set<std::string>("editor.fontFamily", "Kalahari Missing Font");

    SettingsDialog dialog(nullptr);
    openPage(dialog, QStringLiteral("Editor"), QStringLiteral("General"));
    CHECK_FALSE(dialog.hasChanges());

    // Another option of the page changes: the font name stays
    QSpinBox* fontSize = nullptr;
    for (QSpinBox* spin : dialog.findChildren<QSpinBox*>()) {
        if (spin->suffix() == QStringLiteral(" pt") && spin->isEnabled()) {
            fontSize = spin;
        }
    }
    REQUIRE(fontSize != nullptr);
    fontSize->setValue(size == 20 ? 21 : 20);
    CHECK(dialog.applyChanges() == QStringList{QStringLiteral("editor.fontSize")});
    CHECK(settings.get<std::string>("editor.fontFamily") == "Kalahari Missing Font");

    settings.set<std::string>("editor.fontFamily", family);
    settings.set<int>("editor.fontSize", size);
}

TEST_CASE("Settings dialog: the diagnostic menu is not stored", "[gui][settings]") {
    SettingsDialog dialog(nullptr, true);
    openPage(dialog, QStringLiteral("Advanced"), QStringLiteral("General"));

    QCheckBox* diagnostic = nullptr;
    for (QCheckBox* box : dialog.findChildren<QCheckBox*>()) {
        if (box->text() == QStringLiteral("Enable Diagnostic Menu")) {
            diagnostic = box;
        }
    }
    REQUIRE(diagnostic != nullptr);
    CHECK(diagnostic->isChecked());

    int turnedOff = 0;
    QObject::connect(&dialog, &SettingsDialog::diagnosticModeChanged,
                     [&turnedOff](bool enabled) { turnedOff += enabled ? 0 : 1; });
    diagnostic->setChecked(false);
    CHECK(dialog.applyChanges().isEmpty());
    CHECK(turnedOff == 1);
}

TEST_CASE("Lengths convert between units", "[gui][settings]") {
    using kalahari::gui::LengthUnit;
    using kalahari::gui::convertLength;
    using Catch::Approx;
    CHECK(convertLength(25.4, LengthUnit::Millimeters, LengthUnit::Inches) == Approx(1.0));
    CHECK(convertLength(1.0, LengthUnit::Inches, LengthUnit::Pixels) == Approx(96.0));
    CHECK(convertLength(1.0, LengthUnit::Inches, LengthUnit::Points) == Approx(72.0));
    CHECK(convertLength(2.5, LengthUnit::Centimeters, LengthUnit::Millimeters) == Approx(25.0));
    CHECK(kalahari::gui::lengthUnitFromName(QStringLiteral("cm")) == LengthUnit::Centimeters);
    CHECK(kalahari::gui::lengthUnitFromName(QStringLiteral("unknown")) == LengthUnit::Millimeters);
}

TEST_CASE("A length field keeps its stored value in any unit", "[gui][settings]") {
    using kalahari::gui::LengthUnit;
    kalahari::gui::LengthSpinBox field(LengthUnit::Millimeters, 0.0, 100.0);
    field.setDisplayUnit(LengthUnit::Millimeters);
    field.setStoredValue(25.0);
    CHECK(field.value() == Catch::Approx(25.0));

    // Shown rounded, stored as it was
    field.setDisplayUnit(LengthUnit::Pixels);
    CHECK(field.value() == Catch::Approx(94.0));
    field.setDisplayUnit(LengthUnit::Inches);
    CHECK(field.value() == Catch::Approx(0.98));
    field.setDisplayUnit(LengthUnit::Millimeters);
    CHECK(field.storedValue() == Catch::Approx(25.0));

    // An edit in another unit is stored in the field's own unit
    field.setDisplayUnit(LengthUnit::Centimeters);
    field.setValue(3.0);
    CHECK(field.storedValue() == Catch::Approx(30.0));
}

TEST_CASE("Settings dialog: the length unit changes the fields, not the lengths", "[gui][settings]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    settings.set<std::string>("ui.lengthUnit", "mm");
    SettingsDialog dialog(nullptr);
    openAllPages(dialog);
    REQUIRE_FALSE(dialog.hasChanges());

    QComboBox* unit = nullptr;
    for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
        if (combo->findData("in") >= 0) {
            unit = combo;
        }
    }
    REQUIRE(unit != nullptr);
    const auto lengths = dialog.findChildren<kalahari::gui::LengthSpinBox*>();
    REQUIRE_FALSE(lengths.isEmpty());

    QStringList applied;
    QObject::connect(&dialog, &SettingsDialog::settingsApplied,
                     [&applied](const QStringList& keys) { applied = keys; });
    unit->setCurrentIndex(unit->findData("in"));
    for (const auto* length : lengths) {
        CHECK(length->displayUnit() == kalahari::gui::LengthUnit::Inches);
    }
    dialog.applyChanges();
    CHECK(applied == QStringList{QStringLiteral("ui.lengthUnit")});
    CHECK(settings.get<std::string>("ui.lengthUnit", "") == "in");

    settings.set<std::string>("ui.lengthUnit", "mm");
}

TEST_CASE("clearLayout hides the widgets it removes", "[gui][settings]") {
    // Regression: the Settings icon preview is rebuilt with clearLayout while the modal
    // dialog runs, where deleteLater waits until the dialog closes; the removed icons
    // stayed visible and were drawn over the preview's top-left corner
    QWidget preview;
    auto* layout = new QHBoxLayout(&preview);
    auto* icon = new QLabel(QStringLiteral("icon"));
    layout->addWidget(icon);
    preview.show();
    QApplication::processEvents();
    REQUIRE(icon->isVisible());

    kalahari::gui::utils::clearLayout(layout);

    CHECK(layout->count() == 0);
    CHECK(icon->isHidden());
}
