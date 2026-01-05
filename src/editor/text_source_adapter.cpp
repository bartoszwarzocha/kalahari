/// @file text_source_adapter.cpp
/// @brief Implementation of ITextSource adapters (OpenSpec #00043 Phase 12.1)

#include <kalahari/editor/text_source_adapter.h>
#include <kalahari/editor/kml_document_model.h>
#include <QTextDocument>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>

namespace kalahari::editor {

// =============================================================================
// QTextDocumentSource Implementation
// =============================================================================

QTextDocumentSource::QTextDocumentSource(QTextDocument* document)
    : m_document(document) {
    if (m_document) {
        m_textWidth = m_document->textWidth();
    }
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

QTextLayout* QTextDocumentSource::layout(size_t index) const {
    QTextBlock block = blockAt(index);
    if (!block.isValid()) return nullptr;
    return block.layout();
}

bool QTextDocumentSource::hasLayout(size_t index) const {
    QTextLayout* lay = layout(index);
    return lay != nullptr && lay->lineCount() > 0;
}

void QTextDocumentSource::ensureLayouted(size_t /*first*/, size_t /*last*/) {
    // QTextDocument handles layout automatically via QAbstractTextDocumentLayout
    // No explicit action needed - blocks are laid out on demand
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
    m_textWidth = width;
    if (m_document) {
        m_document->setTextWidth(width);
    }
}

double QTextDocumentSource::textWidth() const {
    return m_textWidth;
}

void QTextDocumentSource::setFont(const QFont& font) {
    if (m_document) {
        m_document->setDefaultFont(font);
    }
}

QFont QTextDocumentSource::font() const {
    if (m_document) {
        return m_document->defaultFont();
    }
    return QFont();
}

QTextBlock QTextDocumentSource::blockAt(size_t index) const {
    if (!m_document) return QTextBlock();
    return m_document->findBlockByNumber(static_cast<int>(index));
}

// =============================================================================
// KmlDocumentModelSource Implementation
// =============================================================================

KmlDocumentModelSource::KmlDocumentModelSource(KmlDocumentModel* model)
    : m_model(model) {
}

size_t KmlDocumentModelSource::paragraphCount() const {
    if (!m_model) return 0;
    return m_model->paragraphCount();
}

QString KmlDocumentModelSource::paragraphText(size_t index) const {
    if (!m_model) return QString();
    return m_model->paragraphText(index);
}

size_t KmlDocumentModelSource::paragraphLength(size_t index) const {
    if (!m_model) return 0;
    return m_model->paragraphLength(index);
}

QString KmlDocumentModelSource::plainText() const {
    if (!m_model) return QString();
    return m_model->plainText();
}

size_t KmlDocumentModelSource::characterCount() const {
    if (!m_model) return 0;
    return m_model->characterCount();
}

QTextLayout* KmlDocumentModelSource::layout(size_t index) const {
    if (!m_model) return nullptr;
    return m_model->layout(index);
}

bool KmlDocumentModelSource::hasLayout(size_t index) const {
    if (!m_model) return false;
    return m_model->isLayouted(index);
}

void KmlDocumentModelSource::ensureLayouted(size_t first, size_t last) {
    if (m_model) {
        m_model->ensureLayouted(first, last);
    }
}

double KmlDocumentModelSource::paragraphY(size_t index) const {
    if (!m_model) return 0.0;
    return m_model->paragraphY(index);
}

double KmlDocumentModelSource::paragraphHeight(size_t index) const {
    if (!m_model) return 20.0;
    return m_model->paragraphHeight(index);
}

double KmlDocumentModelSource::totalHeight() const {
    if (!m_model) return 0.0;
    return m_model->totalHeight();
}

size_t KmlDocumentModelSource::paragraphAtY(double y) const {
    if (!m_model) return 0;
    return m_model->paragraphAtY(y);
}

void KmlDocumentModelSource::setTextWidth(double width) {
    if (m_model) {
        m_model->setLineWidth(width);
    }
}

double KmlDocumentModelSource::textWidth() const {
    if (!m_model) return 800.0;
    return m_model->lineWidth();
}

void KmlDocumentModelSource::setFont(const QFont& font) {
    if (m_model) {
        m_model->setFont(font);
    }
}

QFont KmlDocumentModelSource::font() const {
    if (!m_model) return QFont();
    return m_model->font();
}

}  // namespace kalahari::editor
