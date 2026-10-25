/// @file version_history_dialog.h
/// @brief The window of File > Version History...: the versions of a text, to read, compare,
/// bring back or take a fragment from

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <kalahari/core/chapter_versions.h>
#include <kalahari/core/kind_words.h>
#include <kalahari/editor/version_comparison.h>

#include <QHash>
#include <QList>
#include <QString>

#include <functional>
#include <optional>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QListWidget;
class QToolButton;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief The versions of a text, newest first, and the text of the one chosen
///
/// The list gives each version its description, or where it comes from, with when it was kept
/// and its words; the versions the program kept by itself can be hidden. The preview shows the
/// text of the version in the look of the text's editor, read-only: its text, or its changes
/// to the text as it is now (editor::compareVersions()), word by word, with Alt+Down and Alt+Up
/// going from change to change. Its text can be selected and copied (Ctrl+A, Ctrl+C).
///
/// Restore This Version and Insert Selected Fragment close the window with the writer's
/// choice (choice()): whoever opened it does it in the text's editor. Change Description (F2)
/// and Delete (Delete, after a question) change the versions at once, as Save the Current Text
/// as a Version does. In a book open read-only only reading, comparing and copying work: the
/// other buttons are greyed out and say why. While versions are turned off only Save the
/// Current Text as a Version is.
///
/// Close is the default button: Enter brings nothing back by mistake.
class VersionHistoryDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the writer chose to do with a version
    enum class Choice {
        None,     ///< Nothing: the window was closed
        Restore,  ///< Bring the version back in place of the text (chosenKml(): its text)
        Insert    ///< Insert a fragment of it at the cursor (chosenKml(): the fragment)
    };

    /// @brief The text whose versions the window shows
    struct Text {
        QString elementId;      ///< Its element
        QString title;          ///< Its title
        QString bookTitle;      ///< The title of its book
        core::KindWords kind;   ///< The words of its kind ("chapter")
        QString kml;            ///< The text as it is in the editor now
        const editor::BookEditor* editor = nullptr;  ///< Its editor: the look of the preview
        bool readOnly = false;  ///< The book is open read-only
        QString whyNotSave;     ///< Why Save the Current Text as a Version is greyed out (the
                                ///< book is read-only, versions are turned off); empty: it is not
    };

    /// @brief Keep the text as it is now as a version, as Save Version does, over @p parent
    using SaveVersion = std::function<void(QWidget* parent)>;

    /// @brief The window of the versions of @p text
    /// @param text The text
    /// @param saveVersion What Save the Current Text as a Version does
    /// @param parent Parent widget
    VersionHistoryDialog(Text text, SaveVersion saveVersion, QWidget* parent = nullptr);

    /// @brief What the writer chose; None while the window is open
    Choice choice() const { return m_choice; }

    /// @brief The version chosen (choice())
    const core::ChapterVersion& chosenVersion() const { return m_chosen; }

    /// @brief The text chosen: the whole version to restore, or the fragment to insert
    const QString& chosenKml() const { return m_chosenKml; }

    // Its parts, which the tests use as the writer does

    /// @brief The list of the versions
    QListWidget* versionList() const { return m_list; }

    /// @brief The preview of the version chosen
    editor::BookEditor* preview() const { return m_preview; }

    /// @brief "Show the versions saved by the program"
    QCheckBox* automaticBox() const { return m_automaticBox; }

    /// @brief Whether the preview shows the changes to the text as it is now, else the text of
    /// the version
    bool showsChanges() const { return m_showsChanges; }

    /// @brief Show the changes (true) or the text of the version (false)
    void showChanges(bool changes);

    /// @brief Choose the version kept in .kml file @p versionFile
    /// @return Whether the list shows it
    bool selectVersion(const QString& versionFile);

    /// @brief The version chosen in the list; nullopt: none
    std::optional<core::ChapterVersion> selectedVersion() const;

    /// @brief The line over the preview: the words, or which change of how many
    QString summary() const;

    /// @brief The change the preview shows (from 0); -1: none
    int currentChange() const { return m_change; }

    /// @brief Go to the next change (@p next) or the previous one, from the last to the first
    /// and back
    void goToChange(bool next);

    /// @brief Read the versions again and show them, @p versionFile chosen (empty: the one
    /// chosen now, else the newest)
    void reload(const QString& versionFile = QString());

    /// @brief Restore This Version, Insert Selected Fragment, Change Description and Delete:
    /// the buttons, for the tests
    QPushButton* restoreButton() const { return m_restoreButton; }
    QPushButton* insertButton() const { return m_insertButton; }
    QPushButton* renameButton() const { return m_renameButton; }
    QPushButton* deleteButton() const { return m_deleteButton; }
    QPushButton* saveButton() const { return m_saveButton; }

private:
    /// @brief List the versions that the list shows now (automaticBox())
    void fillList(const QString& versionFile);

    /// @brief Show the version chosen in the preview, with the line over it
    void showSelected();

    /// @brief The buttons for the version chosen and the selection of the preview
    void updateButtons();

    /// @brief The look of the preview: that of the text's editor, without the views that move
    /// the text (typewriter scrolling, focus)
    void setUpPreview();

    /// @brief The text of version @p version (cached); nullopt when its file cannot be read
    std::optional<QString> versionKml(const core::ChapterVersion& version);

    /// @brief The comparison of @p version with the text as it is now (cached)
    const editor::VersionComparison* comparisonOf(const core::ChapterVersion& version);

    /// @brief The colors of the comparison on the paper of the preview
    editor::ComparisonColors comparisonColors() const;

    /// @brief Show change @p index of the comparison shown: the cursor at its start, in view,
    /// the change marked in its paragraph, and which change of how many over the preview
    void showChange(int index);

    /// @brief The text selected in the preview as a fragment of the version (KML); empty: none
    QString selectedFragment() const;

    /// @brief Copy the selection of the preview with its formatting: in the changes, the text of
    /// the version (as Insert Selected Fragment takes it), else what the preview shows
    void copySelected();

    void renameSelected();
    void deleteSelected();
    void restoreSelected();
    void insertSelected();

    Text m_text;
    SaveVersion m_saveVersion;
    QList<core::ChapterVersion> m_versions;  ///< All of them, newest first
    QHash<QString, QString> m_kml;           ///< By their files
    QHash<QString, editor::VersionComparison> m_comparisons;  ///< By their files

    QListWidget* m_list = nullptr;
    QCheckBox* m_automaticBox = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_textButton = nullptr;
    QPushButton* m_changesButton = nullptr;
    QButtonGroup* m_modes = nullptr;
    QToolButton* m_previousButton = nullptr;
    QToolButton* m_nextButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_legendLabel = nullptr;
    editor::BookEditor* m_preview = nullptr;
    QPushButton* m_renameButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_insertButton = nullptr;
    QPushButton* m_restoreButton = nullptr;

    bool m_showsChanges = true;
    int m_change = -1;
    QString m_shownFile;  ///< The version in the preview
    Choice m_choice = Choice::None;
    core::ChapterVersion m_chosen;
    QString m_chosenKml;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
