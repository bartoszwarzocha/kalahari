/// @file auto_correct_page.cpp
/// @brief Settings page: Editor > Auto-correct
///
/// The editors follow these settings by themselves (EditorPanel subscribes to them).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/command_registry.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

EditorAutoCorrectPage::EditorAutoCorrectPage(QWidget* parent)
    : SettingsPage(parent)
{
    // Short check boxes, one below another, and the details in a note that wraps: the page
    // fits a narrow window, as the other pages do, without scrolling sideways
    QFormLayout* replace = addGroup(tr("Replace as You Type"));
    addCheckBox(replace, tr("Straight quotes with typographic ones"), "editor.autoCorrect.quotes");
    addCheckBox(replace, tr("Apostrophe ' with ’"), "editor.autoCorrect.apostrophe");
    QCheckBox* dialogueDash = addCheckBox(
        replace, tr("Hyphen at the start of a paragraph with a dash"), "editor.autoCorrect.dialogueDash");

    // The dash, chosen only for the hyphen changed
    auto* dash = new QComboBox();
    dash->addItem(tr("Em Dash (—)"), QStringLiteral("emDash"));
    dash->addItem(tr("En Dash (–)"), QStringLiteral("enDash"));
    QLabel* dashLabel =
        addField(replace, tr("Dialogue dash:"), dash, "editor.autoCorrect.dialogueDashType");
    const auto syncDash = [dialogueDash, dash, dashLabel]() {
        dash->setEnabled(dialogueDash->isChecked());
        dashLabel->setEnabled(dialogueDash->isChecked());
    };
    connect(dialogueDash, &QCheckBox::toggled, this, syncDash);
    whenLoaded(syncDash);

    addCheckBox(replace, tr("“...” with an ellipsis “…”"), "editor.autoCorrect.ellipsis");
    addNote(replace, tr("The quotation marks follow the language of the book: „…” and ‚…’ in "
                        "Polish, “…” and ‘…’ in English. Outside the book, as in a file opened "
                        "from the disk, they follow the language of the program. A language "
                        "chosen on the Spelling page goes before both. The hyphen becomes a dash "
                        "when a space is typed after it."));

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
