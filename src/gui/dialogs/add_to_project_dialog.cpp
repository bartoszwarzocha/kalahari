/// @file add_to_project_dialog.cpp
/// @brief Implementation of AddToProjectDialog
///
/// OpenSpec #00033: Project File System - Phase F

#include "kalahari/gui/dialogs/add_to_project_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QButtonGroup>
#include <QPushButton>
#include <QFileInfo>
#include <QSignalBlocker>

#include <utility>

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
    , m_kindCombo(nullptr)
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

    setupUI();
    populateSections();
    populateParts();
    populateKinds();
    createConnections();

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

    // Kind of the new element: a chapter, a prologue, a preface...
    m_kindCombo = new QComboBox(targetGroup);
    formLayout->addRow(tr("Kind:"), m_kindCombo);

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

    // Part selection: the kinds follow it
    connect(m_partCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddToProjectDialog::onPartChanged);

    // Kind selection
    connect(m_kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
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

    // Data contains the section's place in the book
    m_sectionCombo->addItem(tr("Front Matter"), static_cast<int>(kalahari::core::BookPlace::Front));
    m_sectionCombo->addItem(tr("Body"), static_cast<int>(kalahari::core::BookPlace::Main));
    m_sectionCombo->addItem(tr("Back Matter"), static_cast<int>(kalahari::core::BookPlace::Back));

    // Default to Body section
    m_sectionCombo->setCurrentIndex(1);
}

kalahari::core::BookPlace AddToProjectDialog::currentPlace() const {
    return static_cast<kalahari::core::BookPlace>(m_sectionCombo->currentData().toInt());
}

void AddToProjectDialog::populateParts() {
    const QSignalBlocker blocker(m_partCombo);
    m_partCombo->clear();

    // The parts of the body; a file can also go to the body itself
    const auto& pm = kalahari::core::ProjectManager::getInstance();
    const kalahari::core::ProjectBook* book = pm.book();
    const bool isBodySection = currentPlace() == kalahari::core::BookPlace::Main;
    if (book && isBodySection) {
        for (const kalahari::core::ProjectElement& element : book->mainElements) {
            if (pm.formOf(element) == kalahari::core::ElementForm::Group) {
                if (m_partCombo->count() == 0) {
                    m_partCombo->addItem(tr("(No part)"), QString());
                }
                m_partCombo->addItem(element.title, element.id);
            }
        }
    }

    // Shown only when there is a part to choose
    const bool hasParts = m_partCombo->count() > 0;
    m_partCombo->setVisible(hasParts);
    m_partLabel->setVisible(hasParts);
}

void AddToProjectDialog::populateKinds() {
    const QSignalBlocker blocker(m_kindCombo);
    m_kindCombo->clear();

    // Text kinds the project offers there, named in the language of the program
    const auto& pm = kalahari::core::ProjectManager::getInstance();
    const kalahari::core::BookPlace place = currentPlace();
    const QString groupId = m_partCombo->currentData().toString();
    m_kinds = pm.textKindsFor(place, groupId);
    const kalahari::core::KindRef chapterKind = pm.chapterKindFor(place, groupId);
    const QString language =
        QString::fromStdString(kalahari::core::SettingsManager::getInstance().getLanguage());
    for (const kalahari::core::KindRef& kind : std::as_const(m_kinds)) {
        m_kindCombo->addItem(kind.kind->name.text(language));
        if (kind.kind == chapterKind.kind) {
            m_kindCombo->setCurrentIndex(m_kindCombo->count() - 1);
        }
    }
    m_kindCombo->setEnabled(!m_kinds.isEmpty());
}

void AddToProjectDialog::validateInput() {
    bool valid = true;

    // Title is required
    if (m_titleEdit->text().trimmed().isEmpty()) {
        valid = false;
    }

    // A kind must be selected; there is none when no element can be added there
    if (m_kindCombo->currentIndex() < 0 || m_kindCombo->currentIndex() >= m_kinds.size()) {
        valid = false;
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

    // Update part combo visibility and contents, and the kinds of the section
    populateParts();
    populateKinds();
    validateInput();
}

void AddToProjectDialog::onPartChanged() {
    populateKinds();
    validateInput();
}

void AddToProjectDialog::onTitleChanged(const QString& text) {
    Q_UNUSED(text)
    validateInput();
}

void AddToProjectDialog::onAccept() {
    const int kindIndex = m_kindCombo->currentIndex();
    if (kindIndex < 0 || kindIndex >= m_kinds.size()) {
        return;
    }

    // Populate result structure; the part combo has a part only in the body
    m_result.place = currentPlace();
    m_result.groupId = m_partCombo->currentData().toString();
    m_result.kind = m_kinds.at(kindIndex);
    m_result.newTitle = m_titleEdit->text().trimmed();
    m_result.copyFile = m_copyRadio->isChecked();

    accept();
}
