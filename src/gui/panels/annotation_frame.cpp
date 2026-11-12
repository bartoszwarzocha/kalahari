/// @file annotation_frame.cpp
/// @brief The frame an annotation's text is written in, at its place in the text

#include "kalahari/gui/panels/annotation_frame.h"
#include "kalahari/gui/panels/annotation_card.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTextEdit>

namespace kalahari::gui {

namespace {

constexpr int BAR_WIDTH = 4;          ///< The bar in the kind's color on the left, as on the cards
constexpr qreal CORNER_RADIUS = 8.0;  ///< The rounded corners, as on the cards

/// @brief How much of the kind's color the frame's border has
constexpr double BORDER_SHARE = 0.55;

}  // namespace

AnnotationFrame::AnnotationFrame(QWidget* editor)
    : WritingFrame(editor, new QTextEdit(), QStringLiteral("annotationFrame"))
{
    // Who the annotation is by, on the right of the header; its room stays when no one is
    // shown: the X stays in the corner
    QFont authorFont = font();
    authorFont.setPointSizeF(authorFont.pointSizeF() * 0.9);
    authorFont.setBold(true);
    m_authorLabel = new QLabel(this);
    m_authorLabel->setObjectName(QStringLiteral("annotationFrameAuthor"));
    m_authorLabel->setFont(authorFont);
    m_authorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QSizePolicy authorPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    authorPolicy.setRetainSizeWhenHidden(true);
    m_authorLabel->setSizePolicy(authorPolicy);
    m_authorLabel->hide();
    headerLayout()->insertWidget(1, m_authorLabel, 1);

    // The text is plain
    textEdit()->setAcceptRichText(false);
    textEdit()->setPlaceholderText(tr("Type the text of the annotation..."));
    connect(textEdit(), &QTextEdit::textChanged, this,
            [this]() { m_saveButton->setEnabled(canSave()); });

    // What the keys do, and the button
    hintLabel()->setText(tr("%1 to save, Esc to cancel").arg(saveKeysText()));
    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setToolTip(tr("Save (%1)").arg(saveKeysText()));
    m_saveButton->setEnabled(false);
    connect(m_saveButton, &QPushButton::clicked, this, &AnnotationFrame::requestSave);
    footerLayout()->addWidget(m_saveButton);
    addKeyWidget(m_saveButton);
}

void AnnotationFrame::setKind(editor::AnnotationKind kind) {
    setTitle(AnnotationCard::kindTitle(kind));
}

void AnnotationFrame::setAuthor(const QString& author) {
    m_author = author.trimmed();
    m_authorLabel->setVisible(!m_author.isEmpty());
    updateAuthorLabel();
}

void AnnotationFrame::setText(const QString& text) {
    textEdit()->setPlainText(text);
    m_saveButton->setEnabled(canSave());
}

QString AnnotationFrame::text() const {
    return textEdit()->toPlainText();
}

bool AnnotationFrame::canSave() const {
    return !text().trimmed().isEmpty();
}

void AnnotationFrame::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const AnnotationCardColors& shown = colors();
    const QColor background =
        shown.background.isValid() ? shown.background : palette().color(QPalette::Base);
    const QColor kind = shown.kind.isValid() ? shown.kind : palette().color(QPalette::Mid);

    // As a card of the panel: a rounded card, the bar in the kind's color on the left; a
    // border sets it off from the paper
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(card, CORNER_RADIUS, CORNER_RADIUS);
    painter.fillPath(shape, background);
    painter.save();
    painter.setClipPath(shape);
    painter.fillRect(QRectF(0, 0, BAR_WIDTH, height()), kind);
    painter.restore();
    painter.setPen(QPen(mixedColor(background, kind, BORDER_SHARE), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);
}

void AnnotationFrame::resizeEvent(QResizeEvent* event) {
    WritingFrame::resizeEvent(event);
    updateAuthorLabel();
}

void AnnotationFrame::updateAuthorLabel() {
    m_authorLabel->setText(m_authorLabel->fontMetrics().elidedText(m_author, Qt::ElideRight,
                                                                   m_authorLabel->width()));
    m_authorLabel->setToolTip(m_authorLabel->text() != m_author ? m_author : QString());
}

}  // namespace kalahari::gui
