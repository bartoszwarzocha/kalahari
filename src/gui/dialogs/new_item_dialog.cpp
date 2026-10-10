/// @file new_item_dialog.cpp
/// @brief Implementation of NewItemDialog
///
/// OpenSpec #00033: Project File System - Phase C

#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QPixmap>
#include <QScrollArea>
#include <QSignalBlocker>

#include <algorithm>
#include <string>
#include <vector>

using namespace kalahari::gui::dialogs;

namespace {

/// The writer's last choice in the Sections field for a template that shows sections
constexpr const char* SECTIONS_SETTING = "project.sectionSet";

/// The writer's own names of the sections, when the last choice was own names
constexpr const char* SECTION_NAMES_SETTING = "project.sectionNames";

/// Set of names @p id, or the first set when the program has no such set
const kalahari::core::SectionNameSet& sectionNameSet(const QString& id) {
    const QList<kalahari::core::SectionNameSet>& sets =
        kalahari::core::ProjectBook::sectionNameSets();
    const auto found = std::find_if(sets.cbegin(), sets.cend(),
                                    [&id](const kalahari::core::SectionNameSet& set) {
                                        return set.id == id;
                                    });
    return found != sets.cend() ? *found : sets.first();
}

}  // namespace

// ============================================================================
// Constructor
// ============================================================================

NewItemDialog::NewItemDialog(NewItemMode mode, QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_descriptionLabel(nullptr)
    , m_templateList(nullptr)
    , m_searchEdit(nullptr)
    , m_nameEdit(nullptr)
    , m_authorEdit(nullptr)
    , m_languageCombo(nullptr)
    , m_locationEdit(nullptr)
    , m_browseBtn(nullptr)
    , m_subfolderCheck(nullptr)
    , m_authorLabel(nullptr)
    , m_languageLabel(nullptr)
    , m_locationLabel(nullptr)
    , m_folderIcon(nullptr)
    , m_folderLabel(nullptr)
    , m_buttonBox(nullptr)
    , m_createBtn(nullptr)
{
    // Set window title based on mode
    if (m_mode == NewItemMode::Project) {
        setWindowTitle(tr("New Book"));
    } else {
        setWindowTitle(tr("New File"));
    }

    // Set dialog size constraints
    setMinimumSize(700, 500);
    resize(850, 620);

    // Initialize result structure
    m_result.mode = m_mode;
    m_result.createSubfolder = true;

    setupUI();
    createConnections();
    loadDefaults();
    populateTemplates();

    // Select first template if available
    if (m_templateList->count() > 0) {
        m_templateList->setCurrentRow(0);
    }

    validateInput();
}

// ============================================================================
// Public Methods
// ============================================================================

NewItemResult NewItemDialog::result() const {
    return m_result;
}

// ============================================================================
// UI Setup
// ============================================================================

void NewItemDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(11);
    mainLayout->setContentsMargins(11, 11, 11, 11);

    // Create splitter for left/right panels
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // Add description panel (LEFT) and template grid (RIGHT)
    splitter->addWidget(createDescriptionPanel());
    splitter->addWidget(createTemplateGrid());

    // Set initial sizes: 250px for description, rest for templates
    splitter->setSizes({250, 500});

    mainLayout->addWidget(splitter, 1);

    // Add details group (BOTTOM)
    mainLayout->addWidget(createDetailsGroup(), 0);

    // Dialog buttons
    m_buttonBox = new QDialogButtonBox(this);
    QString createBtnText = (m_mode == NewItemMode::Project) ? tr("Create Book") : tr("Create");
    m_createBtn = m_buttonBox->addButton(createBtnText, QDialogButtonBox::AcceptRole);
    m_buttonBox->addButton(QDialogButtonBox::Cancel);
    m_createBtn->setEnabled(false);
    mainLayout->addWidget(m_buttonBox);
}

QWidget* NewItemDialog::createDescriptionPanel() {
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setSpacing(11);
    layout->setContentsMargins(11, 11, 11, 11);

    // Fixed width for description panel
    panel->setFixedWidth(250);

    // Large icon (64x64)
    m_iconLabel = new QLabel(panel);
    m_iconLabel->setFixedSize(64, 64);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setScaledContents(true);
    layout->addWidget(m_iconLabel, 0, Qt::AlignHCenter);

    // Template name (bold)
    m_titleLabel = new QLabel(panel);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel);

    // Separator line
    QFrame* separator = new QFrame(panel);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    // Description text (rich text); it scrolls when the panel is too low for the features
    m_descriptionLabel = new QLabel(panel);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_descriptionLabel->setTextFormat(Qt::RichText);
    QScrollArea* descriptionScroll = new QScrollArea(panel);
    descriptionScroll->setFrameShape(QFrame::NoFrame);
    descriptionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    descriptionScroll->setWidgetResizable(true);
    descriptionScroll->setWidget(m_descriptionLabel);
    layout->addWidget(descriptionScroll, 1);

    return panel;
}

QWidget* NewItemDialog::createTemplateGrid() {
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setSpacing(6);
    layout->setContentsMargins(0, 0, 0, 0);

    // Group box
    QGroupBox* group = new QGroupBox(tr("Templates"), panel);
    QVBoxLayout* groupLayout = new QVBoxLayout(group);

    // Search filter (for future use)
    m_searchEdit = new QLineEdit(group);
    m_searchEdit->setPlaceholderText(tr("Search templates..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->hide(); // Hidden for now, enable when many templates available
    groupLayout->addWidget(m_searchEdit);

    // Template list with icon mode
    m_templateList = new QListWidget(group);
    m_templateList->setViewMode(QListView::IconMode);
    m_templateList->setIconSize(QSize(48, 48));
    m_templateList->setSpacing(10);
    m_templateList->setResizeMode(QListView::Adjust);
    m_templateList->setMovement(QListView::Static);
    m_templateList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_templateList->setFlow(QListView::LeftToRight);
    m_templateList->setWrapping(true);
    m_templateList->setGridSize(QSize(110, 96));
    m_templateList->setWordWrap(true);  // Translated names may need two lines
    groupLayout->addWidget(m_templateList, 1);

    layout->addWidget(group);
    return panel;
}

QWidget* NewItemDialog::createDetailsGroup() {
    QGroupBox* group = new QGroupBox(tr("Details"), this);
    QGridLayout* layout = new QGridLayout(group);
    layout->setSpacing(6);
    layout->setContentsMargins(11, 11, 11, 11);

    int row = 0;

    // Name/Title
    QString nameLabel = (m_mode == NewItemMode::Project) ? tr("Book Title:") : tr("File Name:");
    QLabel* nameLbl = new QLabel(nameLabel, group);
    layout->addWidget(nameLbl, row, 0);

    m_nameEdit = new QLineEdit(group);
    m_nameEdit->setPlaceholderText((m_mode == NewItemMode::Project)
        ? tr("Enter book title...")
        : tr("Enter file name..."));
    m_nameEdit->setToolTip((m_mode == NewItemMode::Project)
        ? tr("The title of your new book")
        : tr("The name of the new file"));
    layout->addWidget(m_nameEdit, row, 1, 1, 2);
    row++;

    // Project-only fields
    if (m_mode == NewItemMode::Project) {
        // Author
        m_authorLabel = new QLabel(tr("Author:"), group);
        layout->addWidget(m_authorLabel, row, 0);

        m_authorEdit = new QLineEdit(group);
        m_authorEdit->setPlaceholderText(tr("Enter author name..."));
        m_authorEdit->setToolTip(tr("The author name for the book metadata"));
        layout->addWidget(m_authorEdit, row, 1, 1, 2);
        row++;

        // Language
        m_languageLabel = new QLabel(tr("Language:"), group);
        layout->addWidget(m_languageLabel, row, 0);

        m_languageCombo = new QComboBox(group);
        m_languageCombo->setToolTip(tr("Primary language for the book content"));
        // Add common languages
        m_languageCombo->addItem(tr("English"), "en");
        m_languageCombo->addItem(tr("Polish"), "pl");
        m_languageCombo->addItem(tr("German"), "de");
        m_languageCombo->addItem(tr("French"), "fr");
        m_languageCombo->addItem(tr("Spanish"), "es");
        m_languageCombo->addItem(tr("Italian"), "it");
        m_languageCombo->addItem(tr("Portuguese"), "pt");
        m_languageCombo->addItem(tr("Russian"), "ru");
        m_languageCombo->addItem(tr("Chinese"), "zh");
        m_languageCombo->addItem(tr("Japanese"), "ja");
        layout->addWidget(m_languageCombo, row, 1, 1, 2);
        row++;

        // Sections: the names of the three sections of the book, in its language, or none
        QLabel* sectionsLabel = new QLabel(tr("Sections:"), group);
        layout->addWidget(sectionsLabel, row, 0);

        m_sectionsCombo = new SectionsComboBox(group);
        m_sectionsCombo->setObjectName(QStringLiteral("newBookSectionsCombo"));
        sectionsLabel->setBuddy(m_sectionsCombo);
        layout->addWidget(m_sectionsCombo, row, 1, 1, 2);
        row++;

        // The writer's own names of the sections
        m_sectionNamesLabel = new QLabel(tr("Section names:"), group);
        layout->addWidget(m_sectionNamesLabel, row, 0);

        m_sectionNames = new SectionNamesEdit(Qt::Horizontal, group);
        m_sectionNames->setObjectName(QStringLiteral("newBookSectionNames"));
        m_sectionNamesLabel->setBuddy(m_sectionNames->firstField());
        layout->addWidget(m_sectionNames, row, 1, 1, 2);
        m_sectionNamesLabel->hide();
        m_sectionNames->hide();
        row++;

        // Location
        m_locationLabel = new QLabel(tr("Location:"), group);
        layout->addWidget(m_locationLabel, row, 0);

        m_locationEdit = new QLineEdit(group);
        m_locationEdit->setPlaceholderText(tr("Select book folder..."));
        m_locationEdit->setToolTip(tr("The folder where the book will be created"));
        layout->addWidget(m_locationEdit, row, 1);

        m_browseBtn = new QPushButton(tr("Browse..."), group);
        m_browseBtn->setToolTip(tr("Browse for book folder"));
        layout->addWidget(m_browseBtn, row, 2);
        row++;

        // Create subfolder checkbox
        m_subfolderCheck = new QCheckBox(tr("Create subfolder with book name"), group);
        m_subfolderCheck->setChecked(true);
        m_subfolderCheck->setToolTip(tr("When checked, creates a new folder named after the book inside the selected location"));
        layout->addWidget(m_subfolderCheck, row, 1, 1, 2);
        row++;

        // The folder of the new book, or why the book cannot be made there
        QWidget* folderLine = new QWidget(group);
        QHBoxLayout* folderLayout = new QHBoxLayout(folderLine);
        folderLayout->setContentsMargins(0, 0, 0, 0);
        m_folderIcon = new QLabel(folderLine);
        m_folderIcon->setFixedSize(16, 16);
        folderLayout->addWidget(m_folderIcon, 0, Qt::AlignTop);
        m_folderLabel = new QLabel(folderLine);
        m_folderLabel->setObjectName(QStringLiteral("newBookFolderLabel"));
        m_folderLabel->setTextFormat(Qt::PlainText);
        m_folderLabel->setWordWrap(true);
        folderLayout->addWidget(m_folderLabel, 1);
        layout->addWidget(folderLine, row, 1, 1, 2);
    }

    // Make columns stretch properly
    layout->setColumnStretch(1, 1);

    return group;
}

void NewItemDialog::createConnections() {
    // Template selection
    connect(m_templateList, &QListWidget::currentItemChanged,
            this, &NewItemDialog::onTemplateSelected);

    // Name change
    connect(m_nameEdit, &QLineEdit::textChanged,
            this, &NewItemDialog::onTitleChanged);

    // Project mode connections
    if (m_mode == NewItemMode::Project) {
        connect(m_browseBtn, &QPushButton::clicked,
                this, &NewItemDialog::onBrowseLocation);
        connect(m_locationEdit, &QLineEdit::textChanged,
                this, [this](const QString&) { validateInput(); });
        connect(m_subfolderCheck, &QCheckBox::toggled,
                this, [this](bool) { validateInput(); });

        // The names of the sets are in the language of the book
        connect(m_languageCombo, &QComboBox::currentIndexChanged,
                this, [this](int) { populateSections(); });
        connect(m_sectionsCombo, &QComboBox::currentIndexChanged,
                this, [this](int) { onSectionsChosen(); });
        connect(m_sectionNames, &SectionNamesEdit::edited,
                this, [this]() { m_sectionNamesTyped = true; });
    }

    // Dialog buttons
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &NewItemDialog::onAccept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    // Theme changes
    connect(&kalahari::core::ArtProvider::getInstance(),
            &kalahari::core::ArtProvider::resourcesChanged,
            this, &NewItemDialog::onThemeChanged);
}

void NewItemDialog::populateTemplates() {
    m_templateList->clear();

    auto& registry = TemplateRegistry::getInstance();
    auto& art = kalahari::core::ArtProvider::getInstance();

    // Get templates based on mode
    std::vector<TemplateInfo> templates;
    if (m_mode == NewItemMode::Project) {
        templates = registry.getProjectTemplates();
    } else {
        templates = registry.getFileTemplates();
    }

    // Add templates to list
    for (const auto& tmpl : templates) {
        QListWidgetItem* item = new QListWidgetItem(m_templateList);
        item->setText(tmpl.name);
        item->setData(Qt::UserRole, tmpl.id);
        item->setToolTip(tmpl.description);

        // Get icon (48x48 for grid)
        QIcon icon = art.getIcon(tmpl.iconId, kalahari::core::IconContext::Dialog);
        if (!icon.isNull()) {
            item->setIcon(icon);
        }

        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        item->setFlags(item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    }
}

void NewItemDialog::updateDescription(const QString& templateId) {
    if (templateId.isEmpty()) {
        m_iconLabel->clear();
        m_titleLabel->clear();
        m_descriptionLabel->clear();
        return;
    }

    auto& registry = TemplateRegistry::getInstance();
    TemplateInfo info = registry.getTemplate(templateId);

    if (!info.isValid()) {
        return;
    }

    // Update icon (64x64) using getThemedIcon for auto-refresh support
    auto& art = kalahari::core::ArtProvider::getInstance();
    QIcon icon = art.getThemedIcon(info.iconId);
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(64, 64));
    } else {
        m_iconLabel->clear();
    }

    // Update title
    m_titleLabel->setText(info.name);

    // Build rich description
    QString html;
    html += "<p>" + info.description.toHtmlEscaped().replace("\n\n", "</p><p>").replace("\n", "<br/>") + "</p>";

    // Add features list
    if (!info.features.isEmpty()) {
        html += "<p><b>" + tr("Features:") + "</b></p><ul>";
        for (const QString& feature : info.features) {
            html += "<li>" + feature.toHtmlEscaped() + "</li>";
        }
        html += "</ul>";
    }

    m_descriptionLabel->setText(html);
}

void NewItemDialog::validateInput() {
    bool valid = true;

    // Name is always required
    if (m_nameEdit->text().trimmed().isEmpty()) {
        valid = false;
    }

    // For project mode, location is also required, and the book's folder must be new or empty
    if (m_mode == NewItemMode::Project) {
        const QString location = m_locationEdit->text().trimmed();
        if (location.isEmpty()) {
            valid = false;
        }

        const bool subfolder = m_subfolderCheck->isChecked();
        const QString folder =
            location.isEmpty() ? QString()
                               : kalahari::core::ProjectManager::newProjectFolder(
                                     location, m_nameEdit->text(), subfolder);
        QString folderName = QFileInfo(folder).fileName();
        if (folderName.isEmpty()) {
            folderName = QDir::toNativeSeparators(folder);  // a drive or the root
        }
        QString warningIcon;
        if (folder.isEmpty()) {
            m_folderLabel->clear();
        } else if (kalahari::core::ProjectManager::canHoldNewProject(folder)) {
            m_folderLabel->setText(tr("The book will be created in the folder '%1'.")
                                       .arg(folderName));
        } else {
            valid = false;
            warningIcon = QStringLiteral("common.warning");
            m_folderLabel->setText(
                subfolder ? tr("The folder '%1' is already in this location and is not empty. "
                               "Change the title or the location.").arg(folderName)
                          : tr("The folder '%1' is not empty. Choose another folder or create "
                               "a subfolder with the book name.").arg(folderName));
        }
        m_folderIcon->setPixmap(
            warningIcon.isEmpty()
                ? QPixmap()
                : kalahari::core::ArtProvider::getInstance().getIcon(warningIcon).pixmap(16, 16));
    }

    // Template must be selected
    if (!m_templateList->currentItem()) {
        valid = false;
    }

    m_createBtn->setEnabled(valid);
}

void NewItemDialog::loadDefaults() {
    auto& settings = kalahari::core::SettingsManager::getInstance();

    if (m_mode == NewItemMode::Project) {
        // Load default author
        std::string defaultAuthor = settings.get<std::string>("project.defaultAuthor");
        if (!defaultAuthor.empty()) {
            m_authorEdit->setText(QString::fromStdString(defaultAuthor));
        }

        // Load default language
        std::string defaultLang = settings.get<std::string>("project.defaultLanguage");
        int langIndex = m_languageCombo->findData(QString::fromStdString(defaultLang));
        if (langIndex >= 0) {
            m_languageCombo->setCurrentIndex(langIndex);
        }

        // Load default location
        std::string defaultLocation = settings.get<std::string>("project.defaultLocation");
        if (defaultLocation.empty()) {
            // Use Documents folder as default
            QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
            m_locationEdit->setText(docsPath);
        } else {
            m_locationEdit->setText(QString::fromStdString(defaultLocation));
        }

        // Sections: the writer's last choice for a template that shows them, none for the
        // others
        populateSections();
        m_sectionsShown = QString::fromStdString(settings.get<std::string>(SECTIONS_SETTING));
        if (m_sectionsCombo->findData(m_sectionsShown) < 0) {
            m_sectionsShown = kalahari::core::ProjectBook::sectionNameSets().first().id;
        }
        m_sectionsHidden = QString::fromLatin1(SectionsComboBox::NO_SECTIONS);
        m_namesSet = SectionsComboBox::isNameSet(m_sectionsShown)
                         ? m_sectionsShown
                         : kalahari::core::ProjectBook::sectionNameSets().first().id;
        if (m_sectionsShown == QLatin1String(kalahari::core::ProjectBook::CUSTOM_SECTIONS)) {
            QStringList names;
            for (const std::string& name :
                 settings.get<std::vector<std::string>>(SECTION_NAMES_SETTING)) {
                names << QString::fromStdString(name);
            }
            m_sectionNames->setNames(names);
            m_sectionNamesTyped = true;
        }
    }
}

void NewItemDialog::populateSections() {
    if (!m_sectionsCombo) {
        return;
    }
    const QString language = m_languageCombo->currentData().toString();
    m_sectionsCombo->setLanguage(language);
    m_sectionNames->setLanguage(language);
    fillSectionNames();
}

bool NewItemDialog::showsSections(const QString& templateId) {
    const QString typeId = TemplateRegistry::getInstance().getTemplate(templateId).typeId;
    if (typeId.isEmpty()) {
        return false;  // a user project
    }
    const kalahari::core::BookTypePackage* type =
        kalahari::core::ProjectManager::getInstance().bookTypes().package(typeId);
    return type != nullptr && type->partsLayer;
}

void NewItemDialog::showSectionsChoice() {
    if (!m_sectionsCombo) {
        return;
    }
    const QListWidgetItem* current = m_templateList->currentItem();
    const bool shown = current && showsSections(current->data(Qt::UserRole).toString());
    {
        const QSignalBlocker blocker(m_sectionsCombo);
        m_sectionsCombo->choose(shown ? m_sectionsShown : m_sectionsHidden);
    }
    onSectionsChosen();
}

void NewItemDialog::onSectionsChosen() {
    const QString chosen = m_sectionsCombo->choice();
    const QListWidgetItem* current = m_templateList->currentItem();
    if (current && showsSections(current->data(Qt::UserRole).toString())) {
        m_sectionsShown = chosen;
    } else {
        m_sectionsHidden = chosen;
    }
    if (SectionsComboBox::isNameSet(chosen)) {
        m_namesSet = chosen;
    }

    const bool custom = chosen == QLatin1String(kalahari::core::ProjectBook::CUSTOM_SECTIONS);
    m_sectionNamesLabel->setVisible(custom);
    m_sectionNames->setVisible(custom);
    fillSectionNames();
}

void NewItemDialog::fillSectionNames() {
    if (m_sectionNamesTyped || !m_sectionsCombo) {
        return;
    }
    const QString language = m_languageCombo->currentData().toString();
    const kalahari::core::SectionNameSet& set = sectionNameSet(m_namesSet);
    QStringList names;
    for (kalahari::core::BookPlace place : SECTION_PLACES) {
        names << set.name(place, language);
    }
    m_sectionNames->setNames(names);
}

kalahari::core::BookSections NewItemDialog::chosenSections() const {
    kalahari::core::BookSections sections;
    const QString chosen = m_sectionsCombo->choice();
    if (chosen == QLatin1String(SectionsComboBox::NO_SECTIONS)) {
        sections.shown = false;  // with the names of the first set, if the writer shows them
        return sections;
    }
    sections.set = chosen;
    if (chosen == QLatin1String(kalahari::core::ProjectBook::CUSTOM_SECTIONS)) {
        sections.names = m_sectionNames->names();
    }
    return sections;
}

void NewItemDialog::saveSectionsChoice() const {
    const QListWidgetItem* current = m_templateList->currentItem();
    if (!current || !showsSections(current->data(Qt::UserRole).toString())) {
        return;  // the others start without sections
    }
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const QString chosen = m_sectionsCombo->choice();
    settings.set<std::string>(SECTIONS_SETTING, chosen.toStdString());
    if (chosen == QLatin1String(kalahari::core::ProjectBook::CUSTOM_SECTIONS)) {
        std::vector<std::string> names;
        for (const QString& name : m_sectionNames->names()) {
            names.push_back(name.toStdString());
        }
        settings.set<std::vector<std::string>>(SECTION_NAMES_SETTING, names);
    }
}

// ============================================================================
// Slots
// ============================================================================

void NewItemDialog::onTemplateSelected(QListWidgetItem* current, QListWidgetItem* previous) {
    Q_UNUSED(previous)

    if (!current) {
        updateDescription(QString());
        validateInput();
        return;
    }

    QString templateId = current->data(Qt::UserRole).toString();
    updateDescription(templateId);
    showSectionsChoice();
    validateInput();
}

void NewItemDialog::onBrowseLocation() {
    QString currentPath = m_locationEdit->text();
    if (currentPath.isEmpty()) {
        currentPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    QString folder = QFileDialog::getExistingDirectory(
        this,
        tr("Select Book Location"),
        currentPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!folder.isEmpty()) {
        m_locationEdit->setText(QDir::toNativeSeparators(folder));
    }
}

void NewItemDialog::onTitleChanged(const QString& text) {
    Q_UNUSED(text)
    validateInput();
}

void NewItemDialog::onAccept() {
    // Populate result structure
    m_result.mode = m_mode;
    m_result.title = m_nameEdit->text().trimmed();

    // Get selected template
    QListWidgetItem* current = m_templateList->currentItem();
    if (current) {
        m_result.templateId = current->data(Qt::UserRole).toString();
    }

    // Project-specific fields
    if (m_mode == NewItemMode::Project) {
        m_result.author = m_authorEdit->text().trimmed();
        m_result.language = m_languageCombo->currentData().toString();
        m_result.location = m_locationEdit->text().trimmed();
        m_result.createSubfolder = m_subfolderCheck->isChecked();
        m_result.sections = chosenSections();
        saveSectionsChoice();
    }

    accept();
}

void NewItemDialog::onThemeChanged() {
    // Refresh template icons
    populateTemplates();

    // Refresh the icon of the folder line
    validateInput();

    // Refresh description icon
    QListWidgetItem* current = m_templateList->currentItem();
    if (current) {
        QString templateId = current->data(Qt::UserRole).toString();
        updateDescription(templateId);
    }
}
