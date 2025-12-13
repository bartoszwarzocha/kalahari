/// @file add_to_project_dialog.cpp
/// @brief Implementation of AddToProjectDialog
///
/// OpenSpec #00033: Project File System - Phase F

#include "kalahari/gui/dialogs/add_to_project_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/document.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QButtonGroup>
#include <QPushButton>
#include <QFileInfo>

using namespace kalahari::gui::dialogs;

// ============================================================================
// Constructor
// ============================================================================

AddToProjectDialog::AddToProjectDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_fileLabel(nullptr)
    , m_sectionCombo(nullptr)
    , m_partCombo(nullptr)
    , m_partLabel(nullptr)
    , m_titleEdit(nullptr)
    , m_copyRadio(nullptr)
    , m_moveRadio(nullptr)
    , m_buttonBox(nullptr)
    , m_addBtn(nullptr)
{
    setWindowTitle(tr("Add File to Project"));

    // Set window icon (using driveFileMove as representative of adding file to project)
    setWindowIcon(kalahari::core::ArtProvider::getInstance().getIcon("common.driveFileMove"));

    // Set dialog size constraints
    setMinimumWidth(400);
    setMaximumWidth(600);

    // Initialize result structure with defaults
    m_result.copyFile = true;
    m_result.targetSection = "body";

    setupUI();
    createConnections();
    populateSections();
    populateParts();

    // Set initial title from file name
    m_titleEdit->setText(extractFileName(m_filePath));

    validateInput();
}

// ============================================================================
// Public Methods
// ============================================================================

AddToProjectResult AddToProjectDialog::result() const {
    return m_result;
}

// ============================================================================
// UI Setup
// ============================================================================

void AddToProjectDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(11);
    mainLayout->setContentsMargins(11, 11, 11, 11);

    // File info group
    QGroupBox* fileGroup = new QGroupBox(tr("File"), this);
    QVBoxLayout* fileLayout = new QVBoxLayout(fileGroup);

    // Extract just the file name for display
    QFileInfo fileInfo(m_filePath);
    m_fileLabel = new QLabel(fileInfo.fileName(), fileGroup);
    m_fileLabel->setToolTip(m_filePath);
    fileLayout->addWidget(m_fileLabel);

    mainLayout->addWidget(fileGroup);

    // Target group
    QGroupBox* targetGroup = new QGroupBox(tr("Target Location"), this);
    QFormLayout* formLayout = new QFormLayout(targetGroup);
    formLayout->setSpacing(6);
    formLayout->setContentsMargins(11, 11, 11, 11);

    // Target section combo
    m_sectionCombo = new QComboBox(targetGroup);
    m_sectionCombo->setToolTip(tr("Select the project section where the file will be added"));
    formLayout->addRow(tr("Section:"), m_sectionCombo);

    // Target part combo (only visible for body section)
    m_partLabel = new QLabel(tr("Part:"), targetGroup);
    m_partCombo = new QComboBox(targetGroup);
    m_partCombo->setToolTip(tr("Select the part where the file will be added (body section only)"));
    formLayout->addRow(m_partLabel, m_partCombo);

    // Title input
    m_titleEdit = new QLineEdit(targetGroup);
    m_titleEdit->setPlaceholderText(tr("Enter display title..."));
    m_titleEdit->setToolTip(tr("The title that will be shown in the Navigator panel"));
    formLayout->addRow(tr("Title:"), m_titleEdit);

    mainLayout->addWidget(targetGroup);

    // Action group
    QGroupBox* actionGroup = new QGroupBox(tr("Action"), this);
    QVBoxLayout* actionLayout = new QVBoxLayout(actionGroup);
    actionLayout->setSpacing(6);
    actionLayout->setContentsMargins(11, 11, 11, 11);

    m_copyRadio = new QRadioButton(tr("Copy file to project"), actionGroup);
    m_copyRadio->setToolTip(tr("Create a copy of the file in the project folder (original file remains unchanged)"));
    m_copyRadio->setChecked(true);
    actionLayout->addWidget(m_copyRadio);

    m_moveRadio = new QRadioButton(tr("Move file to project"), actionGroup);
    m_moveRadio->setToolTip(tr("Move the file into the project folder (original file will be deleted)"));
    actionLayout->addWidget(m_moveRadio);

    // Button group to make radio buttons exclusive
    QButtonGroup* radioGroup = new QButtonGroup(this);
    radioGroup->addButton(m_copyRadio);
    radioGroup->addButton(m_moveRadio);

    mainLayout->addWidget(actionGroup);

    // Add stretch to push buttons to bottom
    mainLayout->addStretch(1);

    // Dialog buttons
    m_buttonBox = new QDialogButtonBox(this);
    m_addBtn = m_buttonBox->addButton(tr("Add to Project"), QDialogButtonBox::AcceptRole);
    m_buttonBox->addButton(QDialogButtonBox::Cancel);
    m_addBtn->setEnabled(false);
    mainLayout->addWidget(m_buttonBox);
}

void AddToProjectDialog::createConnections() {
    // Section selection
    connect(m_sectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddToProjectDialog::onSectionChanged);

    // Part selection (also triggers validation)
    connect(m_partCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { validateInput(); });

    // Title change
    connect(m_titleEdit, &QLineEdit::textChanged,
            this, &AddToProjectDialog::onTitleChanged);

    // Dialog buttons
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &AddToProjectDialog::onAccept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

void AddToProjectDialog::populateSections() {
    m_sectionCombo->clear();

    // Add all available sections
    // Data contains internal section ID
    m_sectionCombo->addItem(tr("Front Matter"), "frontmatter");
    m_sectionCombo->addItem(tr("Body"), "body");
    m_sectionCombo->addItem(tr("Back Matter"), "backmatter");
    m_sectionCombo->addItem(tr("Mind Maps"), "mindmaps");
    m_sectionCombo->addItem(tr("Timelines"), "timelines");

    // Default to Body section
    int bodyIndex = m_sectionCombo->findData("body");
    if (bodyIndex >= 0) {
        m_sectionCombo->setCurrentIndex(bodyIndex);
    }
}

void AddToProjectDialog::populateParts() {
    m_partCombo->clear();

    // Get parts from ProjectManager
    auto& pm = kalahari::core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        m_partCombo->setEnabled(false);
        return;
    }

    auto* doc = pm.getDocument();
    if (!doc) {
        m_partCombo->setEnabled(false);
        return;
    }

    // Access the book structure through the document
    // For now, we'll check if body section is selected
    QString currentSection = m_sectionCombo->currentData().toString();
    bool isBodySection = (currentSection == "body");

    m_partCombo->setVisible(isBodySection);
    m_partLabel->setVisible(isBodySection);

    if (!isBodySection) {
        return;
    }

    // Get book from document and populate parts
    const auto& book = doc->getBook();
    const auto& parts = book.getBody();

    if (parts.empty()) {
        // No parts exist, add a placeholder
        m_partCombo->addItem(tr("(No parts available)"), "");
        m_partCombo->setEnabled(false);
    } else {
        m_partCombo->setEnabled(true);
        for (const auto& part : parts) {
            QString partTitle = QString::fromStdString(part->getTitle());
            QString partId = QString::fromStdString(part->getId());
            m_partCombo->addItem(partTitle, partId);
        }
    }
}

void AddToProjectDialog::validateInput() {
    bool valid = true;

    // Title is required
    if (m_titleEdit->text().trimmed().isEmpty()) {
        valid = false;
    }

    // Section must be selected
    if (m_sectionCombo->currentIndex() < 0) {
        valid = false;
    }

    // If body section, part must be selected (unless no parts available)
    QString currentSection = m_sectionCombo->currentData().toString();
    if (currentSection == "body") {
        QString partId = m_partCombo->currentData().toString();
        if (partId.isEmpty() && m_partCombo->isEnabled()) {
            valid = false;
        }
    }

    m_addBtn->setEnabled(valid);
}

QString AddToProjectDialog::extractFileName(const QString& filePath) const {
    QFileInfo fileInfo(filePath);
    return fileInfo.completeBaseName(); // File name without extension
}

// ============================================================================
// Slots
// ============================================================================

void AddToProjectDialog::onSectionChanged(int index) {
    Q_UNUSED(index)

    // Update part combo visibility and contents
    populateParts();
    validateInput();
}

void AddToProjectDialog::onTitleChanged(const QString& text) {
    Q_UNUSED(text)
    validateInput();
}

void AddToProjectDialog::onAccept() {
    // Populate result structure
    m_result.targetSection = m_sectionCombo->currentData().toString();
    m_result.newTitle = m_titleEdit->text().trimmed();
    m_result.copyFile = m_copyRadio->isChecked();

    // Part ID only relevant for body section
    if (m_result.targetSection == "body") {
        m_result.targetPart = m_partCombo->currentData().toString();
    } else {
        m_result.targetPart.clear();
    }

    accept();
}
