/// @file editor_appearance.cpp
/// @brief Visual appearance configuration implementation (OpenSpec #00042 Phase 5)

#include <kalahari/editor/editor_appearance.h>

namespace kalahari::editor {

// =============================================================================
// PageLayout
// =============================================================================

QSizeF PageLayout::pageSizeMm() const
{
    switch (pageSize) {
        case PageSize::A4:
            return QSizeF(210.0, 297.0);
        case PageSize::A5:
            return QSizeF(148.0, 210.0);
        case PageSize::B5:
            return QSizeF(176.0, 250.0);
        case PageSize::Trade6x9:
            return QSizeF(152.4, 228.6);
        case PageSize::Letter:
            return QSizeF(215.9, 279.4);
        case PageSize::Legal:
            return QSizeF(215.9, 355.6);
        case PageSize::Custom:
            return QSizeF(customWidth, customHeight);
    }
    return QSizeF(210.0, 297.0);
}

PageLayout::PageSize PageLayout::pageSizeFromId(const QString& id)
{
    for (PageSize size : {PageSize::A4, PageSize::A5, PageSize::B5, PageSize::Trade6x9,
                          PageSize::Letter, PageSize::Legal, PageSize::Custom}) {
        if (pageSizeId(size) == id) {
            return size;
        }
    }
    return PageSize::A4;
}

QString PageLayout::pageSizeId(PageSize size)
{
    switch (size) {
        case PageSize::A4: return QStringLiteral("A4");
        case PageSize::A5: return QStringLiteral("A5");
        case PageSize::B5: return QStringLiteral("B5");
        case PageSize::Trade6x9: return QStringLiteral("6x9");
        case PageSize::Letter: return QStringLiteral("Letter");
        case PageSize::Legal: return QStringLiteral("Legal");
        case PageSize::Custom: return QStringLiteral("Custom");
    }
    return QStringLiteral("A4");
}

}  // namespace kalahari::editor
