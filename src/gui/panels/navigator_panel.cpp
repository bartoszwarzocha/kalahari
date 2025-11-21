/// @file navigator_panel.cpp
/// @brief Navigator panel implementation

#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/document.h"
#include "kalahari/core/book.h"
#include "kalahari/core/part.h"
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

    // No document loaded yet - tree will be populated via loadDocument()
    m_treeWidget->setHeaderLabel(tr("Project Structure (no document loaded)"));

    // Connect double-click signal (Task #00015)
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int column) {
                Q_UNUSED(column);
                auto& logger = core::Logger::getInstance();
                QString itemTitle = item->text(0);
                logger.info("NavigatorPanel: Item double-clicked: {}",
                           itemTitle.toStdString());

                // Emit signal to MainWindow to open chapter in editor
                // Phase 0: Opens whole document (no per-chapter editing yet)
                // Phase 1+: Will open specific chapter content
                emit chapterDoubleClicked(itemTitle);
            });

    layout->addWidget(m_treeWidget);
    setLayout(layout);

    logger.debug("NavigatorPanel initialized");
}

void NavigatorPanel::clearDocument() {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::clearDocument()");

    m_treeWidget->clear();
    m_treeWidget->setHeaderLabel(tr("Project Structure (no document loaded)"));
}

void NavigatorPanel::loadDocument(const core::Document& document) {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::loadDocument() - Document: {}", document.getTitle());

    // Clear existing items
    m_treeWidget->clear();
    m_treeWidget->setHeaderLabel(tr("Project Structure"));

    // Create root item: Document title
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_treeWidget);
    rootItem->setText(0, QString::fromStdString(document.getTitle()));
    rootItem->setExpanded(true);

    // Get book structure
    const auto& book = document.getBook();

    // Add Front Matter section
    const auto& frontMatter = book.getFrontMatter();
    if (!frontMatter.empty()) {
        QTreeWidgetItem* frontMatterItem = new QTreeWidgetItem(rootItem);
        frontMatterItem->setText(0, tr("Front Matter"));

        for (const auto& element : frontMatter) {
            QTreeWidgetItem* item = new QTreeWidgetItem(frontMatterItem);
            item->setText(0, QString::fromStdString(element->getTitle()));
        }

        frontMatterItem->setExpanded(false);  // Collapsed by default
    }

    // Add Body section (Parts → Chapters)
    const auto& body = book.getBody();
    if (!body.empty()) {
        QTreeWidgetItem* bodyItem = new QTreeWidgetItem(rootItem);
        bodyItem->setText(0, tr("Body"));

        for (const auto& part : body) {
            QTreeWidgetItem* partItem = new QTreeWidgetItem(bodyItem);
            partItem->setText(0, QString::fromStdString(part->getTitle()));

            const auto& chapters = part->getChapters();
            for (const auto& chapter : chapters) {
                QTreeWidgetItem* chapterItem = new QTreeWidgetItem(partItem);
                chapterItem->setText(0, QString::fromStdString(chapter->getTitle()));
            }

            partItem->setExpanded(true);  // Expand parts by default
        }

        bodyItem->setExpanded(true);  // Expand body by default
    }

    // Add Back Matter section
    const auto& backMatter = book.getBackMatter();
    if (!backMatter.empty()) {
        QTreeWidgetItem* backMatterItem = new QTreeWidgetItem(rootItem);
        backMatterItem->setText(0, tr("Back Matter"));

        for (const auto& element : backMatter) {
            QTreeWidgetItem* item = new QTreeWidgetItem(backMatterItem);
            item->setText(0, QString::fromStdString(element->getTitle()));
        }

        backMatterItem->setExpanded(false);  // Collapsed by default
    }

    logger.debug("NavigatorPanel::loadDocument() complete");
}

} // namespace gui
} // namespace kalahari
