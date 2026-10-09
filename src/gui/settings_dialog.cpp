/// @file settings_dialog.cpp
/// @brief Implementation of SettingsDialog

#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

SettingsDialog::SettingsDialog(QWidget* parent, bool diagnosticMode,
                               editor::SpellCheckService* spelling)
    : KalahariDialog(parent)
    , m_navTree(nullptr)
    , m_pageStack(nullptr)
    , m_diagnosticMode(diagnosticMode)
    , m_spelling(spelling)
    , m_lengthUnit(currentLengthUnit())
{
    setHeading(tr("Settings"),
               tr("Choose a group on the left; Apply and OK save only the options you changed."));
    setHeadingIcon(QStringLiteral("edit.settings"));
    // A low heading leaves the room to the pages
    setCompactHeading(true);
    setApplyButtonVisible(true);
    setModal(true);
    // Tall enough for the longest editor page where the screen allows; a page that does
    // not fit scrolls
    QSize size(800, 700);
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
    addPage(editor, tr("Spelling"), [this]() { return new EditorSpellingPage(m_spelling); });
    addPlannedPage(editor, tr("Auto-correct"),
                   tr("Planned features:\n"
                      "- Automatic capitalization\n"
                      "- Common typo corrections\n"
                      "- Custom replacement rules"));
    addPlannedPage(editor, tr("Completion"),
                   tr("Planned features:\n"
                      "- Word completion suggestions\n"
                      "- Character name completion\n"
                      "- Location name completion"));

    addPage(nullptr, tr("Annotations"), []() { return new AnnotationsPage(); });

    QTreeWidgetItem* files = category(tr("Files"));
    addPlannedPage(files, tr("Backup"),
                   tr("Planned features:\n"
                      "- Automatic backup frequency\n"
                      "- Backup location selection\n"
                      "- Number of backup copies to keep\n"
                      "- Restore from backup"));
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
        const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
        auto* placeholder = new QWidget();
        auto* layout = new QVBoxLayout(placeholder);
        auto* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: %1;")
            .arg(theme.palette.windowText.name()));
        layout->addWidget(titleLabel);
        auto* descriptionLabel = new QLabel(tr("These settings will be available in a future version.")
                                            + QStringLiteral("\n\n") + description);
        descriptionLabel->setWordWrap(true);
        descriptionLabel->setStyleSheet(QStringLiteral("color: %1; margin-top: 20px;")
            .arg(theme.palette.placeholderText.name()));
        layout->addWidget(descriptionLabel);
        layout->addStretch();
        return placeholder;
    });
    // Greyed out in the tree, like the options the program does not use yet
    item->setForeground(0, core::ThemeManager::getInstance().getCurrentTheme().palette.mid);
    item->setToolTip(0, tr("Coming in future version"));
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

bool SettingsDialog::hasChanges() const {
    for (const SettingsPage* page : m_pages) {
        if (page->isChanged()) {
            return true;
        }
    }
    return false;
}

QStringList SettingsDialog::applyChanges() {
    QStringList changedKeys;
    if (!hasChanges()) {
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
    applyChanges();
    KalahariDialog::accept();
}

} // namespace gui
} // namespace kalahari
