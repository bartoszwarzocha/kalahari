/// @file annotation_card.h
/// @brief A card of the Annotations panel: one comment, to-do or note

#pragma once

#include <kalahari/gui/panels/annotation_colors.h>
#include <kalahari/gui/panels/annotation_entry.h>

#include <QColor>
#include <QFrame>

class QCheckBox;
class QLabel;
class QToolButton;

namespace kalahari::gui {

/// @brief A card of the Annotations panel
///
/// A bar in the color of the annotation's kind on the left and a background tinted with
/// it; a header with the kind, the chapter and the date (a to-do also has its check box)
/// and a menu button; below it who made the annotation and its text. The tooltip says who
/// made it and when, and the chapter's whole title. The card only shows the annotation: a
/// click on it selects it, a double click asks to edit it (in the frame at its place in
/// the text).
class AnnotationCard : public QFrame {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit AnnotationCard(QWidget* parent = nullptr);

    /// @brief The name of a kind, as the cards and the frame the text is written in show it
    /// @param done A resolved comment is named so
    static QString kindTitle(editor::AnnotationKind kind, bool done = false);

    /// @brief The annotation it shows
    const AnnotationEntry& entry() const { return m_entry; }

    /// @brief Show an annotation
    void setEntry(const AnnotationEntry& entry);

    /// @brief The card's colors (for its kind, on the panel's theme)
    void setColors(const AnnotationCardColors& colors);

    /// @brief The colors it is shown in
    const AnnotationCardColors& colors() const { return m_colors; }

    /// @brief The color its text is shown in (dimmed for a done one)
    QColor textColor() const;

    /// @brief Mark the card as the selected one (an outline)
    void setSelected(bool selected);

    /// @brief Whether it is the selected card
    bool isSelected() const { return m_selected; }

    /// @brief Whether the keys are in the list of cards: the selected card's outline is in
    /// the highlight color then, and a quiet one otherwise
    void setListFocused(bool focused);

    /// @brief Whether the keys are in the list of cards
    bool isListFocused() const { return m_listFocused; }

    /// @brief The text as shown
    QString text() const;

    /// @brief Show the card's menu under its menu button (also asked for with the keys)
    void showMenu();

signals:
    /// @brief The card was clicked outside its check box and menu button
    void clicked();

    /// @brief Editing the annotation was asked for (a double click, or Edit in the menu)
    void editRequested();

    /// @brief The to-do was done or the comment resolved, or either brought back
    void doneToggled(bool done);

    /// @brief Removing the annotation was asked for
    void deleteRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    /// @brief Kind, state, chapter, date, author and text
    void updateContent();

    /// @brief The colors of the texts: the kind's name, the chapter and date, the author and
    /// the text
    void updateTextColors();

    /// @brief The chapter's title and the author, shortened to the room they have
    void updateElidedLabels();

    /// @brief The menu button's icon, in the colors of the icons
    void updateMenuIcon();

    AnnotationEntry m_entry;
    AnnotationCardColors m_colors;
    QString m_appliedStyle;  ///< The style sheet the texts' colors are in
    QCheckBox* m_doneBox{nullptr};
    QLabel* m_kindLabel{nullptr};
    QLabel* m_chapterLabel{nullptr};
    QLabel* m_dateLabel{nullptr};
    QToolButton* m_menuButton{nullptr};
    QLabel* m_authorLabel{nullptr};
    QLabel* m_textLabel{nullptr};
    bool m_selected = false;
    bool m_listFocused = false;
};

}  // namespace kalahari::gui
