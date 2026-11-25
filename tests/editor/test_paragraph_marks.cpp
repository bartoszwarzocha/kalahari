// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_paragraph_marks.cpp
/// @brief The marks beside the paragraphs (the paperclips of the materials linked with them,
///        the signs of the kinds of the cards linked with them beside the paperclips, the
///        scissors of the clippings cut from them): in the left margin in the color of the
///        text, with their paragraphs as the text is edited, chosen with a click, a menu or
///        the context menu of the paragraph

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_types.h>
#include "kalahari_tests/editor/editor_test_utils.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QHelpEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QTimer>
#include <QToolTip>

#include <memory>
#include <optional>
#include <utility>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

const QStringList TEXT{QStringLiteral("The storm came from the west."),
                       QStringLiteral("- We turn back - said Tau."),
                       QStringLiteral("- And the water? - asked Nadia."),
                       QStringLiteral("Tau was silent for a long time.")};

/// Editor holding @p paragraphs, sized like a small window
std::unique_ptr<BookEditor> editorWith(const QStringList& paragraphs) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(kmlOf(paragraphs));
    return editor;
}

/// A red square: the marks show it in the color of the text
QIcon redIcon() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::red);
    return QIcon(pixmap);
}

/// The mark of paragraph @p paragraph naming @p ids with @p titles
ParagraphMark markOf(int paragraph, const QStringList& ids, const QStringList& titles,
                     ParagraphMarkKind kind = ParagraphMarkKind::Materials) {
    return ParagraphMark{.paragraph = paragraph, .ids = ids, .titles = titles, .kind = kind};
}

/// The scissors of paragraph @p paragraph naming clippings @p ids with @p titles
ParagraphMark scissorsOf(int paragraph, const QStringList& ids, const QStringList& titles) {
    return markOf(paragraph, ids, titles, ParagraphMarkKind::Clippings);
}

/// A blue square: the scissors, beside the red paperclip
QIcon blueIcon() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::blue);
    return {pixmap};
}

/// Image of the editor with one pixel per logical pixel (see test_editor_stage2.cpp)
QImage editorImage(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// The smallest rectangle around the pixels that differ between two images of the same size
QRect differingArea(const QImage& a, const QImage& b) {
    QRect area;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                area |= QRect(x, y, 1, 1);
            }
        }
    }
    return area;
}

/// Caret rectangle at a position (moves the cursor there)
QRectF caretAt(BookEditor& editor, const CursorPosition& position) {
    editor.setCursorPosition(position);
    return editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
}

/// A click of the left button at @p point
void click(BookEditor& editor, const QPointF& point) {
    QMouseEvent press(QEvent::MouseButtonPress, point, editor.mapToGlobal(point), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, point, editor.mapToGlobal(point),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &release);
}

/// Key @p key pressed in @p editor
void press(BookEditor& editor, Qt::Key key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &event);
}

/// The paragraphs the marks of @p editor are at
QList<int> markedParagraphs(const BookEditor& editor) {
    QList<int> paragraphs;
    for (const ParagraphMark& mark : editor.paragraphMarks()) {
        paragraphs << mark.paragraph;
    }
    return paragraphs;
}

/// The point of the mark of kind @p kind beside paragraph @p paragraph, found from the text
/// leftwards; null when there is none
QPointF markPoint(BookEditor& editor, int paragraph, ParagraphMarkKind kind) {
    const CursorPosition cursor = editor.cursorPosition();
    const QRectF line = caretAt(editor, {.paragraph = paragraph, .offset = 0});
    editor.setCursorPosition(cursor);
    editorImage(editor);  // shown: the marks of the paragraphs in view are found
    for (int step = 1; step < static_cast<int>(line.left()); ++step) {
        const QPointF point(line.left() - step, line.center().y());
        if (const std::optional<ParagraphMark> mark = editor.paragraphMarkAt(point);
            mark && mark->kind == kind) {
            return point;
        }
    }
    return {};
}

/// In the first popup menu that shows, the entry @p submenu leads to; its entry @p choice is
/// chosen. The texts of the entries of the submenu are noted; none without it.
class SubmenuChooser : public QObject {
public:
    SubmenuChooser(QString submenu, QString choice)
        : m_submenu(std::move(submenu)), m_choice(std::move(choice)) {
        QCoreApplication::instance()->installEventFilter(this);
    }
    ~SubmenuChooser() override { QCoreApplication::instance()->removeEventFilter(this); }
    SubmenuChooser(const SubmenuChooser&) = delete;
    SubmenuChooser& operator=(const SubmenuChooser&) = delete;

    QStringList texts;  ///< The entries of the submenu

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        auto* menu = qobject_cast<QMenu*>(watched);
        if (menu != nullptr && event->type() == QEvent::Show && menu->parent() != nullptr &&
            qobject_cast<QMenu*>(menu->parent()) == nullptr) {
            menu->setAttribute(Qt::WA_DontShowOnScreen);
            QAction* chosen = nullptr;
            for (QAction* action : menu->actions()) {
                if (action->menu() != nullptr && action->text() == m_submenu) {
                    for (QAction* item : action->menu()->actions()) {
                        texts << item->text();
                        if (item->text() == m_choice) {
                            chosen = item;
                        }
                    }
                }
            }
            if (chosen != nullptr) {
                chosen->trigger();
            }
            QTimer::singleShot(0, menu, [menu]() { menu->close(); });
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QString m_submenu;
    QString m_choice;
};

}  // namespace

TEST_CASE("Paragraph marks: a paperclip in the left margin, in the color of the text",
          "[editor][marks]") {
    auto marked = editorWith(TEXT);
    auto plain = editorWith(TEXT);
    marked->setParagraphMarks({markOf(1, {QStringLiteral("m1")}, {QStringLiteral("Map")}),
                               markOf(9, {QStringLiteral("m9")}, {QStringLiteral("Far")})},
                              redIcon());

    // A paragraph past the end of the text has none
    CHECK(marked->paragraphMarks() ==
          QList<ParagraphMark>{markOf(1, {QStringLiteral("m1")}, {QStringLiteral("Map")})});

    // Left of the text, beside the middle of the first line of its paragraph
    const QRectF line = caretAt(*marked, {1, 0});
    caretAt(*marked, {3, 4});
    plain->setCursorPosition(marked->cursorPosition());
    const QImage withMark = editorImage(*marked);
    const QRect mark = differingArea(withMark, editorImage(*plain));
    REQUIRE_FALSE(mark.isEmpty());
    INFO("mark " << mark.x() << "," << mark.y() << " " << mark.width() << "x" << mark.height()
                 << ", line at " << line.left() << "," << line.top());
    CHECK(mark.right() < line.left());
    CHECK(mark.left() > 0);
    CHECK(mark.top() <= line.center().y());
    CHECK(mark.bottom() >= line.center().y());
    CHECK(mark.height() <= line.height());

    // Not in the red of the icon: in the color of the text, read on the paper
    for (int y = mark.top(); y <= mark.bottom(); ++y) {
        for (int x = mark.left(); x <= mark.right(); ++x) {
            const QColor pixel = QColor::fromRgb(withMark.pixel(x, y));
            REQUIRE(pixel.red() - pixel.green() < 40);
        }
    }

    // Where the mouse finds it; not over the text
    const auto found = marked->paragraphMarkAt(QPointF(mark.center()));
    REQUIRE(found.has_value());
    CHECK(found->ids == QStringList{QStringLiteral("m1")});
    CHECK_FALSE(marked->paragraphMarkAt(line.center() + QPointF(20.0, 0.0)).has_value());

    SECTION("a click chooses its material; the cursor stays") {
        QStringList chosen;
        QObject::connect(marked.get(), &BookEditor::paragraphMarkClicked,
                         [&chosen](const QString& id) { chosen << id; });
        click(*marked, QPointF(mark.center()));
        CHECK(chosen == QStringList{QStringLiteral("m1")});
        CHECK(marked->cursorPosition() == CursorPosition{3, 4});
    }

    SECTION("the pointer is a hand over it, and its tooltip names the materials") {
        const QPointF point(mark.center());
        QMouseEvent over(QEvent::MouseMove, point, marked->mapToGlobal(point), Qt::NoButton,
                         Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(marked.get(), &over);
        CHECK(marked->cursor().shape() == Qt::PointingHandCursor);
        QHelpEvent help(QEvent::ToolTip, point.toPoint(), marked->mapToGlobal(point.toPoint()));
        QCoreApplication::sendEvent(marked.get(), &help);
        CHECK(QToolTip::text().contains(QStringLiteral("<b>Linked materials</b><br>Map")));
        QToolTip::hideText();
    }

    SECTION("new marks take the place of the old ones; none, no paperclip") {
        marked->setParagraphMarks({}, redIcon());
        CHECK(marked->paragraphMarks().isEmpty());
        CHECK(differingArea(editorImage(*marked), editorImage(*plain)).isEmpty());
    }
}

TEST_CASE("Paragraph marks: a mark stays with its paragraph as the text is edited",
          "[editor][marks]") {
    auto editor = editorWith(TEXT);
    editor->setParagraphMarks({markOf(1, {QStringLiteral("m1")}, {QStringLiteral("Map")}),
                               markOf(2, {QStringLiteral("m2")}, {QStringLiteral("Photo")})},
                              redIcon());
    REQUIRE(markedParagraphs(*editor) == (QList<int>{1, 2}));

    // A paragraph added before them, at the start of the text or at the start of the marked
    // paragraph: they go down with their text
    editor->setCursorPosition({0, 0});
    press(*editor, Qt::Key_Return);
    CHECK(markedParagraphs(*editor) == (QList<int>{2, 3}));
    editor->setCursorPosition({2, 0});
    press(*editor, Qt::Key_Return);
    CHECK(markedParagraphs(*editor) == (QList<int>{3, 4}));
    CHECK(editor->paragraphMarks().first().titles == QStringList{QStringLiteral("Map")});

    // Undone, they go back
    editor->undo();
    editor->undo();
    CHECK(markedParagraphs(*editor) == (QList<int>{1, 2}));

    // A paragraph joined to the one before: its mark goes with its text; two marks on one
    // paragraph are one, naming the materials of both
    editor->setCursorPosition({2, 0});
    press(*editor, Qt::Key_Backspace);
    REQUIRE(editor->paragraphMarks().size() == 1);
    const ParagraphMark joined = editor->paragraphMarks().first();
    CHECK(joined.paragraph == 1);
    CHECK(joined.ids == (QStringList{QStringLiteral("m1"), QStringLiteral("m2")}));
    CHECK(joined.titles == (QStringList{QStringLiteral("Map"), QStringLiteral("Photo")}));

    // The whole text replaced: the marks stay at the paragraphs of their numbers
    editor->setParagraphMarks({markOf(2, {QStringLiteral("m2")}, {QStringLiteral("Photo")})},
                              redIcon());
    editor->replaceWithKml(kmlOf({QStringLiteral("One."), QStringLiteral("Two."),
                                  QStringLiteral("Three.")}));
    CHECK(markedParagraphs(*editor) == QList<int>{2});

    // A text loaded has none
    editor->fromKml(kmlOf(TEXT));
    CHECK(editor->paragraphMarks().isEmpty());
}

TEST_CASE("Paragraph marks: a mark of several materials chooses one in a menu; the context "
          "menu of the paragraph has them too",
          "[editor][marks]") {
    auto editor = editorWith(TEXT);
    editor->setParagraphMarks({markOf(1, {QStringLiteral("m1"), QStringLiteral("m2")},
                                      {QStringLiteral("Map"), QStringLiteral("Salt & Water")})},
                              redIcon());
    QStringList chosen;
    QObject::connect(editor.get(), &BookEditor::paragraphMarkClicked,
                     [&chosen](const QString& id) { chosen << id; });

    // A click on the mark: a menu with the titles, a "&" shown as it is
    const QRectF line = caretAt(*editor, {1, 0});
    editorImage(*editor);  // shown: the marks of the paragraphs in view are found
    QPointF point;
    for (double x = line.left() - 1.0; x > 0.0 && point.isNull(); x -= 1.0) {
        if (editor->paragraphMarkAt(QPointF(x, line.center().y()))) {
            point = QPointF(x, line.center().y());
        }
    }
    REQUIRE_FALSE(point.isNull());
    const QStringList items = runPopupMenu([&]() { click(*editor, point); },
                                           QStringLiteral("Salt && Water"));
    CHECK(items == (QStringList{QStringLiteral("Map"), QStringLiteral("Salt && Water")}));
    CHECK(chosen == QStringList{QStringLiteral("m2")});

    // The context menu of the paragraph, also from the keyboard: Linked Materials, with them
    chosen.clear();
    {
        SubmenuChooser chooser(QStringLiteral("Linked Mate&rials"), QStringLiteral("Map"));
        editor->setCursorPosition({1, 3});
        QContextMenuEvent menuKey(QContextMenuEvent::Keyboard, QPoint(), QPoint());
        QCoreApplication::sendEvent(editor.get(), &menuKey);
        CHECK(chooser.texts ==
              (QStringList{QStringLiteral("Map"), QStringLiteral("Salt && Water")}));
    }
    CHECK(chosen == QStringList{QStringLiteral("m1")});

    // Another paragraph has no such entry
    {
        SubmenuChooser chooser(QStringLiteral("Linked Mate&rials"), QString());
        editor->setCursorPosition({0, 3});
        QContextMenuEvent menuKey(QContextMenuEvent::Keyboard, QPoint(), QPoint());
        QCoreApplication::sendEvent(editor.get(), &menuKey);
        CHECK(chooser.texts.isEmpty());
    }
}

TEST_CASE("Paragraph marks: the scissors of the clippings, left of the paperclip, each kind "
          "set on its own",
          "[editor][marks][clippings]") {
    auto both = editorWith(TEXT);
    auto clipped = editorWith(TEXT);
    const ParagraphMark paperclip =
        markOf(1, {QStringLiteral("m1")}, {QStringLiteral("Map")});
    const QString rained = QStringLiteral("\"Rained\" (3 words, today, 7:42)");
    both->setParagraphMarks({paperclip}, redIcon());
    clipped->setParagraphMarks({paperclip}, redIcon());
    // The kind given is theirs, whatever the marks say
    both->setParagraphMarks(
        {markOf(1, {QStringLiteral("c1")}, {rained}),
         scissorsOf(2, {QStringLiteral("c2")}, {QStringLiteral("\"Wind\" (1 word, today, 7:40)")})},
        blueIcon(), ParagraphMarkKind::Clippings);

    // Beside one paragraph, the paperclip, then the scissors
    const QList<ParagraphMark> marks = both->paragraphMarks();
    REQUIRE(marks.size() == 3);
    CHECK(marks.at(0) == paperclip);
    CHECK(marks.at(1) == scissorsOf(1, {QStringLiteral("c1")}, {rained}));
    CHECK(marks.at(2).paragraph == 2);
    CHECK(marks.at(2).kind == ParagraphMarkKind::Clippings);

    // The scissors left of the paperclip, which stays where it was
    both->setCursorPosition({.paragraph = 3, .offset = 4});
    clipped->setCursorPosition({.paragraph = 3, .offset = 4});
    const QPointF clip = markPoint(*both, 1, ParagraphMarkKind::Materials);
    const QPointF scissors = markPoint(*both, 1, ParagraphMarkKind::Clippings);
    REQUIRE_FALSE(clip.isNull());
    REQUIRE_FALSE(scissors.isNull());
    CHECK(scissors.x() < clip.x());
    CHECK(clip == markPoint(*clipped, 1, ParagraphMarkKind::Materials));
    CHECK(both->paragraphMarkAt(scissors)->ids == QStringList{QStringLiteral("c1")});
    // Alone beside its paragraph, a mark is next to the text
    const QPointF alone = markPoint(*both, 2, ParagraphMarkKind::Clippings);
    REQUIRE_FALSE(alone.isNull());
    CHECK(alone.x() > scissors.x());

    // Drawn in the color of the text, as the paperclip: the blue of the icon is not shown
    const QImage image = editorImage(*both);
    const QImage withoutScissors = editorImage(*clipped);
    const QRect drawn = differingArea(image, withoutScissors);
    REQUIRE_FALSE(drawn.isEmpty());
    for (int y = drawn.top(); y <= drawn.bottom(); ++y) {
        for (int x = drawn.left(); x <= drawn.right(); ++x) {
            const QColor pixel = QColor::fromRgb(image.pixel(x, y));
            REQUIRE(pixel.blue() - pixel.green() < 40);
        }
    }

    SECTION("its tooltip says what was cut there, and what a click does") {
        QHelpEvent help(QEvent::ToolTip, scissors.toPoint(),
                        both->mapToGlobal(scissors.toPoint()));
        QCoreApplication::sendEvent(both.get(), &help);
        CHECK(QToolTip::text().contains(
            QStringLiteral("<b>Cut here</b><br>&quot;Rained&quot; (3 words, today, 7:42)<br><i>A "
                           "click or the menu of the paragraph: Show Clipping, Restore "
                           "Here</i>")));
        QToolTip::hideText();
    }

    SECTION("new paperclips leave the scissors; no scissors leave the paperclips") {
        both->setParagraphMarks({}, redIcon());
        REQUIRE(both->paragraphMarks().size() == 2);
        CHECK(both->paragraphMarks().first().kind == ParagraphMarkKind::Clippings);
        both->setParagraphMarks({paperclip}, redIcon());
        both->setParagraphMarks({}, blueIcon(), ParagraphMarkKind::Clippings);
        CHECK(both->paragraphMarks() == QList<ParagraphMark>{paperclip});
        CHECK(differingArea(editorImage(*both), editorImage(*clipped)).isEmpty());
    }

    SECTION("a paragraph joined to the one before: the marks of each kind are one") {
        both->setCursorPosition({.paragraph = 2, .offset = 0});
        press(*both, Qt::Key_Backspace);
        const QList<ParagraphMark> joined = both->paragraphMarks();
        REQUIRE(joined.size() == 2);
        CHECK(joined.at(0) == paperclip);
        CHECK(joined.at(1).ids == (QStringList{QStringLiteral("c1"), QStringLiteral("c2")}));
    }
}

TEST_CASE("Paragraph marks: the scissors show the clipping or restore it there, from a click "
          "or the context menu",
          "[editor][marks][clippings]") {
    auto editor = editorWith(TEXT);
    editor->setParagraphMarks({scissorsOf(1, {QStringLiteral("c1")}, {QStringLiteral("Rained")})},
                              blueIcon(), ParagraphMarkKind::Clippings);
    QStringList shown;
    QStringList restored;
    QObject::connect(editor.get(), &BookEditor::paragraphMarkClicked,
                     [&shown](const QString& id) { shown << id; });
    QObject::connect(editor.get(), &BookEditor::clippingRestoreRequested,
                     [&restored](const QString& id) { restored << id; });
    const QPointF point = markPoint(*editor, 1, ParagraphMarkKind::Clippings);
    REQUIRE_FALSE(point.isNull());
    const QStringList items{QStringLiteral("&Show Clipping"), QStringLiteral("&Restore Here")};

    // A click: a menu, also for one clipping; the cursor stays
    editor->setCursorPosition({.paragraph = 3, .offset = 4});
    CHECK(runPopupMenu([&]() { click(*editor, point); }, QStringLiteral("&Restore Here")) ==
          items);
    CHECK(restored == QStringList{QStringLiteral("c1")});
    CHECK(runPopupMenu([&]() { click(*editor, point); }, QStringLiteral("&Show Clipping")) ==
          items);
    CHECK(shown == QStringList{QStringLiteral("c1")});
    CHECK(editor->cursorPosition() == CursorPosition{3, 4});

    // The context menu of the paragraph, from the keyboard: Clippings From Here
    {
        SubmenuChooser chooser(QStringLiteral("Clippings &From Here"),
                               QStringLiteral("&Restore Here"));
        editor->setCursorPosition({.paragraph = 1, .offset = 3});
        QContextMenuEvent menuKey(QContextMenuEvent::Keyboard, QPoint(), QPoint());
        QCoreApplication::sendEvent(editor.get(), &menuKey);
        CHECK(chooser.texts == items);
    }
    CHECK(restored == (QStringList{QStringLiteral("c1"), QStringLiteral("c1")}));

    // A text open read-only: the clipping is shown, not restored
    editor->setReadOnly(true);
    {
        SubmenuChooser chooser(QStringLiteral("Clippings &From Here"),
                               QStringLiteral("&Restore Here"));
        editor->setCursorPosition({.paragraph = 1, .offset = 3});
        QContextMenuEvent menuKey(QContextMenuEvent::Keyboard, QPoint(), QPoint());
        QCoreApplication::sendEvent(editor.get(), &menuKey);
        CHECK(chooser.texts == items);
    }
    CHECK(restored.size() == 2);
    editor->setReadOnly(false);

    // Several clippings cut from one paragraph: a submenu of each, by its title
    editor->setParagraphMarks(
        {scissorsOf(1, {QStringLiteral("c1"), QStringLiteral("c2")},
                    {QStringLiteral("Rained"), QStringLiteral("Salt & Water")})},
        blueIcon(), ParagraphMarkKind::Clippings);
    SubmenuChooser chooser(QStringLiteral("Clippings &From Here"), QString());
    editor->setCursorPosition({.paragraph = 1, .offset = 3});
    QContextMenuEvent menuKey(QContextMenuEvent::Keyboard, QPoint(), QPoint());
    QCoreApplication::sendEvent(editor.get(), &menuKey);
    CHECK(chooser.texts ==
          (QStringList{QStringLiteral("Rained"), QStringLiteral("Salt && Water")}));
}

TEST_CASE("Paragraph marks: the signs of the kinds of the cards beside the paperclip",
          "[editor][marks][cards]") {
    auto editor = editorWith(TEXT);
    const auto cardMark = [](const char* id, const char* title, const char* icon) {
        ParagraphMark mark{1, {QString::fromLatin1(id)}, {QString::fromLatin1(title)}};
        mark.icon = QString::fromLatin1(icon);
        return mark;
    };
    editor->setParagraphMarks({markOf(1, {QStringLiteral("m1")}, {QStringLiteral("Map")}),
                               cardMark("c1", "Anna", "character"),
                               cardMark("c2", "Book of Ashes", "item"),
                               cardMark("c3", "Jan", "character")},
                              redIcon(), ParagraphMarkKind::Materials,
                              {{QStringLiteral("character"), redIcon()},
                               {QStringLiteral("item"), redIcon()}});

    // One mark for each icon: the paperclip first, then the kinds of the cards
    const QList<ParagraphMark> marks = editor->paragraphMarks();
    REQUIRE(marks.size() == 3);
    CHECK(marks.at(0).icon.isEmpty());
    CHECK(marks.at(1).icon == QStringLiteral("character"));
    CHECK(marks.at(1).ids == (QStringList{QStringLiteral("c1"), QStringLiteral("c3")}));
    CHECK(marks.at(2).ids == QStringList{QStringLiteral("c2")});

    // From the text to the left: the paperclip, the characters, the item
    QStringList chosen;
    QObject::connect(editor.get(), &BookEditor::paragraphMarkClicked,
                     [&chosen](const QString& id) { chosen << id; });
    const QRectF line = caretAt(*editor, {1, 0});
    editorImage(*editor);  // shown: the marks of the paragraphs in view are found
    QStringList found;
    QPointF item;
    for (int x = static_cast<int>(line.left()) - 1; x > 0; --x) {
        const std::optional<ParagraphMark> mark =
            editor->paragraphMarkAt(QPointF(x, line.center().y()));
        const QString first = mark ? mark->ids.first() : QString();
        if (!first.isEmpty() && (found.isEmpty() || found.last() != first)) {
            found << first;
        }
        if (first == QStringLiteral("c2") && item.isNull()) {
            item = QPointF(x, line.center().y());
        }
    }
    CHECK(found == (QStringList{QStringLiteral("m1"), QStringLiteral("c1"),
                                QStringLiteral("c2")}));
    REQUIRE_FALSE(item.isNull());
    click(*editor, item);
    CHECK(chosen == QStringList{QStringLiteral("c2")});

    // The context menu of the paragraph: Linked Cards, with the cards of all the signs
    chosen.clear();
    {
        SubmenuChooser chooser(QStringLiteral("Linked Card&s"), QStringLiteral("Jan"));
        editor->setCursorPosition({1, 3});
        QContextMenuEvent menuKey(QContextMenuEvent::Keyboard, QPoint(), QPoint());
        QCoreApplication::sendEvent(editor.get(), &menuKey);
        CHECK(chooser.texts == (QStringList{QStringLiteral("Anna"), QStringLiteral("Jan"),
                                            QStringLiteral("Book of Ashes")}));
    }
    CHECK(chosen == QStringList{QStringLiteral("c3")});
    {
        SubmenuChooser chooser(QStringLiteral("Linked Mate&rials"), QString());
        editor->setCursorPosition({1, 3});
        QContextMenuEvent menuKey(QContextMenuEvent::Keyboard, QPoint(), QPoint());
        QCoreApplication::sendEvent(editor.get(), &menuKey);
        CHECK(chooser.texts == QStringList{QStringLiteral("Map")});
    }
}
