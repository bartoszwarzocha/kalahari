/// @file annotations_page.cpp
/// @brief Settings page: Annotations
///
/// The comments, to-dos and notes: who new ones are by and how large their marks in the
/// text are (the editors follow the size by themselves: EditorPanel subscribes to it).

#include "kalahari/gui/settings/settings_pages.h"

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

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
