/// @file recent_books_manager.cpp
/// @brief Implementation of RecentBooksManager
///
/// OpenSpec #00030: Manages recent books list with QSettings persistence

#include "kalahari/core/recent_books_manager.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/art_provider.h"
#include <QSettings>
#include <QFileInfo>
#include <QAction>

namespace kalahari {
namespace core {

RecentBooksManager& RecentBooksManager::getInstance() {
    static RecentBooksManager instance;
    return instance;
}

RecentBooksManager::RecentBooksManager()
    : QObject(nullptr)
    , m_recentMenu(nullptr)
    , m_clearAction(nullptr)
{
    loadRecentFiles();
}

void RecentBooksManager::addRecentFile(const QString& filePath) {
    auto& logger = Logger::getInstance();

    if (filePath.isEmpty()) {
        return;
    }

    // Normalize path
    QString normalizedPath = QFileInfo(filePath).absoluteFilePath();

    // Remove if already in list (will be re-added at top)
    m_recentFiles.removeAll(normalizedPath);

    // Add to beginning of list
    m_recentFiles.prepend(normalizedPath);

    // Trim to max size
    while (m_recentFiles.size() > MAX_RECENT_FILES) {
        m_recentFiles.removeLast();
    }

    saveRecentFiles();
    updateMenu();

    logger.debug("RecentBooksManager: Added '{}' to recent files ({} total)",
        normalizedPath.toStdString(), m_recentFiles.size());

    emit recentFilesChanged();
}

void RecentBooksManager::removeRecentFile(const QString& filePath) {
    QString normalizedPath = QFileInfo(filePath).absoluteFilePath();

    if (m_recentFiles.removeAll(normalizedPath) > 0) {
        saveRecentFiles();
        updateMenu();
        emit recentFilesChanged();
    }
}

void RecentBooksManager::clearRecentFiles() {
    auto& logger = Logger::getInstance();

    if (!m_recentFiles.isEmpty()) {
        m_recentFiles.clear();
        saveRecentFiles();
        updateMenu();
        logger.info("RecentBooksManager: Cleared all recent files");
        emit recentFilesChanged();
    }
}

QStringList RecentBooksManager::getRecentFiles() const {
    return m_recentFiles;
}

bool RecentBooksManager::isEmpty() const {
    return m_recentFiles.isEmpty();
}

void RecentBooksManager::createRecentBooksMenu(QMenu* parentMenu) {
    auto& logger = Logger::getInstance();

    if (!parentMenu) {
        logger.error("RecentBooksManager: Parent menu is null");
        return;
    }

    // Create submenu if not exists
    if (!m_recentMenu) {
        m_recentMenu = new QMenu(tr("Recent Books"), parentMenu);

        // Set icon for submenu
        auto& artProvider = ArtProvider::getInstance();
        m_recentMenu->setIcon(artProvider.getIcon("file.open"));
    }

    // Find position after "Open Book..." (should be near the top)
    QAction* insertBefore = nullptr;
    QList<QAction*> actions = parentMenu->actions();

    for (int i = 0; i < actions.size(); ++i) {
        // Look for Close Book action (should come after Open and Recent)
        if (actions[i]->text().contains("Close", Qt::CaseInsensitive)) {
            insertBefore = actions[i];
            break;
        }
    }

    // Insert submenu before Close Book (or at end)
    if (insertBefore) {
        parentMenu->insertMenu(insertBefore, m_recentMenu);
    } else {
        parentMenu->addMenu(m_recentMenu);
    }

    updateMenu();

    logger.debug("RecentBooksManager: Created Recent Books submenu ({} items)",
        m_recentFiles.size());
}

void RecentBooksManager::loadRecentFiles() {
    QSettings settings;
    m_recentFiles = settings.value("recentFiles").toStringList();

    // Validate paths - remove non-existent files
    QStringList validFiles;
    for (const QString& path : m_recentFiles) {
        if (QFileInfo::exists(path)) {
            validFiles.append(path);
        }
    }

    if (validFiles.size() != m_recentFiles.size()) {
        m_recentFiles = validFiles;
        saveRecentFiles();
    }
}

void RecentBooksManager::saveRecentFiles() {
    QSettings settings;
    settings.setValue("recentFiles", m_recentFiles);
}

void RecentBooksManager::updateMenu() {
    if (!m_recentMenu) {
        return;
    }

    // Clear existing items
    m_recentMenu->clear();

    if (m_recentFiles.isEmpty()) {
        // Add disabled "No Recent Files" item
        QAction* emptyAction = m_recentMenu->addAction(tr("No Recent Files"));
        emptyAction->setEnabled(false);
    } else {
        // Add file actions with numbers (1-9, then 0 for 10th)
        for (int i = 0; i < m_recentFiles.size(); ++i) {
            const QString& filePath = m_recentFiles[i];
            QString displayName = getDisplayName(filePath);

            // Create numbered action text (1-9, 0 for 10th)
            QString actionText;
            if (i < 9) {
                actionText = QString("&%1. %2").arg(i + 1).arg(displayName);
            } else {
                actionText = QString("1&0. %1").arg(displayName);
            }

            QAction* action = m_recentMenu->addAction(actionText);
            action->setData(filePath);
            action->setToolTip(filePath);

            // Connect to emit signal with file path
            connect(action, &QAction::triggered, this, [this, filePath]() {
                emit recentFileClicked(filePath);
            });
        }

        // Add separator and Clear action
        m_recentMenu->addSeparator();
        m_clearAction = m_recentMenu->addAction(tr("Clear Recent Files"));
        connect(m_clearAction, &QAction::triggered, this, &RecentBooksManager::clearRecentFiles);
    }
}

QString RecentBooksManager::getDisplayName(const QString& filePath) const {
    QFileInfo fileInfo(filePath);
    return fileInfo.completeBaseName();  // Filename without extension
}

} // namespace core
} // namespace kalahari
