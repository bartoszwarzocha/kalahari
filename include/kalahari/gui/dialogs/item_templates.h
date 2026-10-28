/// @file item_templates.h
/// @brief Template registry for NewItemDialog - file templates
///
/// TemplateRegistry provides a centralized, extensible system for managing
/// templates used in the New File window. Designed for future Python plugin
/// extensibility. The New Book window lists the book types of the installed packages
/// (BookTypeRegistry), not templates.
///
/// OpenSpec #00033: Project File System - Phase C

#pragma once

#include <QString>
#include <QStringList>
#include <map>
#include <vector>

namespace kalahari {
namespace gui {
namespace dialogs {

// ============================================================================
// TemplateInfo - Template metadata for display
// ============================================================================

/// @brief Metadata structure for template display in NewItemDialog
///
/// Contains all information needed to display and describe a template
/// to the user in the New File dialog.
///
/// @note Future: Python plugins will be able to register custom templates
/// using this structure via the TemplateRegistry API.
struct TemplateInfo {
    QString id;              ///< Unique identifier (e.g., "template.chapter")
    QString name;            ///< Display name (e.g., "Chapter")
    QString description;     ///< Multi-line description for preview area
    QString iconId;          ///< Icon command ID for ArtProvider (e.g., "template.chapter")
    QStringList features;    ///< Feature bullet points for preview
    QString fileExtension;   ///< Output file extension (e.g., ".rtf")
    bool isBuiltin = true;   ///< True for built-in templates, false for plugin-added

    /// @brief Check if template info is valid
    /// @return True if id and name are non-empty
    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
};

// ============================================================================
// TemplateRegistry - Singleton template manager
// ============================================================================

/// @brief Central registry for file templates
///
/// TemplateRegistry manages all available templates for creating new files. It follows the
/// singleton pattern like ArtProvider and provides a plugin-friendly API for future
/// extensibility.
///
/// Usage:
/// @code
/// // Get available file templates
/// auto fileTemplates = TemplateRegistry::getInstance().getFileTemplates();
///
/// // Get specific template
/// auto chapterTemplate = TemplateRegistry::getInstance().getTemplate("template.chapter");
///
/// // Plugin registration (future)
/// TemplateInfo customTemplate;
/// customTemplate.id = "plugin.custom_note";
/// customTemplate.name = "Custom Note";
/// // ... set other fields
/// TemplateRegistry::getInstance().registerFileTemplate(customTemplate);
/// @endcode
///
/// @see TemplateInfo
class TemplateRegistry {
public:
    /// @brief Get singleton instance
    /// @return Reference to the singleton TemplateRegistry
    static TemplateRegistry& getInstance();

    // ========================================================================
    // Plugin-friendly registration API
    // ========================================================================

    /// @brief Register a new file template
    /// @param info Template metadata
    /// @note If template with same ID exists, it will be replaced
    /// @note Emits no signals - caller should refresh UI if needed
    void registerFileTemplate(const TemplateInfo& info);

    /// @brief Unregister a template by ID
    /// @param id Template ID to remove
    /// @return True if template was found and removed
    bool unregisterTemplate(const QString& id);

    // ========================================================================
    // Query API
    // ========================================================================

    /// @brief Get all registered file templates
    /// @return Vector of TemplateInfo for file templates
    /// @note Templates are returned in registration order (builtin first)
    std::vector<TemplateInfo> getFileTemplates() const;

    /// @brief Get template by ID
    /// @param id Template ID to find
    /// @return TemplateInfo if found, invalid TemplateInfo otherwise
    /// @note Check result with TemplateInfo::isValid()
    TemplateInfo getTemplate(const QString& id) const;

    /// @brief Check if template exists
    /// @param id Template ID to check
    /// @return True if template is registered
    bool hasTemplate(const QString& id) const;

    // ========================================================================
    // Utility
    // ========================================================================

    /// @brief Get count of registered file templates
    /// @return Number of file templates
    size_t fileTemplateCount() const { return m_fileTemplates.size(); }

private:
    /// @brief Private constructor (singleton)
    TemplateRegistry();

    /// @brief Destructor
    ~TemplateRegistry() = default;

    /// @brief Deleted copy constructor
    TemplateRegistry(const TemplateRegistry&) = delete;

    /// @brief Deleted assignment operator
    TemplateRegistry& operator=(const TemplateRegistry&) = delete;

    /// @brief Load built-in file templates
    void loadBuiltinTemplates();

    /// @brief Load built-in file templates
    void loadBuiltinFileTemplates();

    /// @brief File templates storage (id -> info)
    std::map<QString, TemplateInfo> m_fileTemplates;

    /// @brief Order of file template IDs (for consistent iteration)
    std::vector<QString> m_fileTemplateOrder;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
