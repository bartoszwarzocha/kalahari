/// @file image_filter_coordinator.h
/// @brief Image > Filter > Same Filter for All Images...: the filter of one image for every
///        image of the book, with Undo

#pragma once

#include "kalahari/editor/text_images.h"

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class QStatusBar;
class QTabWidget;
class QWidget;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class EditorPanel;

/// @brief Gives every image of the book the filter of one image
///
/// The filter is the look the Filter menu gives (Original, Grayscale, Sepia, Black and White,
/// Washout) and, with Black and White, its threshold; the brightness, the contrast and the
/// transparency of each image stay its own. The window asks first how many images change.
/// The open texts change in their editors, each as one step of its undo history; the texts
/// not open change in their files, unless they have changes not saved yet (then the program
/// keeps the new text with them). A file outside the book changes alone.
///
/// The notice under the text says how many images changed, with Undo: the open texts undo
/// their step, the others get their text back. A later change of a text it changed, or
/// another book, makes it final.
class ImageFilterCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Says something under the text of a tab, with a button
    /// (DocumentCoordinator::showNotice())
    using NoticeShower = std::function<void(EditorPanel* editor, const QString& message,
                                            const QString& actionText,
                                            const QString& actionToolTip,
                                            std::function<void()> action)>;

    /// @brief Answers whether to change @p count images to @p filterName (the window asks)
    using Confirmer = std::function<bool(int count, const QString& filterName)>;

    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param showNotice Says under the text how many images changed, with Undo (none: the
    ///        status bar says it, without Undo)
    /// @param statusBar Where short messages go (may be null)
    /// @param window Over which the window asking opens
    /// @param parent Parent object
    ImageFilterCoordinator(QTabWidget* centralTabs, NoticeShower showNotice, QStatusBar* statusBar,
                           QWidget* window, QObject* parent = nullptr);

    ImageFilterCoordinator(const ImageFilterCoordinator&) = delete;
    ImageFilterCoordinator& operator=(const ImageFilterCoordinator&) = delete;

    /// @brief The filter of the image at @p position of @p text for every image of the book
    /// @return How many images changed; 0 when none did or the writer said no; -1 when the
    ///         image is not there or the text cannot change
    int applyToBook(editor::BookEditor& text, int position);

    /// @brief Whether the last change can still be undone
    [[nodiscard]] bool canUndo() const;

    /// @brief Undo the last change
    bool undo();

    /// @brief Replace the window that asks first (for the tests)
    void setConfirmer(Confirmer confirm) { m_confirm = std::move(confirm); }

    /// @brief The name of a filter as the windows say it: "Sepia", "Black and White"
    [[nodiscard]] static QString filterName(editor::ImageFilter filter);

private:
    /// @brief A text not open that a change changed
    struct SavedText {
        QString elementId;
        QString kml;         ///< Its text before the change
        bool saved = false;  ///< The change was saved to its file (it had no other changes)
    };

    /// @brief What a change did, so that it can be undone
    struct Step {
        QString filterName;
        QList<SavedText> savedTexts;
        /// The open texts it changed; one of them changed again or closed: the step is
        /// forgotten (forgetUndo()), so these are never left behind
        QList<editor::BookEditor*> editors;
    };

    /// @brief The editors of the open texts, by element (a text outside the book: empty)
    [[nodiscard]] QList<std::pair<QString, editor::BookEditor*>> openEditors() const;

    /// @brief The panel of an editor (null without one)
    [[nodiscard]] EditorPanel* panelOf(const editor::BookEditor& text) const;

    /// @brief The window that asks first
    bool confirm(int count, const QString& filterName) const;

    void forgetUndo();
    void say(const QString& message) const;

    QTabWidget* m_centralTabs;
    NoticeShower m_showNotice;
    QStatusBar* m_statusBar;
    QWidget* m_window;
    Confirmer m_confirm;
    std::optional<Step> m_undo;
    QList<QMetaObject::Connection> m_undoWatch;
};

}  // namespace kalahari::gui
