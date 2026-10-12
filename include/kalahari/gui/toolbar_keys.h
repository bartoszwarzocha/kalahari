/// @file toolbar_keys.h
/// @brief After a command given in a toolbar the keys go back where they were

#pragma once

#include <QMetaObject>
#include <QObject>

class QComboBox;
class QToolBar;
class QWidget;

namespace kalahari::gui {

/// @brief Gives the keys back to the element that had them after a command given in a toolbar
///
/// As in Word and LibreOffice: after a choice in a toolbar's field (the font, its size) is
/// made with Enter, Escape or in the field's list, the keys go back to the element that had
/// them, the text or a panel's field; the arrows of the size field, like the buttons, do not
/// take them. A floating toolbar is a window of its own: a click on it gives it the keys
/// (Windows, Linux), and they go back when the click is not in one of its fields, and after
/// a command given there. In the fields one types as in a docked toolbar, and the keys move
/// between them with Tab.
class ToolbarKeys : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param mainWindow The window whose toolbars give the keys back
    /// @param parent Parent object
    explicit ToolbarKeys(QWidget* mainWindow, QObject* parent = nullptr);

    /// @brief Whether a widget is in a field of the toolbar: it takes the keys at a click
    [[nodiscard]] static bool isField(const QWidget* widget, const QToolBar* toolbar);

protected:
    /// @brief Toolbars and fields made later; the keys that close a field; a field's list
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief A toolbar's commands and fields give the keys back; shown floating, it does not
    ///        take them
    void watchToolbar(QToolBar* toolbar);

    /// @brief A field gives the keys back when a choice is made in it
    void watchField(QWidget* field);

    /// @brief Another window took the keys
    void onActiveWindowChanged();

    /// @brief Another element took the keys
    void onFocusChanged(QWidget* old, QWidget* now);

    /// @brief A command was given in a toolbar
    void onCommand();

    /// @brief An item was chosen in a combo box: in its list, or with the arrows in its field
    void onItemChosen();

    /// @brief A field's list was closed
    void onListClosed(const QComboBox* combo);

    /// @brief Give the keys back to the element that had them, when a toolbar has them and no
    ///        menu is open
    void giveBackKeys();

    /// @brief Give the keys back once the event being handled is done
    void giveBackKeysLater();

    /// @brief The toolbar of the main window the widget is in, also in a list of its field
    [[nodiscard]] const QToolBar* toolbarOf(const QWidget* widget) const;

    /// @brief The window, when it is a floating toolbar of the main window
    [[nodiscard]] QToolBar* floatingToolbar(QWidget* window) const;

    /// @brief Remember the element that had the keys last, outside the toolbars
    void setLastElement(QWidget* element);

    QWidget* m_mainWindow;
    // The element is forgotten when it goes: another one may come at the same address
    QWidget* m_lastElement = nullptr;  ///< Had the keys last, outside the toolbars
    QMetaObject::Connection m_lastElementGone;  ///< Forgets m_lastElement when it goes
    // Only compared, never used: a field's list or a size field
    const QObject* m_openList = nullptr;     ///< The combo box whose list is open
    const QObject* m_chosenInList = nullptr; ///< An item was chosen in its list, still shown
    const QObject* m_clickedField = nullptr; ///< A size field took the keys at a click
};

}  // namespace kalahari::gui
