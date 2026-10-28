/// @file settings_dialog.cpp
/// @brief Implementation of SettingsDialog

#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/settings/grammar_page.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/settings/shortcuts_page.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QApplication>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QFont>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari {
namespace gui {

SettingsDialog::SettingsDialog(QWidget* parent, bool diagnosticMode,
                               editor::SpellCheckService* spelling, GrammarCoordinator* grammar)
    : KalahariDialog(parent)
    , m_navTree(nullptr)
    , m_pageStack(nullptr)
    , m_diagnosticMode(diagnosticMode)
    , m_spelling(spelling)
    , m_grammar(grammar)
    , m_lengthUnit(currentLengthUnit())
{
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

    connect(m_navTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current) { showPage(current); });
    connect(this, &KalahariDialog::applyClicked, this, &SettingsDialog::onApply);

    m_navTree->setCurrentItem(m_navTree->topLevelItem(0));
}

void SettingsDialog::createNavigationTree() {
    const auto category = [this](const QString& title) {
        auto* item = new QTreeWidgetItem(m_navTree);
        item->setText(0, title);
        item->setExpanded(true);
        return item;
    };
    addPage(nullptr, tr("General"), [this]() {
        auto* generalPage = new GeneralPage();
        connect(generalPage, &GeneralPage::lengthUnitChanged, this,
                [this](const QString& unit) { setLengthUnit(lengthUnitFromName(unit)); });
        return generalPage;
    });

    QTreeWidgetItem* appearance = category(tr("Appearance"));
    addPage(appearance, tr("General"), []() { return new AppearanceGeneralPage(); });
    addPage(appearance, tr("Theme"), []() { return new ThemePage(); });
    addPage(appearance, tr("Icons"), []() { return new IconsPage(); });
    addPage(appearance, tr("Dashboard"), []() { return new DashboardPage(); });

    QTreeWidgetItem* editor = category(tr("Editor"));
    addPage(editor, tr("General"), []() { return new EditorGeneralPage(); });
    addPage(editor, tr("Colors"), []() { return new EditorColorsPage(); });
    addPage(editor, tr("Cursor"), []() { return new EditorCursorPage(); });
    addPage(editor, tr("Pages and Margins"), []() { return new EditorPagesPage(); });
    m_pageItems[SPELLING_PAGE] =
        addPage(editor, tr("Spelling"), [this]() { return new EditorSpellingPage(m_spelling); });
    addPage(editor, tr("Grammar"), [this]() { return new EditorGrammarPage(m_grammar); });
    addPage(editor, tr("Auto-correct"), []() { return new EditorAutoCorrectPage(); });
    //: Settings > Editor > Versions: the versions of the texts. In Polish: Wersje
    m_pageItems[VERSIONS_PAGE] =
        addPage(editor, tr("Versions"), []() { return new EditorVersionsPage(); });
    addPlannedPage(editor, tr("Completion"),
                   tr("Planned features:\n"
                      "- Word completion suggestions\n"
                      "- Character name completion\n"
                      "- Location name completion"));

    m_pageItems[ANNOTATIONS_PAGE] =
        addPage(nullptr, tr("Annotations"), []() { return new AnnotationsPage(); });
    addPage(nullptr, tr("Navigator"), []() { return new NavigatorPage(); });
    m_pageItems[SHORTCUTS_PAGE] =
        addPage(nullptr, tr("Keyboard Shortcuts"), []() { return new ShortcutsPage(); });

    QTreeWidgetItem* files = category(tr("Files"));
    addPage(files, tr("Folders"), []() { return new FoldersPage(); });
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
    auto factory = m_factories.find(item);
    if (factory == m_factories.end()) {
        // A category: show its first page
        if (item->childCount() > 0) {
            m_navTree->setCurrentItem(item->child(0));
        }
        return;
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
    m_pageStack->setCurrentWidget(built->second);
}

void SettingsDialog::showShortcutsPage() {
    openPage(SHORTCUTS_PAGE);
}

bool SettingsDialog::openPage(const QString& page) {
    const auto item = m_pageItems.find(page);
    if (item == m_pageItems.end()) {
        return false;
    }
    m_navTree->setCurrentItem(item->second);
    // The cursor in the search of the shortcuts, so the keys go there at once (the window
    // gives it to the search when it shows)
    if (page == QLatin1String(SHORTCUTS_PAGE)) {
        if (auto* shortcuts = m_pageStack->findChild<ShortcutsPage*>()) {
            shortcuts->setFocus(Qt::OtherFocusReason);
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

} // namespace gui
} // namespace kalahari
