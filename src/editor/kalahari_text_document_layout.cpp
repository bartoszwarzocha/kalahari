/// @file kalahari_text_document_layout.cpp
/// @brief Custom QAbstractTextDocumentLayout implementation (OpenSpec #00043)

#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QPainter>
#include <QTextFrame>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructor
// =============================================================================

KalahariTextDocumentLayout::KalahariTextDocumentLayout(QTextDocument* doc)
    : QAbstractTextDocumentLayout(doc) {
    if (doc) {
        m_font = doc->defaultFont();
    }
}

// =============================================================================
// Configuration
// =============================================================================

void KalahariTextDocumentLayout::setTextWidth(qreal width) {
    if (m_textWidth != width) {
        m_textWidth = width;
        m_positionsDirty = true;

        // Re-layout all blocks with new width
        QTextBlock block = document()->begin();
        while (block.isValid()) {
            QTextBlock mutableBlock = block;
            layoutBlock(mutableBlock);
            block = block.next();
        }

        updateBlockPositions();
        emit documentSizeChanged(documentSize());
        emit update();
    }
}

void KalahariTextDocumentLayout::setFont(const QFont& font) {
    if (m_font != font) {
        m_font = font;
        m_positionsDirty = true;

        // Re-layout all blocks with new font
        QTextBlock block = document()->begin();
        while (block.isValid()) {
            QTextBlock mutableBlock = block;
            layoutBlock(mutableBlock);
            block = block.next();
        }

        updateBlockPositions();
        emit documentSizeChanged(documentSize());
        emit update();
    }
}

// =============================================================================
// Document Changed - Core Layout Logic
// =============================================================================

void KalahariTextDocumentLayout::documentChanged(int from, int charsRemoved, int charsAdded) {
    Q_UNUSED(charsRemoved);
    Q_UNUSED(charsAdded);

    // Find affected blocks and re-layout them
    QTextBlock block = document()->findBlock(from);

    // Layout at least the changed block and a few after (for paragraph merges/splits)
    int blocksToLayout = 3;
    while (block.isValid() && blocksToLayout > 0) {
        QTextBlock mutableBlock = block;
        layoutBlock(mutableBlock);
        block = block.next();
        --blocksToLayout;
    }

    // Mark positions as dirty - they need recalculation
    m_positionsDirty = true;

    emit documentSizeChanged(documentSize());
    emit update();
}

void KalahariTextDocumentLayout::layoutBlock(QTextBlock& block) {
    if (!block.isValid()) return;

    QTextLayout* layout = block.layout();
    if (!layout) return;

    // Set font from block format or default
    QTextCharFormat charFormat = block.charFormat();
    QFont blockFont = charFormat.font();
    if (blockFont == QFont()) {
        blockFont = m_font;
    }
    layout->setFont(blockFont);

    // Calculate effective width
    qreal effectiveWidth = m_textWidth;
    if (effectiveWidth <= 0) {
        effectiveWidth = 10000;  // Very large default for no wrapping
    }

    // Get alignment from QTextBlockFormat and configure QTextOption
    QTextBlockFormat blockFormat = block.blockFormat();
    Qt::Alignment alignment = blockFormat.alignment();
    if (alignment == 0) {
        alignment = Qt::AlignLeft;  // Default to left if not set
    }

    QTextOption textOption;
    textOption.setAlignment(alignment);
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout->setTextOption(textOption);

    // Prepare layout with lines starting at y=0
    layout->beginLayout();
    qreal y = 0;

    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLineWidth(effectiveWidth);

        // Calculate X position based on alignment
        qreal x = 0;
        if (alignment & Qt::AlignHCenter) {
            x = (effectiveWidth - line.naturalTextWidth()) / 2.0;
        } else if (alignment & Qt::AlignRight) {
            x = effectiveWidth - line.naturalTextWidth();
        }
        // Qt::AlignJustify is handled by QTextLine automatically when width is set

        line.setPosition(QPointF(x, y));
        y += line.height();
    }

    layout->endLayout();
}

// =============================================================================
// Block Position Management
// =============================================================================

void KalahariTextDocumentLayout::updateBlockPositions() const {
    if (!m_positionsDirty) return;

    int blockCount = document()->blockCount();
    m_blockYPositions.resize(static_cast<size_t>(blockCount));

    qreal y = 0;
    QTextBlock block = document()->begin();
    int index = 0;

    while (block.isValid() && index < blockCount) {
        m_blockYPositions[static_cast<size_t>(index)] = y;
        y += blockHeight(block);
        block = block.next();
        ++index;
    }

    m_cachedDocumentHeight = y;
    m_positionsDirty = false;
}

qreal KalahariTextDocumentLayout::blockY(int blockNumber) const {
    updateBlockPositions();

    if (blockNumber < 0 || static_cast<size_t>(blockNumber) >= m_blockYPositions.size()) {
        return 0;
    }

    return m_blockYPositions[static_cast<size_t>(blockNumber)];
}

qreal KalahariTextDocumentLayout::blockHeight(const QTextBlock& block) const {
    if (!block.isValid()) return 0;

    QTextLayout* layout = block.layout();
    if (!layout) return 0;

    // Use boundingRect which gives tight bounds without extra leading
    qreal height = layout->boundingRect().height();
    if (height > 0) {
        return height;
    }

    // Fallback: estimate from font
    QFontMetricsF fm(m_font);
    return fm.height();
}

// =============================================================================
// QAbstractTextDocumentLayout Overrides
// =============================================================================

void KalahariTextDocumentLayout::draw(QPainter* painter, const PaintContext& context) {
    if (!painter) return;

    painter->save();

    // Clip to context rect
    if (!context.clip.isEmpty()) {
        painter->setClipRect(context.clip);
    }

    // Draw each visible block
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        QTextLayout* layout = block.layout();
        if (layout) {
            qreal y = blockY(block.blockNumber());
            QRectF blockRect(0, y, m_textWidth, blockHeight(block));

            // Only draw if intersects clip
            if (context.clip.isEmpty() || blockRect.intersects(context.clip)) {
                layout->draw(painter, QPointF(0, y));
            }
        }
        block = block.next();
    }

    painter->restore();
}

int KalahariTextDocumentLayout::hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const {
    updateBlockPositions();

    // Find block at Y position using binary search
    qreal targetY = point.y();

    int blockNumber = 0;
    QTextBlock block = document()->begin();

    while (block.isValid()) {
        qreal y = blockY(block.blockNumber());
        qreal height = blockHeight(block);

        if (targetY >= y && targetY < y + height) {
            // Found the block, now find position within it
            QTextLayout* layout = block.layout();
            if (layout) {
                // Adjust point to block-local coordinates
                QPointF localPoint(point.x(), targetY - y);

                for (int i = 0; i < layout->lineCount(); ++i) {
                    QTextLine line = layout->lineAt(i);
                    if (localPoint.y() >= line.y() && localPoint.y() < line.y() + line.height()) {
                        int pos = line.xToCursor(localPoint.x(),
                            accuracy == Qt::ExactHit ? QTextLine::CursorOnCharacter : QTextLine::CursorBetweenCharacters);
                        return block.position() + pos;
                    }
                }

                // After last line - return end of block
                if (accuracy == Qt::FuzzyHit) {
                    return block.position() + block.length() - 1;
                }
            }
            break;
        }

        block = block.next();
        ++blockNumber;
    }

    // Not found - return end of document for fuzzy hit
    if (accuracy == Qt::FuzzyHit) {
        return document()->characterCount() - 1;
    }

    return -1;
}

int KalahariTextDocumentLayout::pageCount() const {
    return 1;  // Continuous layout, single page
}

QSizeF KalahariTextDocumentLayout::documentSize() const {
    updateBlockPositions();
    return QSizeF(m_textWidth, m_cachedDocumentHeight);
}

QRectF KalahariTextDocumentLayout::frameBoundingRect(QTextFrame* frame) const {
    if (frame == document()->rootFrame()) {
        return QRectF(0, 0, m_textWidth, documentSize().height());
    }
    return QRectF();
}

QRectF KalahariTextDocumentLayout::blockBoundingRect(const QTextBlock& block) const {
    if (!block.isValid()) return QRectF();

    qreal y = blockY(block.blockNumber());
    qreal height = blockHeight(block);

    return QRectF(0, y, m_textWidth, height);
}

}  // namespace kalahari::editor
