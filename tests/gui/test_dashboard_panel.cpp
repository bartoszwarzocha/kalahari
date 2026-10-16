/// @file test_dashboard_panel.cpp
/// @brief The Dashboard on a small screen: its parts go one under another and its texts
///        wrap, so that nothing is cut off

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "kalahari/core/art_provider.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/dashboard_layouts.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/color_config_widget.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QProxyStyle>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QStyleOption>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>

#include <vector>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

// The panel on a 1366x768 screen (logical pixels, without the menus, toolbars and status bar)
const QSize AT_125_PANELS_CLOSED(1093, 457);
const QSize AT_125_PANELS_OPEN(371, 457);  // the Navigator and the panels on the right
const QSize AT_150_PANELS_CLOSED(910, 360);
const QSize AT_150_PANELS_OPEN(203, 360);

/// Keeps the recent books of the test run as they were, with one book in the list
class RecentBookGuard {
public:
    explicit RecentBookGuard(const QString& path = QStringLiteral("Przykladowa powiesc.klh"))
        : m_saved(core::RecentBooksManager::getInstance().getRecentFiles()) {
        auto& recent = core::RecentBooksManager::getInstance();
        recent.clearRecentFiles();
        recent.addRecentFile(QDir::temp().absoluteFilePath(path));
    }
    ~RecentBookGuard() {
        auto& recent = core::RecentBooksManager::getInstance();
        recent.clearRecentFiles();
        for (auto it = m_saved.crbegin(); it != m_saved.crend(); ++it) {
            recent.addRecentFile(*it);
        }
    }
    RecentBookGuard(const RecentBookGuard&) = delete;
    RecentBookGuard& operator=(const RecentBookGuard&) = delete;

private:
    QStringList m_saved;
};

/// The program's texts twice as long, as in a language that needs more words for them
/// (the names of the keys stay as they are)
class LongerTexts : public QTranslator {
public:
    QString translate(const char* context, const char* sourceText,
                      const char* /*disambiguation*/, int /*n*/) const override {
        if (!QByteArray(context).startsWith("kalahari::")) {
            return {};
        }
        const QString text = QString::fromUtf8(sourceText);
        return text + QLatin1Char(' ') + text;
    }
    [[nodiscard]] bool isEmpty() const override { return false; }
};

/// A click of the left mouse button in the middle of a widget
void click(QWidget* widget) {
    const QPointF middle = QRectF(widget->rect()).center();
    for (const QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        QMouseEvent event(type, middle, widget->mapToGlobal(middle), Qt::LeftButton,
                          type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(widget, &event);
    }
}

/// Show a panel to Qt without a window on the screen: the screen of the machine running the
/// tests (or its window manager) limits none of the sizes the tests give the panel
void showOffScreen(QWidget& panel) {
    panel.setAttribute(Qt::WA_DontShowOnScreen);
    panel.show();
}

/// Let the layouts follow a change of size (they ask for it in posted events)
void settle() {
    for (int pass = 0; pass < 5; ++pass) {
        QApplication::processEvents();
    }
}

QScrollArea* scrollAreaOf(DashboardPanel& dashboard) {
    auto* area = dashboard.findChild<QScrollArea*>();
    REQUIRE(area != nullptr);
    REQUIRE(area->widget() != nullptr);
    return area;
}

/// The text of a label or a button, for the messages
QString nameOf(const QWidget* widget) {
    if (const auto* label = qobject_cast<const QLabel*>(widget)) {
        return label->text().isEmpty() ? QStringLiteral("the logo") : label->text();
    }
    if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
        return button->text().isEmpty() ? button->accessibleName() : button->text();
    }
    return QString::fromLatin1(widget->metaObject()->className());
}

/// What of the Dashboard a panel of @p size cuts off: texts and buttons beyond its width or
/// beyond what holds them, or narrower or lower than they need
QStringList cutOff(DashboardPanel& dashboard, QSize size) {
    dashboard.resize(size);
    settle();
    QScrollArea* area = scrollAreaOf(dashboard);
    QWidget* content = area->widget();
    const int shownWidth = area->viewport()->width();

    QStringList found;
    if (content->width() > shownWidth) {
        found << QStringLiteral("the content is %1 px wide in %2 px")
                     .arg(content->width())
                     .arg(shownWidth);
    }
    for (QWidget* widget : content->findChildren<QWidget*>()) {
        if (!widget->isVisibleTo(content) ||
            (!qobject_cast<QLabel*>(widget) && !qobject_cast<QAbstractButton*>(widget))) {
            continue;
        }
        const QString name = nameOf(widget);
        const QRect rect(widget->mapTo(content, QPoint(0, 0)), widget->size());
        if (rect.left() < 0 || rect.right() >= shownWidth) {
            found << name + QStringLiteral(" is beyond the panel");
        }
        for (QWidget* holder = widget->parentWidget(); holder != content;
             holder = holder->parentWidget()) {
            const QRect inHolder(widget->mapTo(holder, QPoint(0, 0)), widget->size());
            if (!holder->rect().contains(inHolder)) {
                found << name + QStringLiteral(" is beyond ") + nameOf(holder);
                break;
            }
        }
        // The logo is as large as the header makes it
        if (widget->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored) {
            continue;
        }
        const int neededHeight = widget->hasHeightForWidth()
            ? widget->heightForWidth(widget->width())
            : widget->minimumSizeHint().height();
        if (widget->width() < widget->minimumSizeHint().width() ||
            widget->height() < neededHeight) {
            found << QStringLiteral("%1 is squeezed to %2x%3")
                         .arg(name)
                         .arg(widget->width())
                         .arg(widget->height());
        }
    }
    return found;
}

/// What a panel of @p size cuts off, in one text that a failed check shows (empty: nothing)
std::string cutOffText(DashboardPanel& dashboard, QSize size) {
    return cutOff(dashboard, size).join(QStringLiteral("; ")).toStdString();
}

/// A shortcut of the Dashboard: its keys and its command
struct Shortcut {
    QLabel* keys;
    QLabel* command;
};

/// The shortcuts of the Dashboard, in the order they are shown
std::vector<Shortcut> shortcutsOf(DashboardPanel& dashboard) {
    auto* frame = dashboard.findChild<QFrame*>(QStringLiteral("shortcutsFrame"));
    REQUIRE(frame != nullptr);
    DashboardHintsLayout* layout = nullptr;
    for (QLabel* label : frame->findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("font-weight: bold"))) {
            layout = dynamic_cast<DashboardHintsLayout*>(label->parentWidget()->layout());
        }
    }
    REQUIRE(layout != nullptr);
    std::vector<Shortcut> shortcuts;
    for (int index = 0; index + 1 < layout->count(); index += 2) {
        auto* keys = qobject_cast<QLabel*>(layout->itemAt(index)->widget());
        auto* command = qobject_cast<QLabel*>(layout->itemAt(index + 1)->widget());
        REQUIRE(keys != nullptr);
        REQUIRE(command != nullptr);
        shortcuts.push_back({keys, command});
    }
    return shortcuts;
}

QLabel* labelWith(DashboardPanel& dashboard, const QString& text) {
    QLabel* found = nullptr;
    for (QLabel* label : dashboard.findChildren<QLabel*>()) {
        if (label->text() == text) {
            found = label;
        }
    }
    INFO("No label " << text.toStdString());
    REQUIRE(found != nullptr);
    return found;
}

/// A widget's place in the Dashboard's content
QRect placeOf(DashboardPanel& dashboard, const QWidget* widget) {
    QWidget* content = scrollAreaOf(dashboard)->widget();
    return {widget->mapTo(content, QPoint(0, 0)), widget->size()};
}

} // namespace

TEST_CASE("Dashboard: the content in the middle, three quarters of a wide panel",
          "[gui][dashboard]") {
    // A content that likes 528 px (one row of shortcuts) and can be 140 px narrow
    class Content : public QWidget {
    public:
        [[nodiscard]] QSize sizeHint() const override { return {528, 300}; }
        [[nodiscard]] QSize minimumSizeHint() const override { return {140, 100}; }
    };
    QWidget panel;
    auto* layout = new DashboardContentLayout(&panel);
    auto* content = new Content;
    layout->addWidget(content);

    CHECK(layout->contentWidth(1079) == 809);  // three quarters
    CHECK(layout->contentWidth(600) == 528);   // as wide as it likes, with the margins
    CHECK(layout->contentWidth(357) == 325);   // all but a margin on each side
    CHECK(layout->contentWidth(189) == 157);
    CHECK(layout->contentWidth(150) == 140);   // not narrower than it can be
    CHECK(layout->minimumSize().width() == 140 + 2 * DashboardContentLayout::SIDE_MARGIN);

    layout->setGeometry(QRect(0, 0, 357, 400));
    CHECK(content->geometry() == QRect(16, 0, 325, 400));
}

TEST_CASE("Dashboard: nothing is cut off on a small screen", "[gui][dashboard]") {
    // The user's report: at 125% with the side panels open the title and the third shortcut
    // were cut off at the right
    registerAllCommands(CommandCallbacks{});
    RecentBookGuard recentBook;

    SECTION("in the program's texts") {
        DashboardPanel dashboard;
        showOffScreen(dashboard);
        for (const QSize size : {AT_125_PANELS_CLOSED, AT_125_PANELS_OPEN, AT_150_PANELS_CLOSED,
                                 AT_150_PANELS_OPEN, QSize(600, 457)}) {
            INFO("panel " << size.width() << "x" << size.height());
            CHECK(cutOffText(dashboard, size) == std::string());
        }
    }

    SECTION("with a long name in the path of a recent book") {
        // A folder named with a long number, which has no place to end a line in it
        RecentBookGuard longPath(
            QStringLiteral("6e1ac65f-8806-5aba-876d-5f142cdd2d6b/Przykladowa powiesc.klh"));
        DashboardPanel dashboard;
        showOffScreen(dashboard);
        for (const QSize size : {AT_125_PANELS_OPEN, AT_150_PANELS_OPEN}) {
            INFO("panel " << size.width() << "x" << size.height());
            CHECK(cutOffText(dashboard, size) == std::string());
            CHECK_FALSE(scrollAreaOf(dashboard)->horizontalScrollBar()->isVisible());
        }
    }

    SECTION("in longer texts, as of another language") {
        LongerTexts longerTexts;
        QCoreApplication::installTranslator(&longerTexts);
        {
            DashboardPanel dashboard;
            showOffScreen(dashboard);
            for (const QSize size : {AT_125_PANELS_OPEN, AT_150_PANELS_OPEN}) {
                INFO("panel " << size.width() << "x" << size.height());
                CHECK(cutOffText(dashboard, size) == std::string());
            }
        }
        QCoreApplication::removeTranslator(&longerTexts);
    }
}

TEST_CASE("Dashboard: the checkbox keeps its size where the style puts margins around it",
          "[gui][dashboard]") {
    // macOS puts margins around a checkbox for layouts, inside its rectangle: on a narrow
    // panel they took the box beyond its row, which squeezed it
    class MarginsAroundCheckBox : public QProxyStyle {
    public:
        QRect subElementRect(SubElement element, const QStyleOption* option,
                             const QWidget* widget) const override {
            if (element == SE_CheckBoxLayoutItem) {
                return option->rect.adjusted(2, 2, -3, -2);
            }
            return QProxyStyle::subElementRect(element, option, widget);
        }
        QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                               const QWidget* widget) const override {
            const QSize contents = QProxyStyle::sizeFromContents(type, option, size, widget);
            return type == CT_CheckBox ? contents + QSize(5, 4) : contents;
        }
    };
    registerAllCommands(CommandCallbacks{});
    RecentBookGuard recentBook;
    MarginsAroundCheckBox style;
    DashboardPanel dashboard;
    auto* checkBox = dashboard.findChild<QCheckBox*>();
    REQUIRE(checkBox != nullptr);
    checkBox->setStyle(&style);
    showOffScreen(dashboard);
    for (const QSize size : {AT_125_PANELS_OPEN, AT_150_PANELS_OPEN}) {
        INFO("panel " << size.width() << "x" << size.height());
        CHECK(cutOffText(dashboard, size) == std::string());
        CHECK(checkBox->size() == checkBox->sizeHint());
    }
}

TEST_CASE("Dashboard: a panel narrower than a word scrolls sideways", "[gui][dashboard]") {
    registerAllCommands(CommandCallbacks{});
    DashboardPanel dashboard;
    showOffScreen(dashboard);
    dashboard.resize(120, 360);
    settle();
    QScrollArea* area = scrollAreaOf(dashboard);
    CHECK(area->widget()->width() > area->viewport()->width());
    CHECK(area->horizontalScrollBar()->isVisible());

    dashboard.resize(AT_150_PANELS_OPEN);
    settle();
    CHECK_FALSE(area->horizontalScrollBar()->isVisible());
}

TEST_CASE("Dashboard: the shortcuts in a row, in two columns, or each command under its keys",
          "[gui][dashboard]") {
    // Three hints of 100x20, 60x20 and 100x20 keys with 90x20, 110x20 and 80x20 commands
    QWidget holder;
    auto* layout = new DashboardHintsLayout(&holder);
    std::vector<QWidget*> boxes;
    for (const QSize size : {QSize(100, 20), QSize(90, 20), QSize(60, 20), QSize(110, 20),
                             QSize(100, 20), QSize(80, 20)}) {
        auto* box = new QWidget(&holder);
        box->setFixedSize(size);
        boxes.push_back(box);
    }
    for (std::size_t index = 0; index < boxes.size(); index += 2) {
        layout->addHint(boxes[index], boxes[index + 1]);
    }
    REQUIRE(layout->count() == 6);
    CHECK(layout->sizeHint() == QSize(100 + 12 + 90 + 48 + 60 + 12 + 110 + 48 + 100 + 12 + 80,
                                      20));
    CHECK(layout->minimumSize() == QSize(110, 20));

    // All in one row, in the middle
    layout->setGeometry(QRect(0, 0, 700, 100));
    CHECK(boxes[0]->geometry() == QRect(14, 0, 100, 20));
    CHECK(boxes[1]->geometry() == QRect(126, 0, 90, 20));
    CHECK(boxes[2]->geometry() == QRect(264, 0, 60, 20));
    CHECK(boxes[3]->geometry() == QRect(336, 0, 110, 20));
    CHECK(boxes[4]->geometry() == QRect(494, 0, 100, 20));
    CHECK(boxes[5]->geometry() == QRect(606, 0, 80, 20));
    CHECK(layout->heightForWidth(700) == 20);

    // One under another, the keys to the right of their column, the commands to the left
    layout->setGeometry(QRect(0, 0, 300, 100));
    CHECK(boxes[0]->geometry() == QRect(39, 0, 100, 20));
    CHECK(boxes[1]->geometry() == QRect(151, 0, 90, 20));
    CHECK(boxes[2]->geometry() == QRect(79, 28, 60, 20));
    CHECK(boxes[3]->geometry() == QRect(151, 28, 110, 20));
    CHECK(boxes[4]->geometry() == QRect(39, 56, 100, 20));
    CHECK(boxes[5]->geometry() == QRect(151, 56, 80, 20));
    CHECK(layout->heightForWidth(300) == 76);

    // Too narrow for the columns: each command under its keys, in the middle
    layout->setGeometry(QRect(0, 0, 150, 200));
    CHECK(boxes[0]->geometry() == QRect(25, 0, 100, 20));
    CHECK(boxes[1]->geometry() == QRect(30, 20, 90, 20));
    CHECK(boxes[2]->geometry() == QRect(45, 52, 60, 20));
    CHECK(boxes[3]->geometry() == QRect(20, 72, 110, 20));
    CHECK(boxes[4]->geometry() == QRect(25, 104, 100, 20));
    CHECK(boxes[5]->geometry() == QRect(35, 124, 80, 20));
    CHECK(layout->heightForWidth(150) == 144);

    // A hidden hint takes no room
    boxes[2]->hide();
    boxes[3]->hide();
    CHECK(layout->sizeHint() == QSize(100 + 12 + 90 + 48 + 100 + 12 + 80, 20));
    layout->setGeometry(QRect(0, 0, 700, 100));
    CHECK(boxes[0]->geometry() == QRect(129, 0, 100, 20));
    CHECK(boxes[4]->geometry() == QRect(379, 0, 100, 20));
}

TEST_CASE("Dashboard: the shortcuts one under another on a narrow panel", "[gui][dashboard]") {
    registerAllCommands(CommandCallbacks{});
    DashboardPanel dashboard;
    showOffScreen(dashboard);
    auto* frame = dashboard.findChild<QFrame*>(QStringLiteral("shortcutsFrame"));
    REQUIRE(frame != nullptr);
    const std::vector<Shortcut> shortcuts = shortcutsOf(dashboard);
    REQUIRE(shortcuts.size() == 3);
    // The keys as the menus write them: Ctrl+Shift+N, on macOS Shift and Command with N
    const QString newBookKeys =
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N).toString(QKeySequence::NativeText);
    CHECK(shortcuts[1].keys->text().contains(QLatin1Char('>') + newBookKeys + QLatin1Char('<')));
    CHECK(shortcuts[1].command->text() == QStringLiteral("New Book"));

    // One row on a wide panel, each command beside its keys
    dashboard.resize(AT_125_PANELS_CLOSED);
    settle();
    const int rowMiddle = placeOf(dashboard, shortcuts[0].keys).center().y();
    for (const Shortcut& shortcut : shortcuts) {
        CHECK(std::abs(placeOf(dashboard, shortcut.keys).center().y() - rowMiddle) <= 1);
        CHECK(std::abs(placeOf(dashboard, shortcut.command).center().y() - rowMiddle) <= 1);
        CHECK(placeOf(dashboard, shortcut.command).left() >
              placeOf(dashboard, shortcut.keys).right());
    }

    // One under another on a narrower one, in two columns in the middle
    dashboard.resize(AT_125_PANELS_OPEN);
    settle();
    const int keysRight = placeOf(dashboard, shortcuts[0].keys).right();
    const int commandsLeft = placeOf(dashboard, shortcuts[0].command).left();
    for (std::size_t index = 0; index < shortcuts.size(); ++index) {
        const QRect keys = placeOf(dashboard, shortcuts[index].keys);
        const QRect command = placeOf(dashboard, shortcuts[index].command);
        CHECK(keys.right() == keysRight);
        CHECK(command.left() == commandsLeft);
        CHECK(std::abs(command.center().y() - keys.center().y()) <= 1);
        if (index > 0) {
            CHECK(keys.top() > placeOf(dashboard, shortcuts[index - 1].keys).bottom());
        }
    }
    int columnsLeft = keysRight;
    int columnsRight = commandsLeft;
    for (const Shortcut& shortcut : shortcuts) {
        columnsLeft = std::min(columnsLeft, placeOf(dashboard, shortcut.keys).left());
        columnsRight = std::max(columnsRight, placeOf(dashboard, shortcut.command).right());
    }
    const int middle = placeOf(dashboard, frame).center().x();
    CHECK(std::abs((columnsLeft + columnsRight) / 2 - middle) <= 1);

    // Each command under its keys on the narrowest one, both in the middle. Where the keys
    // are written shorter (macOS) the two columns fit it: there the panel is made narrower
    // than the columns
    const auto* hints =
        dynamic_cast<const DashboardHintsLayout*>(shortcuts[0].keys->parentWidget()->layout());
    REQUIRE(hints != nullptr);
    int keysColumn = 0;
    int commandsColumn = 0;
    for (int index = 0; index + 1 < hints->count(); index += 2) {
        keysColumn = std::max(keysColumn, hints->itemAt(index)->sizeHint().width());
        commandsColumn = std::max(commandsColumn, hints->itemAt(index + 1)->sizeHint().width());
    }
    const int columnsWidth = keysColumn + DashboardHintsLayout::KEYS_SPACING + commandsColumn;
    QSize narrow = AT_150_PANELS_OPEN;
    dashboard.resize(narrow);
    settle();
    for (int pass = 0; pass < 3 && hints->contentsRect().width() >= columnsWidth; ++pass) {
        narrow.rwidth() -= hints->contentsRect().width() - columnsWidth + 1;
        dashboard.resize(narrow);
        settle();
    }
    INFO("panel " << narrow.width() << " px wide, columns " << columnsWidth << " px");
    REQUIRE(hints->contentsRect().width() < columnsWidth);
    const int narrowMiddle = placeOf(dashboard, frame).center().x();
    for (const Shortcut& shortcut : shortcuts) {
        const QRect keys = placeOf(dashboard, shortcut.keys);
        const QRect command = placeOf(dashboard, shortcut.command);
        CHECK(command.top() > keys.bottom());
        CHECK(std::abs(keys.center().x() - narrowMiddle) <= 1);
        CHECK(std::abs(command.center().x() - narrowMiddle) <= 1);
    }
}

TEST_CASE("Dashboard: the logo beside the title, above it on a narrow panel",
          "[gui][dashboard]") {
    registerAllCommands(CommandCallbacks{});
    DashboardPanel dashboard;
    showOffScreen(dashboard);
    QLabel* title = labelWith(dashboard, QStringLiteral("Welcome to Kalahari"));
    QLabel* tagline = labelWith(dashboard, QStringLiteral("A Comprehensive Writer's IDE"));
    QWidget* logo = nullptr;
    for (QLabel* label : title->parentWidget()->findChildren<QLabel*>()) {
        if (label->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored) {
            logo = label;  // as large as the header makes it
        }
    }
    REQUIRE(logo != nullptr);

    // A high panel has room for the whole logo, beside the title
    dashboard.resize(1093, 800);
    settle();
    CHECK(logo->size() == QSize(256, 256));
    CHECK(placeOf(dashboard, title).left() > placeOf(dashboard, logo).right());
    CHECK(placeOf(dashboard, tagline).left() == placeOf(dashboard, title).left());

    // A low one leaves room under it for the shortcuts and the recent books
    dashboard.resize(AT_150_PANELS_CLOSED);
    settle();
    CHECK(logo->width() == logo->height());
    CHECK(logo->height() <= AT_150_PANELS_CLOSED.height() * 2 / 5);
    CHECK(placeOf(dashboard, title).left() > placeOf(dashboard, logo).right());

    // A narrow one puts the logo above the title, both in the middle
    dashboard.resize(AT_125_PANELS_OPEN);
    settle();
    CHECK(logo->width() == logo->height());
    CHECK(placeOf(dashboard, title).top() > placeOf(dashboard, logo).bottom());
    CHECK(std::abs(placeOf(dashboard, title).center().x() -
                   placeOf(dashboard, logo).center().x()) <= 1);
    CHECK(title->height() == title->heightForWidth(title->width()));

    // The title wraps where even its own line is too narrow
    const int oneLine = title->height();
    dashboard.resize(AT_150_PANELS_OPEN);
    settle();
    CHECK(title->height() > oneLine);
}

TEST_CASE("Dashboard: a narrow card puts the book's icon above its texts", "[gui][dashboard]") {
    registerAllCommands(CommandCallbacks{});
    // A book's path with short names, and one with a long name, which has no place to end a
    // line in it (the path wraps inside it, so it does not keep the card wide)
    const QString path = GENERATE(
        QStringLiteral("Przykladowa powiesc.klh"),
        QStringLiteral("6e1ac65f-8806-5aba-876d-5f142cdd2d6b/Przykladowa powiesc.klh"));
    INFO("path " << path.toStdString());
    RecentBookGuard recentBook(path);
    DashboardPanel dashboard;
    showOffScreen(dashboard);
    QLabel* bookTitle = labelWith(dashboard, QStringLiteral("Przykladowa powiesc"));
    auto* icon = dashboard.findChild<QFrame*>(QStringLiteral("fileIconFrame"));
    REQUIRE(icon != nullptr);

    dashboard.resize(AT_125_PANELS_OPEN);
    settle();
    CHECK(placeOf(dashboard, bookTitle).left() > placeOf(dashboard, icon).right());
    CHECK(bookTitle->width() >= DashboardCardLayout::TEXT_ROOM_BESIDE);

    // Beside the icon the texts would wrap after each word or two
    dashboard.resize(AT_150_PANELS_OPEN);
    settle();
    CHECK(placeOf(dashboard, bookTitle).top() > placeOf(dashboard, icon).bottom());
    CHECK(placeOf(dashboard, bookTitle).left() == placeOf(dashboard, icon).left());
}

TEST_CASE("Dashboard: a card puts its icon above texts that beside it would wrap word by word",
          "[gui][dashboard]") {
    // Texts that like 400 px (a long path) and can be 50 px narrow (their longest word)
    class Texts : public QWidget {
    public:
        explicit Texts(int likedWidth) : m_likedWidth(likedWidth) {}
        [[nodiscard]] QSize sizeHint() const override { return {m_likedWidth, 80}; }
        [[nodiscard]] QSize minimumSizeHint() const override { return {50, 80}; }

    private:
        int m_likedWidth;
    };
    const int likedWidth = GENERATE(400, 100);
    INFO("texts liking " << likedWidth << " px");
    QWidget card;
    auto* layout = new DashboardCardLayout(&card);
    auto* icon = new QWidget;
    icon->setFixedSize(56, 56);
    auto* texts = new Texts(likedWidth);
    layout->addWidget(icon);
    layout->addWidget(texts);
    const int margins = 2 * DashboardCardLayout::MARGIN;
    const int besideIcon = 56 + DashboardCardLayout::SPACING;

    // Beside the icon where the texts have the room they need there
    const int roomNeeded = std::min(likedWidth, DashboardCardLayout::TEXT_ROOM_BESIDE);
    layout->setGeometry(QRect(0, 0, margins + besideIcon + roomNeeded, 200));
    CHECK(texts->x() == DashboardCardLayout::MARGIN + besideIcon);
    CHECK(texts->width() == roomNeeded);

    // Above them where they would have less, though their longest word fits
    layout->setGeometry(QRect(0, 0, margins + besideIcon + roomNeeded - 1, 200));
    CHECK(texts->x() == DashboardCardLayout::MARGIN);
    CHECK(texts->y() > icon->geometry().bottom());
    CHECK(texts->width() == besideIcon + roomNeeded - 1);
}

TEST_CASE("Dashboard: the text of the auto-load checkbox changes it", "[gui][dashboard]") {
    auto& settings = core::SettingsManager::getInstance();
    const bool saved = settings.get<bool>("startup.autoLoadLastProject");
    settings.set("startup.autoLoadLastProject", false);
    {
        DashboardPanel dashboard;
        showOffScreen(dashboard);
        auto* checkBox = dashboard.findChild<QCheckBox*>();
        REQUIRE(checkBox != nullptr);
        QLabel* text = labelWith(dashboard, QStringLiteral("Open last book on startup"));
        CHECK(checkBox->accessibleName() == text->text());
        CHECK(text->buddy() == checkBox);
        REQUIRE_FALSE(checkBox->isChecked());

        click(text);
        CHECK(checkBox->isChecked());
        CHECK(settings.get<bool>("startup.autoLoadLastProject"));
        click(text);
        CHECK_FALSE(checkBox->isChecked());
        CHECK_FALSE(settings.get<bool>("startup.autoLoadLastProject"));
    }
    settings.set("startup.autoLoadLastProject", saved);
}

TEST_CASE("Dashboard: the auto-load checkbox follows the Settings window", "[gui][dashboard]") {
    // Round 3 report 19.3: the option changed in the Settings window stayed as it was on the
    // Dashboard until the program started again
    auto& settings = core::SettingsManager::getInstance();
    settings.set("startup.autoLoadLastProject", false);
    DashboardPanel dashboard;
    showOffScreen(dashboard);
    auto* onDashboard = dashboard.findChild<QCheckBox*>();
    REQUIRE(onDashboard != nullptr);
    REQUIRE_FALSE(onDashboard->isChecked());

    // The Settings window opens on its General page, with the same option
    SettingsDialog dialog(nullptr);
    QCheckBox* inSettings = nullptr;
    for (QCheckBox* box : dialog.findChildren<QCheckBox*>()) {
        if (box->text() == QStringLiteral("Open last book on startup")) {
            inSettings = box;
        }
    }
    REQUIRE(inSettings != nullptr);
    REQUIRE_FALSE(inSettings->isChecked());

    inSettings->click();
    CHECK_FALSE(onDashboard->isChecked());  // until Apply or OK
    dialog.applyChanges();
    CHECK(onDashboard->isChecked());
    CHECK(settings.get<bool>("startup.autoLoadLastProject"));

    inSettings->click();
    dialog.applyChanges();
    CHECK_FALSE(onDashboard->isChecked());
    CHECK_FALSE(settings.get<bool>("startup.autoLoadLastProject"));

    // The Dashboard changes it as before
    onDashboard->click();
    CHECK(settings.get<bool>("startup.autoLoadLastProject"));
}

namespace {

/// Open the page @p title under @p category of the Settings window
void openSettingsPage(SettingsDialog& dialog, const QString& category, const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        QTreeWidgetItem* parent = (*it)->parent();
        if ((*it)->text(0) == title && parent != nullptr && parent->text(0) == category) {
            tree->setCurrentItem(*it);
            return;
        }
    }
    FAIL("No page " << title.toStdString());
}

/// The list of the Settings window with the item @p data, set to it
void choose(SettingsDialog& dialog, const QString& data) {
    for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
        const int index = combo->findData(data);
        if (index >= 0) {
            combo->setCurrentIndex(index);
            return;
        }
    }
    FAIL("No list with " << data.toStdString());
}

/// The colors a pixmap is drawn in, without its transparent pixels
std::set<QRgb> colorsOf(const QPixmap& pixmap) {
    std::set<QRgb> colors;
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) == 255) {
                colors.insert(pixel);
            }
        }
    }
    return colors;
}

/// The colors and the icon style of the test run, as they were, when the test ends
class ThemeColorsGuard {
public:
    ThemeColorsGuard()
        : m_theme(QString::fromStdString(core::ThemeManager::getInstance().getCurrentTheme().name))
        , m_saved(core::SettingsManager::getInstance().get<nlohmann::json>("themes"))
        , m_savedIcons(core::SettingsManager::getInstance().get<nlohmann::json>("icons.themes"))
        , m_iconStyle(core::ArtProvider::getInstance().getIconTheme()) {}
    ~ThemeColorsGuard() {
        try {
            auto& settings = core::SettingsManager::getInstance();
            for (const auto& [key, saved] : {std::pair{"themes", &m_saved},
                                             std::pair{"icons.themes", &m_savedIcons}}) {
                if (saved->is_null()) {
                    settings.removeKey(key);
                } else {
                    settings.set<nlohmann::json>(key, *saved);
                }
            }
            core::ArtProvider::getInstance().setIconTheme(m_iconStyle);
            core::ThemeManager::getInstance().reloadTheme(m_theme);
        } catch (...) {
            // A destructor may not throw: the later tests set the colors they need
            std::fputs("ThemeColorsGuard: the colors of the test run could not be given back\n",
                       stderr);
        }
    }
    ThemeColorsGuard(const ThemeColorsGuard&) = delete;
    ThemeColorsGuard& operator=(const ThemeColorsGuard&) = delete;

private:
    QString m_theme;
    nlohmann::json m_saved;
    nlohmann::json m_savedIcons;
    QString m_iconStyle;
};

}  // namespace

TEST_CASE("Dashboard: its icons keep no color of another theme after the colors are reset",
          "[gui][dashboard][theme]") {
    // Report of 16.10: the Light theme, two-tone icons, a greenish second color, the theme's
    // colors reset, then the Dark theme: the toolbars are right, the Dashboard showed green
    const ThemeColorsGuard guard;
    auto& settings = core::SettingsManager::getInstance();
    auto& themes = core::ThemeManager::getInstance();
    for (const char* theme : {"Light", "Dark"}) {
        settings.removeKey(std::string("themes.") + theme);
        settings.removeKey(std::string("icons.themes.") + theme);
    }
    REQUIRE(themes.reloadTheme(QStringLiteral("Dark")));
    registerAllIcons();
    const RecentBookGuard book;
    DashboardPanel dashboard;
    auto* bookIcon = dashboard.findChild<QLabel*>(QStringLiteral("cardBookIcon"));
    REQUIRE(bookIcon != nullptr);

    SettingsDialog dialog(nullptr);
    openSettingsPage(dialog, QStringLiteral("Appearance"), QStringLiteral("Theme"));
    openSettingsPage(dialog, QStringLiteral("Appearance"), QStringLiteral("Icons"));
    choose(dialog, QStringLiteral("Light"));
    dialog.applyChanges();
    choose(dialog, QStringLiteral("twotone"));
    dialog.applyChanges();
    REQUIRE(themes.getCurrentTheme().name == "Light");

    ColorConfigWidget* secondary = nullptr;
    for (ColorConfigWidget* widget : dialog.findChildren<ColorConfigWidget*>()) {
        if (widget->toolTip() == QStringLiteral("Secondary icon color used for icon accents")) {
            secondary = widget;
        }
    }
    REQUIRE(secondary != nullptr);
    const QColor green(QStringLiteral("#3CB371"));
    secondary->setColor(green);
    dialog.applyChanges();
    REQUIRE(core::ArtProvider::getInstance().getSecondaryColor() == green);
    // The Dashboard draws its icons in the icon colors, as the toolbars
    CHECK(colorsOf(bookIcon->pixmap()).count(green.rgb()) == 1);

    QPushButton* reset = nullptr;
    for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Reset to Theme Defaults")) {
            reset = button;
        }
    }
    REQUIRE(reset != nullptr);
    reset->click();
    dialog.applyChanges();
    choose(dialog, QStringLiteral("Dark"));
    dialog.applyChanges();

    // The Dark theme's own icon colors, in the toolbars and on the Dashboard
    const core::Theme dark = themes.loadTheme(QStringLiteral("Dark"));
    REQUIRE(themes.getCurrentTheme().name == "Dark");
    CHECK(core::ArtProvider::getInstance().getPrimaryColor() == dark.colors.primary);
    CHECK(core::ArtProvider::getInstance().getSecondaryColor() == dark.colors.secondary);
    const std::set<QRgb> drawn = colorsOf(bookIcon->pixmap());
    INFO("Icons " << dark.colors.primary.name().toStdString() << " / "
                  << dark.colors.secondary.name().toStdString());
    CHECK(drawn.count(dark.colors.primary.rgb()) == 1);
    CHECK(drawn.count(dark.colors.secondary.rgb()) == 1);
    CHECK(drawn.count(green.rgb()) == 0);
}

TEST_CASE("Dashboard: the theme page has no colors of its own for the Dashboard icons",
          "[gui][dashboard][theme]") {
    // Its icons take the icon colors; the fields of the Dashboard colors, and of the icon of a
    // news column that the Dashboard does not have, changed nothing that one could see
    SettingsDialog dialog(nullptr);
    openSettingsPage(dialog, QStringLiteral("Appearance"), QStringLiteral("Theme"));
    QStringList fields;
    for (const ColorConfigWidget* widget : dialog.findChildren<ColorConfigWidget*>()) {
        fields << widget->toolTip();
    }
    CHECK(fields.contains(QStringLiteral("Color for information panel headers")));
    for (const char* gone : {"Primary dashboard accent color", "Secondary dashboard accent color",
                             "Primary color for info panels", "Secondary color for info panels"}) {
        INFO(gone);
        CHECK_FALSE(fields.contains(QString::fromLatin1(gone)));
    }
}
