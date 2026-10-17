/// @file annotations_page.cpp
/// @brief Settings page: Annotations
///
/// The comments, to-dos and notes: who new ones are by, how large their marks in the text are
/// (the editors follow the size by themselves: EditorPanel subscribes to it) and their colors
/// (they reach the editors and the panel through the theme: ThemeManager follows them).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/editor/annotation.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

AnnotationsPage::AnnotationsPage(QWidget* parent)
    : SettingsPage(parent)
{
    // New ones only: an annotation keeps the author it was written by
    QFormLayout* added = addGroup(tr("New Annotations"));
    auto* author = new QLineEdit();
    author->setObjectName(QStringLiteral("annotationsAuthor"));
    author->setPlaceholderText(tr("Your name or pen name"));
    addField(added, tr("Author:"), author, "annotations.author");
    addNote(added, tr("The name shown on new annotations. Without it, they have no author."));

    QFormLayout* marks = addGroup(tr("Marks in the Text"));
    auto* markSize = new QSpinBox();
    markSize->setObjectName(QStringLiteral("annotationsMarkSize"));
    markSize->setRange(50, 300);
    markSize->setSingleStep(10);
    markSize->setSuffix(tr(" %"));
    markSize->setToolTip(tr("Size of the marks of comments, to-dos and notes in the text "
                            "(100% suits the text's font)"));
    addField(marks, tr("Mark Size:"), markSize, "editor.annotationMarkSize");

    // Each kind's color on the light and on the dark paper
    QFormLayout* colors = addGroup(tr("Colors"));
    addNote(colors, tr("The colors of the annotations' marks in the text, of the frame an "
                       "annotation is written in on the paper and of the cards in the Annotations "
                       "panel. A light panel has the colors of the light paper, a dark one those "
                       "of the dark paper (View > Dark Paper)."));
    using editor::AnnotationKind;
    addThemeColor(colors, tr("Comment on light paper"),
                  editor::annotationColorKey(AnnotationKind::Comment, false));
    addThemeColor(colors, tr("Comment on dark paper"),
                  editor::annotationColorKey(AnnotationKind::Comment, true));
    addThemeColor(colors, tr("To-do on light paper"),
                  editor::annotationColorKey(AnnotationKind::Todo, false));
    addThemeColor(colors, tr("To-do on dark paper"),
                  editor::annotationColorKey(AnnotationKind::Todo, true));
    addThemeColor(colors, tr("Note on light paper"),
                  editor::annotationColorKey(AnnotationKind::Note, false));
    addThemeColor(colors, tr("Note on dark paper"),
                  editor::annotationColorKey(AnnotationKind::Note, true));
    addRestoreColorsButton(colors);

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
