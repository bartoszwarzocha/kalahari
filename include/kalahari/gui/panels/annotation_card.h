/// @file annotation_card.h
/// @brief A card of the Annotations panel: one comment, TODO or note

#pragma once

#include <kalahari/gui/panels/annotation_entry.h>

#include <QColor>
#include <QFrame>

class QCheckBox;
class QLabel;
class QTextEdit;
class QToolButton;

namespace kalahari::gui {

/// @brief A card of the Annotations panel
///
/// A bar in the color of the annotation's kind on the left and a background tinted with
/// it; a header with the kind, the chapter and the date (a TODO also has its check box)
/// and a menu button; below it the annotation's text. A click on the text edits it in
/// place: the card reports every change, and Esc, Ctrl+Enter or a click elsewhere ends
/// the editing.
class AnnotationCard : public QFrame {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit AnnotationCard(QWidget* parent = nullptr);

    /// @brief The annotation it shows
    const AnnotationEntry& entry() const { return m_entry; }

    /// @brief Show an annotation
    ///
    /// While the text is edited, it stays as typed.
    void setEntry(const AnnotationEntry& entry);

    /// @brief Colors from the window's theme
    /// @param kindColor The color of the annotation's kind
    /// @param background The panel's background; the card's own is a tint of it
    /// @param tint How much of the kind's color the card's background has (0 to 1)
    void setColors(const QColor& kindColor, const QColor& background, double tint);

    /// @brief Mark the card as the selected one (an outline)
    void setSelected(bool selected);

    /// @brief Whether it is the selected card
    bool isSelected() const { return m_selected; }

    /// @brief Edit the text: it gets the keyboard focus, with the cursor at its end
    void startEditing();

    /// @brief Whether its text is being edited
    bool isEditing() const { return m_editing; }

    /// @brief The text as shown, also while it is edited
    QString text() const;

signals:
    /// @brief The card was clicked outside its text, check box and menu button
    void clicked();

    /// @brief Editing of the text started
    void editingStarted();

    /// @brief The text changed while it is edited
    void textEdited(const QString& text);

    /// @brief Esc or Ctrl+Enter in the text: the writer is done with it
    void editingDismissed();

    /// @brief Editing of the text ended: its text lost the keyboard focus
    void editingFinished();

    /// @brief The TODO was done or the comment resolved, or either brought back
    void doneToggled(bool done);

    /// @brief Removing the annotation was asked for
    void deleteRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Kind, state, chapter and date in the header
    void updateHeader();

    /// @brief The text in the theme's text color, dimmed when the annotation is done
    void updateTextColor();

    /// @brief The chapter's title, shortened to the room it has
    void updateChapterLabel();

    /// @brief The text box as high as its text
    void updateTextHeight();

    /// @brief The menu button's icon, in the colors of the icons
    void updateMenuIcon();

    /// @brief The menu of the menu button: done or resolved, restore, delete
    void showMenu();

    AnnotationEntry m_entry;
    QCheckBox* m_doneBox{nullptr};
    QLabel* m_kindLabel{nullptr};
    QLabel* m_chapterLabel{nullptr};
    QLabel* m_dateLabel{nullptr};
    QToolButton* m_menuButton{nullptr};
    QTextEdit* m_textEdit{nullptr};
    QColor m_kindColor;
    QColor m_background;
    bool m_selected = false;
    bool m_editing = false;
    bool m_settingText = false;  ///< The text is set from the annotation, not typed
};

}  // namespace kalahari::gui
