/// @file annotation_card.cpp
/// @brief A card of the Annotations panel: one comment, to-do or note

#include "kalahari/gui/panels/annotation_card.h"
#include "kalahari/core/art_provider.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QVBoxLayout>

namespace kalahari::gui {

namespace {

constexpr int BAR_WIDTH = 4;            ///< The bar in the kind's color on the left
constexpr qreal CORNER_RADIUS = 8.0;    ///< The rounded corners of the card
constexpr int OUTLINE_WIDTH = 2;        ///< The outline of the selected card
constexpr int QUIET_OUTLINE_WIDTH = 1;  ///< Its outline while the keys are elsewhere

/// @brief Show a text in a label, shortened to the room the label has
void setElidedText(QLabel* label, const QString& text) {
    label->setText(label->fontMetrics().elidedText(text, Qt::ElideRight, label->width()));
}

}  // namespace

AnnotationCard::AnnotationCard(QWidget* parent)
    : QFrame(parent)
{
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(BAR_WIDTH + 8, 6, 8, 7);
    layout->setSpacing(2);

    // Header: [check box] kind, chapter ... date [menu]
    auto* header = new QHBoxLayout();
    header->setSpacing(6);

    m_doneBox = new QCheckBox(this);
    m_doneBox->setAccessibleName(tr("Done"));
    m_doneBox->setFocusPolicy(Qt::NoFocus);
    connect(m_doneBox, &QCheckBox::toggled, this, &AnnotationCard::doneToggled);
    header->addWidget(m_doneBox);

    QFont headerFont = font();
    headerFont.setPointSizeF(headerFont.pointSizeF() * 0.9);
    QFont kindFont = headerFont;
    kindFont.setBold(true);

    m_kindLabel = new QLabel(this);
    m_kindLabel->setObjectName(QStringLiteral("annotationKind"));
    m_kindLabel->setFont(kindFont);
    header->addWidget(m_kindLabel);

    m_chapterLabel = new QLabel(this);
    m_chapterLabel->setObjectName(QStringLiteral("annotationMeta"));
    m_chapterLabel->setFont(headerFont);
    m_chapterLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    header->addWidget(m_chapterLabel, 1);

    m_dateLabel = new QLabel(this);
    m_dateLabel->setObjectName(QStringLiteral("annotationMeta"));
    m_dateLabel->setFont(headerFont);
    header->addWidget(m_dateLabel);

    m_menuButton = new QToolButton(this);
    m_menuButton->setAutoRaise(true);
    m_menuButton->setFocusPolicy(Qt::NoFocus);
    m_menuButton->setToolTip(tr("More"));
    m_menuButton->setAccessibleName(tr("More"));
    updateMenuIcon();
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &AnnotationCard::updateMenuIcon);
    connect(m_menuButton, &QToolButton::clicked, this, &AnnotationCard::showMenu);
    header->addWidget(m_menuButton);

    layout->addLayout(header);

    // Who made it, above the text, as comments in a word processor show it
    m_authorLabel = new QLabel(this);
    m_authorLabel->setObjectName(QStringLiteral("annotationAuthor"));
    m_authorLabel->setFont(kindFont);
    m_authorLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_authorLabel);

    // The text, as it was written
    m_textLabel = new QLabel(this);
    m_textLabel->setObjectName(QStringLiteral("annotationText"));
    m_textLabel->setTextFormat(Qt::PlainText);
    m_textLabel->setWordWrap(true);
    layout->addWidget(m_textLabel);
}

QString AnnotationCard::kindTitle(editor::AnnotationKind kind, bool done) {
    switch (kind) {
    case editor::AnnotationKind::Comment:
        return done ? tr("Comment (resolved)") : tr("Comment");
    case editor::AnnotationKind::Todo:
        return tr("To do");
    case editor::AnnotationKind::Note:
        return tr("Note");
    }
    return tr("Comment");
}

void AnnotationCard::updateMenuIcon() {
    const QIcon menuIcon = core::ArtProvider::getInstance().getIcon("common.moreHoriz");
    m_menuButton->setIcon(menuIcon);
    m_menuButton->setText(menuIcon.isNull() ? QStringLiteral("...") : QString());
}

void AnnotationCard::setEntry(const AnnotationEntry& entry) {
    if (entry == m_entry && !m_kindLabel->text().isEmpty()) {
        return;
    }
    m_entry = entry;
    updateContent();
}

void AnnotationCard::setColors(const AnnotationCardColors& colors) {
    if (colors == m_colors) {
        return;
    }
    m_colors = colors;
    updateTextColors();
    update();
}

QColor AnnotationCard::textColor() const {
    const bool dimmed = m_entry.annotation.done || m_entry.annotation.text.trimmed().isEmpty();
    return dimmed ? m_colors.secondary : m_colors.text;
}

void AnnotationCard::setSelected(bool selected) {
    if (selected != m_selected) {
        m_selected = selected;
        update();
    }
}

void AnnotationCard::setListFocused(bool focused) {
    if (focused != m_listFocused) {
        m_listFocused = focused;
        update();
    }
}

QString AnnotationCard::text() const {
    return m_textLabel->text();
}

void AnnotationCard::updateContent() {
    const editor::Annotation& annotation = m_entry.annotation;
    m_kindLabel->setText(kindTitle(annotation.kind, annotation.done));

    // A to-do is checked off in the card
    const bool todo = annotation.kind == editor::AnnotationKind::Todo;
    m_doneBox->setVisible(todo);
    {
        const QSignalBlocker blocker(m_doneBox);
        m_doneBox->setChecked(todo && annotation.done);
    }

    // Day and month (with the year when it is not this one). The tooltip says who made it
    // and when, and the chapter's whole title: the same wherever the card is pointed at
    const QString author = annotation.author.trimmed();
    QString date;
    QString made = author;
    if (annotation.created.isValid()) {
        const QDateTime local = annotation.created.toLocalTime();
        const QLocale locale;
        const bool thisYear = local.date().year() == QDate::currentDate().year();
        date = locale.toString(local.date(), thisYear ? tr("dd.MM") : tr("dd.MM.yyyy"));
        const QString when = locale.toString(local, QLocale::ShortFormat);
        made = made.isEmpty() ? when : tr("%1, %2").arg(made, when);
    }
    m_dateLabel->setText(date);
    QStringList toolTip;
    if (!made.isEmpty()) {
        toolTip << made;
    }
    if (!m_entry.chapterTitle.isEmpty()) {
        toolTip << m_entry.chapterTitle;
    }
    setToolTip(toolTip.join(QLatin1Char('\n')));

    // The author's line only when there is one
    m_authorLabel->setVisible(!author.isEmpty());

    // One without text says so
    m_textLabel->setText(annotation.text.trimmed().isEmpty() ? tr("(no text)") : annotation.text);

    updateTextColors();
    updateElidedLabels();
}

void AnnotationCard::updateTextColors() {
    if (!m_colors.text.isValid()) {
        return;  // no colors yet: those of the palette
    }

    // In a style sheet, the colors stay through every polish of the panel's style
    const QString style = QStringLiteral("QLabel#annotationKind { color: %1; }"
                                         "QLabel#annotationMeta { color: %2; }"
                                         "QLabel#annotationAuthor, QLabel#annotationText"
                                         " { color: %3; }")
                              .arg(m_colors.kindName.name(), m_colors.secondary.name(),
                                   textColor().name());
    if (style != m_appliedStyle) {
        m_appliedStyle = style;
        setStyleSheet(style);
    }
}

void AnnotationCard::updateElidedLabels() {
    setElidedText(m_chapterLabel, m_entry.chapterTitle);
    setElidedText(m_authorLabel, m_entry.annotation.author.trimmed());
}

void AnnotationCard::showMenu() {
    const editor::Annotation& annotation = m_entry.annotation;
    QMenu menu(this);

    // The keys are shown only: the panel handles them
    QAction* editAction = menu.addAction(tr("Edit"), this, &AnnotationCard::editRequested);
    editAction->setShortcut(QKeySequence(Qt::Key_F2));

    QString doneText;
    if (annotation.kind == editor::AnnotationKind::Todo) {
        doneText = annotation.done ? tr("Restore") : tr("Mark as Done");
    } else if (annotation.kind == editor::AnnotationKind::Comment) {
        doneText = annotation.done ? tr("Restore") : tr("Mark as Resolved");
    }
    if (!doneText.isEmpty()) {
        const bool done = !annotation.done;
        QAction* doneAction =
            menu.addAction(doneText, this, [this, done]() { emit doneToggled(done); });
        doneAction->setShortcut(QKeySequence(Qt::Key_Space));
    }
    menu.addSeparator();
    QAction* deleteAction = menu.addAction(tr("Delete"), this, &AnnotationCard::deleteRequested);
    deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));

    menu.exec(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height())));
}

void AnnotationCard::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect()), CORNER_RADIUS, CORNER_RADIUS);
    painter.fillPath(shape, m_colors.background.isValid() ? m_colors.background
                                                          : palette().color(QPalette::Base));

    // The bar in the kind's color, cut by the rounded corners
    painter.save();
    painter.setClipPath(shape);
    painter.fillRect(QRect(0, 0, BAR_WIDTH, height()),
                     m_colors.kind.isValid() ? m_colors.kind : palette().color(QPalette::Mid));
    painter.restore();

    if (m_selected) {
        // The highlight while the keys are in the list (where they act on this card)
        const int width = m_listFocused ? OUTLINE_WIDTH : QUIET_OUTLINE_WIDTH;
        QColor color = palette().color(QPalette::Highlight);
        if (!m_listFocused) {
            color = m_colors.secondary.isValid() ? m_colors.secondary
                                                 : palette().color(QPalette::Mid);
        }
        const qreal inset = width / 2.0;
        painter.setPen(QPen(color, width));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(inset, inset, -inset, -inset),
                                CORNER_RADIUS - inset, CORNER_RADIUS - inset);
    }
}

void AnnotationCard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void AnnotationCard::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit editRequested();
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}

void AnnotationCard::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    updateElidedLabels();
}

}  // namespace kalahari::gui
