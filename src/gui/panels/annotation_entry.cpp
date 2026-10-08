/// @file annotation_entry.cpp
/// @brief What the Annotations panel shows: its filters and orders

#include "kalahari/gui/panels/annotation_entry.h"

#include <algorithm>
#include <tuple>

namespace kalahari::gui {

namespace {

/// @brief Whether an annotation made at a time passes the date filter
bool matchesDate(const QDateTime& created, AnnotationDateFilter date, const QDateTime& now) {
    if (date == AnnotationDateFilter::Any) {
        return true;
    }
    if (!created.isValid()) {
        return false;  // made at an unknown time
    }
    switch (date) {
    case AnnotationDateFilter::Today:
        return created.toLocalTime().date() == now.toLocalTime().date();
    case AnnotationDateFilter::Last7Days:
        return created >= now.addDays(-7);
    case AnnotationDateFilter::Last30Days:
        return created >= now.addDays(-30);
    case AnnotationDateFilter::Any:
        break;
    }
    return true;
}

}  // namespace

bool matchesExceptKind(const AnnotationEntry& entry, const AnnotationFilter& filter,
                       const QDateTime& now) {
    const editor::Annotation& annotation = entry.annotation;
    if (filter.state == AnnotationStateFilter::Open && annotation.done) {
        return false;
    }
    if (filter.state == AnnotationStateFilter::Done && !annotation.done) {
        return false;
    }
    if (!matchesDate(annotation.created, filter.date, now)) {
        return false;
    }
    const QString text = filter.text.trimmed();
    return text.isEmpty() || annotation.text.contains(text, Qt::CaseInsensitive);
}

bool matches(const AnnotationEntry& entry, const AnnotationFilter& filter, const QDateTime& now) {
    bool kindShown = true;
    switch (entry.annotation.kind) {
    case editor::AnnotationKind::Comment:
        kindShown = filter.comments;
        break;
    case editor::AnnotationKind::Todo:
        kindShown = filter.todos;
        break;
    case editor::AnnotationKind::Note:
        kindShown = filter.notes;
        break;
    }
    return kindShown && matchesExceptKind(entry, filter, now);
}

void sortEntries(std::vector<AnnotationEntry>& entries, AnnotationSort sort) {
    const auto textOrder = [](const AnnotationEntry& a, const AnnotationEntry& b) {
        return std::tie(a.chapterOrder, a.textOrder) < std::tie(b.chapterOrder, b.textOrder);
    };
    if (sort == AnnotationSort::TextOrder) {
        std::stable_sort(entries.begin(), entries.end(), textOrder);
        return;
    }

    // The newest first; those made at an unknown time last, in the order of the text
    std::stable_sort(entries.begin(), entries.end(),
                     [&textOrder](const AnnotationEntry& a, const AnnotationEntry& b) {
                         const QDateTime& timeA = a.annotation.created;
                         const QDateTime& timeB = b.annotation.created;
                         if (timeA.isValid() != timeB.isValid()) {
                             return timeA.isValid();
                         }
                         if (timeA.isValid() && timeA != timeB) {
                             return timeA > timeB;
                         }
                         return textOrder(a, b);
                     });
}

}  // namespace kalahari::gui
