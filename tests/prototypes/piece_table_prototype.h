/**
 * @file piece_table_prototype.h
 * @brief Piece Table prototype for OpenSpec #00043 Research & Spike
 *
 * This is a standalone prototype to benchmark piece table performance
 * against traditional QString operations.
 *
 * Piece Table concept:
 * - Original buffer: readonly, contains initial text
 * - Add buffer: append-only, contains all inserted text
 * - Pieces: vector of (source, start, length) describing document order
 *
 * Complexity:
 * - Insert: O(log N) with balanced tree, O(N) with vector (this prototype)
 * - Delete: O(log N) with balanced tree, O(N) with vector (this prototype)
 * - Text access: O(N) to reconstruct, O(1) with caching
 */

#pragma once

#include <QString>
#include <QElapsedTimer>
#include <vector>
#include <optional>

namespace prototype {

/**
 * @brief Simple Piece Table implementation for benchmarking
 */
class PieceTable {
public:
    enum class Source { Original, Add };

    struct Piece {
        Source source;
        size_t start;
        size_t length;
    };

    PieceTable() = default;

    /**
     * @brief Initialize with original text (simulates file load)
     */
    explicit PieceTable(const QString& text)
        : m_originalBuffer(text)
    {
        if (!text.isEmpty()) {
            m_pieces.push_back({Source::Original, 0, static_cast<size_t>(text.length())});
        }
        invalidateCache();
    }

    /**
     * @brief Insert text at position
     * @param position Character position (0-based)
     * @param text Text to insert
     *
     * Complexity: O(N) for piece vector, O(1) for add buffer append
     */
    void insert(size_t position, const QString& text) {
        if (text.isEmpty()) return;

        // Add text to add buffer
        size_t addStart = static_cast<size_t>(m_addBuffer.length());
        m_addBuffer.append(text);

        // Find piece containing position
        size_t currentPos = 0;
        for (size_t i = 0; i < m_pieces.size(); ++i) {
            size_t pieceEnd = currentPos + m_pieces[i].length;

            if (position <= pieceEnd) {
                size_t offsetInPiece = position - currentPos;

                if (offsetInPiece == 0) {
                    // Insert before this piece
                    m_pieces.insert(m_pieces.begin() + i,
                        {Source::Add, addStart, static_cast<size_t>(text.length())});
                } else if (offsetInPiece == m_pieces[i].length) {
                    // Insert after this piece
                    m_pieces.insert(m_pieces.begin() + i + 1,
                        {Source::Add, addStart, static_cast<size_t>(text.length())});
                } else {
                    // Split piece and insert in middle
                    Piece& piece = m_pieces[i];
                    Piece secondHalf = {
                        piece.source,
                        piece.start + offsetInPiece,
                        piece.length - offsetInPiece
                    };
                    piece.length = offsetInPiece;

                    // Insert new piece and second half
                    m_pieces.insert(m_pieces.begin() + i + 1,
                        {Source::Add, addStart, static_cast<size_t>(text.length())});
                    m_pieces.insert(m_pieces.begin() + i + 2, secondHalf);
                }

                invalidateCache();
                return;
            }

            currentPos = pieceEnd;
        }

        // Append at end
        m_pieces.push_back({Source::Add, addStart, static_cast<size_t>(text.length())});
        invalidateCache();
    }

    /**
     * @brief Remove text at position
     * @param position Start position
     * @param length Number of characters to remove
     *
     * Complexity: O(N) for piece vector manipulation
     */
    void remove(size_t position, size_t length) {
        if (length == 0) return;

        size_t endPosition = position + length;
        size_t currentPos = 0;
        size_t i = 0;

        while (i < m_pieces.size() && currentPos < endPosition) {
            size_t pieceEnd = currentPos + m_pieces[i].length;

            if (pieceEnd <= position) {
                // Piece is before deletion range
                currentPos = pieceEnd;
                ++i;
                continue;
            }

            if (currentPos >= endPosition) {
                // Past deletion range
                break;
            }

            size_t deleteStart = std::max(position, currentPos);
            size_t deleteEnd = std::min(endPosition, pieceEnd);
            size_t offsetInPiece = deleteStart - currentPos;
            size_t deleteLength = deleteEnd - deleteStart;

            if (offsetInPiece == 0 && deleteLength == m_pieces[i].length) {
                // Delete entire piece
                m_pieces.erase(m_pieces.begin() + i);
                // Don't increment i
            } else if (offsetInPiece == 0) {
                // Delete from start of piece
                m_pieces[i].start += deleteLength;
                m_pieces[i].length -= deleteLength;
                currentPos = pieceEnd;
                ++i;
            } else if (offsetInPiece + deleteLength == m_pieces[i].length) {
                // Delete from end of piece
                m_pieces[i].length -= deleteLength;
                currentPos = pieceEnd;
                ++i;
            } else {
                // Delete from middle - split piece
                Piece& piece = m_pieces[i];
                Piece secondHalf = {
                    piece.source,
                    piece.start + offsetInPiece + deleteLength,
                    piece.length - offsetInPiece - deleteLength
                };
                piece.length = offsetInPiece;
                m_pieces.insert(m_pieces.begin() + i + 1, secondHalf);
                currentPos = pieceEnd;
                i += 2;
            }
        }

        invalidateCache();
    }

    /**
     * @brief Get full document text
     *
     * Complexity: O(N) to reconstruct, O(1) if cached
     */
    QString text() const {
        if (m_textCache.has_value()) {
            return m_textCache.value();
        }

        QString result;
        result.reserve(length());

        for (const auto& piece : m_pieces) {
            const QString& buffer = (piece.source == Source::Original)
                ? m_originalBuffer
                : m_addBuffer;
            result.append(buffer.mid(piece.start, piece.length));
        }

        m_textCache = result;
        return result;
    }

    /**
     * @brief Get text in range
     */
    QString text(size_t start, size_t len) const {
        // For prototype, just use full text and substring
        // Real implementation would be smarter
        return text().mid(start, len);
    }

    /**
     * @brief Get total document length
     *
     * Complexity: O(N) pieces, could be O(1) with cached value
     */
    size_t length() const {
        size_t total = 0;
        for (const auto& piece : m_pieces) {
            total += piece.length;
        }
        return total;
    }

    /**
     * @brief Get number of pieces (for diagnostics)
     */
    size_t pieceCount() const {
        return m_pieces.size();
    }

    /**
     * @brief Get add buffer size (for diagnostics)
     */
    size_t addBufferSize() const {
        return static_cast<size_t>(m_addBuffer.length());
    }

private:
    void invalidateCache() {
        m_textCache.reset();
    }

    QString m_originalBuffer;
    QString m_addBuffer;
    std::vector<Piece> m_pieces;

    mutable std::optional<QString> m_textCache;
};

/**
 * @brief Traditional QString-based document for comparison
 */
class TraditionalDocument {
public:
    TraditionalDocument() = default;

    explicit TraditionalDocument(const QString& text)
        : m_text(text)
    {}

    void insert(size_t position, const QString& text) {
        m_text.insert(position, text);
    }

    void remove(size_t position, size_t length) {
        m_text.remove(position, length);
    }

    QString text() const {
        return m_text;
    }

    QString text(size_t start, size_t len) const {
        return m_text.mid(start, len);
    }

    size_t length() const {
        return static_cast<size_t>(m_text.length());
    }

private:
    QString m_text;
};

} // namespace prototype
