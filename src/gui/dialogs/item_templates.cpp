/// @file item_templates.cpp
/// @brief Implementation of TemplateRegistry for NewItemDialog: the file templates
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
    auto fileIt = m_fileTemplates.find(id);
    if (fileIt != m_fileTemplates.end()) {
        return fileIt->second;
    }

    // Not found - return invalid template
    return TemplateInfo{};
}

bool TemplateRegistry::hasTemplate(const QString& id) const {
    return m_fileTemplates.find(id) != m_fileTemplates.end();
}

// ============================================================================
// Built-in Template Loading
// ============================================================================

void TemplateRegistry::loadBuiltinTemplates() {
    loadBuiltinFileTemplates();
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
