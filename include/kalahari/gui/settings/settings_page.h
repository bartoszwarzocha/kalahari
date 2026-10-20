/// @file settings_page.h
/// @brief Base class of the Settings dialog pages: controls bound to setting keys
///
/// A page binds each control to a setting key. The page then fills the controls from
/// the settings, tells whether anything was changed and writes only the changed
/// values: pages do not copy values by hand. A setting that takes effect only after
/// a restart (settings_schema::requiresRestart) is marked on the page automatically.

#pragma once

#include <QString>
#include <QColor>
#include <QWidget>

#include <nlohmann/json.hpp>

#include <deque>
#include <functional>
#include <string>
#include <vector>

class QAbstractButton;
class QCheckBox;
class QFormLayout;
class QLabel;
class QLayout;
class QPushButton;
class QVBoxLayout;

namespace kalahari {
namespace editor {
enum class TextIssueKind;
}  // namespace editor

namespace gui {

class ColorConfigWidget;

/// @brief One page of the Settings dialog
class SettingsPage : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit SettingsPage(QWidget* parent = nullptr);

    /// @brief Fill the controls from the current settings
    void load();

    /// @brief Whether any control differs from the value it was filled with
    [[nodiscard]] bool isChanged() const;

    /// @brief Write the changed values
    /// @return Keys of the settings written (session-only options have no key)
    virtual std::vector<std::string> apply();

    /// @brief A changed value that Apply and OK cannot write
    struct Problem {
        QString text;              ///< Why, in a sentence or two
        QWidget* field = nullptr;  ///< The control of the value
    };

    /// @brief Check the changed values before Apply or OK writes them
    ///
    /// For a value that cannot be written the dialog shows the page, puts the focus on the
    /// control of the value, says why and writes nothing.
    /// @return The first changed value that cannot be written; an empty text when all can be
    [[nodiscard]] virtual Problem problem() const;
    /// @brief The muted explanatory texts of the page (addNote())
    [[nodiscard]] const std::vector<QLabel*>& notes() const { return m_notes; }

    /// @brief The color of the notes: the theme's muted one, darker or lighter as far as
    ///        the window needs to read it (WCAG AA, as normal text)
    [[nodiscard]] static QColor noteColor();

    /// @brief The color of the notes on another background (a list, a card): darker or
    ///        lighter as far as @p background needs to read it too
    [[nodiscard]] static QColor noteColorOn(const QColor& background);

    /// @brief A check box with this property set to true shows a setting that other pages show
    ///        too: the Settings dialog keeps all of them alike, applied or not (by object name)
    static constexpr const char* SHARED_CHECK_BOX = "sharedCheckBox";

protected:
    /// @brief How one control is bound to a setting
    struct Binding {
        std::function<std::string()> key;                      ///< Setting key (may follow other controls)
        std::function<nlohmann::json()> stored;                ///< Current value of the setting
        std::function<nlohmann::json()> shown;                 ///< Value the control shows
        std::function<void(const nlohmann::json&)> show;       ///< Put a value into the control
        std::function<void(const nlohmann::json&)> store;      ///< Write a changed value
        nlohmann::json loaded;                                 ///< Value shown after filling
    };

    /// @brief Bind a check box, spin box, combo box, font combo box, line edit or color widget
    ///
    /// Combo boxes compare their item data with the stored value. Spin boxes store
    /// shown / scale (e.g. 50 % shown for 0.5 stored with scale 100). A LengthSpinBox
    /// is compared and written in its stored unit, whatever unit it shows.
    /// @param field The control
    /// @param key Setting key
    /// @param scale Spin boxes only: shown value per stored unit
    /// @return The binding, to adjust its functions
    Binding& bind(QWidget* field, const std::string& key, double scale = 1.0);

    /// @brief Bind a value with custom functions (all of key, stored, shown, show, store)
    Binding& bindCustom(Binding binding);

    /// @brief The binding made last (by bind(), addField() or addCheckBox())
    [[nodiscard]] Binding& lastBinding() { return m_bindings.back(); }

    /// @brief Fill one binding again (e.g. its key follows another control)
    void reload(Binding& binding);

    /// @brief Whether the control of a binding differs from the value it was filled with
    [[nodiscard]] static bool bindingChanged(const Binding& binding);

    /// @brief The page's top-level layout
    [[nodiscard]] QVBoxLayout* pageLayout() const { return m_layout; }

    /// @brief Add a titled group with a form to the page
    QFormLayout* addGroup(const QString& title);

    /// @brief Add a labelled control to a form and bind it
    /// @return The label (to grey it out with the control)
    QLabel* addField(QFormLayout* form, const QString& label, QWidget* field,
                     const std::string& key, double scale = 1.0);

    /// @brief Add a check box spanning the form and bind it
    QCheckBox* addCheckBox(QFormLayout* form, const QString& text, const std::string& key);

    /// @brief Add a muted explanatory text to a layout
    ///
    /// Muted, yet readable: it has at least the contrast of normal text (WCAG AA) with the
    /// window in every theme, also after the theme changes.
    QLabel* addNote(QLayout* layout, const QString& text);

    /// @brief Put @p note under check box or radio button @p button in line with its text
    static void alignWithText(QLabel* note, const QAbstractButton* button);

    /// @brief Grey out an option the program does not use yet
    void markNotUsedYet(QWidget* field, QLabel* label = nullptr);

    /// @brief Add an editor color of the theme (the underlines, the annotations), bound to its
    ///        setting
    ///
    /// The color is kept for the theme in use when the page is filled
    /// (ThemeManager::editorColorSetting()); once another theme is in use, the page shows its
    /// colors. A color equal to the theme's own is not kept, so the theme's color applies again.
    /// @param form The form of the group
    /// @param label What the color is for
    /// @param themeKey Name from the theme file's "editor" section
    ColorConfigWidget* addThemeColor(QFormLayout* form, const QString& label,
                                     const std::string& themeKey);

    /// @brief Add the color of a kind of issue: of its underline, which the frames of its cards
    ///        in the Spelling and Grammar panel have too
    void addIssueColor(QFormLayout* form, editor::TextIssueKind kind);

    /// @brief Add the check box "Optimize colors in themes": one setting, on every page with the
    ///        theme's editor colors (SHARED_CHECK_BOX)
    QCheckBox* addOptimizeColorsCheckBox(QFormLayout* form);

    /// @brief Add a button that shows the default colors of the colors added since the last
    ///        such button (Apply and OK store them, as any change)
    QPushButton* addRestoreColorsButton(QFormLayout* form);

    /// @brief Run a function after every load() (e.g. to grey out dependent controls)
    ///
    /// Signals like toggled() do not fire when a control already shows the loaded
    /// value, so states that follow a control are synced here as well.
    void whenLoaded(std::function<void()> sync);

    /// @brief Text explaining that an option needs a restart
    [[nodiscard]] static QString restartText();

private:
    /// @brief Give the notes the theme's muted color, readable on the window
    void updateNoteColors();

    QVBoxLayout* m_layout;
    std::deque<Binding> m_bindings;  ///< A deque keeps references to bindings valid
    std::vector<std::function<void()>> m_afterLoad;
    std::vector<QLabel*> m_notes;    ///< The notes, widgets of the page as long as it lives
    std::vector<std::function<void()>> m_restoreColors;  ///< Show the default of each color
    std::size_t m_restoreButtonColors = 0;  ///< Colors a restore button was added for already
};

} // namespace gui
} // namespace kalahari
