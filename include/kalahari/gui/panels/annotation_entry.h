/// @file annotation_entry.h
/// @brief An annotation as the Annotations panel lists it, and what the panel shows

#pragma once

#include <kalahari/editor/annotation.h>

#include <QDateTime>
#include <QString>

#include <vector>

namespace kalahari::gui {

/// @brief What tells an annotation of the book from every other: its chapter and its id
/// @param elementId Its chapter in the book; empty: a document outside it
inline QString annotationKey(const QString& elementId, const QString& annotationId) {
    return elementId + QLatin1Char('/') + annotationId;
}

/// @brief An annotation with the chapter it is in
struct AnnotationEntry {
    editor::Annotation annotation;  ///< The annotation
    QString elementId;              ///< Its chapter in the book; empty: a document outside it
    QString chapterTitle;           ///< The title of its chapter (or document)
    int chapterOrder = 0;           ///< Where its chapter is in the book
    int textOrder = 0;              ///< Where it is in its chapter's text

    bool operator==(const AnnotationEntry& other) const = default;

    /// @brief What tells it from every other entry (see annotationKey())
    QString key() const { return annotationKey(elementId, annotation.id); }
};

/// @brief Which annotations the panel lists
enum class AnnotationScope {
    Chapter,  ///< Those of the chapter (or document) in front
    Book,     ///< Those of every chapter of the book
};

/// @brief Which annotations by their state
enum class AnnotationStateFilter {
    Open,  ///< Not done: open to-dos, unresolved comments and the notes
    Done,  ///< Done to-dos and resolved comments
    All,   ///< Every one
};

/// @brief Which annotations by when they were made
enum class AnnotationDateFilter {
    Any,         ///< Whenever (also when it is not known)
    Today,       ///< Today
    Last7Days,   ///< In the last 7 days
    Last30Days,  ///< In the last 30 days
};

/// @brief The order of the listed annotations
enum class AnnotationSort {
    TextOrder,  ///< As in the book: chapter by chapter, in the order of the text
    Newest,     ///< The newest first
    ByKind,     ///< Comments, to-dos, then notes; each kind in the order of the text
};

/// @brief Which annotations the panel shows
struct AnnotationFilter {
    QString text;                                               ///< Text they contain, in any case; empty: any
    bool comments = true;                                       ///< Comments are shown
    bool todos = true;                                          ///< To-dos are shown
    bool notes = true;                                          ///< Notes are shown
    AnnotationStateFilter state = AnnotationStateFilter::Open;  ///< By state
    AnnotationDateFilter date = AnnotationDateFilter::Any;      ///< By date
};

/// @brief Whether an entry passes the filter, its kind aside
///
/// The panel counts the annotations of each kind with it, also of a kind not shown.
/// @param now The time the date filter counts back from
bool matchesExceptKind(const AnnotationEntry& entry, const AnnotationFilter& filter,
                       const QDateTime& now);

/// @brief Whether an entry passes the filter
/// @param now The time the date filter counts back from
bool matches(const AnnotationEntry& entry, const AnnotationFilter& filter, const QDateTime& now);

/// @brief Put entries in an order
void sortEntries(std::vector<AnnotationEntry>& entries, AnnotationSort sort);

}  // namespace kalahari::gui
