/// @file item_templates.cpp
/// @brief Implementation of TemplateRegistry for NewItemDialog
///
/// OpenSpec #00033: Project File System - Phase C

#include "kalahari/gui/dialogs/item_templates.h"
#include <QCoreApplication>

using namespace kalahari::gui::dialogs;

// ============================================================================
// Singleton Implementation
// ============================================================================

TemplateRegistry& TemplateRegistry::getInstance() {
    static TemplateRegistry instance;
    return instance;
}

TemplateRegistry::TemplateRegistry() {
    loadBuiltinTemplates();
}

// ============================================================================
// Registration API
// ============================================================================

void TemplateRegistry::registerProjectTemplate(const TemplateInfo& info) {
    if (!info.isValid()) {
        return;
    }

    // Check if already exists - if so, just update
    bool exists = m_projectTemplates.find(info.id) != m_projectTemplates.end();
    m_projectTemplates[info.id] = info;

    // Add to order list if new
    if (!exists) {
        m_projectTemplateOrder.push_back(info.id);
    }
}

void TemplateRegistry::registerFileTemplate(const TemplateInfo& info) {
    if (!info.isValid()) {
        return;
    }

    // Check if already exists - if so, just update
    bool exists = m_fileTemplates.find(info.id) != m_fileTemplates.end();
    m_fileTemplates[info.id] = info;

    // Add to order list if new
    if (!exists) {
        m_fileTemplateOrder.push_back(info.id);
    }
}

bool TemplateRegistry::unregisterTemplate(const QString& id) {
    // Try project templates first
    auto projectIt = m_projectTemplates.find(id);
    if (projectIt != m_projectTemplates.end()) {
        m_projectTemplates.erase(projectIt);
        // Remove from order list
        auto orderIt = std::find(m_projectTemplateOrder.begin(),
                                  m_projectTemplateOrder.end(), id);
        if (orderIt != m_projectTemplateOrder.end()) {
            m_projectTemplateOrder.erase(orderIt);
        }
        return true;
    }

    // Try file templates
    auto fileIt = m_fileTemplates.find(id);
    if (fileIt != m_fileTemplates.end()) {
        m_fileTemplates.erase(fileIt);
        // Remove from order list
        auto orderIt = std::find(m_fileTemplateOrder.begin(),
                                  m_fileTemplateOrder.end(), id);
        if (orderIt != m_fileTemplateOrder.end()) {
            m_fileTemplateOrder.erase(orderIt);
        }
        return true;
    }

    return false;
}

// ============================================================================
// Query API
// ============================================================================

std::vector<TemplateInfo> TemplateRegistry::getProjectTemplates() const {
    std::vector<TemplateInfo> result;
    result.reserve(m_projectTemplateOrder.size());

    for (const auto& id : m_projectTemplateOrder) {
        auto it = m_projectTemplates.find(id);
        if (it != m_projectTemplates.end()) {
            result.push_back(it->second);
        }
    }

    return result;
}

std::vector<TemplateInfo> TemplateRegistry::getFileTemplates() const {
    std::vector<TemplateInfo> result;
    result.reserve(m_fileTemplateOrder.size());

    for (const auto& id : m_fileTemplateOrder) {
        auto it = m_fileTemplates.find(id);
        if (it != m_fileTemplates.end()) {
            result.push_back(it->second);
        }
    }

    return result;
}

TemplateInfo TemplateRegistry::getTemplate(const QString& id) const {
    // Try project templates first
    auto projectIt = m_projectTemplates.find(id);
    if (projectIt != m_projectTemplates.end()) {
        return projectIt->second;
    }

    // Try file templates
    auto fileIt = m_fileTemplates.find(id);
    if (fileIt != m_fileTemplates.end()) {
        return fileIt->second;
    }

    // Not found - return invalid template
    return TemplateInfo{};
}

bool TemplateRegistry::hasTemplate(const QString& id) const {
    return m_projectTemplates.find(id) != m_projectTemplates.end() ||
           m_fileTemplates.find(id) != m_fileTemplates.end();
}

// ============================================================================
// Built-in Template Loading
// ============================================================================

void TemplateRegistry::loadBuiltinTemplates() {
    loadBuiltinProjectTemplates();
    loadBuiltinFileTemplates();
}

void TemplateRegistry::loadBuiltinProjectTemplates() {
    // ========================================================================
    // Novel Template (default)
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.novel");
        info.name = QCoreApplication::translate("TemplateRegistry", "Novel");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A traditional novel structure with parts and chapters.\n\n"
            "Includes front matter (title page, dedication) and back matter "
            "(epilogue, acknowledgments). Perfect for fiction writing with "
            "a clear hierarchical organization.");
        info.iconId = QStringLiteral("template.novel");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Part/Chapter structure"),
            QCoreApplication::translate("TemplateRegistry", "Front matter (title, dedication)"),
            QCoreApplication::translate("TemplateRegistry", "Back matter (epilogue, notes)"),
            QCoreApplication::translate("TemplateRegistry", "Word count tracking"),
            QCoreApplication::translate("TemplateRegistry", "Character & location banks")
        };
        info.fileExtension = QString(); // Projects don't have extensions
        info.isBuiltin = true;
        registerProjectTemplate(info);
    }

    // ========================================================================
    // Short Story Collection Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.shortStories");
        info.name = QCoreApplication::translate("TemplateRegistry", "Short Story Collection");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A collection of independent short stories.\n\n"
            "Flat structure without parts - each story stands alone. "
            "Great for anthologies, collections, or episodic content.");
        info.iconId = QStringLiteral("template.shortStories");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Flat story structure"),
            QCoreApplication::translate("TemplateRegistry", "Independent stories"),
            QCoreApplication::translate("TemplateRegistry", "Per-story statistics"),
            QCoreApplication::translate("TemplateRegistry", "Easy reordering"),
            QCoreApplication::translate("TemplateRegistry", "Export individual stories")
        };
        info.fileExtension = QString();
        info.isBuiltin = true;
        registerProjectTemplate(info);
    }

    // ========================================================================
    // Non-fiction Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.nonfiction");
        info.name = QCoreApplication::translate("TemplateRegistry", "Non-fiction");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A non-fiction book with flat chapter structure.\n\n"
            "Designed for essays, guides, memoirs, and technical writing. "
            "Includes bibliography and index support.");
        info.iconId = QStringLiteral("template.nonfiction");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Flat chapter structure"),
            QCoreApplication::translate("TemplateRegistry", "Bibliography support"),
            QCoreApplication::translate("TemplateRegistry", "Index generation"),
            QCoreApplication::translate("TemplateRegistry", "Footnotes & citations"),
            QCoreApplication::translate("TemplateRegistry", "Research notes section")
        };
        info.fileExtension = QString();
        info.isBuiltin = true;
        registerProjectTemplate(info);
    }

    // ========================================================================
    // Screenplay Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.screenplay");
        info.name = QCoreApplication::translate("TemplateRegistry", "Screenplay");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A screenplay or stage play structure.\n\n"
            "Organized by acts and scenes with proper screenplay formatting. "
            "Suitable for film, TV, or theater scripts.");
        info.iconId = QStringLiteral("template.screenplay");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Act/Scene structure"),
            QCoreApplication::translate("TemplateRegistry", "Screenplay formatting"),
            QCoreApplication::translate("TemplateRegistry", "Character list"),
            QCoreApplication::translate("TemplateRegistry", "Scene descriptions"),
            QCoreApplication::translate("TemplateRegistry", "Dialogue formatting")
        };
        info.fileExtension = QString();
        info.isBuiltin = true;
        registerProjectTemplate(info);
    }

    // ========================================================================
    // Poetry Collection Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.poetry");
        info.name = QCoreApplication::translate("TemplateRegistry", "Poetry Collection");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A collection of poems organized by sections.\n\n"
            "Flexible structure for organizing poems into thematic sections. "
            "Supports various poetry formats and styles.");
        info.iconId = QStringLiteral("template.poetry");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Section/Poem structure"),
            QCoreApplication::translate("TemplateRegistry", "Thematic grouping"),
            QCoreApplication::translate("TemplateRegistry", "Verse formatting"),
            QCoreApplication::translate("TemplateRegistry", "Line count tracking"),
            QCoreApplication::translate("TemplateRegistry", "Stanza support")
        };
        info.fileExtension = QString();
        info.isBuiltin = true;
        registerProjectTemplate(info);
    }

    // ========================================================================
    // Empty Project Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.empty");
        info.name = QCoreApplication::translate("TemplateRegistry", "Empty Project");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A blank project with no predefined structure.\n\n"
            "Start from scratch and build your own structure. "
            "Recommended for advanced users who want full control.");
        info.iconId = QStringLiteral("template.empty");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "No predefined structure"),
            QCoreApplication::translate("TemplateRegistry", "Full customization"),
            QCoreApplication::translate("TemplateRegistry", "Add elements manually"),
            QCoreApplication::translate("TemplateRegistry", "For advanced users")
        };
        info.fileExtension = QString();
        info.isBuiltin = true;
        registerProjectTemplate(info);
    }
}

void TemplateRegistry::loadBuiltinFileTemplates() {
    // ========================================================================
    // Chapter Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.chapter");
        info.name = QCoreApplication::translate("TemplateRegistry", "Chapter");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A new chapter for your book.\n\n"
            "Rich text document with formatting support. "
            "The primary content unit for writing.");
        info.iconId = QStringLiteral("template.chapter");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Rich text formatting"),
            QCoreApplication::translate("TemplateRegistry", "Word count tracking"),
            QCoreApplication::translate("TemplateRegistry", "Auto-save support"),
            QCoreApplication::translate("TemplateRegistry", "Export to RTF/DOCX/PDF")
        };
        info.fileExtension = QStringLiteral(".rtf");
        info.isBuiltin = true;
        registerFileTemplate(info);
    }

    // ========================================================================
    // Mind Map Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.mindmap");
        info.name = QCoreApplication::translate("TemplateRegistry", "Mind Map");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A visual mind map for brainstorming.\n\n"
            "Organize ideas, plot points, and connections visually. "
            "Great for planning and outlining.");
        info.iconId = QStringLiteral("template.mindmap");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Visual node editor"),
            QCoreApplication::translate("TemplateRegistry", "Drag & drop organization"),
            QCoreApplication::translate("TemplateRegistry", "Color coding"),
            QCoreApplication::translate("TemplateRegistry", "Export to image")
        };
        info.fileExtension = QStringLiteral(".kmap");
        info.isBuiltin = true;
        registerFileTemplate(info);
    }

    // ========================================================================
    // Timeline Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.timeline");
        info.name = QCoreApplication::translate("TemplateRegistry", "Timeline");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A chronological timeline for your story.\n\n"
            "Track events, character arcs, and plot progression. "
            "Visualize the temporal structure of your narrative.");
        info.iconId = QStringLiteral("template.timeline");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Event tracking"),
            QCoreApplication::translate("TemplateRegistry", "Multiple timelines"),
            QCoreApplication::translate("TemplateRegistry", "Character tracking"),
            QCoreApplication::translate("TemplateRegistry", "Date/time support")
        };
        info.fileExtension = QStringLiteral(".ktl");
        info.isBuiltin = true;
        registerFileTemplate(info);
    }

    // ========================================================================
    // Note Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.note");
        info.name = QCoreApplication::translate("TemplateRegistry", "Note");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A quick note for ideas and research.\n\n"
            "Capture thoughts, research snippets, and reminders. "
            "Searchable and taggable.");
        info.iconId = QStringLiteral("template.note");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Quick capture"),
            QCoreApplication::translate("TemplateRegistry", "Tags and categories"),
            QCoreApplication::translate("TemplateRegistry", "Full-text search"),
            QCoreApplication::translate("TemplateRegistry", "Link to chapters")
        };
        info.fileExtension = QStringLiteral(".json");
        info.isBuiltin = true;
        registerFileTemplate(info);
    }

    // ========================================================================
    // Character Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.character");
        info.name = QCoreApplication::translate("TemplateRegistry", "Character");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A character profile sheet.\n\n"
            "Document your characters with structured fields for "
            "appearance, personality, backstory, and relationships.");
        info.iconId = QStringLiteral("template.character");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Structured profile"),
            QCoreApplication::translate("TemplateRegistry", "Image attachment"),
            QCoreApplication::translate("TemplateRegistry", "Relationship mapping"),
            QCoreApplication::translate("TemplateRegistry", "Scene references")
        };
        info.fileExtension = QStringLiteral(".json");
        info.isBuiltin = true;
        registerFileTemplate(info);
    }

    // ========================================================================
    // Location Template
    // ========================================================================
    {
        TemplateInfo info;
        info.id = QStringLiteral("template.location");
        info.name = QCoreApplication::translate("TemplateRegistry", "Location");
        info.description = QCoreApplication::translate("TemplateRegistry",
            "A location or setting profile.\n\n"
            "Document places in your story with descriptions, "
            "maps, and associated scenes.");
        info.iconId = QStringLiteral("template.location");
        info.features = QStringList{
            QCoreApplication::translate("TemplateRegistry", "Location details"),
            QCoreApplication::translate("TemplateRegistry", "Image/map attachment"),
            QCoreApplication::translate("TemplateRegistry", "Scene references"),
            QCoreApplication::translate("TemplateRegistry", "Hierarchy support")
        };
        info.fileExtension = QStringLiteral(".json");
        info.isBuiltin = true;
        registerFileTemplate(info);
    }
}
