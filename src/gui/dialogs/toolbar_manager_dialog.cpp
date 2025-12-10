/// @file toolbar_manager_dialog.cpp
/// @brief Implementation of ToolbarManagerDialog
///
/// OpenSpec #00031: Toolbar System

#include "kalahari/gui/dialogs/toolbar_manager_dialog.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/core/art_provider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>

using namespace kalahari::gui::dialogs;

// ============================================================================
// Constructor
// ============================================================================

ToolbarManagerDialog::ToolbarManagerDialog(ToolbarManager* manager, QWidget* parent)
    : QDialog(parent)
    , m_toolbarList(nullptr)
    , m_categoryCombo(nullptr)
    , m_searchFilter(nullptr)
    , m_availableCommands(nullptr)
    , m_currentToolbar(nullptr)
    , m_moveUpBtn(nullptr)
    , m_moveDownBtn(nullptr)
    , m_removeBtn(nullptr)
    , m_separatorBtn(nullptr)
    , m_addCommandBtn(nullptr)
    , m_newToolbarBtn(nullptr)
    , m_deleteToolbarBtn(nullptr)
    , m_renameToolbarBtn(nullptr)
    , m_resetBtn(nullptr)
    , m_buttonBox(nullptr)
    , m_modified(false)
    , m_toolbarManager(manager)
{
    setWindowTitle(tr("Customize Toolbars"));
    setMinimumSize(900, 600);
    resize(1000, 650);

    // Initialize built-in toolbar IDs
    m_builtInToolbarIds << "file" << "edit" << "book" << "view" << "tools" << "format";

    setupUI();
    createConnections();
    loadToolbarConfigs();
    populateToolbarList();
    populateAvailableCommands();

    // Select first toolbar if available
    if (m_toolbarList->count() > 0) {
        m_toolbarList->setCurrentRow(0);
    }

    updateButtonStates();
}

// ============================================================================
// UI Setup
// ============================================================================

void ToolbarManagerDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create 3-column splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // Add three panels
    splitter->addWidget(createToolbarListPanel());
    splitter->addWidget(createAvailableCommandsPanel());
    splitter->addWidget(createCurrentToolbarPanel());

    // Set initial sizes (left smaller, center and right equal)
    splitter->setSizes({220, 350, 350});

    mainLayout->addWidget(splitter, 1);

    // Dialog buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_resetBtn = new QPushButton(tr("Reset to Defaults"), this);
    buttonLayout->addWidget(m_resetBtn);

    buttonLayout->addStretch();

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Apply | QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    buttonLayout->addWidget(m_buttonBox);

    mainLayout->addLayout(buttonLayout);
}

QWidget* ToolbarManagerDialog::createToolbarListPanel() {
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    // Group box
    QGroupBox* group = new QGroupBox(tr("Toolbars"), panel);
    QVBoxLayout* groupLayout = new QVBoxLayout(group);

    // Toolbar list
    m_toolbarList = new QListWidget(group);
    m_toolbarList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_toolbarList->setMinimumWidth(180);
    groupLayout->addWidget(m_toolbarList, 1);

    // Toolbar management buttons
    QHBoxLayout* toolbarBtnLayout = new QHBoxLayout();

    m_newToolbarBtn = new QPushButton(tr("New..."), group);
    m_newToolbarBtn->setToolTip(tr("Create a new user toolbar"));
    toolbarBtnLayout->addWidget(m_newToolbarBtn);

    m_renameToolbarBtn = new QPushButton(tr("Rename..."), group);
    m_renameToolbarBtn->setToolTip(tr("Rename the selected toolbar"));
    toolbarBtnLayout->addWidget(m_renameToolbarBtn);

    m_deleteToolbarBtn = new QPushButton(tr("Delete"), group);
    m_deleteToolbarBtn->setToolTip(tr("Delete the selected user toolbar"));
    toolbarBtnLayout->addWidget(m_deleteToolbarBtn);

    groupLayout->addLayout(toolbarBtnLayout);

    layout->addWidget(group);
    return panel;
}

QWidget* ToolbarManagerDialog::createAvailableCommandsPanel() {
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    // Group box
    QGroupBox* group = new QGroupBox(tr("Available Commands"), panel);
    QVBoxLayout* groupLayout = new QVBoxLayout(group);

    // Category filter
    QHBoxLayout* filterLayout = new QHBoxLayout();
    QLabel* categoryLabel = new QLabel(tr("Category:"), group);
    filterLayout->addWidget(categoryLabel);

    m_categoryCombo = new QComboBox(group);
    m_categoryCombo->addItem(tr("All Categories"), QString());
    filterLayout->addWidget(m_categoryCombo, 1);
    groupLayout->addLayout(filterLayout);

    // Search filter
    QHBoxLayout* searchLayout = new QHBoxLayout();
    QLabel* searchLabel = new QLabel(tr("Search:"), group);
    searchLayout->addWidget(searchLabel);

    m_searchFilter = new QLineEdit(group);
    m_searchFilter->setPlaceholderText(tr("Filter commands..."));
    m_searchFilter->setClearButtonEnabled(true);
    searchLayout->addWidget(m_searchFilter, 1);
    groupLayout->addLayout(searchLayout);

    // Commands tree
    m_availableCommands = new QTreeWidget(group);
    m_availableCommands->setHeaderLabels({tr("Command"), tr("Shortcut")});
    m_availableCommands->setRootIsDecorated(true);
    m_availableCommands->setSelectionMode(QAbstractItemView::SingleSelection);
    m_availableCommands->header()->setStretchLastSection(true);
    m_availableCommands->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    groupLayout->addWidget(m_availableCommands, 1);

    // Add button
    m_addCommandBtn = new QPushButton(tr("Add to Toolbar >>"), group);
    m_addCommandBtn->setToolTip(tr("Add selected command to the current toolbar"));
    groupLayout->addWidget(m_addCommandBtn);

    layout->addWidget(group);
    return panel;
}

QWidget* ToolbarManagerDialog::createCurrentToolbarPanel() {
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    // Group box
    QGroupBox* group = new QGroupBox(tr("Current Toolbar"), panel);
    QVBoxLayout* groupLayout = new QVBoxLayout(group);

    // Horizontal layout for list and buttons
    QHBoxLayout* contentLayout = new QHBoxLayout();

    // Commands list
    m_currentToolbar = new QListWidget(group);
    m_currentToolbar->setSelectionMode(QAbstractItemView::SingleSelection);
    m_currentToolbar->setDragDropMode(QAbstractItemView::InternalMove);
    m_currentToolbar->setDefaultDropAction(Qt::MoveAction);
    contentLayout->addWidget(m_currentToolbar, 1);

    // Control buttons (vertical)
    QVBoxLayout* btnLayout = new QVBoxLayout();
    btnLayout->setSpacing(4);

    auto& art = kalahari::core::ArtProvider::getInstance();

    m_moveUpBtn = new QPushButton(group);
    m_moveUpBtn->setIcon(art.getIcon("navigation.up", kalahari::core::IconContext::Button));
    m_moveUpBtn->setToolTip(tr("Move Up"));
    m_moveUpBtn->setFixedSize(32, 32);
    btnLayout->addWidget(m_moveUpBtn);

    m_moveDownBtn = new QPushButton(group);
    m_moveDownBtn->setIcon(art.getIcon("navigation.down", kalahari::core::IconContext::Button));
    m_moveDownBtn->setToolTip(tr("Move Down"));
    m_moveDownBtn->setFixedSize(32, 32);
    btnLayout->addWidget(m_moveDownBtn);

    btnLayout->addSpacing(10);

    m_removeBtn = new QPushButton(group);
    m_removeBtn->setIcon(art.getIcon("action.delete", kalahari::core::IconContext::Button));
    m_removeBtn->setToolTip(tr("Remove from Toolbar"));
    m_removeBtn->setFixedSize(32, 32);
    btnLayout->addWidget(m_removeBtn);

    btnLayout->addSpacing(10);

    m_separatorBtn = new QPushButton(tr("---"), group);
    m_separatorBtn->setToolTip(tr("Add Separator"));
    m_separatorBtn->setFixedSize(32, 32);
    btnLayout->addWidget(m_separatorBtn);

    btnLayout->addStretch();

    contentLayout->addLayout(btnLayout);
    groupLayout->addLayout(contentLayout, 1);

    layout->addWidget(group);
    return panel;
}

void ToolbarManagerDialog::createConnections() {
    // Toolbar list selection
    connect(m_toolbarList, &QListWidget::currentItemChanged,
            this, &ToolbarManagerDialog::onToolbarSelected);

    // Available commands
    connect(m_availableCommands, &QTreeWidget::itemDoubleClicked,
            this, &ToolbarManagerDialog::onCommandDoubleClicked);
    connect(m_addCommandBtn, &QPushButton::clicked,
            this, &ToolbarManagerDialog::onAddCommand);

    // Current toolbar
    connect(m_currentToolbar, &QListWidget::itemSelectionChanged,
            this, &ToolbarManagerDialog::onCurrentToolbarSelectionChanged);

    // Command buttons
    connect(m_moveUpBtn, &QPushButton::clicked, this, &ToolbarManagerDialog::onMoveUp);
    connect(m_moveDownBtn, &QPushButton::clicked, this, &ToolbarManagerDialog::onMoveDown);
    connect(m_removeBtn, &QPushButton::clicked, this, &ToolbarManagerDialog::onRemoveCommand);
    connect(m_separatorBtn, &QPushButton::clicked, this, &ToolbarManagerDialog::onAddSeparator);

    // Filtering
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolbarManagerDialog::onCategoryChanged);
    connect(m_searchFilter, &QLineEdit::textChanged,
            this, &ToolbarManagerDialog::onSearchTextChanged);

    // Toolbar management
    connect(m_newToolbarBtn, &QPushButton::clicked, this, &ToolbarManagerDialog::onNewToolbar);
    connect(m_deleteToolbarBtn, &QPushButton::clicked, this, &ToolbarManagerDialog::onDeleteToolbar);
    connect(m_renameToolbarBtn, &QPushButton::clicked, this, &ToolbarManagerDialog::onRenameToolbar);

    // Dialog buttons
    connect(m_resetBtn, &QPushButton::clicked, this, &ToolbarManagerDialog::onReset);
    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &ToolbarManagerDialog::onApply);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &ToolbarManagerDialog::onAccept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ============================================================================
// Data Loading
// ============================================================================

void ToolbarManagerDialog::loadToolbarConfigs() {
    m_pendingChanges.clear();
    m_originalConfigs.clear();
    m_toolbarNames.clear();

    if (!m_toolbarManager) {
        return;
    }

    // Load all toolbars from ToolbarManager
    QStringList toolbarIds = m_toolbarManager->getToolbarIds();

    for (const QString& id : toolbarIds) {
        QStringList commands = m_toolbarManager->getToolbarCommands(id);
        QString name = m_toolbarManager->getToolbarName(id);

        m_originalConfigs[id] = commands;
        m_pendingChanges[id] = commands;
        m_toolbarNames[id] = name;
    }
}

void ToolbarManagerDialog::saveToolbarConfigs() {
    if (!m_toolbarManager) {
        setModified(false);
        return;
    }

    // Apply all pending changes to ToolbarManager
    for (auto it = m_pendingChanges.constBegin(); it != m_pendingChanges.constEnd(); ++it) {
        const QString& toolbarId = it.key();
        const QStringList& commands = it.value();

        // Check if this toolbar still exists or was deleted
        if (!m_toolbarNames.contains(toolbarId)) {
            continue;
        }

        // Check if configuration changed from original
        if (m_originalConfigs.contains(toolbarId) && m_originalConfigs[toolbarId] == commands) {
            // No change for this toolbar
            continue;
        }

        // Apply changes via ToolbarManager API
        // setToolbarCommands will rebuild and save automatically
        m_toolbarManager->setToolbarCommands(toolbarId, commands);
    }

    // Handle user toolbar renames
    for (auto it = m_toolbarNames.constBegin(); it != m_toolbarNames.constEnd(); ++it) {
        if (m_toolbarManager->isUserToolbar(it.key())) {
            QString currentName = m_toolbarManager->getToolbarName(it.key());
            if (currentName != it.value()) {
                m_toolbarManager->renameUserToolbar(it.key(), it.value());
            }
        }
    }

    // Save configurations to settings
    m_toolbarManager->saveConfigurations();

    setModified(false);
}

void ToolbarManagerDialog::populateToolbarList() {
    m_toolbarList->clear();

    // Built-in toolbars section header
    QListWidgetItem* builtInHeader = new QListWidgetItem(tr("Built-in Toolbars"));
    builtInHeader->setFlags(Qt::NoItemFlags);
    QFont headerFont = builtInHeader->font();
    headerFont.setBold(true);
    builtInHeader->setFont(headerFont);
    builtInHeader->setBackground(palette().alternateBase());
    m_toolbarList->addItem(builtInHeader);

    // Add built-in toolbars
    for (const QString& id : m_builtInToolbarIds) {
        if (m_toolbarNames.contains(id)) {
            QListWidgetItem* item = new QListWidgetItem(m_toolbarNames[id]);
            item->setData(Qt::UserRole, id);
            m_toolbarList->addItem(item);
        }
    }

    // User toolbars section header
    QListWidgetItem* userHeader = new QListWidgetItem(tr("User Toolbars"));
    userHeader->setFlags(Qt::NoItemFlags);
    userHeader->setFont(headerFont);
    userHeader->setBackground(palette().alternateBase());
    m_toolbarList->addItem(userHeader);

    // Add user toolbars (those starting with user_ prefix)
    for (auto it = m_toolbarNames.constBegin(); it != m_toolbarNames.constEnd(); ++it) {
        if (isUserToolbar(it.key())) {
            QListWidgetItem* item = new QListWidgetItem(QString::fromUtf8("\u2605 ") + it.value());
            item->setData(Qt::UserRole, it.key());
            m_toolbarList->addItem(item);
        }
    }

    // Plugin toolbars section header
    QListWidgetItem* pluginHeader = new QListWidgetItem(tr("Plugin Toolbars"));
    pluginHeader->setFlags(Qt::NoItemFlags);
    pluginHeader->setFont(headerFont);
    pluginHeader->setBackground(palette().alternateBase());
    m_toolbarList->addItem(pluginHeader);

    // Add plugin toolbars (those starting with plugin_ prefix)
    for (auto it = m_toolbarNames.constBegin(); it != m_toolbarNames.constEnd(); ++it) {
        if (isPluginToolbar(it.key())) {
            QListWidgetItem* item = new QListWidgetItem(QString::fromUtf8("\U0001F50C ") + it.value());
            item->setData(Qt::UserRole, it.key());
            m_toolbarList->addItem(item);
        }
    }
}

void ToolbarManagerDialog::populateAvailableCommands() {
    m_availableCommands->clear();

    // Populate category combo if not done yet
    if (m_categoryCombo->count() <= 1) {
        auto categories = CommandRegistry::getInstance().getCategories();
        for (const auto& category : categories) {
            m_categoryCombo->addItem(QString::fromStdString(category),
                                      QString::fromStdString(category));
        }
    }

    // Get all commands
    auto commands = CommandRegistry::getInstance().getAllCommands();

    // Group by category
    QMap<QString, QTreeWidgetItem*> categoryItems;

    for (const auto& cmd : commands) {
        QString category = QString::fromStdString(cmd.category);
        QString cmdId = QString::fromStdString(cmd.id);
        QString label = QString::fromStdString(cmd.label);
        QString shortcut = cmd.shortcut.toString();

        // Create category item if needed
        if (!categoryItems.contains(category)) {
            QTreeWidgetItem* catItem = new QTreeWidgetItem(m_availableCommands);
            catItem->setText(0, category);
            catItem->setFlags(catItem->flags() & ~Qt::ItemIsSelectable);
            QFont catFont = catItem->font(0);
            catFont.setBold(true);
            catItem->setFont(0, catFont);
            catItem->setExpanded(true);
            categoryItems[category] = catItem;
        }

        // Add command item
        QTreeWidgetItem* cmdItem = new QTreeWidgetItem(categoryItems[category]);
        cmdItem->setText(0, label);
        cmdItem->setText(1, shortcut);
        cmdItem->setData(0, Qt::UserRole, cmdId);

        // Set icon if available
        auto& art = kalahari::core::ArtProvider::getInstance();
        QIcon icon = art.getIcon(cmdId, kalahari::core::IconContext::Menu);
        if (!icon.isNull()) {
            cmdItem->setIcon(0, icon);
        }
    }

    m_availableCommands->expandAll();
    filterAvailableCommands();
}

void ToolbarManagerDialog::populateCurrentToolbar(const QString& toolbarId) {
    m_currentToolbar->clear();

    if (toolbarId.isEmpty() || !m_pendingChanges.contains(toolbarId)) {
        return;
    }

    const QStringList& commands = m_pendingChanges[toolbarId];
    auto& art = kalahari::core::ArtProvider::getInstance();

    for (const QString& cmdId : commands) {
        QListWidgetItem* item = new QListWidgetItem();

        if (cmdId == ToolbarConstants::SEPARATOR_MARKER) {
            item->setText(tr("--- Separator ---"));
            item->setData(Qt::UserRole, ToolbarConstants::SEPARATOR_MARKER);
            item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
        } else {
            // Get command info
            const Command* cmd = CommandRegistry::getInstance().getCommand(cmdId.toStdString());
            if (cmd) {
                item->setText(QString::fromStdString(cmd->label));
                item->setData(Qt::UserRole, cmdId);

                QIcon icon = art.getIcon(cmdId, kalahari::core::IconContext::Menu);
                if (!icon.isNull()) {
                    item->setIcon(icon);
                }
            } else {
                // Command not found - show ID
                item->setText(cmdId + tr(" (not found)"));
                item->setData(Qt::UserRole, cmdId);
                // Use palette highlight color for theme consistency
                item->setForeground(palette().color(QPalette::Highlight));
            }
        }

        m_currentToolbar->addItem(item);
    }
}

void ToolbarManagerDialog::filterAvailableCommands() {
    QString categoryFilter = m_categoryCombo->currentData().toString();
    QString searchText = m_searchFilter->text().toLower();

    // Iterate through all category items
    for (int i = 0; i < m_availableCommands->topLevelItemCount(); ++i) {
        QTreeWidgetItem* catItem = m_availableCommands->topLevelItem(i);
        QString category = catItem->text(0);

        // Check category filter
        bool categoryMatch = categoryFilter.isEmpty() || category == categoryFilter;

        int visibleChildren = 0;

        // Filter children
        for (int j = 0; j < catItem->childCount(); ++j) {
            QTreeWidgetItem* cmdItem = catItem->child(j);
            QString label = cmdItem->text(0).toLower();
            QString cmdId = cmdItem->data(0, Qt::UserRole).toString().toLower();

            bool searchMatch = searchText.isEmpty() ||
                               label.contains(searchText) ||
                               cmdId.contains(searchText);

            bool visible = categoryMatch && searchMatch;
            cmdItem->setHidden(!visible);

            if (visible) {
                visibleChildren++;
            }
        }

        // Hide category if no visible children
        catItem->setHidden(visibleChildren == 0);
    }
}

// ============================================================================
// Slots - Toolbar Selection
// ============================================================================

void ToolbarManagerDialog::onToolbarSelected(QListWidgetItem* current, QListWidgetItem* previous) {
    Q_UNUSED(previous)

    if (!current) {
        m_selectedToolbarId.clear();
        m_currentToolbar->clear();
        updateButtonStates();
        return;
    }

    QString toolbarId = current->data(Qt::UserRole).toString();
    if (toolbarId.isEmpty()) {
        // Header item selected
        m_selectedToolbarId.clear();
        m_currentToolbar->clear();
        updateButtonStates();
        return;
    }

    m_selectedToolbarId = toolbarId;
    populateCurrentToolbar(toolbarId);
    updateButtonStates();
}

// ============================================================================
// Slots - Command Operations
// ============================================================================

void ToolbarManagerDialog::onCommandDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)

    if (!item || !item->parent()) {
        // Category item or no item - ignore
        return;
    }

    onAddCommand();
}

void ToolbarManagerDialog::onAddCommand() {
    if (m_selectedToolbarId.isEmpty()) {
        return;
    }

    QTreeWidgetItem* selectedItem = m_availableCommands->currentItem();
    if (!selectedItem || !selectedItem->parent()) {
        // No command selected or category selected
        return;
    }

    QString cmdId = selectedItem->data(0, Qt::UserRole).toString();
    if (cmdId.isEmpty()) {
        return;
    }

    // Add to current toolbar
    m_pendingChanges[m_selectedToolbarId].append(cmdId);
    populateCurrentToolbar(m_selectedToolbarId);
    setModified(true);

    // Select the newly added item
    m_currentToolbar->setCurrentRow(m_currentToolbar->count() - 1);
}

void ToolbarManagerDialog::onRemoveCommand() {
    if (m_selectedToolbarId.isEmpty()) {
        return;
    }

    int currentRow = m_currentToolbar->currentRow();
    if (currentRow < 0) {
        return;
    }

    m_pendingChanges[m_selectedToolbarId].removeAt(currentRow);
    populateCurrentToolbar(m_selectedToolbarId);
    setModified(true);

    // Restore selection
    if (currentRow >= m_currentToolbar->count()) {
        currentRow = m_currentToolbar->count() - 1;
    }
    if (currentRow >= 0) {
        m_currentToolbar->setCurrentRow(currentRow);
    }
}

void ToolbarManagerDialog::onMoveUp() {
    if (m_selectedToolbarId.isEmpty()) {
        return;
    }

    int currentRow = m_currentToolbar->currentRow();
    if (currentRow <= 0) {
        return;
    }

    QStringList& commands = m_pendingChanges[m_selectedToolbarId];
    commands.swapItemsAt(currentRow, currentRow - 1);
    populateCurrentToolbar(m_selectedToolbarId);
    setModified(true);

    m_currentToolbar->setCurrentRow(currentRow - 1);
}

void ToolbarManagerDialog::onMoveDown() {
    if (m_selectedToolbarId.isEmpty()) {
        return;
    }

    int currentRow = m_currentToolbar->currentRow();
    if (currentRow < 0 || currentRow >= m_currentToolbar->count() - 1) {
        return;
    }

    QStringList& commands = m_pendingChanges[m_selectedToolbarId];
    commands.swapItemsAt(currentRow, currentRow + 1);
    populateCurrentToolbar(m_selectedToolbarId);
    setModified(true);

    m_currentToolbar->setCurrentRow(currentRow + 1);
}

void ToolbarManagerDialog::onAddSeparator() {
    if (m_selectedToolbarId.isEmpty()) {
        return;
    }

    int insertPos = m_currentToolbar->currentRow();
    if (insertPos < 0) {
        insertPos = m_pendingChanges[m_selectedToolbarId].count();
    } else {
        insertPos++; // Insert after current selection
    }

    m_pendingChanges[m_selectedToolbarId].insert(insertPos, ToolbarConstants::SEPARATOR_MARKER);
    populateCurrentToolbar(m_selectedToolbarId);
    setModified(true);

    m_currentToolbar->setCurrentRow(insertPos);
}

// ============================================================================
// Slots - Filtering
// ============================================================================

void ToolbarManagerDialog::onCategoryChanged(int index) {
    Q_UNUSED(index)
    filterAvailableCommands();
}

void ToolbarManagerDialog::onSearchTextChanged(const QString& text) {
    Q_UNUSED(text)
    filterAvailableCommands();
}

// ============================================================================
// Slots - Toolbar Management
// ============================================================================

void ToolbarManagerDialog::onNewToolbar() {
    if (!m_toolbarManager) {
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this,
        tr("New Toolbar"),
        tr("Enter toolbar name:"),
        QLineEdit::Normal,
        QString(),
        &ok);

    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    // Create toolbar via ToolbarManager (it will generate unique ID)
    QString toolbarId = m_toolbarManager->createUserToolbar(name.trimmed());

    // Update local state
    m_pendingChanges[toolbarId] = QStringList();
    m_toolbarNames[toolbarId] = name.trimmed();
    m_originalConfigs[toolbarId] = QStringList();

    populateToolbarList();
    setModified(true);

    // Select the new toolbar
    for (int i = 0; i < m_toolbarList->count(); ++i) {
        QListWidgetItem* item = m_toolbarList->item(i);
        if (item->data(Qt::UserRole).toString() == toolbarId) {
            m_toolbarList->setCurrentItem(item);
            break;
        }
    }
}

void ToolbarManagerDialog::onDeleteToolbar() {
    if (m_selectedToolbarId.isEmpty() || !isUserToolbar(m_selectedToolbarId)) {
        return;
    }

    if (!m_toolbarManager) {
        return;
    }

    QMessageBox::StandardButton result = QMessageBox::question(this,
        tr("Delete Toolbar"),
        tr("Are you sure you want to delete the toolbar '%1'?")
            .arg(m_toolbarNames[m_selectedToolbarId]),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    // Delete via ToolbarManager
    m_toolbarManager->deleteUserToolbar(m_selectedToolbarId);

    // Update local state
    m_pendingChanges.remove(m_selectedToolbarId);
    m_toolbarNames.remove(m_selectedToolbarId);
    m_originalConfigs.remove(m_selectedToolbarId);

    m_selectedToolbarId.clear();
    populateToolbarList();
    setModified(true);

    if (m_toolbarList->count() > 1) {
        m_toolbarList->setCurrentRow(1); // Select first actual toolbar
    }
}

void ToolbarManagerDialog::onRenameToolbar() {
    if (m_selectedToolbarId.isEmpty() || !isUserToolbar(m_selectedToolbarId)) {
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this,
        tr("Rename Toolbar"),
        tr("Enter new name:"),
        QLineEdit::Normal,
        m_toolbarNames[m_selectedToolbarId],
        &ok);

    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    m_toolbarNames[m_selectedToolbarId] = name.trimmed();
    populateToolbarList();
    setModified(true);

    // Re-select the renamed toolbar
    for (int i = 0; i < m_toolbarList->count(); ++i) {
        QListWidgetItem* item = m_toolbarList->item(i);
        if (item->data(Qt::UserRole).toString() == m_selectedToolbarId) {
            m_toolbarList->setCurrentItem(item);
            break;
        }
    }
}

// ============================================================================
// Slots - Dialog Actions
// ============================================================================

void ToolbarManagerDialog::onApply() {
    saveToolbarConfigs();
}

void ToolbarManagerDialog::onReset() {
    if (!m_toolbarManager) {
        return;
    }

    QMessageBox::StandardButton result = QMessageBox::question(this,
        tr("Reset Toolbars"),
        tr("Are you sure you want to reset all toolbars to their default configurations?\n\n"
           "This will remove all user-defined toolbars and restore built-in toolbars to defaults."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    // Reset via ToolbarManager
    m_toolbarManager->resetToDefaults();

    // Reload from ToolbarManager
    loadToolbarConfigs();
    populateToolbarList();

    if (m_toolbarList->count() > 1) {
        m_toolbarList->setCurrentRow(1);
    }

    setModified(false);
}

void ToolbarManagerDialog::onAccept() {
    if (m_modified) {
        saveToolbarConfigs();
    }
    accept();
}

void ToolbarManagerDialog::onCurrentToolbarSelectionChanged() {
    updateButtonStates();
}

// ============================================================================
// State Updates
// ============================================================================

void ToolbarManagerDialog::updateButtonStates() {
    bool hasToolbar = !m_selectedToolbarId.isEmpty();
    bool isUser = hasToolbar && isUserToolbar(m_selectedToolbarId);
    bool hasSelection = m_currentToolbar->currentRow() >= 0;
    int currentRow = m_currentToolbar->currentRow();
    int itemCount = m_currentToolbar->count();

    // Toolbar management buttons
    m_deleteToolbarBtn->setEnabled(isUser);
    m_renameToolbarBtn->setEnabled(isUser);

    // Command buttons
    m_addCommandBtn->setEnabled(hasToolbar && m_availableCommands->currentItem() &&
                                 m_availableCommands->currentItem()->parent());
    m_removeBtn->setEnabled(hasToolbar && hasSelection);
    m_moveUpBtn->setEnabled(hasToolbar && hasSelection && currentRow > 0);
    m_moveDownBtn->setEnabled(hasToolbar && hasSelection && currentRow < itemCount - 1);
    m_separatorBtn->setEnabled(hasToolbar);

    // Apply button
    m_buttonBox->button(QDialogButtonBox::Apply)->setEnabled(m_modified);
}

// ============================================================================
// Helper Methods
// ============================================================================

QString ToolbarManagerDialog::sanitizeToolbarName(const QString& name) const {
    QString sanitized = name.toLower().trimmed();
    sanitized.replace(QRegularExpression("[^a-z0-9_]"), "_");
    sanitized.replace(QRegularExpression("_+"), "_");
    return sanitized;
}

bool ToolbarManagerDialog::isBuiltInToolbar(const QString& toolbarId) const {
    return m_builtInToolbarIds.contains(toolbarId);
}

bool ToolbarManagerDialog::isPluginToolbar(const QString& toolbarId) const {
    return toolbarId.startsWith(ToolbarConstants::PLUGIN_TOOLBAR_PREFIX);
}

bool ToolbarManagerDialog::isUserToolbar(const QString& toolbarId) const {
    return toolbarId.startsWith(ToolbarConstants::USER_TOOLBAR_PREFIX);
}

void ToolbarManagerDialog::setModified(bool modified) {
    m_modified = modified;

    // Update window title to show modified state
    QString title = tr("Customize Toolbars");
    if (m_modified) {
        title += " *";
    }
    setWindowTitle(title);

    updateButtonStates();
}
