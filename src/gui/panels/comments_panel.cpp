/// @file comments_panel.cpp
/// @brief Implementation of Comments panel (OpenSpec #00042 Task 7.9)

#include "kalahari/gui/panels/comments_panel.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kml_document.h"
#include "kalahari/editor/kml_paragraph.h"
#include "kalahari/editor/kml_comment.h"
#include "kalahari/core/logger.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace kalahari {
namespace gui {

/// @brief Custom role for storing paragraph index in list items
static constexpr int ParagraphIndexRole = Qt::UserRole + 1;

/// @brief Custom role for storing comment ID in list items
static constexpr int CommentIdRole = Qt::UserRole + 2;

CommentsPanel::CommentsPanel(QWidget* parent)
    : QWidget(parent)
    , m_commentsList(nullptr)
    , m_deleteButton(nullptr)
    , m_editButton(nullptr)
    , m_emptyLabel(nullptr)
    , m_editor(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("CommentsPanel constructor called");

    setupUI();

    logger.debug("CommentsPanel initialized");
}

void CommentsPanel::setupUI()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("CommentsPanel::setupUI()");

    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // Create empty state label
    m_emptyLabel = new QLabel(tr("No comments in document"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    mainLayout->addWidget(m_emptyLabel);

    // Create comments list
    m_commentsList = new QListWidget(this);
    m_commentsList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_commentsList->setAlternatingRowColors(true);
    m_commentsList->setWordWrap(true);
    m_commentsList->setVisible(false);

    connect(m_commentsList, &QListWidget::itemClicked,
            this, &CommentsPanel::onItemClicked);
    connect(m_commentsList, &QListWidget::itemDoubleClicked,
            this, &CommentsPanel::onItemDoubleClicked);

    mainLayout->addWidget(m_commentsList, 1);

    // Create button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);

    // Edit button
    m_editButton = new QPushButton(tr("Edit"), this);
    m_editButton->setEnabled(false);
    m_editButton->setToolTip(tr("Edit selected comment"));
    connect(m_editButton, &QPushButton::clicked,
            this, &CommentsPanel::onEditClicked);
    buttonLayout->addWidget(m_editButton);

    // Delete button
    m_deleteButton = new QPushButton(tr("Delete"), this);
    m_deleteButton->setEnabled(false);
    m_deleteButton->setToolTip(tr("Delete selected comment"));
    connect(m_deleteButton, &QPushButton::clicked,
            this, &CommentsPanel::onDeleteClicked);
    buttonLayout->addWidget(m_deleteButton);

    buttonLayout->addStretch(1);

    mainLayout->addLayout(buttonLayout);

    // Connect list selection change to button state
    connect(m_commentsList, &QListWidget::itemSelectionChanged, this, [this]() {
        bool hasSelection = !m_commentsList->selectedItems().isEmpty();
        m_editButton->setEnabled(hasSelection);
        m_deleteButton->setEnabled(hasSelection);
    });

    setLayout(mainLayout);
}

void CommentsPanel::setEditor(editor::BookEditor* editor)
{
    if (m_editor == editor) {
        return;
    }

    // Disconnect from old editor
    disconnectFromEditor();

    m_editor = editor;

    // Connect to new editor
    connectToEditor();

    // Refresh the list
    refresh();
}

void CommentsPanel::connectToEditor()
{
    if (m_editor == nullptr) {
        return;
    }

    // Connect to comment signals
    connect(m_editor, &editor::BookEditor::commentAdded,
            this, &CommentsPanel::onCommentAdded);
    connect(m_editor, &editor::BookEditor::commentRemoved,
            this, &CommentsPanel::onCommentRemoved);

    // Refresh when document changes
    connect(m_editor, &editor::BookEditor::documentChanged,
            this, &CommentsPanel::refresh);
}

void CommentsPanel::disconnectFromEditor()
{
    if (m_editor == nullptr) {
        return;
    }

    disconnect(m_editor, &editor::BookEditor::commentAdded,
               this, &CommentsPanel::onCommentAdded);
    disconnect(m_editor, &editor::BookEditor::commentRemoved,
               this, &CommentsPanel::onCommentRemoved);
    disconnect(m_editor, &editor::BookEditor::documentChanged,
               this, &CommentsPanel::refresh);

    m_editor = nullptr;
}

void CommentsPanel::refresh()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("CommentsPanel::refresh()");

    m_commentsList->clear();

    if (m_editor == nullptr) {
        clear();
        return;
    }

    editor::KmlDocument* doc = m_editor->document();
    if (doc == nullptr) {
        clear();
        return;
    }

    // Collect all comments from all paragraphs
    int totalComments = 0;
    for (int paraIdx = 0; paraIdx < doc->paragraphCount(); ++paraIdx) {
        const editor::KmlParagraph* para = doc->paragraph(paraIdx);
        if (para == nullptr || !para->hasComments()) {
            continue;
        }

        QString paraText = para->plainText();
        const QList<editor::KmlComment>& comments = para->comments();

        for (const editor::KmlComment& comment : comments) {
            // Create list item
            QString displayText = formatCommentDisplay(comment, paraText);
            QListWidgetItem* item = new QListWidgetItem(displayText, m_commentsList);

            // Store paragraph index and comment ID
            item->setData(ParagraphIndexRole, paraIdx);
            item->setData(CommentIdRole, comment.id());

            // Set tooltip with full comment text
            QString tooltip = tr("Comment: %1\n\nOn text: \"%2\"")
                .arg(comment.text())
                .arg(paraText.mid(comment.startPos(),
                                  comment.endPos() - comment.startPos()));
            item->setToolTip(tooltip);

            ++totalComments;
        }
    }

    // Update visibility based on comment count
    bool hasComments = (totalComments > 0);
    m_emptyLabel->setVisible(!hasComments);
    m_commentsList->setVisible(hasComments);

    logger.debug("CommentsPanel: Loaded {} comments", totalComments);
}

void CommentsPanel::clear()
{
    m_commentsList->clear();
    m_emptyLabel->setVisible(true);
    m_commentsList->setVisible(false);
    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
}

QString CommentsPanel::formatCommentDisplay(const editor::KmlComment& comment,
                                             const QString& paragraphText) const
{
    // Extract the commented text range
    QString commentedText = paragraphText.mid(
        comment.startPos(),
        qMin(comment.endPos() - comment.startPos(), 30)
    );

    // Truncate if too long
    if (comment.endPos() - comment.startPos() > 30) {
        commentedText += QStringLiteral("...");
    }

    // Format: "Comment text" on "commented text"
    QString displayComment = comment.text();
    if (displayComment.length() > 50) {
        displayComment = displayComment.left(47) + QStringLiteral("...");
    }

    return tr("\"%1\"\non: \"%2\"").arg(displayComment, commentedText);
}

bool CommentsPanel::getSelectedComment(int& paragraphIndex, QString& commentId) const
{
    QList<QListWidgetItem*> selected = m_commentsList->selectedItems();
    if (selected.isEmpty()) {
        return false;
    }

    QListWidgetItem* item = selected.first();
    paragraphIndex = item->data(ParagraphIndexRole).toInt();
    commentId = item->data(CommentIdRole).toString();
    return true;
}

void CommentsPanel::onItemClicked(QListWidgetItem* item)
{
    if (item == nullptr) {
        return;
    }

    int paragraphIndex = item->data(ParagraphIndexRole).toInt();
    QString commentId = item->data(CommentIdRole).toString();

    emit commentClicked(paragraphIndex, commentId);
}

void CommentsPanel::onItemDoubleClicked(QListWidgetItem* item)
{
    if (item == nullptr) {
        return;
    }

    int paragraphIndex = item->data(ParagraphIndexRole).toInt();
    QString commentId = item->data(CommentIdRole).toString();

    // Double-click opens edit dialog
    emit editRequested(paragraphIndex, commentId);
}

void CommentsPanel::onDeleteClicked()
{
    int paragraphIndex;
    QString commentId;

    if (getSelectedComment(paragraphIndex, commentId)) {
        emit deleteRequested(paragraphIndex, commentId);
    }
}

void CommentsPanel::onEditClicked()
{
    int paragraphIndex;
    QString commentId;

    if (getSelectedComment(paragraphIndex, commentId)) {
        emit editRequested(paragraphIndex, commentId);
    }
}

void CommentsPanel::onCommentAdded(int paragraphIndex)
{
    Q_UNUSED(paragraphIndex);
    // Just refresh the entire list for simplicity
    refresh();
}

void CommentsPanel::onCommentRemoved(int paragraphIndex, const QString& commentId)
{
    Q_UNUSED(paragraphIndex);
    Q_UNUSED(commentId);
    // Just refresh the entire list for simplicity
    refresh();
}

} // namespace gui
} // namespace kalahari
