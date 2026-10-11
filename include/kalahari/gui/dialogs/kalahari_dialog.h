/// @file kalahari_dialog.h
/// @brief The base of the program's own dialogs: one look for all of them

#pragma once

#include <QDialog>
#include <QString>

class QDialogButtonBox;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QMargins;
class QShowEvent;
class QSize;
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

protected:
    /// @brief Keep the whole dialog on a small screen: the content scrolls, the heading
    ///        and the buttons stay in view
    void showEvent(QShowEvent* event) override;

    /// @brief Resize to @p wanted, or less where the screen is smaller, and keep the
    ///        whole window on the screen (e.g. after details were shown)
    void fitToScreen(const QSize& wanted);

    /// @brief The window's frame the system adds around the dialog: its own when the window
    ///        exists, else the style's guess
    QMargins frameMargins() const;

    /// @brief The buttons that close the dialog, for a dialog that adds one of its own
    QDialogButtonBox* buttonBox() const { return m_buttonBox; }

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
    QString m_iconId;
    bool m_compactHeading = false;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
