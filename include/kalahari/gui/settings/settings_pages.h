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
class QLineEdit;
class QListWidget;
class QPushButton;

namespace kalahari {
namespace core {
struct Theme;
}
namespace editor {
class SpellCheckService;
}
namespace gui {

class ColorConfigWidget;

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

/// @brief Editor > Spelling: checking as you type, its language and the writer's own words
class EditorSpellingPage : public SettingsPage {
    Q_OBJECT
public:
    /// @param spelling The dictionary of the editors: the page lists its own words and
    ///        changes them when applied (none: the page does not list them)
    explicit EditorSpellingPage(editor::SpellCheckService* spelling = nullptr,
                                QWidget* parent = nullptr);

protected:
    /// @brief Enter in the field of a new word adds it, and does not close the dialog
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Add the word of the field to the list
    void addWord();

    /// @brief Take the selected words off the list
    void removeWords();

    /// @brief Enable the buttons that can do something now
    void updateButtons();

    QListWidget* m_words = nullptr;   ///< The writer's own words
    QLineEdit* m_newWord = nullptr;   ///< A word to add
    QPushButton* m_add = nullptr;
    QPushButton* m_remove = nullptr;
};

/// @brief Editor > Pages and Margins
class EditorPagesPage : public SettingsPage {
    Q_OBJECT
public:
    explicit EditorPagesPage(QWidget* parent = nullptr);
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
