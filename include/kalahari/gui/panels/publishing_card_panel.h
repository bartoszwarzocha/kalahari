// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_card_panel.h
/// @brief The card For Publishers on a tab of its own: the set on top, the versions of the part
///        chosen, the text of the version written in the card and its template beside it

#ifndef KALAHARI_GUI_PANELS_PUBLISHING_CARD_PANEL_H
#define KALAHARI_GUI_PANELS_PUBLISHING_CARD_PANEL_H

#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

class QCheckBox;
class QFrame;
class QHBoxLayout;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;
class QWidget;

namespace kalahari::gui {

class FlowLayout;
class PublishingSetView;
class PublishingTextBox;

/// @brief The card For Publishers (Workshop > For Publishers, Book > For Publishers)
///
/// On top the set for a publisher as a row of tiles, one for each part in the set's order:
/// its name, its version and its length against the target; the check box (Space) puts the
/// part in the set or leaves it out, Alt+Left and Alt+Right move it, the arrows, Home and End
/// choose another part, Enter goes to its versions. Under them the versions of the part chosen:
/// a star marks the version in the set and a rejected one is struck through; Space puts the
/// version in the set (again: takes it out), Insert adds a new one, F2 renames it, Delete
/// deletes it, and the menu key the rest. Beside them the text of the version, written in the
/// card: a synopsis in the program's editor, a sentence with the fields of the pattern of its
/// template over it; a tagline and a description come from the Cover, the note about the
/// author from the back part of the book. On the right its template, its points and its
/// length (PublishingTextBox).
///
/// It is the editor of the version shown (EditorPanel, the tab's "elementId"), so spelling,
/// grammar, the Edit and Format menus work in it as in a chapter. What the card changes is
/// saved at once: the text a moment after the last key, the rest when it changes; the footer
/// says when it was saved last.
class PublishingCardPanel : public EditorPanel {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param parent Parent widget
    PublishingCardPanel(core::ProjectManager& book, CommandRegistry& commands,
                        QWidget* parent = nullptr);
    ~PublishingCardPanel() override;

    PublishingCardPanel(const PublishingCardPanel&) = delete;
    PublishingCardPanel& operator=(const PublishingCardPanel&) = delete;

    /// @brief Show part @p part of the set with its version in the set (else its first one);
    ///        the first part for an unknown one
    /// @param keys The tile of the part gets the keys
    void showPart(const QString& part, bool keys);

    /// @brief Show version @p elementId (a text for publishers, a tagline or a description of
    ///        the cover) in its part
    /// @param keys The text gets the keys
    /// @return false when it is no version of a part of the set
    bool showVersion(const QString& elementId, bool keys);

    /// @brief The set, the versions and the version shown again, as the book has them now
    void refresh();

    /// @brief Write the text of the version shown when it has changes not saved
    /// @return false when it could not be written
    bool saveNow();

    /// @brief The part chosen; empty: none
    [[nodiscard]] QString part() const { return m_part; }

    /// @brief The version shown; empty: none
    [[nodiscard]] QString versionId() const { return m_versionId; }

    /// @brief The tile of part @p part; nullptr: none
    [[nodiscard]] QFrame* tile(const QString& part) const { return m_tiles.value(part); }

    /// @brief The check box of the tile of part @p part (in the set); nullptr: none
    [[nodiscard]] QCheckBox* tileCheck(const QString& part) const;

    /// @brief The parts of the tiles, in their order
    [[nodiscard]] QStringList tileParts() const { return m_order; }

    /// @brief The versions of the part chosen
    [[nodiscard]] QListWidget* versionList() const { return m_versions; }

    /// @brief The template, the points and the length of the version shown
    [[nodiscard]] PublishingTextBox* textBox() const { return m_textBox; }

    /// @brief The field of the pattern @p fieldId of a sentence shown; nullptr: none
    [[nodiscard]] QLineEdit* field(const QString& fieldId) const { return m_fields.value(fieldId); }

    /// @brief Compose from the Fields of a sentence changed by hand
    [[nodiscard]] QPushButton* composeButton() const { return m_composeButton; }

    /// @brief The text of a tagline, a description or the note about the author
    [[nodiscard]] QLabel* outsideText() const { return m_outsideText; }

    /// @brief When the card saved last
    [[nodiscard]] QLabel* savedLabel() const { return m_savedLabel; }

    /// @brief The preview of the whole set, in place of the versions while it is on
    [[nodiscard]] PublishingSetView* preview() const { return m_preview; }

    /// @brief The command bar's button of @p name: "new", "copy", "choose", "reject",
    ///        "copyText", "earlier", "later", "copyAll", "export", "print", "preview",
    ///        "undo", "redo", "settings"; nullptr: none
    [[nodiscard]] QToolButton* commandButton(const QString& name) const {
        return m_buttons.value(name);
    }

    /// @brief Add a new version of the part chosen (Insert), with the template of the version
    ///        shown, and show it
    /// @param copy With the text and the template of the version shown (Copy Version)
    /// @return The new version; empty when none was added
    QString addVersion(bool copy);

    /// @brief Put the version shown in the set, or take it out when it is in the set (Space)
    void toggleVersionInSet();

    /// @brief Reject the version shown, or make a rejected one a draft again
    void toggleRejected();

signals:
    /// @brief The version shown is another one: @p elementId; empty: none
    void versionShown(const QString& elementId);

    /// @brief What the book keeps of version @p elementId changed in the card
    void publishingChanged(const QString& elementId);

    /// @brief The title of version @p elementId changed (F2)
    void titleChanged(const QString& elementId);

    /// @brief Delete: delete version @p elementId as the Navigator does
    void requestDelete(const QString& elementId);

    /// @brief Show in the Cover: a tagline or a description in the list of the cover
    void requestShowInCover(const QString& elementId);

    /// @brief Open: the note about the author in its own tab
    void requestOpen(const QString& elementId);

    /// @brief Export Set...
    void requestExportSet();

    /// @brief Print Set...
    void requestPrintSet();

    /// @brief The gear: Settings > Navigator > Texts for Publishers
    void settingsRequested();

    /// @brief Something to say in the status bar
    void statusMessage(const QString& text);

protected:
    /// @brief The keys of the tiles and of the versions
    bool eventFilter(QObject* watched, QEvent* event) override;
    /// @brief The text is saved when the card goes behind another tab
    void hideEvent(QHideEvent* event) override;

private:
    /// @brief The command bar, the tiles, the versions, the text, the box and the footer
    void buildCard();
    QWidget* createCommands(QWidget* parent);
    QWidget* createText(QWidget* parent);
    /// @brief A command of the bar: @p name, its icon @p iconId, its tooltip @p tip
    QToolButton* addCommand(QHBoxLayout* bar, const QString& name, const QString& iconId,
                            const QString& tip);

    /// @brief A new tile of part @p part
    QFrame* createTile(const QString& part);
    /// @brief The tiles of the parts of the set, as the book has them
    void fillTiles();
    /// @brief The versions of the part chosen, the version shown chosen
    void fillVersions();
    /// @brief The look of the tile of the part chosen, and the rest without it
    void markTiles();
    /// @brief Choose part @p part, its tile with the keys when @p keys
    void choosePart(const QString& part, bool keys);
    /// @brief Show version @p elementId of the part chosen in the text (none: its page)
    void openVersion(const QString& elementId);
    /// @brief The keys of the tile of @p part
    bool tileKey(const QString& part, const QKeyEvent& event);
    /// @brief The keys of the versions
    bool versionKey(const QKeyEvent& event);
    /// @brief Put part @p part in the set or leave it out
    void togglePart(const QString& part);
    /// @brief Move part @p part by @p step places in the set
    void movePart(const QString& part, int step);
    /// @brief The menu of the version shown at @p globalPos
    void showVersionMenu(const QPoint& globalPos);
    /// @brief Fill @p menu with the commands of the version shown
    void fillVersionMenu(QMenu& menu);
    /// @brief Copy the text of the version shown to the clipboard
    void copyText();
    /// @brief The commands of the bar enabled for the version shown
    void updateCommands();
    /// @brief The preview of the set in place of the versions and the text, or not
    void showPreview(bool shown);

    /// @brief Whether the part chosen has its versions written in the card: a sentence or a
    ///        synopsis
    [[nodiscard]] bool writtenHere() const;
    /// @brief The template of the version shown; nullopt for none
    [[nodiscard]] std::optional<core::PublishingTemplate> currentTemplate() const;
    /// @brief The fields of the pattern of a sentence shown, after its template
    void buildFields();
    /// @brief The sentence of the pattern and the fields; empty while no field has a value
    [[nodiscard]] QString composed() const;
    /// @brief Put the sentence composed of the fields in the editor
    void compose();
    /// @brief The text changed: count it, save it in a moment, a sentence changed by hand
    void onTextChanged();
    /// @brief Save @p publishing, the data of the version shown; false when it was not saved
    bool savePublishing(const core::PublishingData& publishing);
    /// @brief The note of a sentence changed by hand, after the data of the version
    void updateManualNote();
    /// @brief The footer after the last save
    void showSaved();

    core::ProjectManager& m_book;
    QString m_part;       ///< The part chosen
    QString m_versionId;  ///< The version shown
    core::PublishingData m_data;  ///< Of the version shown: what the book keeps, with the fields
    bool m_composing = false;     ///< The program puts the sentence in the editor
    bool m_loading = false;       ///< The program puts a version in the editor

    QHash<QString, QToolButton*> m_buttons;
    QWidget* m_tilesArea = nullptr;
    QHash<QString, QFrame*> m_tiles;
    QStringList m_order;          ///< The parts of the tiles
    QStackedWidget* m_body = nullptr;  ///< The versions with the text, or the preview
    QListWidget* m_versions = nullptr;
    QStackedWidget* m_textStack = nullptr;  ///< The editor, a text from elsewhere, nothing
    QWidget* m_patternArea = nullptr;
    FlowLayout* m_patternLayout = nullptr;
    QLabel* m_patternHint = nullptr;
    QHash<QString, QLineEdit*> m_fields;
    QWidget* m_manualNote = nullptr;
    QPushButton* m_composeButton = nullptr;
    QLabel* m_outsideText = nullptr;
    QLabel* m_outsideSource = nullptr;
    QPushButton* m_outsideButton = nullptr;
    QLabel* m_emptyText = nullptr;
    PublishingTextBox* m_textBox = nullptr;
    PublishingSetView* m_preview = nullptr;
    QLabel* m_savedLabel = nullptr;
    QTimer* m_saveTimer = nullptr;    ///< The text, a moment after the last key
    QTimer* m_fieldsTimer = nullptr;  ///< The fields of a sentence, a moment after typing
    QString m_savedAt;                ///< When the card saved last: "13:42"
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PUBLISHING_CARD_PANEL_H
