// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file settings_dialog.cpp
/// @brief Implementation of SettingsDialog

#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/settings/grammar_page.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/settings/shortcuts_page.h"
#include "kalahari/gui/settings/stats_pages.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/window_editor_provider.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// The pause in typing after which the search narrows the tree
constexpr int SEARCH_DELAY_MS = 150;
/// The keys of the search, the same on every system (Cmd+F on macOS)
constexpr QKeyCombination FIND_KEYS(Qt::ControlModifier, Qt::Key_F);

/// @brief Open the advanced settings of a page when an option found is one of them
void openAdvancedOf(QWidget* page, const std::vector<QWidget*>& options) {
    auto* settingsPage = qobject_cast<SettingsPage*>(page);
    const QWidget* advanced = settingsPage != nullptr ? settingsPage->advancedSettings() : nullptr;
    if (advanced == nullptr || !advanced->isHidden()) {
        return;
    }
    if (std::any_of(options.cbegin(), options.cend(),
                    [advanced](const QWidget* option) { return advanced->isAncestorOf(option); })) {
        settingsPage->advancedSettingsButton()->setChecked(true);
    }
}

}  // namespace

SettingsDialog::SettingsDialog(const core::ProjectManager& book, CommandRegistry& commands,
                               QWidget* parent, bool diagnosticMode,
                               editor::SpellCheckService* spelling, GrammarCoordinator* grammar)
    : KalahariDialog(parent),
      m_diagnosticMode(diagnosticMode),
      m_spelling(spelling),
      m_grammar(grammar),
      m_lengthUnit(currentLengthUnit()),
      m_commands(commands),
      m_book(book) {
    setHeading(tr("Settings"),
               tr("Choose a group on the left; Apply and OK save only the options you changed."));
    setHeadingIcon(QStringLiteral("edit.settings"));
    // A low heading leaves the room to the pages
    setCompactHeading(true);
    setApplyButtonVisible(true);
    setModal(true);
    // Tall enough for the longest editor page where the screen allows, wide enough for the
    // search and the keys of the Keyboard Shortcuts page in one row each; a page that does
    // not fit scrolls
    QSize size(840, 700);
    if (const QScreen* screen = this->screen()) {
        size = size.boundedTo(screen->availableGeometry().size() * 0.9);
    }
    resize(size);
    setMinimumSize(600, 400);

    // The search over the tree of the pages, as wide as it, and what it says when nothing has
    // its words beside it
    auto* searchRow = new QHBoxLayout();
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("settingsSearch"));
    m_search->setClearButtonEnabled(true);
    //: The search of the Settings dialog, over the tree of its pages; %1: its key, Ctrl+F. In
    //: Polish: Szukaj w ustawieniach (%1)
    m_search->setPlaceholderText(tr("Search the settings (%1)")
                                     .arg(ShortcutRules::keysText(FIND_KEYS)));
    //: In Polish: Szukaj w ustawieniach
    m_search->setAccessibleName(tr("Search the settings"));
    m_search->installEventFilter(this);
    searchRow->addWidget(m_search, 0, Qt::AlignLeft);
    m_searchEmpty = new QLabel(this);
    m_searchEmpty->setObjectName(QStringLiteral("settingsSearchEmpty"));
    m_searchEmpty->setForegroundRole(QPalette::PlaceholderText);
    m_searchEmpty->hide();
    searchRow->addWidget(m_searchEmpty, 1);
    contentLayout()->addLayout(searchRow);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    m_navTree = new QTreeWidget(splitter);
    m_navTree->setHeaderHidden(true);
    m_navTree->setMinimumWidth(180);
    m_navTree->setMaximumWidth(250);
    m_pageStack = new QStackedWidget(splitter);
    splitter->addWidget(m_navTree);
    splitter->addWidget(m_pageStack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    contentLayout()->addWidget(splitter, 1);
    // The search follows the width of the tree
    m_navTree->installEventFilter(this);

    createNavigationTree();
    updatePlannedItemColors();
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &SettingsDialog::updatePlannedItemColors);

    // The groups as wide as the longest of them, with room for a scroll bar on a low screen:
    // the pages get the rest
    m_navTree->resizeColumnToContents(0);
    const int navWidth =
        m_navTree->columnWidth(0) + 2 * m_navTree->frameWidth() +
        m_navTree->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_navTree);
    splitter->setSizes({navWidth, std::max(width() - navWidth, 0)});
    m_search->setFixedWidth(std::clamp(navWidth, m_navTree->minimumWidth(),
                                       m_navTree->maximumWidth()));

    connect(m_navTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current) { showPage(current); });
    connect(this, &KalahariDialog::applyClicked, this, &SettingsDialog::onApply);

    // The search narrows the tree after a pause in typing; Ctrl+F goes to it from anywhere
    m_searchTimer.setSingleShot(true);
    m_searchTimer.setInterval(SEARCH_DELAY_MS);
    connect(&m_searchTimer, &QTimer::timeout, this, &SettingsDialog::search);
    connect(m_search, &QLineEdit::textChanged, this, [this]() { m_searchTimer.start(); });
    auto* find = new QShortcut(QKeySequence(FIND_KEYS), this);
    find->setContext(Qt::WidgetWithChildrenShortcut);
    connect(find, &QShortcut::activated, this, [this]() {
        m_search->setFocus(Qt::ShortcutFocusReason);
        m_search->selectAll();
    });

    m_navTree->setCurrentItem(m_navTree->topLevelItem(0));
}

void SettingsDialog::createNavigationTree() {
    const auto category = [this](const QString& title) {
        auto* item = new QTreeWidgetItem(m_navTree);
        item->setText(0, title);
        item->setExpanded(true);
        return item;
    };
    m_pageItems[GENERAL_PAGE] = addPage(nullptr, tr("General"), [this]() {
        auto* generalPage = new GeneralPage();
        connect(generalPage, &GeneralPage::lengthUnitChanged, this,
                [this](const QString& unit) { setLengthUnit(lengthUnitFromName(unit)); });
        return generalPage;
    });
    //: Settings > Notifications: where the program's notifications show. In Polish:
    //: Powiadomienia
    m_pageItems[NOTIFICATIONS_PAGE] =
        addPage(nullptr, tr("Notifications"), []() { return new NotificationsPage(); });

    QTreeWidgetItem* appearance = category(tr("Appearance"));
    addPage(appearance, tr("General"), []() { return new AppearanceGeneralPage(); });
    addPage(appearance, tr("Theme"), []() { return new ThemePage(); });
    addPage(appearance, tr("Icons"), []() { return new IconsPage(); });
    addPage(appearance, tr("Dashboard"), []() { return new DashboardPage(); });

    QTreeWidgetItem* editor = category(tr("Editor"));
    addPage(editor, tr("General"), [this]() {
        return new EditorGeneralPage(m_commands, EditorGeneralPage::Part::Text);
    });
    //: Settings > Editor > Writing: the behavior of the editor while writing. In Polish: Pisanie
    m_pageItems[WORD_COUNT_PAGE] = addPage(editor, tr("Writing"), [this]() {
        return new EditorGeneralPage(m_commands, EditorGeneralPage::Part::Writing);
    });
    m_pageItems[FIELDS_PAGE] = m_pageItems[WORD_COUNT_PAGE];
    m_pageItems[PASTE_PAGE] = m_pageItems[WORD_COUNT_PAGE];
    m_pageItems[FOOTNOTES_PAGE] = m_pageItems[WORD_COUNT_PAGE];
    m_pageItems[EDITOR_COLORS_PAGE] =
        addPage(editor, tr("Colors"), []() { return new EditorColorsPage(); });
    m_pageItems[NOTICE_BAR_PAGE] = m_pageItems[EDITOR_COLORS_PAGE];
    addPage(editor, tr("Cursor"), []() { return new EditorCursorPage(); });
    m_pageItems[PAGES_PAGE] = addPage(editor, tr("Pages and Margins"), []() {
        return new EditorPagesPage(EditorPagesPage::Part::Pages);
    });
    //: Settings > Editor > Margin Numbering: the numbers of the lines or paragraphs in the
    //: margin. Short: the list of the pages is as wide as its longest name. In Polish: Numeracja
    m_pageItems[MARGIN_NUMBERING_PAGE] = addPage(editor, tr("Margin Numbering"), []() {
        return new EditorPagesPage(EditorPagesPage::Part::MarginNumbers);
    });
    //: Settings > Editor > Lists: the sets of list formats. In Polish: Listy
    m_pageItems[LISTS_PAGE] = addPage(editor, tr("Lists"), [this]() {
        return new EditorListsPage(m_book, m_commands);
    });
    //: Settings > Editor > Images: new images, the captions of new books. In Polish: Obrazy
    m_pageItems[IMAGES_PAGE] = addPage(editor, tr("Images"), [this]() {
        auto* imagesPage = new EditorImagesPage();
        connect(imagesPage, &EditorImagesPage::colorsPageAsked, this,
                [this]() { openPage(QString::fromLatin1(EDITOR_COLORS_PAGE)); });
        return imagesPage;
    });
    m_pageItems[FIGURES_PANEL_PAGE] = m_pageItems[IMAGES_PAGE];
    //: Settings > Editor > Tables: new tables, the captions of tables. In Polish: Tabele
    m_pageItems[TABLES_PAGE] =
        addPage(editor, tr("Tables"), []() { return new EditorTablesPage(); });
    m_pageItems[SPELLING_PAGE] =
        addPage(editor, tr("Spelling"), [this]() {
            auto* spellingPage = new EditorSpellingPage(m_spelling, m_book.isProjectOpen());
            // Open Book Dictionary: OK, then the dictionary on its tab
            connect(spellingPage, &EditorSpellingPage::bookDictionaryAsked, this, [this]() {
                accept();
                m_bookDictionaryAsked = result() == QDialog::Accepted;
            });
            return spellingPage;
        });
    addPage(editor, tr("Grammar"), [this]() { return new EditorGrammarPage(m_book, m_grammar); });
    m_pageItems[AUTO_CORRECT_PAGE] = addPage(
        editor, tr("Auto-correct"), [this]() { return new EditorAutoCorrectPage(m_commands); });
    //: Settings > Editor > Quick Insert: @ and # in the text. In Polish: Szybkie wstawianie
    addPage(editor, tr("Quick Insert"), [this]() { return new EditorQuickInsertPage(m_commands); });
    //: Settings > Editor > Repetitions: the words that come back too close. In Polish: Powtórzenia
    m_pageItems[REPETITIONS_PAGE] = addPage(
        editor, tr("Repetitions"), [this]() { return new EditorRepetitionsPage(m_commands); });
    //: Settings > Editor > Versions: the versions of the texts. In Polish: Wersje
    m_pageItems[VERSIONS_PAGE] =
        addPage(editor, tr("Versions"), [this]() { return new EditorVersionsPage(m_commands); });
    //: Settings > Editor > Clippings: Cut to Clippings and Insert > Clipping from the Workshop.
    //: In Polish: Wycinki
    m_pageItems[CLIPPINGS_PAGE] = addPage(
        editor, tr("Clippings"), [this]() { return new EditorClippingsPage(m_commands); });

    QTreeWidgetItem* annotations = category(tr("Annotations"));
    m_pageItems[ANNOTATIONS_PAGE] = addPage(annotations, tr("General"), []() {
        return new AnnotationsPage(AnnotationsPage::Part::General);
    });
    //: Settings > Annotations > Annotation Cards: the annotations shown as cards under the
    //: paragraphs in the continuous view; not the page Cards of the cards of characters,
    //: locations and items. Short: the list of the pages is as wide as its longest name. In
    //: Polish: Karty uwag
    addPage(annotations, tr("Annotation Cards"),
            []() { return new AnnotationsPage(AnnotationsPage::Part::Cards); });

    QTreeWidgetItem* navigator = category(tr("Navigator"));
    //: Settings > Navigator > Adding: the menus of the nodes, the Add Element window and the
    //: dropped files. In Polish: Dodawanie
    m_pageItems[NAVIGATOR_ADDING_PAGE] = addPage(navigator, tr("Adding"), [this]() {
        return new NavigatorPage(m_book, NavigatorPage::Part::Adding);
    });
    //: Settings > Navigator > Material Lists: what the Materials of the Navigator show, where
    //: the list of a kind opens, the links of the materials; not the page Materials of the
    //: opening of the materials. In Polish: Listy materiałów
    addPage(navigator, tr("Material Lists"),
            [this]() { return new NavigatorPage(m_book, NavigatorPage::Part::Lists); });
    //: Settings > Navigator > Material Kinds: the kinds of materials shown in the Navigator. In
    //: Polish: Typy materiałów
    m_pageItems[NAVIGATOR_PAGE] = addPage(navigator, tr("Material Kinds"), [this]() {
        return new NavigatorPage(m_book, NavigatorPage::Part::Kinds);
    });
    //: Settings > Navigator > Cover: the cover of the book in the Navigator. In Polish: Okładka
    addPage(navigator, tr("Cover"),
            [this]() { return new NavigatorPage(m_book, NavigatorPage::Part::Cover); });
    //: Settings > Navigator > Texts for Publishers: the lengths, the template of a synopsis and
    //: the set. Short: the list of the pages is as wide as its longest name. In Polish: Dla
    //: wydawnictwa
    m_pageItems[PUBLISHING_PAGE] = addPage(navigator, tr("Texts for Publishers"), [this]() {
        return new NavigatorPage(m_book, NavigatorPage::Part::Publishing);
    });
    //: Settings > Cards: the cards of characters, locations and items. In Polish: Karty
    m_pageItems[CARDS_PAGE] = addPage(nullptr, tr("Cards"), []() { return new CardsPage(); });
    //: The page of the materials: how they open, the preview of PDF files and the sources. In
    //: Polish: Materiały
    m_pageItems[MATERIALS_PAGE] =
        addPage(nullptr, tr("Materials"), [this]() {
            auto* materialsPage = new MaterialsPage();
            connect(materialsPage, &MaterialsPage::colorsPageAsked, this,
                    [this]() { openPage(QString::fromLatin1(EDITOR_COLORS_PAGE)); });
            return materialsPage;
        });
    //: Settings > Book Plan: the scenes on the board, the colors of the cards, the chronology.
    //: In Polish: Plan książki
    m_pageItems[PLOT_BOARD_PAGE] =
        addPage(nullptr, tr("Book Plan"), []() { return new PlotBoardPage(); });
    // The pages of the modules that edit window kinds: Mind Maps, Timelines; openPage() opens
    // each by the id of its editor ("mindmap")
    for (const std::shared_ptr<core::IWindowEditorProvider>& provider :
         core::windowEditorProviders()) {
        if (!provider->settingsTitle().isEmpty()) {
            m_pageItems[provider->editorId()] = addPage(
                nullptr, provider->settingsTitle(),
                [provider]() { return provider->createSettingsPage(); });
        }
    }
    //: Settings > Statistics and Goals: the statistics bar, the panel, the counting of the words
    //: and the colors. In Polish: Statystyki i cele
    QTreeWidgetItem* statistics = category(tr("Statistics and Goals"));
    //: In Polish: Pasek i panel
    m_pageItems[STATS_BAR_PAGE] =
        addPage(statistics, tr("Bar and Panel"), []() { return new StatsBarPage(); });
    //: In Polish: Liczenie
    m_pageItems[STATS_COUNTING_PAGE] =
        addPage(statistics, tr("Counting"), []() { return new StatsCountingPage(); });
    //: In Polish: Powiadomienia
    m_pageItems[STATS_NOTIFICATIONS_PAGE] =
        addPage(statistics, tr("Notifications"), []() { return new StatsNotificationsPage(); });
    //: In Polish: Kolory
    m_pageItems[STATS_COLORS_PAGE] =
        addPage(statistics, tr("Colors"), []() { return new StatsColorsPage(); });
    m_pageItems[SHORTCUTS_PAGE] = addPage(nullptr, tr("Keyboard Shortcuts"),
                                          [this]() { return new ShortcutsPage(m_commands); });

    QTreeWidgetItem* files = category(tr("Files"));
    addPage(files, tr("Folders"), []() { return new FoldersPage(); });
    //: The page of the choices the window of the import starts with. In Polish: Import
    m_pageItems[IMPORT_PAGE] = addPage(files, tr("Import"), []() { return new ImportPage(); });
    //: The page of the program the system opens the books and the documents with. In Polish:
    //: Powiązania plików
    m_pageItems[FILE_ASSOCIATIONS_PAGE] = addPage(files, tr("File Associations"), []() {
        return new FileAssociationsPage();
    });
    addPlannedPage(files, tr("Backup"),
                   tr("Planned features:\n"
                      "- Automatic backup frequency\n"
                      "- Restore from backup\n\n"
                      "The folder of the database backups and the number of copies kept are "
                      "on the Folders page."));
    addPlannedPage(files, tr("Auto-save"),
                   tr("Planned features:\n"
                      "- Auto-save interval\n"
                      "- Auto-save on focus loss\n"
                      "- Session recovery options"));
    addPlannedPage(files, tr("Import/Export"),
                   tr("Planned features:\n"
                      "- Default export format\n"
                      "- Import source preferences\n"
                      "- Encoding settings"));

    QTreeWidgetItem* network = category(tr("Network"));
    //: The page of the downloads for the materials with a web address. In Polish: Materiały
    addPage(network, tr("Materials"), []() { return new NetworkMaterialsPage(); });
    addPlannedPage(network, tr("Updates"),
                   tr("Planned features:\n"
                      "- Automatic update checks\n"
                      "- Update channel (stable/beta)\n"
                      "- Plugin updates"));

    QTreeWidgetItem* advanced = category(tr("Advanced"));
    const bool diagnosticMode = m_diagnosticMode;
    addPage(advanced, tr("General"), [this, diagnosticMode]() {
        auto* advancedPage = new AdvancedGeneralPage(diagnosticMode);
        connect(advancedPage, &AdvancedGeneralPage::diagnosticModeChanged,
                this, &SettingsDialog::diagnosticModeChanged);
        return advancedPage;
    });
    addPlannedPage(advanced, tr("Performance"),
                   tr("Planned features:\n"
                      "- Memory usage limits\n"
                      "- Thread pool configuration\n"
                      "- Cache settings\n"
                      "- Hardware acceleration"));
    addPage(advanced, tr("Log"), []() { return new AdvancedLogPage(); });
}

QTreeWidgetItem* SettingsDialog::addPage(QTreeWidgetItem* parent, const QString& title,
                                         PageFactory factory) {
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_navTree);
    item->setText(0, title);
    m_factories[item] = std::move(factory);
    return item;
}

void SettingsDialog::addPlannedPage(QTreeWidgetItem* parent, const QString& title,
                                    const QString& description) {
    QTreeWidgetItem* item = addPage(parent, title, [title, description]() {
        auto* placeholder = new QWidget();
        auto* layout = new QVBoxLayout(placeholder);
        // The color of the window's text: a font, not a style sheet, so that it follows the
        // theme (a style sheet keeps the colors of the theme the page was opened in)
        auto* titleLabel = new QLabel(title);
        QFont titleFont = titleLabel->font();
        titleFont.setPixelSize(18);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        layout->addWidget(titleLabel);
        auto* descriptionLabel = new QLabel(tr("These settings will be available in a future version.")
                                            + QStringLiteral("\n\n") + description);
        descriptionLabel->setWordWrap(true);
        // Muted and readable, as the notes of the pages, also after the theme changes
        const auto styleDescription = [descriptionLabel]() {
            descriptionLabel->setStyleSheet(QStringLiteral("color: %1; margin-top: 20px;")
                                                .arg(SettingsPage::noteColor().name()));
        };
        styleDescription();
        QObject::connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged,
                         descriptionLabel, styleDescription);
        layout->addWidget(descriptionLabel);
        layout->addStretch();
        return placeholder;
    });
    // Muted in the tree, like the notes, and still readable on the list
    m_plannedItems.push_back(item);
    item->setToolTip(0, tr("Coming in future version"));
}

void SettingsDialog::updatePlannedItemColors() {
    const QColor color =
        SettingsPage::noteColorOn(core::ThemeManager::getInstance().getCurrentTheme().palette.base);
    for (QTreeWidgetItem* item : m_plannedItems) {
        item->setForeground(0, color);
    }
}

void SettingsDialog::showPage(QTreeWidgetItem* item) {
    if (!item) {
        return;
    }
    if (m_factories.find(item) == m_factories.end()) {
        // A category: show its first page the search leaves
        for (int child = 0; child < item->childCount(); ++child) {
            if (!item->child(child)->isHidden()) {
                m_navTree->setCurrentItem(item->child(child));
                break;
            }
        }
        return;
    }
    m_pageStack->setCurrentWidget(buildPage(item));
    if (!m_searchWords.isEmpty()) {
        showMarks(item);
    }
}

QWidget* SettingsDialog::buildPage(QTreeWidgetItem* item) {
    auto factory = m_factories.find(item);
    if (factory == m_factories.end()) {
        return nullptr;
    }
    auto built = m_builtPages.find(item);
    if (built == m_builtPages.end()) {
        QElapsedTimer timer;
        timer.start();
        QWidget* content = factory->second();
        // A unit chosen but not applied yet holds for the pages opened after it too
        for (LengthSpinBox* length : content->findChildren<LengthSpinBox*>()) {
            length->setDisplayUnit(m_lengthUnit);
        }
        if (auto* page = qobject_cast<SettingsPage*>(content)) {
            page->load();
            shareCheckBoxes(page);
            m_pages.push_back(page);
            if (auto* themePage = qobject_cast<ThemePage*>(page)) {
                m_themePage = themePage;
                connectPages();
            } else if (auto* iconsPage = qobject_cast<IconsPage*>(page)) {
                m_iconsPage = iconsPage;
                connectPages();
            }
        }
        // A page taller than the dialog scrolls instead of squeezing its groups until
        // their fields overlap
        auto* scrollArea = new QScrollArea();
        scrollArea->setWidget(content);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        m_pageStack->addWidget(scrollArea);
        built = m_builtPages.emplace(item, scrollArea).first;
        core::Logger::getInstance().debug("SettingsDialog: Page '{}' built in {} ms",
                                          item->text(0).toStdString(), timer.elapsed());
    }
    return built->second;
}

QWidget* SettingsDialog::pageOf(QTreeWidgetItem* item) const {
    const auto built = m_builtPages.find(item);
    const auto* scrollArea =
        built != m_builtPages.end() ? qobject_cast<QScrollArea*>(built->second) : nullptr;
    return scrollArea != nullptr ? scrollArea->widget() : nullptr;
}

// =============================================================================
// The search
// =============================================================================

void SettingsDialog::findSettings(const QString& text) {
    m_search->setText(text);
    search();
}

std::vector<QWidget*> SettingsDialog::markedOptions() const {
    QWidget* page = pageOf(m_navTree->currentItem());
    const auto* marks = page != nullptr ? page->findChild<SearchMarks*>(
                                              QString(), Qt::FindDirectChildrenOnly)
                                        : nullptr;
    return marks != nullptr && marks->isVisibleTo(page) ? marks->options()
                                                         : std::vector<QWidget*>();
}

void SettingsDialog::search() {
    m_searchTimer.stop();
    m_searchWords = SettingsSearch(m_search->text());
    m_found.clear();
    const bool searching = !m_searchWords.isEmpty();
    if (searching) {
        // The options of every page: the pages not opened yet are built once, for the search
        for (QTreeWidgetItemIterator it(m_navTree); *it; ++it) {
            buildPage(*it);
        }
    }

    bool anyShown = false;
    for (int top = 0; top < m_navTree->topLevelItemCount(); ++top) {
        QTreeWidgetItem* item = m_navTree->topLevelItem(top);
        bool shown = !searching;
        if (m_factories.find(item) != m_factories.end()) {
            shown = !searching || searchPage(item, QString());
        } else {
            // A category with the words shows all its pages
            const bool category = searching && m_searchWords.matches(item->text(0));
            for (int child = 0; child < item->childCount(); ++child) {
                QTreeWidgetItem* page = item->child(child);
                const bool found = searching && searchPage(page, item->text(0));
                page->setHidden(searching && !category && !found);
                shown = shown || !page->isHidden();
            }
        }
        item->setHidden(!shown);
        anyShown = anyShown || shown;
    }

    //: Under the search of the Settings dialog when nothing has its words. In Polish: Żadna
    //: opcja ani strona nie zawiera „%1”.
    m_searchEmpty->setText(tr("No option or page contains “%1”.")
                               .arg(m_search->text().simplified()));
    m_searchEmpty->setVisible(!anyShown);

    // The page shown stays while the search leaves it, else the first page it leaves
    QTreeWidgetItem* current = m_navTree->currentItem();
    if (current != nullptr && (current->isHidden() ||
                               (current->parent() != nullptr && current->parent()->isHidden()))) {
        current = nullptr;
    }
    if (current == nullptr) {
        for (QTreeWidgetItemIterator it(m_navTree); *it; ++it) {
            if (!(*it)->isHidden() && m_factories.find(*it) != m_factories.end() &&
                ((*it)->parent() == nullptr || !(*it)->parent()->isHidden())) {
                current = *it;
                break;
            }
        }
    }
    if (current != nullptr && current != m_navTree->currentItem()) {
        m_navTree->setCurrentItem(current);  // Its marks with it (showPage())
    } else {
        showMarks(m_navTree->currentItem());
    }
}

bool SettingsDialog::searchPage(QTreeWidgetItem* item, const QString& category) {
    std::vector<QWidget*> options = optionsFound(pageOf(item), m_searchWords);
    // A page with options found is in bold
    QFont font = item->font(0);
    font.setBold(!options.empty());
    item->setFont(0, font);
    const bool named = m_searchWords.matches(category + QLatin1Char(' ') + item->text(0));
    const bool found = named || !options.empty();
    m_found[item] = std::vector<QPointer<QWidget>>(options.begin(), options.end());
    return found;
}

void SettingsDialog::showMarks(QTreeWidgetItem* item) {
    // Only the page shown has marks; none without a search
    for (const auto& [built, container] : m_builtPages) {
        QWidget* page = pageOf(built);
        if (page == nullptr) {
            continue;
        }
        auto* marks = page->findChild<SearchMarks*>(QString(), Qt::FindDirectChildrenOnly);
        std::vector<QWidget*> options;
        if (built == item && !m_searchWords.isEmpty()) {
            for (const QPointer<QWidget>& option : m_found[built]) {
                if (option != nullptr) {
                    options.push_back(option.data());
                }
            }
        }
        if (options.empty() && marks == nullptr) {
            continue;
        }
        if (marks == nullptr) {
            marks = new SearchMarks(page);
        }
        openAdvancedOf(page, options);
        marks->setOptions(options);
        if (!options.empty()) {
            // The first option found in view, once the page has its size
            auto* scrollArea = qobject_cast<QScrollArea*>(container);
            QPointer<QWidget> first = options.front();
            QTimer::singleShot(0, scrollArea, [scrollArea, first]() {
                if (first != nullptr) {
                    scrollArea->ensureWidgetVisible(first);
                }
            });
        }
    }
    // Without a search every page is in the tree again, in a plain font
    if (m_searchWords.isEmpty()) {
        for (QTreeWidgetItemIterator it(m_navTree); *it; ++it) {
            QFont font = (*it)->font(0);
            if (font.bold()) {
                font.setBold(false);
                (*it)->setFont(0, font);
            }
        }
    }
}

bool SettingsDialog::searchKeyPressed(int key) {
    switch (key) {
    case Qt::Key_Escape:
        // Esc clears the words first, the next one closes the dialog
        if (m_search->text().isEmpty()) {
            return false;
        }
        m_search->clear();
        search();
        return true;
    case Qt::Key_Down:
        m_navTree->setFocus(Qt::TabFocusReason);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        // Not OK of the dialog: the search at once, then the first option found (the field of
        // its label), else the tree
        search();
        const std::vector<QWidget*> options = markedOptions();
        QWidget* target = nullptr;
        for (QWidget* option : options) {
            const auto* label = qobject_cast<QLabel*>(option);
            QWidget* field = label != nullptr ? label->buddy() : option;
            if (field != nullptr && field->focusPolicy() != Qt::NoFocus && field->isEnabled()) {
                target = field;
                break;
            }
        }
        if (target != nullptr) {
            target->setFocus(Qt::TabFocusReason);
        } else {
            m_navTree->setFocus(Qt::TabFocusReason);
        }
        return true;
    }
    default:
        return false;
    }
}

bool SettingsDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_navTree && event->type() == QEvent::Resize) {
        m_search->setFixedWidth(m_navTree->width());
    }
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if ((key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier &&
            searchKeyPressed(key->key())) {
            return true;
        }
    }
    return KalahariDialog::eventFilter(watched, event);
}

void SettingsDialog::showShortcutsPage() {
    openPage(SHORTCUTS_PAGE);
}

bool SettingsDialog::openPage(const QString& page) {
    const auto item = m_pageItems.find(page);
    if (item == m_pageItems.end()) {
        return false;
    }
    // A page the search left out of the tree: every page again
    if (item->second->isHidden() ||
        (item->second->parent() != nullptr && item->second->parent()->isHidden())) {
        findSettings(QString());
    }
    m_navTree->setCurrentItem(item->second);
    // The cursor in the search of the shortcuts, so the keys go there at once (the window
    // gives it to the search when it shows)
    if (page == QLatin1String(SHORTCUTS_PAGE)) {
        if (auto* shortcuts = m_pageStack->findChild<ShortcutsPage*>()) {
            shortcuts->setFocus(Qt::OtherFocusReason);
        }
    } else if (page == QLatin1String(FIELDS_PAGE) || page == QLatin1String(PASTE_PAGE) ||
               page == QLatin1String(FOOTNOTES_PAGE) || page == QLatin1String(FIGURES_PANEL_PAGE)) {
        // The cursor in the gray background of the fields, in the Paste Command, in the words
        // before the numbers of the Footnotes panel, in the pictures of the panel Images and
        // Tables, in view
        const auto built = m_builtPages.find(item->second);
        auto* scrollArea =
            built != m_builtPages.end() ? qobject_cast<QScrollArea*>(built->second) : nullptr;
        QString name = QStringLiteral("pasteMode");
        if (page == QLatin1String(FIELDS_PAGE)) {
            name = QStringLiteral("fieldShading");
        } else if (page == QLatin1String(FOOTNOTES_PAGE)) {
            name = QStringLiteral("footnotesPanelBefore");
        } else if (page == QLatin1String(FIGURES_PANEL_PAGE)) {
            name = QStringLiteral("figuresPanelPictures");
        }
        auto* field = scrollArea != nullptr ? scrollArea->findChild<QWidget*>(name) : nullptr;
        if (field != nullptr) {
            field->setFocus(Qt::OtherFocusReason);
            QTimer::singleShot(0, scrollArea, [scrollArea, field]() {
                scrollArea->ensureWidgetVisible(field);
            });
        }
    } else if (page == QLatin1String(MARGIN_NUMBERING_PAGE) ||
               page == QLatin1String(WORD_COUNT_PAGE) || page == QLatin1String(NOTICE_BAR_PAGE)) {
        // The group far down the page in view, the cursor in its choice (of the numbering),
        // in the counting of the notes' words, else in its first field
        const auto built = m_builtPages.find(item->second);
        auto* scrollArea =
            built != m_builtPages.end() ? qobject_cast<QScrollArea*>(built->second) : nullptr;
        QString name = QStringLiteral("marginNumbersGroup");
        if (page == QLatin1String(WORD_COUNT_PAGE)) {
            name = QStringLiteral("wordCountGroup");
        } else if (page == QLatin1String(NOTICE_BAR_PAGE)) {
            name = QStringLiteral("noticeBarGroup");
        }
        auto* group = scrollArea != nullptr ? scrollArea->findChild<QWidget*>(name) : nullptr;
        if (group != nullptr) {
            auto* mode = group->findChild<QComboBox*>(QStringLiteral("marginNumbersMode"));
            auto* notes = group->findChild<QWidget*>(QStringLiteral("wordCountNotes"));
            if (mode != nullptr) {
                mode->setFocus(Qt::OtherFocusReason);
            } else if (notes != nullptr) {
                notes->setFocus(Qt::OtherFocusReason);
            } else if (QWidget* first = group->nextInFocusChain();
                       first != nullptr && group->isAncestorOf(first)) {
                first->setFocus(Qt::OtherFocusReason);
            }
            // Once the dialog shows: the page has its size then
            QTimer::singleShot(0, scrollArea, [scrollArea, group]() {
                scrollArea->ensureWidgetVisible(group);
            });
        }
    }
    return true;
}

void SettingsDialog::setLengthUnit(LengthUnit unit) {
    m_lengthUnit = unit;
    for (LengthSpinBox* length : m_pageStack->findChildren<LengthSpinBox*>()) {
        length->setDisplayUnit(unit);
    }
}

void SettingsDialog::connectPages() {
    if (!m_iconsPage) {
        return;
    }
    // The icon preview shows the colors chosen on the Theme page, once it is opened
    const auto updatePreview = [this]() {
        const auto [primary, secondary] = m_themePage->iconColors();
        m_iconsPage->setPreviewColors(primary, secondary);
    };
    if (m_themePage) {
        connect(m_themePage, &ThemePage::iconColorsChanged, m_iconsPage, updatePreview);
        updatePreview();
    } else {
        m_iconsPage->setPreviewColors(core::ArtProvider::getInstance().getPrimaryColor(),
                                      core::ArtProvider::getInstance().getSecondaryColor());
    }
}

void SettingsDialog::shareCheckBoxes(QWidget* page) {
    // The pages built before are in the stack already, this one not yet
    for (QCheckBox* box : page->findChildren<QCheckBox*>()) {
        if (!box->property(SettingsPage::SHARED_CHECK_BOX).toBool()) {
            continue;
        }
        for (QCheckBox* other : m_pageStack->findChildren<QCheckBox*>(box->objectName())) {
            if (!other->property(SettingsPage::SHARED_CHECK_BOX).toBool()) {
                continue;
            }
            // A choice made on another page and not applied yet holds here too
            box->setChecked(other->isChecked());
            connect(box, &QCheckBox::toggled, other, &QCheckBox::setChecked);
            connect(other, &QCheckBox::toggled, box, &QCheckBox::setChecked);
        }
    }
}

bool SettingsDialog::hasChanges() const {
    for (const SettingsPage* page : m_pages) {
        if (page->isChanged()) {
            return true;
        }
    }
    return false;
}

bool SettingsDialog::changesCanBeWritten() {
    for (SettingsPage* page : m_pages) {
        const SettingsPage::Problem problem = page->problem();
        if (problem.text.isEmpty()) {
            continue;
        }
        // The page with the value shows it, and the message is named after the page
        QString title = tr("Settings");
        for (const auto& [item, container] : m_builtPages) {
            if (container->isAncestorOf(page)) {
                m_navTree->setCurrentItem(item);
                title = item->text(0);
                break;
            }
        }
        if (problem.field) {
            problem.field->setFocus();
        }
        dialogs::MessageDialog::warning(this, title, problem.text);
        return false;
    }
    return true;
}

QStringList SettingsDialog::applyChanges() {
    QStringList changedKeys;
    if (!hasChanges() || !changesCanBeWritten()) {
        return changedKeys;
    }

    QElapsedTimer timer;
    timer.start();
    auto& artProvider = core::ArtProvider::getInstance();

    // Only theme and icon changes take noticeable time (palette, icon re-render)
    const bool visualChange = (m_themePage && m_themePage->isChanged()) ||
                              (m_iconsPage && m_iconsPage->isChanged());
    if (visualChange) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        // One icon refresh at the end instead of one per changed property
        artProvider.beginBatchUpdate();
    }

    for (SettingsPage* page : m_pages) {
        for (const std::string& key : page->apply()) {
            changedKeys.append(QString::fromStdString(key));
        }
    }
    if (!changedKeys.isEmpty()) {
        core::SettingsManager::getInstance().save();
    }

    if (visualChange) {
        artProvider.endBatchUpdate();
        QApplication::restoreOverrideCursor();
    }
    if (!changedKeys.isEmpty()) {
        emit settingsApplied(changedKeys);
    }

    // Timing for diagnosing slow Apply/OK on users' machines
    core::Logger::getInstance().info("SettingsDialog: {} settings applied in {} ms",
                                     changedKeys.size(), timer.elapsed());
    // Editors and panels lay out and repaint later, in the event loop
    QTimer::singleShot(0, qApp, [timer]() {
        core::Logger::getInstance().info("SettingsDialog: window updated {} ms after Apply/OK",
                                         timer.elapsed());
    });
    return changedKeys;
}

void SettingsDialog::onApply() {
    applyChanges();
}

void SettingsDialog::accept() {
    // A value that cannot be written keeps the dialog open on its page
    if (hasChanges() && !changesCanBeWritten()) {
        return;
    }
    applyChanges();
    KalahariDialog::accept();
}

} // namespace kalahari::gui
