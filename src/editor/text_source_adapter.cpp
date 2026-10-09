/// @file text_source_adapter.cpp
/// @brief Implementation of ITextSource adapters (OpenSpec #00043 Phase 12.1)

#include <kalahari/editor/text_source_adapter.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_data.h>
#include <QTextDocument>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// QTextDocumentSource Implementation
// =============================================================================

QTextDocumentSource::QTextDocumentSource(QTextDocument* document)
    : m_document(document) {
}

size_t QTextDocumentSource::paragraphCount() const {
    if (!m_document) return 0;
    return static_cast<size_t>(m_document->blockCount());
}

QString QTextDocumentSource::paragraphText(size_t index) const {
    QTextBlock block = blockAt(index);
    if (!block.isValid()) return QString();
    return block.text();
}

size_t QTextDocumentSource::paragraphLength(size_t index) const {
    QTextBlock block = blockAt(index);
    if (!block.isValid()) return 0;
    return static_cast<size_t>(block.length());
}

QString QTextDocumentSource::plainText() const {
    if (!m_document) return QString();
    return m_document->toPlainText();
}

size_t QTextDocumentSource::characterCount() const {
    if (!m_document) return 0;
    return static_cast<size_t>(m_document->characterCount());
}

std::vector<TextHighlight> QTextDocumentSource::paragraphHighlights(size_t index) const {
    std::vector<TextHighlight> highlights;
    const QTextBlock block = blockAt(index);
    if (!block.isValid()) return highlights;

    // Check results, only those made for the paragraph's current text
    if (const ParagraphData* data = ParagraphData::find(block)) {
        if (!data->spelling.issues.empty() || !data->grammar.issues.empty()) {
            const QString text = block.text();
            for (const ParagraphCheck* check : {&data->spelling, &data->grammar}) {
                if (const auto* issues = check->issuesFor(text)) {
                    highlights.insert(highlights.end(), issues->begin(), issues->end());
                }
            }
        }
    }

    std::stable_sort(highlights.begin(), highlights.end(),
                     [](const TextHighlight& a, const TextHighlight& b) { return a.start < b.start; });
    return highlights;
}

std::vector<AnnotationMark> QTextDocumentSource::paragraphAnnotationMarks(size_t index) const {
    return annotationMarksIn(blockAt(index));
}

QTextLayout* QTextDocumentSource::layout(size_t index) const {
    // The pipeline also reads blocks outside the viewport (the cursor's, the pages'):
    // one waiting for layout gets its lines now
    return KalahariTextDocumentLayout::blockLayout(blockAt(index));
}

bool QTextDocumentSource::hasLayout(size_t index) const {
    QTextLayout* lay = layout(index);
    return lay != nullptr && lay->lineCount() > 0;
}

void QTextDocumentSource::ensureLayouted(size_t first, size_t last) {
    // After a width or font change, a load or a large edit, blocks wait for layout with
    // estimated heights until they are shown
    if (auto* layout = kalahariLayout()) {
        layout->ensureLaidOut(static_cast<int>(first), static_cast<int>(last));
    }
}

double QTextDocumentSource::paragraphY(size_t index) const {
    if (!m_document) return 0.0;

    QTextBlock block = blockAt(index);
    if (!block.isValid()) return 0.0;

    // Use document layout for accurate positioning
    QAbstractTextDocumentLayout* docLayout = m_document->documentLayout();
    if (docLayout) {
        QRectF rect = docLayout->blockBoundingRect(block);
        return rect.y();
    }

    // Fallback: sum heights of previous blocks
    double y = 0.0;
    QTextBlock b = m_document->begin();
    while (b.isValid() && b.blockNumber() < static_cast<int>(index)) {
        QTextLayout* lay = b.layout();
        if (lay && lay->lineCount() > 0) {
            y += lay->boundingRect().height();
        } else {
            // Estimate height
            y += 20.0;  // Default line height
        }
        b = b.next();
    }
    return y;
}

double QTextDocumentSource::paragraphHeight(size_t index) const {
    if (!m_document) return 20.0;

    QTextBlock block = blockAt(index);
    if (!block.isValid()) return 20.0;

    // Use document layout for accurate height
    QAbstractTextDocumentLayout* docLayout = m_document->documentLayout();
    if (docLayout) {
        QRectF rect = docLayout->blockBoundingRect(block);
        return rect.height();
    }

    // Fallback: use layout bounding rect
    QTextLayout* lay = block.layout();
    if (lay && lay->lineCount() > 0) {
        return lay->boundingRect().height();
    }

    return 20.0;  // Default estimate
}

double QTextDocumentSource::totalHeight() const {
    if (!m_document) return 0.0;

    // Use document layout for accurate total height
    QAbstractTextDocumentLayout* docLayout = m_document->documentLayout();
    if (docLayout) {
        return docLayout->documentSize().height();
    }

    // Fallback: sum all block heights
    double total = 0.0;
    QTextBlock block = m_document->begin();
    while (block.isValid()) {
        total += paragraphHeight(static_cast<size_t>(block.blockNumber()));
        block = block.next();
    }
    return total;
}

size_t QTextDocumentSource::paragraphAtY(double y) const {
    if (!m_document || y < 0) return 0;

    // The editor's layout finds the block from its cached positions
    if (const auto* layout = kalahariLayout()) {
        return static_cast<size_t>(std::max(0, layout->blockNumberAtY(y)));
    }

    // Use document layout for accurate hit testing
    QAbstractTextDocumentLayout* docLayout = m_document->documentLayout();
    if (docLayout) {
        int pos = docLayout->hitTest(QPointF(0, y), Qt::FuzzyHit);
        if (pos >= 0) {
            QTextBlock block = m_document->findBlock(pos);
            if (block.isValid()) {
                return static_cast<size_t>(block.blockNumber());
            }
        }
    }

    // Fallback: linear search
    double cumY = 0.0;
    QTextBlock block = m_document->begin();
    while (block.isValid()) {
        double h = paragraphHeight(static_cast<size_t>(block.blockNumber()));
        if (cumY + h > y) {
            return static_cast<size_t>(block.blockNumber());
        }
        cumY += h;
        block = block.next();
    }

    return paragraphCount();
}

void QTextDocumentSource::setTextWidth(double width) {
    // The document's text width is what the layout wraps at. QTextDocument re-lays out
    // the whole document on every setTextWidth() call, even for an unchanged width.
    if (m_document && m_document->textWidth() != width) {
        m_document->setTextWidth(width);
    }
}

double QTextDocumentSource::textWidth() const {
    return m_document ? m_document->textWidth() : 0.0;
}

void QTextDocumentSource::setFont(const QFont& font) {
    // setDefaultFont() re-lays out every block (also for an unchanged font)
    if (m_document && m_document->defaultFont() != font) {
        m_document->setDefaultFont(font);
    }
}

QFont QTextDocumentSource::font() const {
    if (m_document) {
        return m_document->defaultFont();
    }
    return QFont();
}

void QTextDocumentSource::setTypography(const LayoutTypography& typography) {
    // Typography lives in the layout (it is a view setting, the document knows nothing
    // about it); the layout re-lays out the text only when it changes
    if (auto* layout = kalahariLayout()) {
        layout->setTypography(typography);
    }
}

double QTextDocumentSource::lineSpacing() const {
    const auto* layout = kalahariLayout();
    return layout ? layout->typography().lineSpacing : 1.0;
}

double QTextDocumentSource::paragraphSpacing() const {
    const auto* layout = kalahariLayout();
    return layout ? layout->paragraphSpacing() : 0.0;
}

void QTextDocumentSource::setPageFlow(const PageFlow& flow) {
    // Pages are a view setting of the layout, like the typography
    if (auto* layout = kalahariLayout()) {
        layout->setPageFlow(flow);
    }
}

int QTextDocumentSource::pageCount() const {
    const auto* layout = kalahariLayout();
    return layout ? layout->pageCount() : 1;
}

KalahariTextDocumentLayout* QTextDocumentSource::kalahariLayout() const {
    return m_document ? qobject_cast<KalahariTextDocumentLayout*>(m_document->documentLayout())
                      : nullptr;
}

QTextBlock QTextDocumentSource::blockAt(size_t index) const {
    if (!m_document) return QTextBlock();
    return m_document->findBlockByNumber(static_cast<int>(index));
}

}  // namespace kalahari::editor
