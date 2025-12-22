/// @file tags_panel.cpp
/// @brief Implementation of Tags panel (OpenSpec #00042 Task 7.10)

#include "kalahari/gui/panels/tags_panel.h"
#include "kalahari/editor/tag_detector.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kml_document.h"
#include "kalahari/core/logger.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

namespace kalahari {
namespace gui {

/// @brief Custom role for storing paragraph index in tree items
static constexpr int ParagraphIndexRole = Qt::UserRole + 1;

/// @brief Custom role for storing position in tree items
static constexpr int PositionRole = Qt::UserRole + 2;

/// @brief Custom role for storing tag type in tree items
static constexpr int TagTypeRole = Qt::UserRole + 3;

/// @brief Custom role for marking group items
static constexpr int IsGroupRole = Qt::UserRole + 4;

TagsPanel::TagsPanel(QWidget* parent)
    : QWidget(parent)
    , m_tagsTree(nullptr)
    , m_filterCombo(nullptr)
    , m_emptyLabel(nullptr)
    , m_countLabel(nullptr)
    , m_editor(nullptr)
    , m_detector(nullptr)
    , m_ownsDetector(false)
    , m_currentFilter(-1)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("TagsPanel constructor called");

    setupUI();

    logger.debug("TagsPanel initialized");
}

void TagsPanel::setupUI()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("TagsPanel::setupUI()");

    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // Create filter row
    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(6);

    QLabel* filterLabel = new QLabel(tr("Filter:"), this);
    filterLayout->addWidget(filterLabel);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(tr("All Tags"), -1);
    m_filterCombo->addItem(tr("TODO"), static_cast<int>(editor::TagType::Todo));
    m_filterCombo->addItem(tr("FIX"), static_cast<int>(editor::TagType::Fix));
    m_filterCombo->addItem(tr("CHECK"), static_cast<int>(editor::TagType::Check));
    m_filterCombo->addItem(tr("NOTE"), static_cast<int>(editor::TagType::Note));
    m_filterCombo->addItem(tr("WARNING"), static_cast<int>(editor::TagType::Warning));

    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TagsPanel::onFilterChanged);

    filterLayout->addWidget(m_filterCombo, 1);

    m_countLabel = new QLabel(tr("0 tags"), this);
    filterLayout->addWidget(m_countLabel);

    mainLayout->addLayout(filterLayout);

    // Create empty state label
    m_emptyLabel = new QLabel(tr("No tags found in document"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    mainLayout->addWidget(m_emptyLabel);

    // Create tags tree
    m_tagsTree = new QTreeWidget(this);
    m_tagsTree->setHeaderHidden(true);
    m_tagsTree->setRootIsDecorated(true);
    m_tagsTree->setAlternatingRowColors(true);
    m_tagsTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tagsTree->setVisible(false);

    connect(m_tagsTree, &QTreeWidget::itemClicked,
            this, &TagsPanel::onItemClicked);
    connect(m_tagsTree, &QTreeWidget::itemDoubleClicked,
            this, &TagsPanel::onItemDoubleClicked);

    mainLayout->addWidget(m_tagsTree, 1);

    setLayout(mainLayout);
}

void TagsPanel::setEditor(editor::BookEditor* editor)
{
    if (m_editor == editor) {
        return;
    }

    m_editor = editor;

    // If we own a detector, clean it up
    if (m_ownsDetector && m_detector) {
        disconnectFromDetector();
        delete m_detector;
        m_detector = nullptr;
        m_ownsDetector = false;
    }

    if (m_editor) {
        // Create a new detector for this editor
        m_detector = new editor::TagDetector(this);
        m_ownsDetector = true;

        // Set the document
        if (m_editor->document()) {
            m_detector->setDocument(m_editor->document());
        }

        // Connect to detector
        connectToDetector();

        // Connect to editor document changes
        connect(m_editor, &editor::BookEditor::documentChanged,
                this, [this]() {
                    if (m_detector && m_editor) {
                        m_detector->setDocument(m_editor->document());
                    }
                });
    }

    refresh();
}

void TagsPanel::setTagDetector(editor::TagDetector* detector)
{
    if (m_detector == detector) {
        return;
    }

    // Disconnect from old detector
    disconnectFromDetector();

    // If we owned the old detector, delete it
    if (m_ownsDetector && m_detector) {
        delete m_detector;
    }

    m_detector = detector;
    m_ownsDetector = false;

    // Connect to new detector
    connectToDetector();

    // Refresh the list
    refresh();
}

void TagsPanel::connectToDetector()
{
    if (m_detector == nullptr) {
        return;
    }

    connect(m_detector, &editor::TagDetector::tagsChanged,
            this, &TagsPanel::onTagsChanged);
}

void TagsPanel::disconnectFromDetector()
{
    if (m_detector == nullptr) {
        return;
    }

    disconnect(m_detector, &editor::TagDetector::tagsChanged,
               this, &TagsPanel::onTagsChanged);
}

void TagsPanel::refresh()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("TagsPanel::refresh()");

    m_tagsTree->clear();
    m_typeGroupItems.clear();

    if (m_detector == nullptr) {
        clear();
        return;
    }

    // Get all tags
    QList<editor::DetectedTag> tags = m_detector->allTags();

    if (tags.isEmpty()) {
        clear();
        return;
    }

    // Add tags grouped by type
    for (const editor::DetectedTag& tag : tags) {
        if (!passesFilter(tag.type)) {
            continue;
        }

        QTreeWidgetItem* groupItem = getTypeGroupItem(tag.type);
        addTagToTree(tag, groupItem);
    }

    // Update group counts
    updateGroupCounts();

    // Count visible tags
    int visibleCount = 0;
    for (auto it = m_typeGroupItems.begin(); it != m_typeGroupItems.end(); ++it) {
        visibleCount += it.value()->childCount();
    }

    // Update visibility
    bool hasTags = (visibleCount > 0);
    m_emptyLabel->setVisible(!hasTags);
    m_tagsTree->setVisible(hasTags);

    // Update count label
    m_countLabel->setText(tr("%1 tag(s)").arg(visibleCount));

    // Expand all groups
    m_tagsTree->expandAll();

    logger.debug("TagsPanel: Displayed {} tags", visibleCount);
}

void TagsPanel::clear()
{
    m_tagsTree->clear();
    m_typeGroupItems.clear();
    m_emptyLabel->setVisible(true);
    m_tagsTree->setVisible(false);
    m_countLabel->setText(tr("0 tags"));
}

QTreeWidgetItem* TagsPanel::getTypeGroupItem(editor::TagType type)
{
    int typeInt = static_cast<int>(type);

    if (m_typeGroupItems.contains(typeInt)) {
        return m_typeGroupItems[typeInt];
    }

    // Create new group item
    QTreeWidgetItem* groupItem = new QTreeWidgetItem(m_tagsTree);
    groupItem->setText(0, editor::TagDetector::nameForType(type));
    groupItem->setData(0, IsGroupRole, true);
    groupItem->setData(0, TagTypeRole, typeInt);

    // Set icon/color indicator
    QColor typeColor = editor::TagDetector::colorForType(type);
    QPixmap pixmap(16, 16);
    pixmap.fill(typeColor);
    groupItem->setIcon(0, QIcon(pixmap));

    // Make group items bold
    QFont font = groupItem->font(0);
    font.setBold(true);
    groupItem->setFont(0, font);

    m_typeGroupItems[typeInt] = groupItem;
    return groupItem;
}

void TagsPanel::addTagToTree(const editor::DetectedTag& tag, QTreeWidgetItem* parentItem)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(parentItem);

    // Format: "Line X: content"
    QString displayText = formatTagDisplay(tag);
    item->setText(0, displayText);

    // Store navigation data
    item->setData(0, ParagraphIndexRole, tag.paragraphIndex);
    item->setData(0, PositionRole, tag.startPos);
    item->setData(0, TagTypeRole, static_cast<int>(tag.type));
    item->setData(0, IsGroupRole, false);

    // Set tooltip with full content
    QString tooltip = tr("Line %1\n%2: %3")
        .arg(tag.lineNumber)
        .arg(tag.keyword)
        .arg(tag.content.isEmpty() ? tr("(no description)") : tag.content);
    item->setToolTip(0, tooltip);
}

void TagsPanel::updateGroupCounts()
{
    for (auto it = m_typeGroupItems.begin(); it != m_typeGroupItems.end(); ++it) {
        QTreeWidgetItem* groupItem = it.value();
        int count = groupItem->childCount();
        editor::TagType type = static_cast<editor::TagType>(it.key());
        QString typeName = editor::TagDetector::nameForType(type);
        groupItem->setText(0, tr("%1 (%2)").arg(typeName).arg(count));
    }
}

QString TagsPanel::formatTagDisplay(const editor::DetectedTag& tag) const
{
    QString content = tag.content;

    // Truncate long content
    if (content.length() > 50) {
        content = content.left(47) + QStringLiteral("...");
    }

    if (content.isEmpty()) {
        return tr("Line %1").arg(tag.lineNumber);
    }

    return tr("Line %1: %2").arg(tag.lineNumber).arg(content);
}

bool TagsPanel::passesFilter(editor::TagType type) const
{
    if (m_currentFilter < 0) {
        return true;  // All tags
    }

    return static_cast<int>(type) == m_currentFilter;
}

void TagsPanel::onItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    if (item == nullptr) {
        return;
    }

    // Ignore clicks on group items
    if (item->data(0, IsGroupRole).toBool()) {
        return;
    }

    int paragraphIndex = item->data(0, ParagraphIndexRole).toInt();
    int position = item->data(0, PositionRole).toInt();

    emit tagClicked(paragraphIndex, position);
}

void TagsPanel::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    if (item == nullptr) {
        return;
    }

    // Ignore double-clicks on group items
    if (item->data(0, IsGroupRole).toBool()) {
        return;
    }

    int paragraphIndex = item->data(0, ParagraphIndexRole).toInt();
    int position = item->data(0, PositionRole).toInt();

    emit tagDoubleClicked(paragraphIndex, position);
}

void TagsPanel::onFilterChanged(int index)
{
    if (index < 0 || index >= m_filterCombo->count()) {
        return;
    }

    m_currentFilter = m_filterCombo->itemData(index).toInt();
    refresh();
}

void TagsPanel::onTagsChanged()
{
    refresh();
}

} // namespace gui
} // namespace kalahari
