/// @file settings_pages.h
/// @brief The pages of the Settings dialog
///
/// Each page builds its controls and binds them to setting keys (SettingsPage);
/// only the theme, icons and diagnostic options add their own steps.

#pragma once

#include "kalahari/gui/settings/settings_page.h"

#include <QColor>

#include <functional>
#include <utility>
#include <vector>

class QComboBox;
class QHBoxLayout;
class QRadioButton;

namespace kalahari {
namespace core {
struct Theme;
}
namespace gui {

class ColorConfigWidget;
class FolderField;

/// @brief General: startup
class GeneralPage : public SettingsPage {
    Q_OBJECT
public:
    explicit GeneralPage(QWidget* parent = nullptr);

signals:
    /// @brief Another length unit was chosen (not applied yet): the length fields of
    ///        the open pages show it at once
    /// @param unit Name of the unit (ui.lengthUnit)
    void lengthUnitChanged(const QString& unit);
};

/// @brief Appearance > General: language and interface font
class AppearanceGeneralPage : public SettingsPage {
    Q_OBJECT
public:
    explicit AppearanceGeneralPage(QWidget* parent = nullptr);
};

/// @brief Appearance > Theme: the theme and its colors
///
/// Colors are stored per theme. A color shows the stored value or else the theme
/// file's default, and is stored only when changed; applying reloads the theme.
class ThemePage : public SettingsPage {
    Q_OBJECT
public:
    explicit ThemePage(QWidget* parent = nullptr);

    std::vector<std::string> apply() override;

    /// @brief Icon colors as currently shown (for the icon preview)
    [[nodiscard]] std::pair<QColor, QColor> iconColors() const;

signals:
    /// @brief An icon color was edited, or the theme changed
    void iconColorsChanged();

private:
    /// @brief Show the selected theme's colors
    void onThemeChanged();

    /// @brief Show the theme file's colors (stored on Apply like any edit)
    void resetToThemeDefaults();

    /// @brief Add a color bound to a key under the selected theme
    /// @param section Group the color is shown in
    /// @param label Visible name
    /// @param toolTip Tooltip
    /// @param keyPrefix Key before the theme name ("themes." or "icons.themes.")
    /// @param keySuffix Key after the theme name (".palette.window")
    /// @param themeColor The color in a theme loaded from its file
    /// @return The color widget
    ColorConfigWidget* addColor(QLayout* section, const QString& label, const QString& toolTip,
                                const std::string& keyPrefix, const std::string& keySuffix,
                                std::function<QColor(const core::Theme&)> themeColor);

    /// @brief The selected theme's name
    [[nodiscard]] std::string themeName() const;

    QComboBox* m_themeCombo;
    ColorConfigWidget* m_primaryColor = nullptr;
    ColorConfigWidget* m_secondaryColor = nullptr;

    struct ColorField {
        ColorConfigWidget* widget;
        std::function<QColor(const core::Theme&)> themeColor;
        Binding* binding;
    };
    std::vector<ColorField> m_colors;
};

/// @brief Appearance > Icons: icon style and sizes
class IconsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit IconsPage(QWidget* parent = nullptr);

    /// @brief Colors the preview draws the icons in
    void setPreviewColors(const QColor& primary, const QColor& secondary);

private:
    void updatePreview();

    QComboBox* m_iconStyle;
    QHBoxLayout* m_previewLayout;
    QColor m_primary;
    QColor m_secondary;
};

/// @brief Appearance > Dashboard
class DashboardPage : public SettingsPage {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget* parent = nullptr);
};

/// @brief Editor > General: font, typography, typewriter scrolling
class EditorGeneralPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorGeneralPage(QWidget* parent = nullptr);
};

/// @brief Editor > Colors: light and dark paper
class EditorColorsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorColorsPage(QWidget* parent = nullptr);
};

/// @brief Editor > Cursor
class EditorCursorPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorCursorPage(QWidget* parent = nullptr);
};

/// @brief Editor > Pages and Margins
class EditorPagesPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorPagesPage(QWidget* parent = nullptr);
};

/// @brief Annotations: who new comments, to-dos and notes are by, and their marks in the text
class AnnotationsPage : public SettingsPage {
    Q_OBJECT
public:
    explicit AnnotationsPage(QWidget* parent = nullptr);
};

/// @brief Files > Folders: the folders of the books and of the archives, and where and how
/// many backups of the database of each book are kept
///
/// After a change of the folder of the books or of the archives the windows that start in
/// it start there again, also when they remember the folder used last.
class FoldersPage : public SettingsPage {
    Q_OBJECT
public:
    explicit FoldersPage(QWidget* parent = nullptr);

    std::vector<std::string> apply() override;

    /// @brief A changed folder the program cannot use
    [[nodiscard]] Problem problem() const override;

private:
    /// @brief Add the field of a folder kept in setting @p key, which is empty for the
    /// default folder
    void addFolder(QFormLayout* form, const QString& label, const char* key,
                   const QString& defaultFolder, const QString& chooseTitle);

    /// @brief A field of a folder checked before it is written
    struct CheckedFolder {
        FolderField* field;
        Binding* binding;
    };
    std::vector<CheckedFolder> m_folders;

    QRadioButton* m_inEachBook = nullptr;   ///< The backups in the folder of each book
    QRadioButton* m_inOneFolder = nullptr;  ///< The backups in one folder for all books
    FolderField* m_backupFolder = nullptr;
};

/// @brief Advanced > General: the diagnostic menu (this session only, not stored)
class AdvancedGeneralPage : public SettingsPage {
    Q_OBJECT
public:
    /// @param diagnosticMode Whether the diagnostic menu is shown now
    explicit AdvancedGeneralPage(bool diagnosticMode, QWidget* parent = nullptr);

signals:
    /// @brief The diagnostic menu was turned on or off and applied
    void diagnosticModeChanged(bool enabled);
};

/// @brief Advanced > Log
class AdvancedLogPage : public SettingsPage {
    Q_OBJECT
public:
    explicit AdvancedLogPage(QWidget* parent = nullptr);
};

} // namespace gui
} // namespace kalahari
