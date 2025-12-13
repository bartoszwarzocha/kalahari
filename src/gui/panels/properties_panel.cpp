/// @file properties_panel.cpp
/// @brief Contextual properties panel implementation
///
/// OpenSpec #00033 Phase G: Full implementation with QStackedWidget.

#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/document.h"
#include "kalahari/core/book.h"
#include "kalahari/core/book_element.h"
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace kalahari {
namespace gui {

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QWidget(parent)
    , m_stackedWidget(nullptr)
    , m_noProjectLabel(nullptr)
    , m_projectTitleEdit(nullptr)
    , m_projectAuthorEdit(nullptr)
    , m_projectLanguageCombo(nullptr)
    , m_projectGenreEdit(nullptr)
    , m_projectChaptersLabel(nullptr)
    , m_projectWordsLabel(nullptr)
    , m_projectCreatedLabel(nullptr)
    , m_projectModifiedLabel(nullptr)
    , m_chapterTitleEdit(nullptr)
    , m_chapterWordCountLabel(nullptr)
    , m_chapterStatusCombo(nullptr)
    , m_chapterNotesEdit(nullptr)
    , m_isUpdating(false)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel constructor called");

    setupUI();
    connectSignals();

    // Set initial state based on ProjectManager
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        showProjectProperties();
    } else {
        showNoProject();
    }

    logger.debug("PropertiesPanel initialized");
}

void PropertiesPanel::setupUI() {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::setupUI()");

    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Create stacked widget for different views
    m_stackedWidget = new QStackedWidget(this);

    // Add pages in order matching Page enum
    m_stackedWidget->addWidget(createNoProjectPage());   // Page::NoProject = 0
    m_stackedWidget->addWidget(createProjectPage());     // Page::Project = 1
    m_stackedWidget->addWidget(createChapterPage());     // Page::Chapter = 2

    mainLayout->addWidget(m_stackedWidget);
    setLayout(mainLayout);
}

QWidget* PropertiesPanel::createNoProjectPage() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(11, 11, 11, 11);

    m_noProjectLabel = new QLabel(tr("Open a project to see properties"), page);
    m_noProjectLabel->setAlignment(Qt::AlignCenter);
    m_noProjectLabel->setWordWrap(true);

    layout->addStretch(1);
    layout->addWidget(m_noProjectLabel);
    layout->addStretch(1);

    return page;
}

QWidget* PropertiesPanel::createProjectPage() {
    QWidget* page = new QWidget();

    // Use scroll area for long content
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* scrollContent = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(11, 11, 11, 11);
    layout->setSpacing(11);

    // Project Information group
    QGroupBox* infoGroup = new QGroupBox(tr("Project Information"), scrollContent);
    QFormLayout* infoLayout = new QFormLayout(infoGroup);
    infoLayout->setSpacing(6);
    infoLayout->setContentsMargins(11, 11, 11, 11);

    // Title
    m_projectTitleEdit = new QLineEdit(infoGroup);
    m_projectTitleEdit->setToolTip(tr("Project title"));
    m_projectTitleEdit->setPlaceholderText(tr("Enter project title"));
    infoLayout->addRow(tr("Title:"), m_projectTitleEdit);

    // Author
    m_projectAuthorEdit = new QLineEdit(infoGroup);
    m_projectAuthorEdit->setToolTip(tr("Author name"));
    m_projectAuthorEdit->setPlaceholderText(tr("Enter author name"));
    infoLayout->addRow(tr("Author:"), m_projectAuthorEdit);

    // Language
    m_projectLanguageCombo = new QComboBox(infoGroup);
    m_projectLanguageCombo->setToolTip(tr("Project language"));
    m_projectLanguageCombo->addItem(tr("English"), "en");
    m_projectLanguageCombo->addItem(tr("Polish"), "pl");
    m_projectLanguageCombo->addItem(tr("German"), "de");
    m_projectLanguageCombo->addItem(tr("French"), "fr");
    m_projectLanguageCombo->addItem(tr("Spanish"), "es");
    m_projectLanguageCombo->addItem(tr("Italian"), "it");
    m_projectLanguageCombo->addItem(tr("Portuguese"), "pt");
    m_projectLanguageCombo->addItem(tr("Russian"), "ru");
    m_projectLanguageCombo->addItem(tr("Chinese"), "zh");
    m_projectLanguageCombo->addItem(tr("Japanese"), "ja");
    infoLayout->addRow(tr("Language:"), m_projectLanguageCombo);

    // Genre
    m_projectGenreEdit = new QLineEdit(infoGroup);
    m_projectGenreEdit->setToolTip(tr("Book genre (e.g., Fiction, Mystery, Romance)"));
    m_projectGenreEdit->setPlaceholderText(tr("Enter genre"));
    infoLayout->addRow(tr("Genre:"), m_projectGenreEdit);

    layout->addWidget(infoGroup);

    // Statistics group
    QGroupBox* statsGroup = new QGroupBox(tr("Statistics"), scrollContent);
    QFormLayout* statsLayout = new QFormLayout(statsGroup);
    statsLayout->setSpacing(6);
    statsLayout->setContentsMargins(11, 11, 11, 11);

    // Total chapters
    m_projectChaptersLabel = new QLabel("0", statsGroup);
    m_projectChaptersLabel->setToolTip(tr("Total number of chapters in the project"));
    statsLayout->addRow(tr("Total Chapters:"), m_projectChaptersLabel);

    // Total words
    m_projectWordsLabel = new QLabel("0", statsGroup);
    m_projectWordsLabel->setToolTip(tr("Total word count across all chapters"));
    statsLayout->addRow(tr("Total Words:"), m_projectWordsLabel);

    // Created date
    m_projectCreatedLabel = new QLabel("-", statsGroup);
    m_projectCreatedLabel->setToolTip(tr("Date when the project was created"));
    statsLayout->addRow(tr("Created:"), m_projectCreatedLabel);

    // Modified date
    m_projectModifiedLabel = new QLabel("-", statsGroup);
    m_projectModifiedLabel->setToolTip(tr("Date when the project was last modified"));
    statsLayout->addRow(tr("Modified:"), m_projectModifiedLabel);

    layout->addWidget(statsGroup);

    // Add stretch at the bottom
    layout->addStretch(1);

    scrollArea->setWidget(scrollContent);

    // Set page layout
    QVBoxLayout* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scrollArea);

    return page;
}

QWidget* PropertiesPanel::createChapterPage() {
    QWidget* page = new QWidget();

    // Use scroll area for long content
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* scrollContent = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(11, 11, 11, 11);
    layout->setSpacing(11);

    // Chapter Information group
    QGroupBox* infoGroup = new QGroupBox(tr("Chapter Information"), scrollContent);
    QFormLayout* infoLayout = new QFormLayout(infoGroup);
    infoLayout->setSpacing(6);
    infoLayout->setContentsMargins(11, 11, 11, 11);

    // Title
    m_chapterTitleEdit = new QLineEdit(infoGroup);
    m_chapterTitleEdit->setToolTip(tr("Chapter title"));
    m_chapterTitleEdit->setPlaceholderText(tr("Enter chapter title"));
    infoLayout->addRow(tr("Title:"), m_chapterTitleEdit);

    // Word count (read-only)
    m_chapterWordCountLabel = new QLabel("0", infoGroup);
    m_chapterWordCountLabel->setToolTip(tr("Word count for this chapter"));
    infoLayout->addRow(tr("Word Count:"), m_chapterWordCountLabel);

    // Status
    m_chapterStatusCombo = new QComboBox(infoGroup);
    m_chapterStatusCombo->setToolTip(tr("Chapter completion status"));
    m_chapterStatusCombo->addItem(tr("Draft"), "draft");
    m_chapterStatusCombo->addItem(tr("In Progress"), "in_progress");
    m_chapterStatusCombo->addItem(tr("Complete"), "complete");
    m_chapterStatusCombo->addItem(tr("Final"), "final");
    infoLayout->addRow(tr("Status:"), m_chapterStatusCombo);

    layout->addWidget(infoGroup);

    // Notes group
    QGroupBox* notesGroup = new QGroupBox(tr("Notes"), scrollContent);
    QVBoxLayout* notesLayout = new QVBoxLayout(notesGroup);
    notesLayout->setSpacing(6);
    notesLayout->setContentsMargins(11, 11, 11, 11);

    m_chapterNotesEdit = new QTextEdit(notesGroup);
    m_chapterNotesEdit->setToolTip(tr("Notes and comments for this chapter"));
    m_chapterNotesEdit->setPlaceholderText(tr("Enter notes..."));
    m_chapterNotesEdit->setMinimumHeight(100);
    m_chapterNotesEdit->setMaximumHeight(200);
    notesLayout->addWidget(m_chapterNotesEdit);

    layout->addWidget(notesGroup);

    // Add stretch at the bottom
    layout->addStretch(1);

    scrollArea->setWidget(scrollContent);

    // Set page layout
    QVBoxLayout* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scrollArea);

    return page;
}

void PropertiesPanel::connectSignals() {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::connectSignals()");

    auto& pm = core::ProjectManager::getInstance();

    // Connect to ProjectManager signals
    connect(&pm, &core::ProjectManager::projectOpened,
            this, &PropertiesPanel::onProjectOpened);
    connect(&pm, &core::ProjectManager::projectClosed,
            this, &PropertiesPanel::onProjectClosed);

    // Connect project field changes
    connect(m_projectTitleEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onProjectTitleChanged);
    connect(m_projectAuthorEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onProjectAuthorChanged);
    connect(m_projectLanguageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertiesPanel::onProjectLanguageChanged);
    connect(m_projectGenreEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onProjectGenreChanged);

    // Connect chapter field changes
    connect(m_chapterTitleEdit, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onChapterTitleChanged);
    connect(m_chapterStatusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertiesPanel::onChapterStatusChanged);
    connect(m_chapterNotesEdit, &QTextEdit::textChanged,
            this, &PropertiesPanel::onChapterNotesChanged);
}

void PropertiesPanel::showProjectProperties() {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::showProjectProperties()");

    m_currentChapterId.clear();
    populateProjectFields();
    m_stackedWidget->setCurrentIndex(static_cast<int>(Page::Project));
}

void PropertiesPanel::showChapterProperties(const QString& elementId) {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::showChapterProperties() - Element: {}", elementId.toStdString());

    m_currentChapterId = elementId;
    populateChapterFields(elementId);
    m_stackedWidget->setCurrentIndex(static_cast<int>(Page::Chapter));
}

void PropertiesPanel::showNoProject() {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::showNoProject()");

    m_currentChapterId.clear();
    m_stackedWidget->setCurrentIndex(static_cast<int>(Page::NoProject));
}

void PropertiesPanel::refresh() {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::refresh()");

    Page currentPage = static_cast<Page>(m_stackedWidget->currentIndex());

    switch (currentPage) {
        case Page::NoProject:
            // Nothing to refresh
            break;
        case Page::Project:
            populateProjectFields();
            break;
        case Page::Chapter:
            if (!m_currentChapterId.isEmpty()) {
                populateChapterFields(m_currentChapterId);
            }
            break;
    }
}

void PropertiesPanel::onProjectOpened(const QString& projectPath) {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::onProjectOpened() - Path: {}", projectPath.toStdString());

    showProjectProperties();
}

void PropertiesPanel::onProjectClosed() {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::onProjectClosed()");

    showNoProject();
}

void PropertiesPanel::onProjectTitleChanged() {
    if (m_isUpdating) return;

    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) return;

    core::Document* doc = pm.getDocument();
    if (!doc) return;

    QString newTitle = m_projectTitleEdit->text().trimmed();
    if (newTitle.isEmpty()) {
        // Restore original value
        m_projectTitleEdit->setText(QString::fromStdString(doc->getTitle()));
        return;
    }

    logger.info("PropertiesPanel: Project title changed to: {}", newTitle.toStdString());
    doc->setTitle(newTitle.toStdString());
    pm.setDirty(true);
}

void PropertiesPanel::onProjectAuthorChanged() {
    if (m_isUpdating) return;

    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) return;

    core::Document* doc = pm.getDocument();
    if (!doc) return;

    QString newAuthor = m_projectAuthorEdit->text().trimmed();
    logger.info("PropertiesPanel: Project author changed to: {}", newAuthor.toStdString());
    doc->setAuthor(newAuthor.toStdString());
    pm.setDirty(true);
}

void PropertiesPanel::onProjectLanguageChanged(int index) {
    if (m_isUpdating) return;

    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) return;

    core::Document* doc = pm.getDocument();
    if (!doc) return;

    QString langCode = m_projectLanguageCombo->itemData(index).toString();
    logger.info("PropertiesPanel: Project language changed to: {}", langCode.toStdString());
    doc->setLanguage(langCode.toStdString());
    pm.setDirty(true);
}

void PropertiesPanel::onProjectGenreChanged() {
    if (m_isUpdating) return;

    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) return;

    core::Document* doc = pm.getDocument();
    if (!doc) return;

    QString newGenre = m_projectGenreEdit->text().trimmed();
    logger.info("PropertiesPanel: Project genre changed to: {}", newGenre.toStdString());
    doc->setGenre(newGenre.toStdString());
    pm.setDirty(true);
}

void PropertiesPanel::onChapterTitleChanged() {
    if (m_isUpdating) return;
    if (m_currentChapterId.isEmpty()) return;

    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) return;

    core::BookElement* element = pm.findElement(m_currentChapterId);
    if (!element) return;

    QString newTitle = m_chapterTitleEdit->text().trimmed();
    if (newTitle.isEmpty()) {
        // Restore original value
        m_chapterTitleEdit->setText(QString::fromStdString(element->getTitle()));
        return;
    }

    logger.info("PropertiesPanel: Chapter title changed to: {}", newTitle.toStdString());
    element->setTitle(newTitle.toStdString());
    pm.setDirty(true);
}

void PropertiesPanel::onChapterStatusChanged(int index) {
    if (m_isUpdating) return;
    if (m_currentChapterId.isEmpty()) return;

    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) return;

    core::BookElement* element = pm.findElement(m_currentChapterId);
    if (!element) return;

    QString statusCode = m_chapterStatusCombo->itemData(index).toString();
    logger.info("PropertiesPanel: Chapter status changed to: {}", statusCode.toStdString());
    element->setMetadata("status", statusCode.toStdString());
    pm.setDirty(true);
}

void PropertiesPanel::onChapterNotesChanged() {
    if (m_isUpdating) return;
    if (m_currentChapterId.isEmpty()) return;

    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) return;

    core::BookElement* element = pm.findElement(m_currentChapterId);
    if (!element) return;

    QString notes = m_chapterNotesEdit->toPlainText();
    element->setMetadata("notes", notes.toStdString());
    pm.setDirty(true);
}

void PropertiesPanel::populateProjectFields() {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::populateProjectFields()");

    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("PropertiesPanel: No project open, cannot populate fields");
        return;
    }

    const core::Document* doc = pm.getDocument();
    if (!doc) {
        logger.warn("PropertiesPanel: Document is null");
        return;
    }

    // Set updating flag to prevent recursive updates
    m_isUpdating = true;

    // Populate fields
    m_projectTitleEdit->setText(QString::fromStdString(doc->getTitle()));
    m_projectAuthorEdit->setText(QString::fromStdString(doc->getAuthor()));

    // Set language combo
    QString langCode = QString::fromStdString(doc->getLanguage());
    int langIndex = m_projectLanguageCombo->findData(langCode);
    if (langIndex >= 0) {
        m_projectLanguageCombo->setCurrentIndex(langIndex);
    } else {
        m_projectLanguageCombo->setCurrentIndex(0);  // Default to English
    }

    m_projectGenreEdit->setText(QString::fromStdString(doc->getGenre()));

    // Update statistics
    updateProjectStatistics();

    // Update dates
    m_projectCreatedLabel->setText(formatDate(doc->getCreated()));
    m_projectModifiedLabel->setText(formatDate(doc->getModified()));

    m_isUpdating = false;

    logger.debug("PropertiesPanel: Project fields populated");
}

void PropertiesPanel::populateChapterFields(const QString& elementId) {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::populateChapterFields() - Element: {}", elementId.toStdString());

    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("PropertiesPanel: No project open, cannot populate chapter fields");
        return;
    }

    core::BookElement* element = pm.findElement(elementId);
    if (!element) {
        logger.warn("PropertiesPanel: Element not found: {}", elementId.toStdString());
        return;
    }

    // Set updating flag to prevent recursive updates
    m_isUpdating = true;

    // Populate fields
    m_chapterTitleEdit->setText(QString::fromStdString(element->getTitle()));
    m_chapterWordCountLabel->setText(QString::number(element->getWordCount()));

    // Set status combo
    auto statusOpt = element->getMetadata("status");
    QString statusCode = statusOpt ? QString::fromStdString(*statusOpt) : "draft";
    int statusIndex = m_chapterStatusCombo->findData(statusCode);
    if (statusIndex >= 0) {
        m_chapterStatusCombo->setCurrentIndex(statusIndex);
    } else {
        m_chapterStatusCombo->setCurrentIndex(0);  // Default to Draft
    }

    // Set notes
    auto notesOpt = element->getMetadata("notes");
    QString notes = notesOpt ? QString::fromStdString(*notesOpt) : "";
    m_chapterNotesEdit->setPlainText(notes);

    m_isUpdating = false;

    logger.debug("PropertiesPanel: Chapter fields populated");
}

void PropertiesPanel::updateProjectStatistics() {
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel::updateProjectStatistics()");

    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        m_projectChaptersLabel->setText("0");
        m_projectWordsLabel->setText("0");
        return;
    }

    const core::Document* doc = pm.getDocument();
    if (!doc) {
        m_projectChaptersLabel->setText("0");
        m_projectWordsLabel->setText("0");
        return;
    }

    const core::Book& book = doc->getBook();

    // Get chapter count and word count
    size_t chapterCount = book.getChapterCount();
    int wordCount = book.getWordCount();

    m_projectChaptersLabel->setText(QString::number(chapterCount));
    m_projectWordsLabel->setText(QString::number(wordCount));

    logger.debug("PropertiesPanel: Statistics - {} chapters, {} words",
                 chapterCount, wordCount);
}

QString PropertiesPanel::formatDate(const std::chrono::system_clock::time_point& timePoint) const {
    auto time = std::chrono::system_clock::to_time_t(timePoint);

    // Handle epoch time (uninitialized)
    if (time <= 0) {
        return tr("-");
    }

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return QString::fromStdString(oss.str());
}

} // namespace gui
} // namespace kalahari
