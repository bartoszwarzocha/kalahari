/// @file book_editor_cards.cpp
/// @brief BookEditor: the cards of the annotations under their paragraphs

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>

#include <QAction>
#include <QCursor>
#include <QLocale>
#include <QMenu>
#include <QPalette>
#include <QTextBlock>

#include <algorithm>
#include <cmath>
#include <limits>

namespace kalahari::editor {

namespace {

/// How much of the kind's color the fragment of the card under the mouse is filled with, on
/// a light and on a dark paper (as in the mockup approved on 26.10.2026)
constexpr double FRAGMENT_FILL_LIGHT = 0.24;
constexpr double FRAGMENT_FILL_DARK = 0.34;

/// The lightness under which the paper is dark
constexpr int DARK_LIGHTNESS = 128;

/// The room kept between a card scrolled into view and the view's edges, in pixels
constexpr qreal CARD_SCROLL_MARGIN = 30.0;

/// The first line of an annotation's text on the label of its menu, at most this long
constexpr int MENU_LABEL_LENGTH = 40;

/// @brief The layout of a document (nullptr: none, or not Kalahari's)
KalahariTextDocumentLayout* layoutOf(const QTextDocument* document) {
    return document != nullptr
               ? qobject_cast<KalahariTextDocumentLayout*>(document->documentLayout())
               : nullptr;
}

/// @brief A text shown as it is on a menu: an ampersand marks no key
QString literalMenuText(QString text) {
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

}  // namespace

// =============================================================================
// What is shown
// =============================================================================

void BookEditor::setUpAnnotationCards()
{
    // Drawn after each paragraph's text, in its coordinates
    if (m_renderPipeline) {
        m_renderPipeline->setParagraphDecorator([this](QPainter* painter, size_t paragraph) {
            if (!m_annotationCards.isShown() || !m_textBuffer) {
                return;
            }
            const QTextBlock block = m_textBuffer->findBlockByNumber(static_cast<int>(paragraph));
            const qreal top = annotationCardsTop(block);
            if (top >= 0.0) {
                m_annotationCards.paint(painter, block, top, m_textBuffer->textWidth());
            }
        });
    }

    // The room they take under their paragraphs
    if (KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get())) {
        layout->setSpaceBelowProvider([this](const QTextBlock& block) {
            return m_annotationCards.spaceBelow(block, block.document()->textWidth());
        });
    }
}

void BookEditor::updateAnnotationCards()
{
    const AnnotationCardSettings& settings = m_appearance.annotationCards;
    const bool wasShown = m_annotationCards.isShown();
    const bool shown = settings.show && m_viewMode != ViewMode::Page &&
                       (!m_distractionFree || settings.inDistractionFree);
    m_annotationCards.setSettings(settings);
    m_annotationCards.setShown(shown);
    m_annotationCards.setTextFont(m_appearance.typography.textFont);

    // The paper's colors, the kinds' as their marks have them; the card gone to outlined in
    // the program's accent
    const EditorColorMode mode = m_appearance.colorMode;
    AnnotationCards::Colors colors;
    colors.paper = m_appearance.colors.background(mode);
    colors.text = m_appearance.colors.textColor(mode);
    const EditorColors::AnnotationColors& kinds = m_appearance.colors.annotations(mode);
    colors.comment = kinds.comment;
    colors.todo = kinds.todo;
    colors.note = kinds.note;
    colors.selection = palette().color(QPalette::Highlight);
    colors.selection.setAlpha(255);
    m_annotationCards.setColors(colors);

    if (!shown && !m_annotationCards.hovered().isEmpty()) {
        m_annotationCards.setHovered(QString(), std::nullopt);
        if (m_renderPipeline) {
            m_renderPipeline->setFragmentHighlight(std::nullopt);
        }
    }
    updateAnnotationCardRoom();
    if (shown != wasShown) {
        emit annotationCardsShownChanged(shown);
    }
}

void BookEditor::updateAnnotationCardRoom(int first, int last)
{
    if (KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get())) {
        layout->updateSpaceBelow(first, last);
    }
    update();
}

bool BookEditor::showsAnnotationCards() const
{
    return m_annotationCards.isShown();
}

bool BookEditor::showsAnnotationCard(AnnotationKind kind) const
{
    return m_annotationCards.hasCard(kind);
}

bool BookEditor::isAnnotationCollapsed(const QString& id) const
{
    return m_annotationCards.isCollapsed(id);
}

void BookEditor::setAnnotationCollapsed(const QString& id, bool collapsed)
{
    if (m_annotationCards.isCollapsed(id) == collapsed) {
        return;
    }
    m_annotationCards.setCollapsed(id, collapsed);
    const QTextBlock block = annotationCardBlock(id);
    if (block.isValid()) {
        updateAnnotationCardRoom(block.blockNumber(), block.blockNumber());
    }
    // What is under the mouse now
    if (underMouse()) {
        hoverAnnotationCard(mapFromGlobal(QCursor::pos()));
    }
}

void BookEditor::setAllAnnotationsCollapsed(bool collapsed)
{
    m_annotationCards.setAllCollapsed(collapsed);
    updateAnnotationCardRoom();
}

// =============================================================================
// Geometry
// =============================================================================

qreal BookEditor::annotationCardsTop(const QTextBlock& block) const
{
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr || !block.isValid()) {
        return -1.0;
    }
    const int number = block.blockNumber();
    const qreal space = layout->spaceBelow(number);
    return space > 0.0 ? layout->blockHeight(number) - space : -1.0;
}

QTextBlock BookEditor::annotationCardBlock(const QString& id) const
{
    if (!m_textBuffer) {
        return {};
    }
    if (id.isEmpty()) {
        const std::optional<AnnotationCards::Writing>& writing = m_annotationCards.writing();
        return writing && writing->id.isEmpty() ? m_textBuffer->findBlockByNumber(writing->block)
                                                : QTextBlock();
    }

    // Its mark: in the paragraph its fragment ends in (the one before the position of its
    // end, when the fragment ends with a paragraph)
    const std::optional<AnnotationPlace> place = findAnnotation(*m_textBuffer, id);
    if (!place) {
        return {};
    }
    QTextBlock block = m_textBuffer->findBlock(place->end);
    for (int tries = 0; tries < 2 && block.isValid(); ++tries, block = block.previous()) {
        const std::vector<AnnotationMark> marks = annotationMarksIn(block);
        if (std::any_of(marks.cbegin(), marks.cend(),
                        [&id](const AnnotationMark& mark) { return mark.id == id; })) {
            return block;
        }
    }
    return {};
}

QRectF BookEditor::annotationCardRect(const QString& id) const
{
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr || !m_renderPipeline || !m_annotationCards.isShown()) {
        return {};
    }
    const QTextBlock block = annotationCardBlock(id);
    const qreal top = annotationCardsTop(block);
    if (top < 0.0) {
        return {};
    }
    const qreal blockY = layout->blockY(block.blockNumber());
    for (const AnnotationCardBox& card :
         m_annotationCards.cardsOf(block, top, m_textBuffer->textWidth())) {
        if (card.mark.id == id) {
            return QRectF(m_renderPipeline->documentToWidget(
                              QPointF(card.rect.left(), blockY + card.rect.top())),
                          m_renderPipeline->documentToWidget(
                              QPointF(card.rect.right(), blockY + card.rect.bottom())));
        }
    }
    return {};
}

std::optional<AnnotationCardHit> BookEditor::annotationCardAt(const QPointF& point) const
{
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr || !m_renderPipeline || !m_annotationCards.isShown()) {
        return std::nullopt;
    }
    const QPointF document = m_renderPipeline->widgetToDocument(point);
    const int number = layout->blockNumberAtY(document.y());
    const QTextBlock block = m_textBuffer->findBlockByNumber(number);
    const qreal top = annotationCardsTop(block);
    if (top < 0.0) {
        return std::nullopt;
    }
    return m_annotationCards.hitTest(block, top, m_textBuffer->textWidth(),
                                     QPointF(document.x(), document.y() - layout->blockY(number)));
}

void BookEditor::scrollToAnnotationCard(const QString& id)
{
    const QRectF card = annotationCardRect(id);
    if (card.isEmpty() || !m_viewportManager || height() <= 0) {
        return;
    }

    // Its bottom comes into view, then its top; the cursor's line stays in view
    const qreal topEdge = CARD_SCROLL_MARGIN + findReplaceBarHeight();
    const qreal bottomEdge = height() - CARD_SCROLL_MARGIN;
    qreal delta = std::max<qreal>(0.0, card.bottom() - bottomEdge);
    delta = std::min(delta, card.top() - topEdge);
    if (delta > 0.0 && m_renderPipeline) {
        const QRectF caret = m_renderPipeline->cursorRect();
        if (!caret.isEmpty()) {
            delta = std::max<qreal>(0.0, std::min(delta, caret.top() - topEdge));
        }
    }
    if (std::abs(delta) >= 1.0) {
        setScrollOffset(scrollOffset() + delta / m_viewportManager->viewScale());
    }
}

// =============================================================================
// Writing in the frame over the card
// =============================================================================

void BookEditor::setAnnotationCardWriting(const QString& id, AnnotationKind kind, int start,
                                          int end, qreal height)
{
    if (!m_textBuffer) {
        return;
    }
    const std::optional<AnnotationCards::Writing>& earlier = m_annotationCards.writing();
    const QTextBlock before = annotationCardBlock(earlier ? earlier->id : QString());

    AnnotationCards::Writing writing;
    writing.id = id;
    writing.kind = kind;
    // A new one's mark: after the last character of its fragment, in the paragraph of that
    // character; on its place without a fragment
    const int last = m_textBuffer->characterCount() - 1;
    end = std::clamp(end, 0, last);
    start = std::clamp(start, 0, end);
    const QTextBlock markBlock = m_textBuffer->findBlock(end > start ? end - 1 : end);
    writing.block = markBlock.blockNumber();
    writing.offset = std::min(end - markBlock.position(), markBlock.length() - 1);
    const double scale = m_renderPipeline ? m_renderPipeline->context().computed.viewScale : 1.0;
    m_cardWritingHeight = height;
    writing.height = height / scale;
    m_annotationCards.setWriting(writing);

    if (before.isValid()) {
        updateAnnotationCardRoom(before.blockNumber(), before.blockNumber());
    }
    const QTextBlock block = annotationCardBlock(id);
    if (block.isValid()) {
        updateAnnotationCardRoom(block.blockNumber(), block.blockNumber());
    }
    scrollToAnnotationCard(id);
}

void BookEditor::setAnnotationCardWritingHeight(qreal height)
{
    const std::optional<AnnotationCards::Writing>& writing = m_annotationCards.writing();
    if (!writing) {
        return;
    }
    const QString id = writing->id;
    m_cardWritingHeight = height;
    fitAnnotationCardWriting();
    scrollToAnnotationCard(id);
}

void BookEditor::fitAnnotationCardWriting()
{
    std::optional<AnnotationCards::Writing> writing = m_annotationCards.writing();
    if (!writing) {
        return;
    }
    const double scale = m_renderPipeline ? m_renderPipeline->context().computed.viewScale : 1.0;
    writing->height = m_cardWritingHeight / scale;
    m_annotationCards.setWriting(*writing);
    const QTextBlock block = annotationCardBlock(writing->id);
    if (block.isValid()) {
        updateAnnotationCardRoom(block.blockNumber(), block.blockNumber());
    }
}

void BookEditor::clearAnnotationCardWriting()
{
    const std::optional<AnnotationCards::Writing> writing = m_annotationCards.writing();
    if (!writing) {
        return;
    }
    const QTextBlock block = annotationCardBlock(writing->id);
    m_annotationCards.clearWriting();
    m_cardWritingHeight = 0.0;
    if (block.isValid()) {
        updateAnnotationCardRoom(block.blockNumber(), block.blockNumber());
    }
}

// =============================================================================
// The mouse
// =============================================================================

bool BookEditor::hoverAnnotationCard(const QPointF& point)
{
    const std::optional<AnnotationCardHit> hit = annotationCardAt(point);
    const QString id = hit ? hit->id : QString();
    const std::optional<AnnotationCardPart> part =
        hit ? std::optional<AnnotationCardPart>(hit->part) : std::nullopt;
    if (id == m_annotationCards.hovered() && part == m_annotationCards.hoveredPart()) {
        return hit.has_value();
    }
    const bool otherCard = id != m_annotationCards.hovered();
    m_annotationCards.setHovered(id, part);

    // The fragment of the card under the mouse, in its kind's color (as the settings say)
    if (otherCard && m_renderPipeline) {
        std::optional<EditorRenderPipeline::FragmentHighlight> fragment;
        const std::optional<AnnotationPlace> place =
            !id.isEmpty() && m_appearance.annotationCards.highlightFragment && m_textBuffer
                ? findAnnotation(*m_textBuffer, id)
                : std::nullopt;
        if (place && place->start != place->end) {
            const QColor kind = m_annotationCards.kindColor(place->annotation.kind);
            QColor fill = kind;
            const bool dark = m_annotationCards.colors().paper.lightness() < DARK_LIGHTNESS;
            fill.setAlphaF(dark ? FRAGMENT_FILL_DARK : FRAGMENT_FILL_LIGHT);
            fragment = EditorRenderPipeline::FragmentHighlight{
                calculateCursorPosition(place->start), calculateCursorPosition(place->end), fill,
                kind};
        }
        m_renderPipeline->setFragmentHighlight(fragment);
    }
    update();
    return hit.has_value();
}

bool BookEditor::pressAnnotationCard(const QPointF& point)
{
    const std::optional<AnnotationCardHit> hit = annotationCardAt(point);
    m_pressedCard.clear();
    if (!hit) {
        return false;
    }
    switch (hit->part) {
    case AnnotationCardPart::Line:
        setAnnotationCollapsed(hit->id, false);
        break;
    case AnnotationCardPart::Menu:
        showAnnotationCardMenu(hit->id, mapToGlobal(point.toPoint()));
        break;
    case AnnotationCardPart::Done:
        markAnnotationDone(hit->id);
        break;
    case AnnotationCardPart::ShowAll: {
        m_annotationCards.showWhole(hit->id);
        const QTextBlock block = annotationCardBlock(hit->id);
        if (block.isValid()) {
            updateAnnotationCardRoom(block.blockNumber(), block.blockNumber());
        }
        break;
    }
    case AnnotationCardPart::Card:
        // Its fragment is selected, and its card in the panel; a second click edits it
        setFocus(Qt::MouseFocusReason);
        goToAnnotation(hit->id);
        m_pressedCard = hit->id;
        emit annotationCardClicked(hit->id);
        break;
    }
    return true;
}

QString BookEditor::annotationCardToolTip(const QPointF& point) const
{
    const std::optional<AnnotationCardHit> hit = annotationCardAt(point);
    if (!hit) {
        return {};
    }
    switch (hit->part) {
    case AnnotationCardPart::Menu:
        return tr("Annotation menu");
    case AnnotationCardPart::Done:
        return tr("Mark as done");
    case AnnotationCardPart::Line:
        return tr("Click to expand");
    case AnnotationCardPart::ShowAll:
    case AnnotationCardPart::Card:
        break;
    }
    return {};
}

void BookEditor::leaveEvent(QEvent* event)
{
    if (!m_annotationCards.hovered().isEmpty()) {
        m_annotationCards.setHovered(QString(), std::nullopt);
        if (m_renderPipeline) {
            m_renderPipeline->setFragmentHighlight(std::nullopt);
        }
        update();
    }
    QWidget::leaveEvent(event);
}

// =============================================================================
// The menus
// =============================================================================

QString BookEditor::annotationAt(const CursorPosition& position) const
{
    if (!m_textBuffer) {
        return {};
    }
    const int at = calculateAbsolutePosition(position);
    QString found;
    int shortest = std::numeric_limits<int>::max();
    for (const AnnotationPlace& place : annotationsIn(*m_textBuffer)) {
        // A to-do done and a resolved comment have no mark: they are in the panel only
        if (!place.annotation.done && place.start <= at && at <= place.end &&
            place.end - place.start < shortest) {
            shortest = place.end - place.start;
            found = place.annotation.id;
        }
    }
    return found;
}

void BookEditor::addAnnotationActions(QMenu& menu, const QString& id, bool forCard)
{
    const std::optional<AnnotationPlace> place =
        m_textBuffer ? findAnnotation(*m_textBuffer, id) : std::nullopt;
    if (!place) {
        return;
    }
    const Annotation& annotation = place->annotation;

    // In the text's menu, which one it is: its kind and the start of its text
    if (!forCard) {
        QString first = annotation.text.trimmed().section(QLatin1Char('\n'), 0, 0);
        if (first.size() > MENU_LABEL_LENGTH) {
            first = first.left(MENU_LABEL_LENGTH).trimmed() + QChar(0x2026);
        }
        const QString kind = AnnotationCards::kindTitle(annotation.kind);
        const QString label =
            first.isEmpty() ? kind
                            : QStringLiteral("%1: %2").arg(kind, QLocale().quoteString(first));
        menu.addAction(literalMenuText(label))->setEnabled(false);
    }

    // Each with its access key, unique with the other items of the menus
    // (test_menu_access_keys); those that change the text greyed out in a read-only one
    QAction* edit = menu.addAction(tr("&Edit Annotation"));
    disableWhileReadOnly(edit);
    connect(edit, &QAction::triggered, this, [this, id]() { emit annotationEditRequested(id); });

    if (m_annotationCards.hasCard(annotation.kind)) {
        const bool collapsed = m_annotationCards.isCollapsed(id);
        QAction* toggle =
            menu.addAction(collapsed ? tr("E&xpand Annotation") : tr("Collap&se Annotation"));
        connect(toggle, &QAction::triggered, this,
                [this, id, collapsed]() { setAnnotationCollapsed(id, !collapsed); });
    }

    if (annotation.kind != AnnotationKind::Note) {
        QAction* done = menu.addAction(annotation.kind == AnnotationKind::Comment
                                           ? tr("Mark as &Resolved")
                                           : tr("Ma&rk as Done"));
        disableWhileReadOnly(done);
        connect(done, &QAction::triggered, this, [this, id]() { markAnnotationDone(id); });
    }

    QAction* remove = menu.addAction(tr("Remo&ve Annotation"));
    disableWhileReadOnly(remove);
    connect(remove, &QAction::triggered, this, [this, id]() {
        if (!refusesChange()) {
            removeAnnotation(id);
        }
    });

    if (forCard) {
        menu.addSeparator();
        menu.addAction(tr("Card &Appearance and Colors..."), this,
                       [this]() { emit annotationCardSettingsRequested(); });
    }
}

void BookEditor::showAnnotationCardMenu(const QString& id, const QPoint& globalPos)
{
    QMenu menu(this);
    menu.setToolTipsVisible(true);
    addAnnotationActions(menu, id, true);
    if (!menu.isEmpty()) {
        menu.exec(globalPos);
    }
}

void BookEditor::markAnnotationDone(const QString& id)
{
    if (refusesChange() || !m_textBuffer) {
        return;
    }
    const std::optional<AnnotationPlace> place = findAnnotation(*m_textBuffer, id);
    if (!place || place->annotation.done || place->annotation.kind == AnnotationKind::Note) {
        return;
    }
    Annotation done = place->annotation;
    done.done = true;
    updateAnnotation(done);
}

}  // namespace kalahari::editor
