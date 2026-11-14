// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_frame.h
/// @brief A frame over the editor a text is written in: an annotation's, a note's

#pragma once

#include <kalahari/gui/panels/annotation_colors.h>

#include <QFrame>
#include <QList>
#include <QRectF>

#include <functional>

class QHBoxLayout;
class QLabel;
class QTextEdit;
class QToolButton;

namespace kalahari::gui {

class CommandRegistry;

/// @brief A frame over the editor a text is written in, at its place
///
/// It lies over the editor, under the place it is for (above it when there is no room below),
/// within the text column; or over a card of the text, as wide as it: the card makes room for
/// the frame as high as it is (cardHeightNeeded()), or the frame lies over it within the
/// editor. Its header says what is written and has an X in the corner; its footer says what
/// the keys do. Ctrl+Enter keeps the text; Esc or the X drops it. Going anywhere else in the
/// program closes it, the text kept: a click outside it, or the keys going to another place of
/// the program. It stays open while another application is in front, for a menu or a list
/// over it (its own context menu) and for the editor's scroll bars (it goes along with its
/// place; the keys stay in it). While it has the keys, the window's shortcuts do nothing
/// (bold, the commands of the text... would change the text under it), except saving, closing
/// and quitting; no key and no click goes through it to the editor.
///
/// The frames of the annotations (AnnotationFrame) and of the notes (NoteFrame) are made of it.
class WritingFrame : public QFrame {
    Q_OBJECT

public:
    /// @brief Where the frame goes: its place in the text and the room it has, in the
    /// coordinates of the editor it lies over
    struct Placement {
        QRectF place;   ///< The place it is for: a caret's rectangle
        QRectF column;  ///< The text column (empty: the whole editor)
        QRectF card;    ///< The card it goes over (empty: none; the frame goes to its place)
        /// The card makes room for the frame (cardHeightNeeded()); false: the frame lies over
        /// it, within the editor
        bool cardMakesRoom = true;
    };

    /// @brief Gives the placement as it is now (the view scrolls and changes)
    using PlacementProvider = std::function<Placement()>;

    /// @brief Destructor: the program is not watched any more
    ~WritingFrame() override;

    /// @brief The keys that keep the text, as the frame shows them ("Ctrl+Enter")
    static QString saveKeysText();

    /// @brief Whether the key is the command's shortcut, as the program has it now (also one
    ///        changed in Settings > Keyboard Shortcuts)
    [[nodiscard]] bool hasCommandKeys(const QKeyEvent* event, const char* commandId) const;

    /// @brief What is written: the frame's header, and the name of its text for the readers
    ///        of the screen
    void setTitle(const QString& title);

    /// @brief Whether the text can be kept (Ctrl+Enter does nothing otherwise)
    virtual bool canSave() const { return true; }

    /// @brief The frame's colors
    void setColors(const AnnotationCardColors& colors);

    /// @brief The colors it is shown in
    const AnnotationCardColors& colors() const { return m_colors; }

    /// @brief Follow a place of the text: the frame goes to it again whenever the editor is
    /// painted (scrolled, zoomed, edited) or resized
    void setPlacement(PlacementProvider provider);

    /// @brief Put the frame where its placement says now
    void place();

    /// @brief Give the keys to the text, with the cursor at its end
    void startTyping();

signals:
    /// @brief Ctrl+Enter: keep the text
    void saveRequested();

    /// @brief Esc or its X: drop the text
    void cancelRequested();

    /// @brief Going anywhere else in the program: close it, the text kept. Asked once, and
    /// never after Esc or its X
    void closeRequested();

    /// @brief Lying over a card that makes room for it (Placement::card), the frame is of
    /// another height than the card
    /// @param height The frame's height, in pixels of the editor
    void cardHeightNeeded(int height);

protected:
    /// @brief Constructor
    /// @param commands The commands of its window
    /// @param editor The editor it lies over (its parent)
    /// @param textEdit Where the text is written (the frame takes it)
    /// @param name The start of the names of the frame and its parts: "annotationFrame" names
    ///        "annotationFrameKind" (the header), "annotationFrameHint", "annotationFrameClose"
    WritingFrame(CommandRegistry& commands, QWidget* editor, QTextEdit* textEdit,
                 const QString& name);

    /// @brief Where the text is written
    QTextEdit* textEdit() const { return m_textEdit; }

    /// @brief The header: the title, then the X; a frame puts its own parts between them
    QHBoxLayout* headerLayout() const { return m_header; }

    /// @brief The footer: what the keys do, then the frame's own parts
    QHBoxLayout* footerLayout() const { return m_footer; }

    /// @brief What the keys do, in the footer
    QLabel* hintLabel() const { return m_hintLabel; }

    /// @brief The start of the names of the frame's parts
    const QString& partName() const { return m_name; }

    /// @brief A part of the frame Tab goes to after the text, in the order they are added (a
    ///        button: Enter presses it)
    void addKeyWidget(QWidget* widget);

    /// @brief Ctrl+Enter: keep the text, when it can be kept
    void requestSave();

    /// @brief Esc or its X: drop the text
    void requestCancel();

    /// @brief Show a window of the frame's own (a dialog over it): while it is open the
    ///        frame stays, and the keys come back to the text after it
    void showOwnWindow(const std::function<void()>& show);

    /// @brief A key pressed in the text, before the text has it
    /// @return true: the frame did what it does
    virtual bool textKeyPressed(QKeyEvent* event);


    /// @brief The style sheet of the frame's parts in these colors
    virtual QString styleSheetFor(const AnnotationCardColors& colors) const;

    /// @brief The text box as high as its text, between a few lines and a limit
    void updateTextHeight();

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    /// @brief Close the frame, the text kept (once)
    void requestClose();

    /// @brief Watch the whole program for the writer going elsewhere (while it is shown)
    void startWatching();

    /// @brief Stop watching the program
    void stopWatching();

    /// @brief A mouse press anywhere in the program, before it goes on
    void onPress(QObject* receiver);

    /// @brief The keys went from one widget of the program to another (or to none)
    void onFocusChanged(QWidget* old, QWidget* now);

    /// @brief After the click or the wheel that took the keys from the frame: unless the
    /// click gave them back, they went elsewhere
    void checkKeys();

    /// @brief Whether the widget is the frame or in it
    bool isFrameWidget(const QWidget* widget) const;

    /// @brief Whether a click on this widget, or the keys going to it, leave the frame
    bool leavesFrame(const QWidget* widget) const;

    /// @brief The colors of the texts and the buttons
    void updateStyle();

    /// @brief Ask for a place() once the editor's event is over
    void schedulePlacement();

    QString m_name;
    AnnotationCardColors m_colors;
    PlacementProvider m_placement;
    bool m_placementPending = false;
    QHBoxLayout* m_header{nullptr};
    QHBoxLayout* m_footer{nullptr};
    QLabel* m_titleLabel{nullptr};
    QTextEdit* m_textEdit{nullptr};
    QLabel* m_hintLabel{nullptr};
    QToolButton* m_closeButton{nullptr};
    QList<QWidget*> m_keyWidgets;      ///< Tab goes to them after the text
    QObject* m_pressWatcher{nullptr};  ///< Tells of the presses of the whole program
    bool m_watching = false;           ///< The program is watched (the frame is shown)
    bool m_closing = false;            ///< It asked to be closed or dropped: it asks no more
    bool m_keysAway = false;           ///< The keys went from it to another application
    bool m_keysLeftByMouse = false;    ///< The keys are leaving it with a click or the wheel
    CommandRegistry& m_commands;  ///< The commands of its window
};

}  // namespace kalahari::gui
