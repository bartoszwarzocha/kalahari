/// @file auto_correct_page.cpp
/// @brief Settings page: Editor > Auto-correct
///
/// The editors follow these settings by themselves (EditorPanel subscribes to them).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/command_registry.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

EditorAutoCorrectPage::EditorAutoCorrectPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* replace = addGroup(tr("Replace as You Type"));
    addCheckBox(replace,
                tr("Straight quotes with the typographic ones of the book's language: „…” and "
                   "‚…’ or “…” and ‘…’"),
                "editor.autoCorrect.quotes");
    addCheckBox(replace, tr("Apostrophe ' with ’"), "editor.autoCorrect.apostrophe");

    // The dash in the same row, as the rest of the sentence
    auto* dashRow = new QWidget();
    auto* dashLayout = new QHBoxLayout(dashRow);
    dashLayout->setContentsMargins(0, 0, 0, 0);
    auto* dialogueDash = new QCheckBox(
        tr("Hyphen and space at the start of a paragraph with a dialogue dash:"));
    auto* dash = new QComboBox();
    dash->addItem(tr("Em Dash (—)"), QStringLiteral("emDash"));
    dash->addItem(tr("En Dash (–)"), QStringLiteral("enDash"));
    dash->setAccessibleName(tr("Dialogue dash"));
    dashLayout->addWidget(dialogueDash);
    dashLayout->addWidget(dash);
    dashLayout->addStretch();
    replace->addRow(dashRow);
    bind(dialogueDash, "editor.autoCorrect.dialogueDash");
    bind(dash, "editor.autoCorrect.dialogueDashType");
    const auto syncDash = [dialogueDash, dash]() { dash->setEnabled(dialogueDash->isChecked()); };
    connect(dialogueDash, &QCheckBox::toggled, this, syncDash);
    whenLoaded(syncDash);

    addCheckBox(replace, tr("“...” with an ellipsis “…”"), "editor.autoCorrect.ellipsis");
    addNote(replace, tr("The quotation marks follow the book's language; outside the book, as "
                        "in a file opened from the disk, the program's language. A language "
                        "chosen on the Spelling page goes before both."));

    // With the keys Undo has now (the user may change them)
    QLabel* undoNote = addNote(pageLayout(), QString());
    const auto showUndoKeys = [undoNote]() {
        const Command* command = CommandRegistry::getInstance().getCommand("edit.undo");
        const QString keys = command != nullptr && !command->shortcut.isEmpty()
            ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
            : QString();
        //: %1 is the keys of Edit > Undo, as Ctrl+Z
        undoNote->setText(
            !keys.isEmpty()
                ? tr("%1 right after a replacement undoes just the replacement.").arg(keys)
                : tr("Edit > Undo right after a replacement undoes just the replacement."));
    };
    showUndoKeys();
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, undoNote,
            showUndoKeys);

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
