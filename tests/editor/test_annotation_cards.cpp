/// @file test_annotation_cards.cpp
/// @brief The cards of the annotations under their paragraphs: the second way of showing the
///        annotations in the continuous view (marks only without them, and in the page view)

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/annotation_cards.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include "editor_test_utils.h"

#include <QContextMenuEvent>
#include <QImage>
#include <QLocale>
#include <QMouseEvent>
#include <QTextBlock>

#include <functional>
#include <memory>
#include <optional>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

namespace {

/// "One two three" with a comment on "two" and a note after "three", "four five" with a
/// to-do on its place after "four", "six seven" without annotations
QString cardsKml() {
    return QStringLiteral(
        "<kml><annotations>"
        "<annotation id=\"c1\" kind=\"comment\" author=\"Anna\" "
        "created=\"2026-10-24T09:30:00Z\">Check the date</annotation>"
        "<annotation id=\"n1\" kind=\"note\">A note</annotation>"
        "<annotation id=\"t1\" kind=\"todo\">Fix it</annotation>"
        "</annotations>"
        "<p>One <anchor ref=\"c1\">two</anchor> three<anchor ref=\"n1\"/></p>"
        "<p>four<anchor ref=\"t1\"/> five</p>"
        "<p>six seven</p></kml>");
}

/// An editor with the annotations of cardsKml(), sized like a small window, the cards shown
/// as @p show says
std::unique_ptr<BookEditor> cardsEditor(bool show = true) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(700, 600));
    editor->fromKml(cardsKml());
    EditorAppearance appearance = editor->appearance();
    appearance.annotationCards.show = show;
    editor->setAppearance(appearance);
    return editor;
}

/// The layout of an editor's text
KalahariTextDocumentLayout* layoutOf(const BookEditor& editor) {
    return qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
}

/// The room under a paragraph
qreal roomUnder(const BookEditor& editor, int paragraph) {
    return layoutOf(editor)->spaceBelow(paragraph);
}

/// Change the cards' settings
void setCards(BookEditor& editor, const std::function<void(AnnotationCardSettings&)>& change) {
    EditorAppearance appearance = editor.appearance();
    change(appearance.annotationCards);
    editor.setAppearance(appearance);
}

/// A point of an annotation's mark: around the end of its fragment, where the editor finds it
std::optional<QPointF> markPoint(const BookEditor& editor, int position, const QString& id) {
    const QRectF place = editor.placeRect(position, true);
    // Each pixel from the top of the place to 16 under it, 12 on each side
    for (int down = 0; down <= qRound(place.height()) + 16; ++down) {
        for (int across = -12; across <= 12; ++across) {
            const QPointF point(place.left() + across, place.top() + down);
            if (editor.annotationMarkAt(point) == id) {
                return point;
            }
        }
    }
    return std::nullopt;
}

/// A click of the left button
void click(BookEditor& editor, const QPointF& point) {
    QMouseEvent press(QEvent::MouseButtonPress, point, editor.mapToGlobal(point), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, point, editor.mapToGlobal(point),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &release);
}

/// A double click of the left button (after a click, as the system sends it)
void doubleClick(BookEditor& editor, const QPointF& point) {
    click(editor, point);
    QMouseEvent twice(QEvent::MouseButtonDblClick, point, editor.mapToGlobal(point),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &twice);
    QMouseEvent release(QEvent::MouseButtonRelease, point, editor.mapToGlobal(point),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &release);
}

/// The mouse moves over a point, without a button
void hover(BookEditor& editor, const QPointF& point) {
    QMouseEvent move(QEvent::MouseMove, point, editor.mapToGlobal(point), Qt::NoButton,
                     Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &move);
}

/// The texts of the context menu at a point (the right button), choosing @p choice
QStringList menuAt(BookEditor& editor, const QPointF& point, const QString& choice = QString()) {
    return runPopupMenu(
        [&editor, point]() {
            QContextMenuEvent event(QContextMenuEvent::Mouse, point.toPoint(),
                                    editor.mapToGlobal(point.toPoint()));
            QCoreApplication::sendEvent(&editor, &event);
        },
        choice);
}

/// Image of the editor with one pixel per logical pixel
QImage editorImage(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// Pixels of an area that differ between two images of the same size
int differingPixels(const QImage& a, const QImage& b, const QRect& area) {
    int count = 0;
    const QRect clipped = area & a.rect();
    for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
        for (int x = clipped.left(); x <= clipped.right(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace

TEST_CASE("Annotation cards: none by default; the setting shows them under the paragraphs",
          "[editor][annotations][cards]") {
    auto editor = cardsEditor(false);
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    CHECK_FALSE(editor->showsAnnotationCards());
    CHECK(roomUnder(*editor, 0) == 0.0);
    CHECK(editor->annotationCardRect(QStringLiteral("c1")).isEmpty());
    const qreal secondTop = layout->blockY(1);
    const QRectF wordBefore = editor->placeRect(4, false);

    bool shownSignal = false;
    QObject::connect(editor.get(), &BookEditor::annotationCardsShownChanged,
                     [&shownSignal](bool shown) { shownSignal = shown; });
    setCards(*editor, [](AnnotationCardSettings& cards) { cards.show = true; });
    REQUIRE(editor->showsAnnotationCards());
    CHECK(shownSignal);

    // Room under the paragraphs with annotations, none under the one without; the text stays
    // where it was and the room is part of its paragraph's height
    CHECK(roomUnder(*editor, 0) > 0.0);
    CHECK(roomUnder(*editor, 1) > 0.0);
    CHECK(roomUnder(*editor, 2) == 0.0);
    CHECK(layout->blockY(1) == Approx(secondTop + roomUnder(*editor, 0)));
    CHECK(layout->blockY(1) == Approx(layout->blockY(0) + layout->blockHeight(0)));
    CHECK(editor->placeRect(4, false) == wordBefore);

    // Under the paragraph of its mark, one under another in the order of the marks
    const QRectF comment = editor->annotationCardRect(QStringLiteral("c1"));
    const QRectF note = editor->annotationCardRect(QStringLiteral("n1"));
    const QRectF todo = editor->annotationCardRect(QStringLiteral("t1"));
    REQUIRE_FALSE(comment.isEmpty());
    REQUIRE_FALSE(note.isEmpty());
    REQUIRE_FALSE(todo.isEmpty());
    CHECK(comment.top() > editor->placeRect(7, true).bottom());
    CHECK(note.top() > comment.bottom());
    CHECK(todo.top() > note.bottom());
    CHECK(todo.bottom() < editor->placeRect(editor->textDocument()->findBlockByNumber(2).position(),
                                            false)
                              .top());
    // Indented from the text's left edge, as wide as the text's right edge allows
    const QRectF column = editor->textColumnRect();
    CHECK(comment.left() > column.left());
    CHECK(comment.right() == Approx(column.right()).margin(1.0));

    SECTION("drawn on the paper") {
        const QImage withCards = editorImage(*editor);
        setCards(*editor, [](AnnotationCardSettings& cards) { cards.show = false; });
        const QImage without = editorImage(*editor);
        CHECK(differingPixels(withCards, without, comment.toAlignedRect()) >
              comment.width() * comment.height() / 4);
    }

    SECTION("of the kinds the settings name") {
        setCards(*editor, [](AnnotationCardSettings& cards) { cards.notes = false; });
        CHECK(editor->annotationCardRect(QStringLiteral("n1")).isEmpty());
        CHECK_FALSE(editor->annotationCardRect(QStringLiteral("c1")).isEmpty());
        CHECK_FALSE(editor->showsAnnotationCard(AnnotationKind::Note));
        CHECK(editor->showsAnnotationCard(AnnotationKind::Comment));
    }

    SECTION("not in the page view, which shows the marks only") {
        editor->setViewMode(ViewMode::Page);
        CHECK_FALSE(editor->showsAnnotationCards());
        CHECK_FALSE(shownSignal);
        CHECK(roomUnder(*editor, 0) == 0.0);
        CHECK(editor->annotationCardRect(QStringLiteral("c1")).isEmpty());
        editor->setViewMode(ViewMode::Continuous);
        CHECK(editor->showsAnnotationCards());
        CHECK(roomUnder(*editor, 0) > 0.0);
    }

    SECTION("in Distraction-Free as the settings say") {
        editor->setDistractionFree(true);
        CHECK(editor->showsAnnotationCards());
        setCards(*editor, [](AnnotationCardSettings& cards) { cards.inDistractionFree = false; });
        CHECK_FALSE(editor->showsAnnotationCards());
        editor->setDistractionFree(false);
        CHECK(editor->showsAnnotationCards());
    }

    SECTION("a compact card is lower than a full one") {
        setCards(*editor, [](AnnotationCardSettings& cards) { cards.compact = true; });
        CHECK(editor->annotationCardRect(QStringLiteral("c1")).height() < comment.height());
    }

    SECTION("their text is sized after the chapter's text") {
        setCards(*editor, [](AnnotationCardSettings& cards) { cards.fontPercent = 130; });
        CHECK(editor->annotationCardRect(QStringLiteral("c1")).height() > comment.height());
    }

    SECTION("an annotation done has no card") {
        Annotation done;
        for (const AnnotationPlace& place : editor->annotations()) {
            if (place.annotation.id == QStringLiteral("t1")) {
                done = place.annotation;
            }
        }
        done.done = true;
        REQUIRE(editor->updateAnnotation(done));
        CHECK(editor->annotationCardRect(QStringLiteral("t1")).isEmpty());
        CHECK(roomUnder(*editor, 1) == 0.0);
    }
}

TEST_CASE("Annotation cards: a click on a mark collapses or expands its card",
          "[editor][annotations][cards]") {
    auto editor = cardsEditor();
    const QRectF expanded = editor->annotationCardRect(QStringLiteral("c1"));
    const qreal room = roomUnder(*editor, 0);
    const std::optional<QPointF> mark = markPoint(*editor, 7, QStringLiteral("c1"));
    REQUIRE(mark.has_value());
    QStringList clicked;
    QObject::connect(editor.get(), &BookEditor::annotationMarkClicked,
                     [&clicked](const QString& id) { clicked << id; });
    editor->setCursorPosition({2, 1});

    // Collapsed: its mark alone; the cursor stays
    click(*editor, *mark);
    CHECK(editor->isAnnotationCollapsed(QStringLiteral("c1")));
    CHECK(editor->annotationCardRect(QStringLiteral("c1")).isEmpty());
    CHECK(roomUnder(*editor, 0) < room);
    CHECK(clicked.isEmpty());
    CHECK(editor->cursorPosition() == CursorPosition{2, 1});

    // Expanded again
    click(*editor, *mark);
    CHECK_FALSE(editor->isAnnotationCollapsed(QStringLiteral("c1")));
    CHECK(editor->annotationCardRect(QStringLiteral("c1")) == expanded);

    SECTION("collapsed to a line with the start of its text, which expands it") {
        setCards(*editor, [](AnnotationCardSettings& cards) { cards.collapsedLine = true; });
        editor->setAnnotationCollapsed(QStringLiteral("c1"), true);
        const QRectF line = editor->annotationCardRect(QStringLiteral("c1"));
        REQUIRE_FALSE(line.isEmpty());
        CHECK(line.height() < expanded.height() / 2.0);
        click(*editor, line.center());
        CHECK_FALSE(editor->isAnnotationCollapsed(QStringLiteral("c1")));
    }

    SECTION("without cards the mark opens the annotation") {
        setCards(*editor, [](AnnotationCardSettings& cards) { cards.show = false; });
        click(*editor, *mark);
        CHECK(clicked == QStringList{QStringLiteral("c1")});
    }

    SECTION("all collapsed or expanded, also those made later") {
        editor->setAllAnnotationsCollapsed(true);
        CHECK(roomUnder(*editor, 0) == 0.0);
        CHECK(roomUnder(*editor, 1) == 0.0);
        editor->setCursorPosition({2, 3});
        const Annotation added =
            editor->addAnnotation(AnnotationKind::Comment, QStringLiteral("New"), QString());
        CHECK(editor->isAnnotationCollapsed(added.id));
        CHECK(roomUnder(*editor, 2) == 0.0);
        editor->setAllAnnotationsCollapsed(false);
        CHECK(roomUnder(*editor, 2) > 0.0);
        CHECK_FALSE(editor->isAnnotationCollapsed(QStringLiteral("c1")));
    }

    SECTION("a chapter opens with them collapsed as the settings say") {
        setCards(*editor, [](AnnotationCardSettings& cards) { cards.startCollapsed = true; });
        CHECK_FALSE(editor->isAnnotationCollapsed(QStringLiteral("c1")));  // until it opens
        editor->fromKml(cardsKml());
        CHECK(editor->isAnnotationCollapsed(QStringLiteral("c1")));
        CHECK(roomUnder(*editor, 0) == 0.0);
    }
}

TEST_CASE("Annotation cards: a click on a card goes to its annotation, a double click edits it",
          "[editor][annotations][cards]") {
    auto editor = cardsEditor();
    const QRectF comment = editor->annotationCardRect(QStringLiteral("c1"));
    REQUIRE_FALSE(comment.isEmpty());
    QStringList clicked;
    QStringList edited;
    QObject::connect(editor.get(), &BookEditor::annotationCardClicked,
                     [&clicked](const QString& id) { clicked << id; });
    QObject::connect(editor.get(), &BookEditor::annotationEditRequested,
                     [&edited](const QString& id) { edited << id; });

    // Its fragment is selected
    const QPointF inCard(comment.center().x(), comment.bottom() - 4.0);
    click(*editor, inCard);
    CHECK(clicked == QStringList{QStringLiteral("c1")});
    CHECK(editor->selectedText() == QStringLiteral("two"));
    CHECK(edited.isEmpty());

    doubleClick(*editor, inCard);
    CHECK(edited == QStringList{QStringLiteral("c1")});
    CHECK(editor->selectedText() == QStringLiteral("two"));

    SECTION("an arrow over a card, its fragment highlighted while the mouse is on it") {
        editor->setCursorPosition({2, 0});
        const QRect fragment = editor->placeRect(4, false).united(editor->placeRect(7, true))
                                   .toAlignedRect();
        const QImage before = editorImage(*editor);
        hover(*editor, inCard);
        CHECK(editor->cursor().shape() == Qt::ArrowCursor);
        const QImage pointed = editorImage(*editor);
        CHECK(differingPixels(before, pointed, fragment) > 0);

        setCards(*editor, [](AnnotationCardSettings& cards) { cards.highlightFragment = false; });
        hover(*editor, QPointF(5.0, 5.0));
        hover(*editor, inCard);
        CHECK(differingPixels(before, editorImage(*editor), fragment) == 0);
    }

    SECTION("the menu of a card") {
        const QStringList items = menuAt(*editor, inCard, QStringLiteral("Mark as &Resolved"));
        CHECK(items == QStringList{QStringLiteral("&Edit Annotation"),
                                   QStringLiteral("Collap&se Annotation"),
                                   QStringLiteral("Mark as &Resolved"),
                                   QStringLiteral("Remo&ve Annotation"),
                                   QStringLiteral("Card &Appearance and Colors...")});
        // Resolved: in the panel only, without a card
        CHECK(editor->annotationCardRect(QStringLiteral("c1")).isEmpty());
    }

    SECTION("the box of a to-do marks it done") {
        const QRectF todo = editor->annotationCardRect(QStringLiteral("t1"));
        REQUIRE_FALSE(todo.isEmpty());
        // The box is on the left of the header, after the bar
        bool done = false;
        for (int across = 2; across < 40 && !done; across += 2) {
            for (int down = 2; down < 30 && !done; down += 2) {
                const QPointF point(todo.left() + across, todo.top() + down);
                editor->setCursorPosition({2, 0});
                hover(*editor, point);
                if (editor->cursor().shape() == Qt::PointingHandCursor) {
                    click(*editor, point);
                    done = editor->annotationCardRect(QStringLiteral("t1")).isEmpty();
                }
            }
        }
        CHECK(done);
    }
}

TEST_CASE("Annotation cards: the menu of the text has the annotation at the click",
          "[editor][annotations][cards]") {
    for (const bool cards : {false, true}) {
        auto editor = cardsEditor(cards);
        QStringList edited;
        QObject::connect(editor.get(), &BookEditor::annotationEditRequested,
                         [&edited](const QString& id) { edited << id; });
        const QPointF inFragment = editor->placeRect(5, false).center();
        const QStringList items = menuAt(*editor, inFragment, QStringLiteral("&Edit Annotation"));
        INFO("cards " << cards);
        REQUIRE(items.size() > 4);
        // Its kind and the start of its text, quoted as the language quotes
        CHECK(items[0] == QStringLiteral("Comment: ") +
                              QLocale().quoteString(QStringLiteral("Check the date")));
        CHECK(items[1] == QStringLiteral("&Edit Annotation"));
        CHECK(items.contains(QStringLiteral("Collap&se Annotation")) == cards);
        CHECK(items.contains(QStringLiteral("Mark as &Resolved")));
        CHECK(items.contains(QStringLiteral("Remo&ve Annotation")));
        CHECK_FALSE(items.contains(QStringLiteral("Card &Appearance and Colors...")));
        CHECK(edited == QStringList{QStringLiteral("c1")});

        // None in the text without annotations
        const QStringList plain = menuAt(*editor, editor->placeRect(
            editor->textDocument()->findBlockByNumber(2).position() + 1, false).center());
        CHECK_FALSE(plain.contains(QStringLiteral("&Edit Annotation")));
    }
}

TEST_CASE("Annotation cards: the room of the frame an annotation is written in",
          "[editor][annotations][cards]") {
    auto editor = cardsEditor();
    const int sixStart = editor->textDocument()->findBlockByNumber(2).position();

    // A new one on "seven": its room under its paragraph
    editor->setAnnotationCardWriting(QString(), AnnotationKind::Note, sixStart + 4, sixStart + 9,
                                     80.0);
    const QRectF room = editor->annotationCardRect(QString());
    REQUIRE_FALSE(room.isEmpty());
    CHECK(room.height() == Approx(80.0).margin(0.5));
    CHECK(roomUnder(*editor, 2) > 0.0);

    // As high as the frame, at any zoom
    editor->setAnnotationCardWritingHeight(120.0);
    CHECK(editor->annotationCardRect(QString()).height() == Approx(120.0).margin(0.5));
    editor->setZoomFactor(1.5);
    CHECK(editor->annotationCardRect(QString()).height() == Approx(120.0).margin(0.5));
    editor->setZoomFactor(1.0);

    editor->clearAnnotationCardWriting();
    CHECK(editor->annotationCardRect(QString()).isEmpty());
    CHECK(roomUnder(*editor, 2) == 0.0);

    // An annotation edited: its card keeps the frame's room, also when it was collapsed
    editor->setAnnotationCollapsed(QStringLiteral("n1"), true);
    editor->setAnnotationCardWriting(QStringLiteral("n1"), AnnotationKind::Note, 13, 13, 90.0);
    CHECK(editor->annotationCardRect(QStringLiteral("n1")).height() == Approx(90.0).margin(0.5));
    editor->clearAnnotationCardWriting();
    CHECK(editor->annotationCardRect(QStringLiteral("n1")).isEmpty());
}

TEST_CASE("Annotation cards: long texts cut short, and shown whole", "[editor][annotations][cards]") {
    auto editor = cardsEditor();
    Annotation comment;
    for (const AnnotationPlace& place : editor->annotations()) {
        if (place.annotation.id == QStringLiteral("c1")) {
            comment = place.annotation;
        }
    }
    comment.text = QStringLiteral("1\n2\n3\n4\n5\n6\n7\n8");
    REQUIRE(editor->updateAnnotation(comment));
    const QRectF whole = editor->annotationCardRect(QStringLiteral("c1"));

    AnnotationCards cards;
    AnnotationCardSettings settings;
    settings.longLines = 3;
    cards.setSettings(settings);
    cards.setShown(true);
    cards.setTextFont(editor->textDocument()->defaultFont());
    const QTextBlock first = editor->textDocument()->firstBlock();
    const qreal width = editor->textDocument()->textWidth();
    std::vector<AnnotationCardBox> boxes = cards.cardsOf(first, 0.0, width);
    REQUIRE(boxes.size() == 2);
    CHECK(boxes[0].mark.id == QStringLiteral("c1"));
    CHECK(boxes[0].shownLines == 3);
    CHECK(boxes[1].shownLines == 0);  // a short one is whole

    // "Show all" under its text
    std::optional<AnnotationCardHit> showAll;
    const QRectF card = boxes[0].rect;
    for (int down = 0; down < qRound(card.height()) && !showAll; ++down) {
        const std::optional<AnnotationCardHit> hit =
            cards.hitTest(first, 0.0, width, QPointF(card.left() + 30.0, card.top() + down));
        if (hit && hit->part == AnnotationCardPart::ShowAll) {
            showAll = hit;
        }
    }
    CHECK(showAll.has_value());

    cards.showWhole(QStringLiteral("c1"));
    boxes = cards.cardsOf(first, 0.0, width);
    CHECK(boxes[0].shownLines == 0);
    CHECK(boxes[0].rect.height() > card.height());

    // In the editor: as the settings say
    setCards(*editor, [](AnnotationCardSettings& settings) { settings.longLines = 3; });
    CHECK(editor->annotationCardRect(QStringLiteral("c1")).height() < whole.height());
}
