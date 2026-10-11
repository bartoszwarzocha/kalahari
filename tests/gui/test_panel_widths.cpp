/// @file test_panel_widths.cpp
/// @brief The side panels fit a narrow dock, and their muted texts can be read
///
/// On a small screen (1366x768 at 150%) a side panel that cannot be narrow takes the editor's
/// room: each of them wraps its rows and texts instead.

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/panels/annotations_panel.h"
#include "kalahari/gui/panels/assistant_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/search_panel.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/height_for_width_scroll_area.h"
#include "kalahari/gui/widgets/segmented_layout.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QToolButton>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWidget>

#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// The widest a side panel may be at its narrowest (the dock adds its frame)
constexpr int MAX_PANEL_MINIMUM = 200;

/// The program's texts half as long again, as in a language that needs more words for them
class LongerTexts : public QTranslator {
public:
    QString translate(const char* context, const char* sourceText,
                      const char* /*disambiguation*/, int /*n*/) const override {
        if (!QByteArray(context).startsWith("kalahari::")) {
            return {};
        }
        const QString text = QString::fromUtf8(sourceText);
        return text + QLatin1Char(' ') + text.left(text.size() / 2);
    }
    [[nodiscard]] bool isEmpty() const override { return false; }
};

/// Install a translator for the length of a test
class TranslatorGuard {
public:
    explicit TranslatorGuard(QTranslator& translator) : m_translator(translator) {
        QApplication::installTranslator(&m_translator);
    }
    ~TranslatorGuard() { QApplication::removeTranslator(&m_translator); }
    TranslatorGuard(const TranslatorGuard&) = delete;
    TranslatorGuard& operator=(const TranslatorGuard&) = delete;

private:
    QTranslator& m_translator;
};

/// Restore the theme a test changes
class ThemeGuard {
public:
    ThemeGuard()
        : m_saved(QString::fromStdString(core::ThemeManager::getInstance().getCurrentTheme().name)) {}
    ~ThemeGuard() { core::ThemeManager::getInstance().reloadTheme(m_saved); }
    ThemeGuard(const ThemeGuard&) = delete;
    ThemeGuard& operator=(const ThemeGuard&) = delete;

private:
    QString m_saved;
};

struct SidePanel {
    const char* name;
    std::function<QWidget*()> create;
};

const std::vector<SidePanel>& sidePanels() {
    static const std::vector<SidePanel> panels = {
        {"Annotations",
         [] {
             // With the to-do buttons, as in the program
             auto* panel = new AnnotationsPanel();
             panel->setTodoActions(new QAction(QStringLiteral("Previous To-Do"), panel),
                                   new QAction(QStringLiteral("Next To-Do"), panel));
             return panel;
         }},
        {"Assistant", [] { return new AssistantPanel(); }},
        {"Log", [] { return new LogPanel(); }},
        {"Navigator", [] { return new NavigatorPanel(); }},
        {"Properties", [] { return new PropertiesPanel(); }},
        {"Search", [] { return new SearchPanel(); }},
    };
    return panels;
}

/// What is seen of @p widget in @p panel (a scroll area shows a part of its content)
QRect seenRect(const QWidget* widget, const QWidget& panel) {
    const QRect seen = widget->visibleRegion().boundingRect();
    return seen.translated(widget->mapTo(&panel, QPoint()));
}

void checkPanelMinimums() {
    // The panels' tool buttons show their icons, as in the program (a button without its icon
    // shows its text)
    registerAllIcons();
    for (const SidePanel& panel : sidePanels()) {
        std::unique_ptr<QWidget> widget(panel.create());
        INFO(panel.name << " panel");
        CHECK(widget->minimumSizeHint().width() <= MAX_PANEL_MINIMUM);
    }
}

}  // namespace

TEST_CASE("Panels: every side panel fits a narrow dock", "[gui][panels]") {
    checkPanelMinimums();
}

TEST_CASE("Panels: the side panels fit a narrow dock also with longer texts", "[gui][panels]") {
    LongerTexts longer;
    TranslatorGuard guard(longer);
    checkPanelMinimums();
}

TEST_CASE("Panels: a row of buttons goes on in the next line where the panel is narrow",
          "[gui][panels]") {
    QWidget panel;
    auto* row = new FlowLayout(&panel, 6, 4);
    std::vector<QPushButton*> buttons;
    for (const char* text : {"Comments 12", "To-dos 3", "Notes 7"}) {
        auto* button = new QPushButton(QString::fromLatin1(text), &panel);
        row->addWidget(button);
        buttons.push_back(button);
    }
    int widest = 0;
    int allInLine = -row->spacing();
    for (QPushButton* button : buttons) {
        widest = std::max(widest, button->sizeHint().width());
        allInLine += button->sizeHint().width() + row->spacing();
    }
    const int lineHeight = buttons.front()->sizeHint().height();

    SECTION("as narrow as its widest button") {
        CHECK(row->minimumSize().width() == widest);
    }
    SECTION("in one line where it has the room") {
        row->setGeometry(QRect(0, 0, allInLine, row->heightForWidth(allInLine)));
        CHECK(row->heightForWidth(allInLine) == lineHeight);
        CHECK(buttons.at(1)->y() == buttons.at(0)->y());
        CHECK(buttons.at(2)->y() == buttons.at(0)->y());
    }
    SECTION("in three lines where only one button fits a line") {
        row->setGeometry(QRect(0, 0, widest, row->heightForWidth(widest)));
        CHECK(row->heightForWidth(widest) == 3 * lineHeight + 2 * row->lineSpacing());
        CHECK(buttons.at(1)->y() > buttons.at(0)->y());
        CHECK(buttons.at(2)->y() > buttons.at(1)->y());
        for (QPushButton* button : buttons) {
            CHECK(button->x() == 0);
        }
    }
}

TEST_CASE("Panels: an expanding item takes the room its line leaves", "[gui][panels]") {
    QWidget panel;
    auto* row = new FlowLayout(&panel, 6, 4);
    auto* fixed = new QPushButton(QStringLiteral("Chapter"), &panel);
    auto* expanding = new QPushButton(QStringLiteral("Date: any"), &panel);
    expanding->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->addWidget(fixed);
    row->addWidget(expanding);
    const int width = fixed->sizeHint().width() + expanding->sizeHint().width() + 100;
    row->setGeometry(QRect(0, 0, width, row->heightForWidth(width)));
    CHECK(fixed->width() == fixed->sizeHint().width());
    CHECK(expanding->geometry().right() == width - 1);
}

TEST_CASE("Panels: joined buttons of a choice go one under another where the panel is narrow",
          "[gui][panels]") {
    QWidget panel;
    auto* choice = new SegmentedLayout(&panel);
    std::vector<QToolButton*> buttons;
    for (const char* text : {"Open", "Done", "All"}) {
        // Tool buttons, as in the Annotations panel: they keep their own width unless told
        auto* button = new QToolButton(&panel);
        button->setText(QString::fromLatin1(text));
        choice->addWidget(button);
        buttons.push_back(button);
    }
    const int row = choice->sizeHint().width();
    const int height = buttons.front()->sizeHint().height();
    int widest = 0;
    for (QToolButton* button : buttons) {
        widest = std::max(widest, button->sizeHint().width());
    }

    SECTION("in a row where it has the room") {
        choice->setGeometry(QRect(0, 0, row, choice->heightForWidth(row)));
        CHECK_FALSE(choice->isStacked());
        CHECK(choice->heightForWidth(row) == height);
        CHECK(buttons.at(1)->x() == buttons.at(0)->geometry().right() + 1);
        CHECK(buttons.at(0)->property("stacked").toBool() == false);
    }
    SECTION("one under another, all as wide, where it has less") {
        CHECK(choice->minimumSize().width() == widest);
        choice->setGeometry(QRect(0, 0, row - 1, choice->heightForWidth(row - 1)));
        CHECK(choice->isStacked());
        CHECK(choice->heightForWidth(row - 1) == 3 * height);
        for (QToolButton* button : buttons) {
            CHECK(button->x() == 0);
            CHECK(button->width() == widest);
            CHECK(button->property("stacked").toBool());
        }
        CHECK(buttons.at(1)->y() == buttons.at(0)->geometry().bottom() + 1);
    }
}

TEST_CASE("Panels: the Annotations panel's controls never cover each other", "[gui][panels]") {
    // A narrow, low panel (1366x768 at 150%, the side panels open): the rows wrap and the
    // filters scroll instead of being drawn over each other
    AnnotationsPanel panel;
    panel.setTodoActions(new QAction(QStringLiteral("Previous To-Do"), &panel),
                         new QAction(QStringLiteral("Next To-Do"), &panel));
    panel.setAttribute(Qt::WA_DontShowOnScreen);
    panel.show();
    for (const QSize size : {QSize(panel.minimumSizeHint().width(), 300), QSize(200, 260),
                             QSize(260, 600), QSize(500, 400)}) {
        INFO("panel " << size.width() << "x" << size.height());
        panel.resize(size);
        for (int pass = 0; pass < 5; ++pass) {
            QApplication::processEvents();
        }
        std::vector<QWidget*> controls;
        for (QWidget* child : panel.findChildren<QWidget*>()) {
            const bool control = qobject_cast<QAbstractButton*>(child) ||
                                 qobject_cast<QComboBox*>(child) || qobject_cast<QLineEdit*>(child);
            if (control && child->isVisible() && !child->parentWidget()->inherits("QComboBox")) {
                controls.push_back(child);
            }
        }
        REQUIRE(controls.size() >= 10);
        for (std::size_t i = 0; i < controls.size(); ++i) {
            // What is seen of a control: the filters scrolled away are not
            const QRect first = seenRect(controls[i], panel);
            for (std::size_t j = i + 1; j < controls.size(); ++j) {
                const QRect second = seenRect(controls[j], panel);
                INFO(controls[i]->metaObject()->className()
                     << " '" << controls[i]->property("text").toString().toStdString() << "' and "
                     << controls[j]->metaObject()->className() << " '"
                     << controls[j]->property("text").toString().toStdString() << "'");
                CHECK_FALSE(first.intersects(second));
            }
        }
    }
}

TEST_CASE("Panels: a scroll area as high as its rows need at its width", "[gui][panels]") {
    auto* content = new QWidget();
    auto* rows = new FlowLayout(content, 6, 4);
    for (int i = 0; i < 6; ++i) {
        rows->addWidget(new QPushButton(QStringLiteral("Button %1").arg(i), content));
    }
    QWidget panel;
    auto* area = new HeightForWidthScrollArea(content, 2, &panel);
    CHECK(area->hasHeightForWidth());
    const int wide = rows->sizeHint().width();
    const int narrow = rows->minimumSize().width();
    CHECK(area->heightForWidth(wide) == rows->heightForWidth(wide));
    CHECK(area->heightForWidth(narrow) == rows->heightForWidth(narrow));
    CHECK(area->heightForWidth(narrow) > area->heightForWidth(wide));
    CHECK(area->minimumSizeHint().height() < area->heightForWidth(narrow));

    // Lower than the rows: the content keeps all its lines and scrolls
    panel.setAttribute(Qt::WA_DontShowOnScreen);
    panel.show();
    area->setGeometry(0, 0, narrow + area->verticalScrollBar()->sizeHint().width(),
                      area->minimumSizeHint().height());
    for (int pass = 0; pass < 5; ++pass) {
        QApplication::processEvents();
    }
    CHECK(content->height() >= rows->heightForWidth(content->width()));
    CHECK(area->verticalScrollBar()->maximum() > 0);
}

TEST_CASE("Settings: the planned pages are muted and readable in both themes",
          "[gui][settings][contrast]") {
    ThemeGuard restore;
    for (const char* themeName : {"Light", "Dark"}) {
        INFO(themeName << " theme");
        auto& themes = core::ThemeManager::getInstance();
        REQUIRE(themes.reloadTheme(QString::fromLatin1(themeName)));
        SettingsDialog dialog(nullptr);
        auto* tree = dialog.findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        const QColor base = themes.getCurrentTheme().palette.base;
        const QColor text = themes.getCurrentTheme().palette.text;
        int planned = 0;
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            if ((*it)->data(0, Qt::ForegroundRole).isValid()) {
                ++planned;
                const QColor color = (*it)->foreground(0).color();
                INFO((*it)->text(0).toStdString());
                CHECK(contrastRatio(color, base) >= MIN_TEXT_CONTRAST);
                // Muted: less contrast than the other pages' text
                CHECK(contrastRatio(color, base) < contrastRatio(text, base));
            }
        }
        CHECK(planned > 0);

        // A change of the theme while the dialog is open recolors them
        const char* other = std::string(themeName) == "Light" ? "Dark" : "Light";
        REQUIRE(themes.reloadTheme(QString::fromLatin1(other)));
        const QColor otherBase = themes.getCurrentTheme().palette.base;
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            if ((*it)->data(0, Qt::ForegroundRole).isValid()) {
                INFO((*it)->text(0).toStdString() << " after the change to " << other);
                CHECK(contrastRatio((*it)->foreground(0).color(), otherBase) >= MIN_TEXT_CONTRAST);
            }
        }
    }
}

TEST_CASE("Settings: the color of the notes is readable on the lists and cards of both themes",
          "[gui][settings][contrast]") {
    ThemeGuard restore;
    for (const char* themeName : {"Light", "Dark"}) {
        INFO(themeName << " theme");
        auto& themes = core::ThemeManager::getInstance();
        REQUIRE(themes.reloadTheme(QString::fromLatin1(themeName)));
        const core::Theme& theme = themes.getCurrentTheme();
        CHECK(contrastRatio(SettingsPage::noteColor(), theme.palette.window) >= MIN_TEXT_CONTRAST);
        for (const QColor& background :
             {theme.palette.window, theme.palette.base, theme.palette.alternateBase}) {
            INFO("on " << background.name().toStdString());
            CHECK(contrastRatio(SettingsPage::noteColorOn(background), background) >=
                  MIN_TEXT_CONTRAST);
        }
    }
}
