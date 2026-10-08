/// @file settings_page.h
/// @brief Base class of the Settings dialog pages: controls bound to setting keys
///
/// A page binds each control to a setting key. The page then fills the controls from
/// the settings, tells whether anything was changed and writes only the changed
/// values: pages do not copy values by hand. A setting that takes effect only after
/// a restart (settings_schema::requiresRestart) is marked on the page automatically.

#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

#include <deque>
#include <functional>
#include <string>
#include <vector>

class QCheckBox;
class QFormLayout;
class QLabel;
class QLayout;
class QVBoxLayout;

namespace kalahari {
namespace gui {

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

    /// @brief Bind a check box, spin box, combo box, font combo box or color widget
    ///
    /// Combo boxes compare their item data with the stored value. Spin boxes store
    /// shown / scale (e.g. 50 % shown for 0.5 stored with scale 100).
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
    QLabel* addNote(QLayout* layout, const QString& text);

    /// @brief Grey out an option the program does not use yet
    void markNotUsedYet(QWidget* field, QLabel* label = nullptr);

    /// @brief Run a function after every load() (e.g. to grey out dependent controls)
    ///
    /// Signals like toggled() do not fire when a control already shows the loaded
    /// value, so states that follow a control are synced here as well.
    void whenLoaded(std::function<void()> sync);

    /// @brief Text explaining that an option needs a restart
    [[nodiscard]] static QString restartText();

private:
    QVBoxLayout* m_layout;
    std::deque<Binding> m_bindings;  ///< A deque keeps references to bindings valid
    std::vector<std::function<void()>> m_afterLoad;
};

} // namespace gui
} // namespace kalahari
