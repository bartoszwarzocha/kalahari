/// @file material_list_coordinator.h
/// @brief The lists of the kinds of materials on their tabs in the middle of the window

#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class QStatusBar;
class QTabWidget;
class QTimer;
class QWidget;

namespace kalahari::gui {

class MaterialListPanel;
class NavigatorCoordinator;

/// @brief Opens the lists of the materials (MaterialListPanel) on tabs in the middle of the
/// window and does what their commands ask
///
/// A kind of the Materials clicked in the Navigator, or Materials itself (All Materials), shows
/// its list on a tab of its own, or brings the tab to the front; or, as the settings say
/// (MaterialList::TABS_SETTING), the one tab of the lists shows it. The lists follow the book:
/// a material added, changed, renamed, deleted or linked, the theme, and the Navigator built
/// again. A list behind another tab is filled again when it comes to the front.
///
/// The commands of a list are those of the Navigator (NavigatorCoordinator): a material's card,
/// the editor of a working note, Open, Rename, Delete, Add; Link... opens the Link window, and
/// a material chosen shows in the Properties panel.
class MaterialListCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Brings the Properties panel to the front; @p keys: with the keys in its first
    /// field (Alt+Enter)
    using PropertiesShower = std::function<void(bool keys)>;

    /// @brief Constructor
    /// @param centralTabs The tabs in the middle of the window
    /// @param navigator The commands of the elements of the book
    /// @param statusBar Where the lists say what they did
    /// @param showProperties Brings the Properties panel to the front
    /// @param parent Parent QObject
    MaterialListCoordinator(QTabWidget* centralTabs, NavigatorCoordinator* navigator,
                            QStatusBar* statusBar, PropertiesShower showProperties,
                            QObject* parent = nullptr);
    ~MaterialListCoordinator() override;

    MaterialListCoordinator(const MaterialListCoordinator&) = delete;
    MaterialListCoordinator& operator=(const MaterialListCoordinator&) = delete;

    /// @brief Show list @p list (MaterialList) on its tab, as MaterialList::TABS_SETTING says
    /// @param keys The list gets the keys
    /// @return The list shown; none without an open book
    MaterialListPanel* openList(const QString& list, bool keys);

    /// @brief The lists open on the tabs, in the order of the tabs
    QList<MaterialListPanel*> lists() const;

    /// @brief The list on the tab in front; none when another tab is in front
    MaterialListPanel* currentList() const;

public slots:
    /// @brief Fill the lists again from the open book: the one in front at once, the others
    /// when they come to the front
    void refresh();

private slots:
    /// @brief What the Materials of the Navigator show changed (NavigatorPanel::
    /// MATERIALS_TREE_SETTING): the Navigator is built again
    void onTreeSettingChanged();

private:
    void refreshNow();
    void connectList(MaterialListPanel* panel);
    void updateTab(MaterialListPanel* panel);
    /// @brief The list in front is filled again if it missed a change
    void onCurrentTabChanged();
    void edit(const QString& elementId, bool text);
    void open(const QString& elementId);
    void rename(const QString& elementId);
    void link(const QString& elementId);
    void add(MaterialListPanel* panel, const QString& list);
    void showInFolder(const QString& elementId);
    QWidget* window() const;

    QTabWidget* m_centralTabs;
    NavigatorCoordinator* m_navigator;
    QStatusBar* m_statusBar;
    PropertiesShower m_showProperties;
    QTimer* m_refreshTimer = nullptr;
    int m_settingsListener = 0;  ///< core::SettingsManager::subscribe() id
};

}  // namespace kalahari::gui
