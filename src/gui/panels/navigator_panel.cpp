/// @file navigator_panel.cpp
/// @brief Navigator panel implementation

#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/logger.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QTreeWidgetItem>

namespace kalahari {
namespace gui {

NavigatorPanel::NavigatorPanel(QWidget* parent)
    : QWidget(parent)
    , m_treeWidget(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create tree widget
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabel(tr("Project Structure"));

    // Add placeholder items
    QTreeWidgetItem* bookItem = new QTreeWidgetItem(m_treeWidget);
    bookItem->setText(0, tr("Book (placeholder)"));

    QTreeWidgetItem* chapter1 = new QTreeWidgetItem(bookItem);
    chapter1->setText(0, tr("Chapter 1"));

    QTreeWidgetItem* chapter2 = new QTreeWidgetItem(bookItem);
    chapter2->setText(0, tr("Chapter 2"));

    bookItem->setExpanded(true);

    layout->addWidget(m_treeWidget);
    setLayout(layout);

    logger.debug("NavigatorPanel initialized");
}

} // namespace gui
} // namespace kalahari
