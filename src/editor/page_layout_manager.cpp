/// @file page_layout_manager.cpp
/// @brief Page layout manager implementation (OpenSpec #00042 Phase 5.3-5.5)

#include <kalahari/editor/page_layout_manager.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/layout_manager.h>
#include <kalahari/editor/paragraph_layout.h>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

// =============================================================================
// Construction / Destruction
// =============================================================================

PageLayoutManager::PageLayoutManager()
    : m_document(nullptr)
    , m_layoutManager(nullptr)
    , m_viewportWidth(800.0)
    , m_valid(false)
    , m_totalHeight(0.0)
    , m_pageWidth(0.0)
    , m_pageHeight(0.0)
    , m_textAreaWidth(0.0)
    , m_textAreaHeight(0.0)
{
    calculatePageDimensions();
}

PageLayoutManager::~PageLayoutManager() = default;

PageLayoutManager::PageLayoutManager(PageLayoutManager&& other) noexcept
    : m_pageLayout(std::move(other.m_pageLayout))
    , m_paginationSettings(std::move(other.m_paginationSettings))
    , m_document(other.m_document)
    , m_layoutManager(other.m_layoutManager)
    , m_viewportWidth(other.m_viewportWidth)
    , m_pages(std::move(other.m_pages))
    , m_valid(other.m_valid)
    , m_totalHeight(other.m_totalHeight)
    , m_pageWidth(other.m_pageWidth)
    , m_pageHeight(other.m_pageHeight)
    , m_textAreaWidth(other.m_textAreaWidth)
    , m_textAreaHeight(other.m_textAreaHeight)
    , m_marginsPixels(other.m_marginsPixels)
{
    other.m_document = nullptr;
    other.m_layoutManager = nullptr;
    other.m_valid = false;
}

PageLayoutManager& PageLayoutManager::operator=(PageLayoutManager&& other) noexcept {
    if (this != &other) {
        m_pageLayout = std::move(other.m_pageLayout);
        m_paginationSettings = std::move(other.m_paginationSettings);
        m_document = other.m_document;
        m_layoutManager = other.m_layoutManager;
        m_viewportWidth = other.m_viewportWidth;
        m_pages = std::move(other.m_pages);
        m_valid = other.m_valid;
        m_totalHeight = other.m_totalHeight;
        m_pageWidth = other.m_pageWidth;
        m_pageHeight = other.m_pageHeight;
        m_textAreaWidth = other.m_textAreaWidth;
        m_textAreaHeight = other.m_textAreaHeight;
        m_marginsPixels = other.m_marginsPixels;

        other.m_document = nullptr;
        other.m_layoutManager = nullptr;
        other.m_valid = false;
    }
    return *this;
}

// =============================================================================
// Configuration
// =============================================================================

void PageLayoutManager::setPageLayout(const PageLayout& layout) {
    m_pageLayout = layout;
    calculatePageDimensions();
    invalidate();
}

const PageLayout& PageLayoutManager::pageLayout() const {
    return m_pageLayout;
}

void PageLayoutManager::setDocument(KmlDocument* document) {
    m_document = document;
    invalidate();
}

KmlDocument* PageLayoutManager::document() const {
    return m_document;
}

void PageLayoutManager::setLayoutManager(LayoutManager* layoutManager) {
    m_layoutManager = layoutManager;
    invalidate();
}

LayoutManager* PageLayoutManager::layoutManager() const {
    return m_layoutManager;
}

void PageLayoutManager::setViewportWidth(qreal width) {
    if (m_viewportWidth != width) {
        m_viewportWidth = width;
        invalidate();
    }
}

qreal PageLayoutManager::viewportWidth() const {
    return m_viewportWidth;
}

void PageLayoutManager::setPaginationSettings(const PaginationSettings& settings) {
    m_paginationSettings = settings;
    invalidate();
}

const PaginationSettings& PageLayoutManager::paginationSettings() const {
    return m_paginationSettings;
}

// =============================================================================
// Pagination
// =============================================================================

int PageLayoutManager::calculatePages() {
    m_pages.clear();
    m_totalHeight = 0.0;

    // Need document and layout manager
    if (!m_document || !m_layoutManager) {
        m_valid = true;
        return 0;
    }

    const int paraCount = m_document->paragraphCount();
    if (paraCount == 0) {
        m_valid = true;
        return 0;
    }

    // Ensure page dimensions are calculated
    calculatePageDimensions();

    // Start first page
    startNewPage();

    qreal currentY = 0.0;
    qreal remainingHeight = m_textAreaHeight;

    // Iterate through all paragraphs
    for (int paraIndex = 0; paraIndex < paraCount; ++paraIndex) {
        // Ensure paragraph has layout
        m_layoutManager->layoutParagraph(paraIndex);
        const ParagraphLayout* layout = m_layoutManager->paragraphLayout(paraIndex);

        if (!layout) {
            continue;
        }

        const int lineCount = layout->lineCount();
        if (lineCount == 0) {
            continue;
        }

        // Process lines from this paragraph
        int lineStart = 0;
        while (lineStart < lineCount) {
            // Calculate how many lines fit on current page
            int linesAvailable = 0;
            qreal heightNeeded = 0.0;

            for (int line = lineStart; line < lineCount; ++line) {
                QRectF lineRect = layout->lineRect(line);
                qreal lineHeight = lineRect.height();

                if (heightNeeded + lineHeight <= remainingHeight) {
                    linesAvailable++;
                    heightNeeded += lineHeight;
                } else {
                    break;
                }
            }

            // Apply widow/orphan control
            int linesRemaining = lineCount - lineStart;
            int linesToAdd = linesAvailable;

            if (linesAvailable > 0 && linesAvailable < linesRemaining) {
                // We're breaking the paragraph across pages
                int linesForNextPage = linesRemaining - linesAvailable;

                // Orphan control: don't leave fewer than minLinesAtTop at top of next page
                if (linesForNextPage < m_paginationSettings.minLinesAtTop &&
                    linesAvailable >= m_paginationSettings.minLinesAtTop) {
                    // Push some lines to next page
                    linesToAdd = linesRemaining - m_paginationSettings.minLinesAtTop;
                    if (linesToAdd < 0) linesToAdd = 0;
                }

                // Widow control: don't leave fewer than minLinesAtBottom at bottom of page
                if (linesToAdd < m_paginationSettings.minLinesAtBottom && lineStart == 0) {
                    // Push entire paragraph to next page
                    linesToAdd = 0;
                }
            }

            // If no lines fit (or pushed to next page), start new page
            if (linesToAdd == 0) {
                // If paragraph is at start and nothing fits, we must add at least one line
                if (lineStart == 0 && remainingHeight >= m_textAreaHeight * 0.5) {
                    // Something is wrong, force at least one line
                    linesToAdd = 1;
                } else {
                    finalizeCurrentPage();
                    startNewPage();
                    currentY = 0.0;
                    remainingHeight = m_textAreaHeight;
                    continue;
                }
            }

            // Add content range to current page
            int lineEnd = lineStart + linesToAdd;
            ContentRange range;
            range.paragraphIndex = paraIndex;
            range.lineStart = lineStart;
            range.lineEnd = lineEnd;
            range.offsetY = currentY;

            m_pages.back().content.push_back(range);

            // Calculate actual height used
            qreal heightUsed = 0.0;
            for (int line = lineStart; line < lineEnd; ++line) {
                QRectF lineRect = layout->lineRect(line);
                heightUsed += lineRect.height();
            }

            currentY += heightUsed;
            remainingHeight -= heightUsed;
            lineStart = lineEnd;
        }
    }

    // Finalize last page
    finalizeCurrentPage();

    // Calculate total height
    if (!m_pages.empty()) {
        const PageInfo& lastPage = m_pages.back();
        m_totalHeight = lastPage.pageY + m_pageHeight;
    }

    m_valid = true;
    return static_cast<int>(m_pages.size());
}

void PageLayoutManager::invalidate() {
    m_valid = false;
}

bool PageLayoutManager::isValid() const {
    return m_valid;
}

int PageLayoutManager::totalPages() const {
    ensureValid();
    return static_cast<int>(m_pages.size());
}

// =============================================================================
// Page Information
// =============================================================================

const PageInfo* PageLayoutManager::pageInfo(int pageNumber) const {
    ensureValid();

    if (pageNumber < 1 || pageNumber > static_cast<int>(m_pages.size())) {
        return nullptr;
    }

    return &m_pages[pageNumber - 1];
}

int PageLayoutManager::pageForPosition(const CursorPosition& position) const {
    ensureValid();

    if (m_pages.empty()) {
        return 0;
    }

    // Find paragraph layout to get line info
    if (!m_layoutManager) {
        return 1;
    }

    m_layoutManager->layoutParagraph(position.paragraph);
    const ParagraphLayout* layout = m_layoutManager->paragraphLayout(position.paragraph);

    if (!layout) {
        return 1;
    }

    // Find which line the cursor is on
    int cursorLine = layout->lineForPosition(position.offset);
    if (cursorLine < 0) {
        cursorLine = 0;
    }

    // Search pages for this paragraph and line
    for (size_t pageIdx = 0; pageIdx < m_pages.size(); ++pageIdx) {
        const PageInfo& page = m_pages[pageIdx];

        for (const ContentRange& range : page.content) {
            if (range.paragraphIndex == position.paragraph) {
                if (cursorLine >= range.lineStart && cursorLine < range.lineEnd) {
                    return static_cast<int>(pageIdx + 1);
                }
            }
        }
    }

    // Default to last page if not found
    return static_cast<int>(m_pages.size());
}

int PageLayoutManager::pageAtY(qreal y) const {
    ensureValid();

    if (m_pages.empty() || y < 0) {
        return 0;
    }

    // Binary search for page containing y
    int left = 0;
    int right = static_cast<int>(m_pages.size()) - 1;

    while (left <= right) {
        int mid = (left + right) / 2;
        const PageInfo& page = m_pages[mid];

        if (y < page.pageY) {
            right = mid - 1;
        } else if (y >= page.pageY + m_pageHeight + m_pageLayout.pageGap) {
            left = mid + 1;
        } else {
            return mid + 1;  // 1-based
        }
    }

    // Return the page we're closest to
    if (left > 0) {
        return left;
    }
    return 1;
}

// =============================================================================
// Geometry
// =============================================================================

qreal PageLayoutManager::pageY(int pageNumber) const {
    const PageInfo* info = pageInfo(pageNumber);
    return info ? info->pageY : 0.0;
}

qreal PageLayoutManager::totalHeight() const {
    ensureValid();
    return m_totalHeight;
}

QRectF PageLayoutManager::pageRect(int pageNumber) const {
    const PageInfo* info = pageInfo(pageNumber);
    return info ? info->pageRect : QRectF();
}

QRectF PageLayoutManager::textAreaRect(int pageNumber) const {
    const PageInfo* info = pageInfo(pageNumber);
    return info ? info->textRect : QRectF();
}

qreal PageLayoutManager::pageCenterOffset() const {
    if (m_pageWidth <= 0 || m_viewportWidth <= 0) {
        return 0.0;
    }

    if (m_viewportWidth > m_pageWidth) {
        return (m_viewportWidth - m_pageWidth) / 2.0;
    }

    return 0.0;
}

// =============================================================================
// Private Methods
// =============================================================================

void PageLayoutManager::ensureValid() const {
    if (!m_valid) {
        const_cast<PageLayoutManager*>(this)->calculatePages();
    }
}

void PageLayoutManager::calculatePageDimensions() {
    // Get page size in pixels at current zoom and DPI (96 DPI assumed)
    // Note: pageSizePixels() already applies zoomLevel internally
    constexpr qreal dpi = 96.0;
    QSizeF pagePixels = m_pageLayout.pageSizePixels(dpi);

    m_pageWidth = pagePixels.width();
    m_pageHeight = pagePixels.height();

    // Calculate margins in pixels (apply zoom here since margins are in mm)
    constexpr qreal mmToInch = 1.0 / 25.4;
    m_marginsPixels = QMarginsF(
        m_pageLayout.margins.left() * mmToInch * dpi * m_pageLayout.zoomLevel,
        m_pageLayout.margins.top() * mmToInch * dpi * m_pageLayout.zoomLevel,
        m_pageLayout.margins.right() * mmToInch * dpi * m_pageLayout.zoomLevel,
        m_pageLayout.margins.bottom() * mmToInch * dpi * m_pageLayout.zoomLevel
    );

    // Calculate text area dimensions
    m_textAreaWidth = m_pageWidth - m_marginsPixels.left() - m_marginsPixels.right();
    m_textAreaHeight = m_pageHeight - m_marginsPixels.top() - m_marginsPixels.bottom();

    // Ensure positive dimensions
    if (m_textAreaWidth < 1.0) m_textAreaWidth = 1.0;
    if (m_textAreaHeight < 1.0) m_textAreaHeight = 1.0;
}

void PageLayoutManager::startNewPage() {
    PageInfo page;
    page.pageNumber = static_cast<int>(m_pages.size()) + 1;

    // Calculate Y position (after previous page + gap)
    if (m_pages.empty()) {
        page.pageY = 0.0;
    } else {
        const PageInfo& prevPage = m_pages.back();
        page.pageY = prevPage.pageY + m_pageHeight + m_pageLayout.pageGap;
    }

    // Calculate centered position
    qreal centerX = pageCenterOffset();

    // Page rectangle
    page.pageRect = QRectF(centerX, page.pageY, m_pageWidth, m_pageHeight);

    // Text area rectangle (inside margins)
    page.textRect = QRectF(
        centerX + m_marginsPixels.left(),
        page.pageY + m_marginsPixels.top(),
        m_textAreaWidth,
        m_textAreaHeight
    );

    m_pages.push_back(std::move(page));
}

void PageLayoutManager::finalizeCurrentPage() {
    // Current implementation doesn't need additional finalization
    // Future: could trim unused space, adjust text rect, etc.
}

}  // namespace kalahari::editor
