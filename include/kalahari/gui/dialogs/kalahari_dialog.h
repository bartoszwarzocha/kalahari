/// @file kalahari_dialog.h
/// @brief The base of the program's own dialogs: one look for all of them

#pragma once

#include <QDialog>
#include <QString>

#include <functional>

class QDialogButtonBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QMargins;
class QShowEvent;
class QSize;
class QToolButton;
class QVBoxLayout;

namespace kalahari {
namespace gui {

class FittingScrollArea;

namespace dialogs {

/// @brief The base of the program's own dialogs
///
/// Each task gets a dialog of its own, derived from this class, so all of them look and
/// behave alike:
/// - a heading on the background of text fields: the dialog's icon, its title (also the
///   window title) and a sentence about what the dialog does; a compact heading takes
///   less room in a large dialog;
/// - the dialog's own content under it, in contentLayout(), with addField() for a field
///   with its label above it; the content gets the room of a resized dialog;
/// - the buttons at the bottom: the one that does the dialog's job, named after it
///   ("Add", "Rename"), Cancel and, when the dialog shows it, Apply. Enter presses the
///   first one, Esc the second; Apply does the job without closing the dialog.
///
/// The colors are roles of the palette of the current theme and the icon comes from
/// ArtProvider, so an open dialog follows a change of theme or icons.
class KalahariDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Create the dialog with an empty heading and content
    /// @param parent Parent widget
    explicit KalahariDialog(QWidget* parent = nullptr);

    /// @brief Set the heading
    /// @param title Title of the dialog, also its window title
    /// @param description Sentence under the title; empty: none
    void setHeading(const QString& title, const QString& description = QString());

    /// @brief Set the heading's icon
    /// @param iconId ArtProvider id (e.g. "template.chapter"); empty: no icon
    void setHeadingIcon(const QString& iconId);

    /// @brief Make the heading low: smaller margins, title and icon
    /// @param compact true for a compact heading (by default it is not)
    void setCompactHeading(bool compact);

    /// @brief The layout of the dialog's own content, between the heading and the buttons
    QVBoxLayout* contentLayout() const { return m_contentLayout; }

    /// @brief Add a field to the content, with its label above it
    /// @param label Label text, without a colon
    /// @param field The field; the label is its buddy
    void addField(const QString& label, QWidget* field);

    /// @brief The button that does the dialog's job; it is the default button
    QPushButton* acceptButton() const { return m_acceptButton; }

    /// @brief The button that closes the dialog without doing anything
    QPushButton* cancelButton() const { return m_cancelButton; }

    /// @brief The Apply button (hidden unless setApplyButtonVisible() shows it)
    QPushButton* applyButton() const { return m_applyButton; }

    /// @brief Name the accept button after what it does
    /// @param text Button text (e.g. "Add")
    void setAcceptText(const QString& text);

    /// @brief Show or hide the Apply button
    /// @param visible true to show it (by default it is hidden)
    void setApplyButtonVisible(bool visible);

    /// @brief At least the minimum width, and the height the heading and the content need
    ///        at that width
    QSize sizeHint() const override;

    /// @brief How the gear of a dialog (addSettingsButton()) opens the Settings dialog
    /// @param opener Opens the Settings dialog over the dialog given at the page given
    ///        (SettingsDialog::openPage()); the program sets it once, with its window
    static void setSettingsOpener(std::function<void(QWidget*, const QString&)> opener);

protected:
    /// @brief Keep the whole dialog on a small screen: the content scrolls, the heading
    ///        and the buttons stay in view
    void showEvent(QShowEvent* event) override;

    /// @brief Resize to @p wanted, or less where the screen is smaller, and keep the
    ///        whole window on the screen (e.g. after details were shown)
    void fitToScreen(const QSize& wanted);

    /// @brief Once the layout has taken in content shown just now (a message, a note), grow
    ///        to the size it needs, within the screen. The window does not get smaller.
    void growForContent();

    /// @brief fitToScreen() with the hints the layout has: for a dialog whose layout has just
    ///        taken in the content (on its LayoutRequest). It asks the layout for nothing, so
    ///        a window that cannot grow does not ask for its layout again and again.
    void resizeOnScreen(const QSize& wanted);

    /// @brief The window's frame the system adds around the dialog: its own when the window
    ///        exists, else the style's guess
    QMargins frameMargins() const;

    /// @brief @p size made higher by the heading and the line under it, at its width: the
    ///        default size of a dialog given for its content and buttons alone
    QSize withHeading(const QSize& size) const;

    /// @brief The buttons that close the dialog, for a dialog that adds one of its own
    QDialogButtonBox* buttonBox() const { return m_buttonBox; }

    /// @brief Add the gear at the right end of the heading: it opens the Settings dialog at
    ///        the page of the dialog's settings, as the gear of a panel does
    ///
    /// Without the program's opener (setSettingsOpener(), as in the tests) there is none.
    /// @param page The page (SettingsDialog::openPage())
    /// @param toolTip What the settings are ("Link Color Settings"), also its accessible name
    /// @return The button; nullptr without an opener
    QToolButton* addSettingsButton(const QString& page, const QString& toolTip);

    /// @brief Add a button at the left end of the row, apart from the ones that close the
    ///        dialog, on every system (e.g. Copy of a message)
    /// @param text Button text, with a mnemonic
    /// @return The button; it does not close the dialog
    QPushButton* addSideButton(const QString& text);

signals:
    /// @brief Apply was pressed: do the dialog's job and keep it open
    void applyClicked();

private:
    /// @brief Set the heading's margins, title size and icon for its style
    void updateHeadingStyle();

    /// @brief Draw the heading's icon at the size and in the colors of the current icons
    void updateHeadingIcon();

    QFrame* m_heading = nullptr;
    QHBoxLayout* m_headingLayout = nullptr;
    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_descriptionLabel = nullptr;
    FittingScrollArea* m_contentArea = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    QHBoxLayout* m_sideButtonLayout = nullptr;
    QPushButton* m_acceptButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QPushButton* m_applyButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QString m_iconId;
    bool m_compactHeading = false;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
