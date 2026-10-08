/// @file annotation_card.cpp
/// @brief A card of the Annotations panel: one comment, TODO or note

#include "kalahari/gui/panels/annotation_card.h"
#include "kalahari/core/art_provider.h"

#include <QAbstractTextDocumentLayout>
#include <QCheckBox>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace kalahari::gui {

namespace {

constexpr int BAR_WIDTH = 4;          ///< The bar in the kind's color on the left
constexpr qreal CORNER_RADIUS = 8.0;  ///< The rounded corners of the card
constexpr int OUTLINE_WIDTH = 2;      ///< The outline of the selected card

/// @brief A color between two others
/// @param amount How much of @p over there is (0 to 1)
QColor mixed(const QColor& under, const QColor& over, double amount) {
    const auto channel = [amount](int a, int b) {
        return static_cast<int>(std::lround(a + (b - a) * amount));
    };
    return QColor(channel(under.red(), over.red()), channel(under.green(), over.green()),
                  channel(under.blue(), over.blue()));
}

/// @brief Esc or Ctrl+Enter: the keys that end the editing of the text
bool isDismissKey(const QKeyEvent& key) {
    if (key.key() == Qt::Key_Escape && key.modifiers() == Qt::NoModifier) {
        return true;
    }
    return (key.key() == Qt::Key_Return || key.key() == Qt::Key_Enter) &&
           key.modifiers().testFlag(Qt::ControlModifier);
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
    m_kindLabel->setFont(kindFont);
    header->addWidget(m_kindLabel);

    m_chapterLabel = new QLabel(this);
    m_chapterLabel->setFont(headerFont);
    m_chapterLabel->setForegroundRole(QPalette::PlaceholderText);
    m_chapterLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    header->addWidget(m_chapterLabel, 1);

    m_dateLabel = new QLabel(this);
    m_dateLabel->setFont(headerFont);
    m_dateLabel->setForegroundRole(QPalette::PlaceholderText);
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

    // The text, edited in place
    m_textEdit = new QTextEdit(this);
    m_textEdit->setAcceptRichText(false);
    m_textEdit->setFrameShape(QFrame::NoFrame);
    m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_textEdit->setTabChangesFocus(true);
    m_textEdit->viewport()->setAutoFillBackground(false);  // the card's background
    m_textEdit->document()->setDocumentMargin(0);
    m_textEdit->setPlaceholderText(tr("Type the annotation..."));
    m_textEdit->installEventFilter(this);
    connect(m_textEdit->document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged, this,
            &AnnotationCard::updateTextHeight);
    connect(m_textEdit, &QTextEdit::textChanged, this, [this]() {
        if (!m_settingText) {
            emit textEdited(m_textEdit->toPlainText());
        }
    });
    layout->addWidget(m_textEdit);

    updateTextHeight();
}

void AnnotationCard::updateMenuIcon() {
    const QIcon menuIcon = core::ArtProvider::getInstance().getIcon("common.moreHoriz");
    m_menuButton->setIcon(menuIcon);
    m_menuButton->setText(menuIcon.isNull() ? QStringLiteral("...") : QString());
}

void AnnotationCard::setEntry(const AnnotationEntry& entry) {
    m_entry = entry;
    updateHeader();

    // The text being typed stays: the annotation follows it, not the other way round
    if (!m_editing && m_textEdit->toPlainText() != entry.annotation.text) {
        m_settingText = true;
        m_textEdit->setPlainText(entry.annotation.text);
        m_settingText = false;
    }
}

void AnnotationCard::setColors(const QColor& kindColor, const QColor& background, double tint) {
    m_kindColor = kindColor;
    m_background = mixed(background, kindColor, tint);
    updateTextColor();  // the theme's text colors may be new too
    update();
}

void AnnotationCard::setSelected(bool selected) {
    if (selected != m_selected) {
        m_selected = selected;
        update();
    }
}

void AnnotationCard::startEditing() {
    m_textEdit->setFocus(Qt::OtherFocusReason);
    m_textEdit->moveCursor(QTextCursor::End);
}

QString AnnotationCard::text() const {
    return m_textEdit->toPlainText();
}

void AnnotationCard::updateHeader() {
    const editor::Annotation& annotation = m_entry.annotation;

    QString kindName;
    switch (annotation.kind) {
    case editor::AnnotationKind::Comment:
        kindName = annotation.done ? tr("Comment (resolved)") : tr("Comment");
        break;
    case editor::AnnotationKind::Todo:
        kindName = tr("TODO");
        break;
    case editor::AnnotationKind::Note:
        kindName = tr("Note");
        break;
    }
    m_kindLabel->setText(kindName);

    // A TODO is checked off in the card
    const bool todo = annotation.kind == editor::AnnotationKind::Todo;
    m_doneBox->setVisible(todo);
    {
        const QSignalBlocker blocker(m_doneBox);
        m_doneBox->setChecked(todo && annotation.done);
    }

    // Day and month (with the year when it is not this one); the time in the tooltip
    QString date;
    QString toolTip = annotation.author;
    if (annotation.created.isValid()) {
        const QDateTime local = annotation.created.toLocalTime();
        const QLocale locale;
        const bool thisYear = local.date().year() == QDate::currentDate().year();
        date = locale.toString(local.date(), thisYear ? tr("dd.MM") : tr("dd.MM.yyyy"));
        const QString when = locale.toString(local, QLocale::ShortFormat);
        toolTip = toolTip.isEmpty() ? when : tr("%1, %2").arg(toolTip, when);
    }
    m_dateLabel->setText(date);
    setToolTip(toolTip);

    updateTextColor();
    updateChapterLabel();
}

void AnnotationCard::updateTextColor() {
    // A done one is shown dimmed
    QPalette textPalette = m_textEdit->palette();
    textPalette.setColor(QPalette::Text,
                         palette().color(m_entry.annotation.done ? QPalette::PlaceholderText
                                                                 : QPalette::Text));
    m_textEdit->setPalette(textPalette);
}

void AnnotationCard::updateChapterLabel() {
    const QString title = m_entry.chapterTitle;
    m_chapterLabel->setText(
        m_chapterLabel->fontMetrics().elidedText(title, Qt::ElideRight, m_chapterLabel->width()));
    m_chapterLabel->setToolTip(title);
}

void AnnotationCard::updateTextHeight() {
    const int textHeight =
        static_cast<int>(std::ceil(m_textEdit->document()->size().height()));
    const int lineHeight = m_textEdit->fontMetrics().lineSpacing();
    m_textEdit->setFixedHeight(std::max(textHeight, lineHeight) + 2 * m_textEdit->frameWidth());
}

void AnnotationCard::showMenu() {
    const editor::Annotation& annotation = m_entry.annotation;
    QMenu menu(this);

    QString doneText;
    if (annotation.kind == editor::AnnotationKind::Todo) {
        doneText = annotation.done ? tr("Restore") : tr("Mark as Done");
    } else if (annotation.kind == editor::AnnotationKind::Comment) {
        doneText = annotation.done ? tr("Restore") : tr("Mark as Resolved");
    }
    if (!doneText.isEmpty()) {
        const bool done = !annotation.done;
        menu.addAction(doneText, this, [this, done]() { emit doneToggled(done); });
        menu.addSeparator();
    }
    QAction* deleteAction = menu.addAction(tr("Delete"), this, &AnnotationCard::deleteRequested);
    deleteAction->setShortcut(QKeySequence::Delete);  // shown only: the panel handles the key

    menu.exec(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height())));
}

void AnnotationCard::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect()), CORNER_RADIUS, CORNER_RADIUS);
    painter.fillPath(shape, m_background.isValid() ? m_background : palette().color(QPalette::Base));

    // The bar in the kind's color, cut by the rounded corners
    painter.save();
    painter.setClipPath(shape);
    painter.fillRect(QRect(0, 0, BAR_WIDTH, height()),
                     m_kindColor.isValid() ? m_kindColor : palette().color(QPalette::Mid));
    painter.restore();

    if (m_selected) {
        const qreal inset = OUTLINE_WIDTH / 2.0;
        painter.setPen(QPen(palette().color(QPalette::Highlight), OUTLINE_WIDTH));
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

void AnnotationCard::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    updateChapterLabel();
}

bool AnnotationCard::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_textEdit) {
        return QFrame::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::FocusIn:
        if (!m_editing) {
            m_editing = true;
            emit editingStarted();
        }
        break;
    case QEvent::FocusOut: {
        // A context menu of the text, or another window for a while: still editing
        const auto reason = static_cast<QFocusEvent*>(event)->reason();
        if (m_editing && reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason) {
            m_editing = false;
            emit editingFinished();
        }
        break;
    }
    case QEvent::ShortcutOverride:
        // Esc and Ctrl+Enter belong to the text, not to a shortcut of the window
        if (isDismissKey(*static_cast<QKeyEvent*>(event))) {
            event->accept();
            return true;
        }
        break;
    case QEvent::KeyPress:
        if (isDismissKey(*static_cast<QKeyEvent*>(event))) {
            emit editingDismissed();
            return true;
        }
        break;
    default:
        break;
    }
    return QFrame::eventFilter(watched, event);
}

}  // namespace kalahari::gui
