/// @file stats_pages.cpp
/// @brief Settings > Statistics and Goals: Bar and Panel, Counting, Notifications and Colors

#include "kalahari/gui/settings/stats_pages.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/widgets/writing_stats_bar.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QToolButton>

#include <cmath>
#include <memory>

namespace kalahari::gui {

using json = nlohmann::json;

namespace {

/// @brief A combo box whose items keep the values of a setting
QComboBox* valueCombo(const std::vector<std::pair<QString, json>>& items)
{
    auto* combo = new QComboBox();
    for (const auto& [text, value] : items) {
        if (value.is_number_integer()) {
            combo->addItem(text, value.get<int>());
        } else {
            combo->addItem(text, QString::fromStdString(value.get<std::string>()));
        }
    }
    return combo;
}

/// @brief A spin box of whole numbers
QSpinBox* numberBox(int minimum, int maximum, const QString& suffix = QString())
{
    auto* box = new QSpinBox();
    box->setRange(minimum, maximum);
    box->setSuffix(suffix);
    box->setGroupSeparatorShown(true);
    return box;
}

/// @brief A row of controls and texts, for a form
QWidget* rowOf(const QList<QWidget*>& parts)
{
    auto* row = new QWidget();
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    for (QWidget* part : parts) {
        layout->addWidget(part);
    }
    layout->addStretch();
    return row;
}

/// @brief The preview of the colors of a theme: a day of writing on the grid, with the line of
/// the goal, the words adding up and the four temperatures of work
class StatsPreview : public QWidget {
public:
    explicit StatsPreview(std::function<StatsColors()> colors, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_colors(std::move(colors))
    {
        setMinimumSize(220, 56);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(tr("Preview of the colors"));
    }

    QSize sizeHint() const override { return {320, 56}; }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        const StatsColors colors = m_colors();
        QPainter painter(this);
        const QRectF area(0.5, 0.5, width() - 1, height() - 1);
        painter.fillRect(rect(), colors.background);
        stats_painter::drawGrid(painter, area, 8, 4, colors.grid);
        // The words of each piece of a day
        const QList<double> pieces{0, 2, 9, 14, 6, 0, 0, 3, 0, 0, 0, 0, 5, 12, 18, 16, 9, 2, 0};
        const QRectF graph(area.left(), area.top() + 4, area.width() * 0.62, area.height() - 4);
        stats_painter::drawArea(painter, graph, pieces, 20, colors.line, colors.fill());
        // The line of the goal and the words adding up to it
        QPen dashed(colors.goal, 1);
        dashed.setDashPattern({3, 3});
        painter.setPen(dashed);
        const double goalY = std::round(area.top() + area.height() * 0.22) + 0.5;
        painter.drawLine(QPointF(area.left(), goalY), QPointF(area.right(), goalY));
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath sum(QPointF(graph.left(), graph.bottom()));
        double total = 0;
        for (int i = 0; i < pieces.size(); ++i) {
            total += pieces[i];
            sum.lineTo(graph.left() + graph.width() * (i + 1) / pieces.size(),
                       graph.bottom() - total / 125.0 * graph.height());
        }
        painter.setPen(QPen(colors.done, 1.6));
        painter.drawPath(sum);
        // The temperatures of work and the times of writing
        const QList<QColor> swatches{colors.muchFast, colors.muchSlow, colors.littleFast,
                                     colors.littleSlow};
        const double size = 10;
        double x = area.width() * 0.66;
        for (const QColor& color : swatches) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRoundedRect(QRectF(x, area.bottom() - size - 4, size, size), 2, 2);
            x += size + 4;
        }
        for (int i = 0; i < 5; ++i) {
            QColor color = colors.hoursHigh;
            color.setAlphaF(0.15 + 0.85 * i / 4.0);
            painter.setBrush(i == 0 ? colors.hoursLow : color);
            painter.drawRect(QRectF(area.width() * 0.66 + i * 9, area.top() + 6, 8, 8));
        }
        painter.setPen(colors.text);
        QFont small = font();
        small.setPointSizeF(std::max(7.0, small.pointSizeF() * 0.85));
        painter.setFont(small);
        painter.drawText(QPointF(area.width() * 0.66 + 50, area.top() + 14),
                         stats_painter::number(1325));
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(colors.frame, 1));
        painter.drawRect(area);
    }

private:
    std::function<StatsColors()> m_colors;
};

}  // namespace

// =============================================================================
// StatsSettingsPage
// =============================================================================

StatsSettingsPage::StatsSettingsPage(QWidget* parent)
    : SettingsPage(parent)
{
}

std::vector<QRadioButton*> StatsSettingsPage::addChoices(QFormLayout* form, const QString& label,
                                                         const std::string& key,
                                                         std::vector<Choice> choices, bool inRow)
{
    auto* holder = new QWidget();
    QBoxLayout* layout = inRow ? static_cast<QBoxLayout*>(new QHBoxLayout(holder))
                               : static_cast<QBoxLayout*>(new QVBoxLayout(holder));
    layout->setContentsMargins(0, 0, 0, 0);
    auto* group = new QButtonGroup(holder);
    std::vector<QRadioButton*> buttons;
    for (const Choice& choice : choices) {
        auto* button = new QRadioButton(choice.text);
        group->addButton(button);
        layout->addWidget(button);
        if (!choice.note.isEmpty()) {
            alignWithText(addNote(layout, choice.note), button);
        }
        buttons.push_back(button);
    }
    if (inRow) {
        layout->addStretch();
    }
    if (label.isEmpty()) {
        form->addRow(holder);
    } else {
        auto* caption = new QLabel(label);
        caption->setBuddy(buttons.front());
        form->addRow(caption, holder);
    }

    auto values = std::make_shared<std::vector<Choice>>(std::move(choices));
    Binding binding;
    binding.key = [key]() { return key; };
    binding.stored = [key, values]() {
        return core::SettingsManager::getInstance().get<json>(key, values->front().value);
    };
    binding.shown = [buttons, values]() {
        for (std::size_t i = 0; i < buttons.size(); ++i) {
            if (buttons[i]->isChecked()) {
                return (*values)[i].value;
            }
        }
        return values->front().value;
    };
    binding.show = [buttons, values](const json& value) {
        for (std::size_t i = 0; i < buttons.size(); ++i) {
            if ((*values)[i].value == value) {
                buttons[i]->setChecked(true);
                return;
            }
        }
        buttons.front()->setChecked(true);
    };
    binding.store = [key](const json& value) {
        core::SettingsManager::getInstance().set<json>(key, value);
    };
    bindCustom(std::move(binding));
    return buttons;
}

QCheckBox* StatsSettingsPage::addOption(QLayout* layout, const QString& text,
                                        const std::string& key, const QString& note)
{
    auto* box = new QCheckBox(text);
    if (auto* form = qobject_cast<QFormLayout*>(layout)) {
        form->addRow(box);
    } else {
        layout->addWidget(box);
    }
    bind(box, key);
    if (!note.isEmpty()) {
        alignWithText(addNote(layout, note), box);
    }
    return box;
}

QLabel* StatsSettingsPage::addFieldNote(QFormLayout* form, const QString& text)
{
    auto* holder = new QVBoxLayout();
    holder->setContentsMargins(0, 0, 0, 0);
    QLabel* note = addNote(holder, text);
    form->addRow(QString(), holder);
    return note;
}

QLabel* StatsSettingsPage::addIntro(const QString& text)
{
    return addNote(pageLayout(), text);
}

QFormLayout* StatsSettingsPage::addAdvanced(const QString& title)
{
    m_advancedToggle = new QToolButton();
    m_advancedToggle->setText(tr("Ad&vanced Settings"));
    m_advancedToggle->setCheckable(true);
    m_advancedToggle->setAutoRaise(true);
    m_advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_advancedToggle->setArrowType(Qt::RightArrow);
    auto* toggleRow = new QHBoxLayout();
    toggleRow->addWidget(m_advancedToggle);
    toggleRow->addStretch();
    pageLayout()->addLayout(toggleRow);

    m_advancedGroup = new QGroupBox(title);
    auto* form = new QFormLayout(m_advancedGroup);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    pageLayout()->addWidget(m_advancedGroup);
    m_advancedGroup->hide();
    connect(m_advancedToggle, &QToolButton::toggled, this, [this](bool shown) {
        m_advancedToggle->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
        m_advancedGroup->setVisible(shown);
    });
    return form;
}

// =============================================================================
// Bar and Panel
// =============================================================================

StatsBarPage::StatsBarPage(QWidget* parent)
    : StatsSettingsPage(parent)
{
    //: In Polish: Pasek statystyk nad kartami tekstów, panel „Statystyki” i pasek stanu. Kolory
    //: są na stronie „Kolory”.
    addIntro(tr("The statistics bar above the tabs of the texts, the Statistics panel and the "
                "status bar. The colors are on the Colors page."));

    QFormLayout* bar = addGroup(tr("Statistics Bar"));
    addOption(bar, tr("&Show the statistics bar"), "stats.bar.show",
              tr("Above the tabs of the texts. The button on the toolbar and the View menu show "
                 "and hide it too."));
    addChoices(bar, tr("Look:"), "stats.bar.mode",
               {{tr("&Graph"), "graph", {}}, {tr("&One line"), "line", {}}}, true);
    addFieldNote(bar, tr("The button in the top right corner of the bar switches it too."));
    QSpinBox* height =
        numberBox(WritingStatsBar::MIN_GRAPH_HEIGHT, 300, tr(" px"));
    addField(bar, tr("Graph &height:"), height, "stats.bar.height");
    addFieldNote(bar, tr("At least %1 px.").arg(WritingStatsBar::MIN_GRAPH_HEIGHT));
    addChoices(bar, tr("Texts on the graph:"), "stats.bar.textSide",
               {{tr("On the &right"), "right", {}}, {tr("On the &left"), "left", {}}}, true);
    auto* shown = new QWidget();
    auto* shownLayout = new QVBoxLayout(shown);
    shownLayout->setContentsMargins(0, 0, 0, 0);
    QCheckBox* words = addOption(shownLayout, tr("&Words today and the temperature of work"),
                                 "stats.bar.showWords");
    addOption(shownLayout, tr("&Time of writing"), "stats.bar.showTime");
    addOption(shownLayout, tr("&Days in a row"), "stats.bar.showStreak");
    auto* shownLabel = new QLabel(tr("Show:"));
    shownLabel->setBuddy(words);
    bar->addRow(shownLabel, shown);
    QComboBox* mini = valueCombo({{tr("Today"), "day"},
                                  {tr("The last 7 days"), "7"},
                                  {tr("The last 30 days"), "30"}});
    addField(bar, tr("&Mini graph in one line:"), mini, "stats.bar.miniGraph");
    addFieldNote(bar, tr("A click on the mini graph switches it too: today, 7 or 30 days."));

    QFormLayout* panel = addGroup(tr("Statistics Panel"));
    auto* views = new QWidget();
    auto* viewsLayout = new QGridLayout(views);
    viewsLayout->setContentsMargins(0, 0, 0, 0);
    const std::vector<std::pair<QString, std::string>> panelViews{
        {tr("Today"), "stats.panel.today"},     {tr("Week"), "stats.panel.week"},
        {tr("30 days"), "stats.panel.month"},   {tr("Book"), "stats.panel.book"},
        {tr("Times of writing"), "stats.panel.hours"},
        {tr("Achievements"), "stats.panel.awards"}};
    QCheckBox* firstView = nullptr;
    for (std::size_t i = 0; i < panelViews.size(); ++i) {
        auto* box = new QCheckBox(panelViews[i].first);
        bind(box, panelViews[i].second);
        viewsLayout->addWidget(box, int(i / 3), int(i % 3));
        if (firstView == nullptr) {
            firstView = box;
        }
    }
    auto* viewsLabel = new QLabel(tr("Views in the panel:"));
    viewsLabel->setBuddy(firstView);
    panel->addRow(viewsLabel, views);
    addFieldNote(panel, tr("Hidden views take no room in the panel."));

    QFormLayout* status = addGroup(tr("Status Bar"));
    addOption(status, tr("Show the time of writing and the days in a &row"), "stats.statusBar",
              tr("Beside the words and the reading time, at the bottom of the window."));

    QFormLayout* advanced = addAdvanced(tr("Graph of the Bar"));
    std::vector<std::pair<QString, json>> fromHours;
    std::vector<std::pair<QString, json>> toHours;
    for (int hour = 0; hour <= 24; ++hour) {
        const QString text = QStringLiteral("%1:00").arg(hour);
        if (hour < 24) {
            fromHours.emplace_back(text, hour);
        }
        if (hour > 0) {
            toHours.emplace_back(text, hour);
        }
    }
    QComboBox* from = valueCombo(fromHours);
    QComboBox* to = valueCombo(toHours);
    bind(from, "stats.bar.fromHour");
    bind(to, "stats.bar.toHour");
    from->setAccessibleName(tr("From"));
    to->setAccessibleName(tr("To"));
    auto* hoursLabel = new QLabel(tr("&Hours: from"));
    hoursLabel->setBuddy(from);
    advanced->addRow(hoursLabel, rowOf({from, new QLabel(tr("to")), to}));
    addFieldNote(advanced, tr("When you write outside these hours, the graph widens by itself."));
    QComboBox* piece = valueCombo({{tr("5 min"), 5}, {tr("10 min"), 10}, {tr("15 min"), 15},
                                   {tr("20 min"), 20}, {tr("30 min"), 30}});
    addField(advanced, tr("&Piece of the graph:"), piece, "stats.bar.pieceMinutes");
    addFieldNote(advanced, tr("The graph shows the words written in each piece."));
    auto* lines = new QWidget();
    auto* linesLayout = new QVBoxLayout(lines);
    linesLayout->setContentsMargins(0, 0, 0, 0);
    QCheckBox* pieces = addOption(linesLayout, tr("Words in each &piece"), "stats.bar.showPieces");
    addOption(linesLayout, tr("Words today with the line of the &goal"),
              "stats.bar.showCumulative");
    auto* linesLabel = new QLabel(tr("Lines:"));
    linesLabel->setBuddy(pieces);
    advanced->addRow(linesLabel, lines);

    pageLayout()->addStretch();
}

// =============================================================================
// Counting
// =============================================================================

StatsCountingPage::StatsCountingPage(QWidget* parent)
    : StatsSettingsPage(parent)
{
    //: In Polish: Jak program liczy słowa, czas pisania i dni z rzędu. Same cele i terminy
    //: ustawiasz na karcie „Cele i terminy pisania” (Narzędzia > Cele i terminy pisania).
    addIntro(tr("How the program counts the words, the time of writing and the days in a row. "
                "The goals and the deadlines themselves are on the tab Writing Goals and "
                "Deadlines (Tools > Writing Goals and Deadlines)."));

    QFormLayout* counting = addGroup(tr("What Counts toward the Goal"));
    addChoices(counting, QString(), "stats.goalCounting",
               {{tr("&Words added: written minus deleted"), "net",
                 tr("When you delete a paragraph, the words of the day go down.")},
                {tr("&All the words written"), "written",
                 tr("Deleting does not lower the words of the day.")}});

    QFormLayout* books = addGroup(tr("Daily and Weekly Goal"));
    addChoices(books, QString(), "stats.dailyGoalAllBooks",
               {{tr("Words of all the &books"), true,
                 tr("The goal is your habit of writing, not that of one book.")},
                {tr("Words of the &open book only"), false, {}}});

    QFormLayout* pasted = addGroup(tr("Pasted Text"));
    //: In Polish: Wliczaj wklejony tekst do statystyk
    addOption(pasted, tr("Count &pasted text in the statistics"), "stats.countPasted",
              //: In Polish: Wklejony albo upuszczony tekst to nie pisanie: nie liczy się do
              //: słów dnia, celów, dni z rzędu ani odznak. Długość książki liczy cały tekst.
              //: To samo pole jest na dole panelu „Statystyki”.
              tr("Text pasted or dropped is not writing: it does not count toward the words "
                 "of the day, the goals, the days in a row or the badges. The length of the "
                 "book counts the whole text. The same box is at the foot of the Statistics "
                 "panel."));

    QFormLayout* streak = addGroup(tr("Days in a Row"));
    auto* minimum = numberBox(1, 100000);
    bind(minimum, "stats.streakMinimumWords");
    minimum->setAccessibleName(tr("Words a day"));
    std::vector<QRadioButton*> rules =
        addChoices(streak, tr("A day counts when you:"), "stats.streakRule",
                   {{tr("&write at least"), "minimumWords", {}},
                    {tr("reach the &daily goal"), "dailyGoal", {}}});
    // The words beside the first choice, in its row
    if (auto* layout = qobject_cast<QBoxLayout*>(rules.front()->parentWidget()->layout())) {
        auto* row = rowOf({});
        auto* rowLayout = static_cast<QHBoxLayout*>(row->layout());
        layout->removeWidget(rules.front());
        rowLayout->insertWidget(0, rules.front());
        rowLayout->insertWidget(1, minimum);
        rowLayout->insertWidget(2, new QLabel(tr("words")));
        layout->insertWidget(0, row);
    }
    addFieldNote(streak, tr("Days off do not break the run."));
    connect(rules.front(), &QRadioButton::toggled, minimum, &QWidget::setEnabled);
    whenLoaded([minimum, first = rules.front()]() { minimum->setEnabled(first->isChecked()); });

    QFormLayout* advanced = addAdvanced(tr("Details of Counting"));
    auto* dayEnd = new QTimeEdit();
    dayEnd->setDisplayFormat(QStringLiteral("H:mm"));
    dayEnd->setTimeRange(QTime(0, 0), QTime(6, 0));
    auto* dayEndLabel = new QLabel(tr("Writing day &ends at:"));
    dayEndLabel->setBuddy(dayEnd);
    advanced->addRow(dayEndLabel, rowOf({dayEnd}));
    Binding dayStart;
    dayStart.key = []() { return std::string("stats.dayStart"); };
    dayStart.stored = []() {
        return json(core::SettingsManager::getInstance().get<int>("stats.dayStart", 240));
    };
    dayStart.shown = [dayEnd]() { return json(dayEnd->time().msecsSinceStartOfDay() / 60000); };
    dayStart.show = [dayEnd](const json& value) {
        dayEnd->setTime(QTime(0, 0).addSecs(qint64(value.get<int>()) * 60));
    };
    dayStart.store = [](const json& value) {
        core::SettingsManager::getInstance().set<int>("stats.dayStart", value.get<int>());
    };
    bindCustom(std::move(dayStart));
    addFieldNote(advanced, tr("The words written after midnight, until this hour, still count "
                              "to the day before."));
    QSpinBox* idle = numberBox(1, 30, tr(" min"));
    auto* idleLabel = new QLabel(tr("Time of writing &stops after:"));
    idleLabel->setBuddy(idle);
    advanced->addRow(idleLabel, rowOf({idle, new QLabel(tr("without writing"))}));
    bind(idle, "stats.idleMinutes");
    addFieldNote(advanced, tr("A longer pause does not count as time of writing."));
    auto* skip = new QCheckBox(tr("Do not count &large changes at once, over"));
    bind(skip, "stats.skipLargeChanges");
    QSpinBox* large = numberBox(50, 100000);
    large->setAccessibleName(tr("Words of a large change"));
    bind(large, "stats.largeChangeWords");
    advanced->addRow(rowOf({skip, large, new QLabel(tr("words"))}));
    //: In Polish: Wycięcie, przywrócenie wersji albo „Zamień wszystko” to nie pisanie.
    QLabel* skipNote = addNote(advanced, tr("Cutting, restoring a version or Replace All is not "
                                            "writing."));
    alignWithText(skipNote, skip);
    connect(skip, &QCheckBox::toggled, large, &QWidget::setEnabled);
    whenLoaded([skip, large]() { large->setEnabled(skip->isChecked()); });
    pageLayout()->addStretch();
}

// =============================================================================
// Notifications
// =============================================================================

StatsNotificationsPage::StatsNotificationsPage(QWidget* parent)
    : StatsSettingsPage(parent)
{
    //: In Polish: Wiadomości pokazują się na pasku na dole okna, z przyciskiem kopiowania. Gdy
    //: panel „Asystent” jest otwarty, asystent też pochwali.
    addIntro(tr("The messages show in the bar at the bottom of the window, with a button that "
                "copies them. When the Assistant panel is open, the assistant praises you too."));

    QFormLayout* messages = addGroup(tr("Messages"));
    addOption(messages, tr("&Daily goal reached"), "stats.notify.dailyGoal");
    addOption(messages, tr("&Weekly goal reached"), "stats.notify.weeklyGoal");
    auto* deadline = new QCheckBox(tr("A deadline is &coming,"));
    bind(deadline, "stats.notify.deadline");
    QSpinBox* days = numberBox(1, 14);
    days->setAccessibleName(tr("Days before the deadline"));
    bind(days, "stats.notify.deadlineDays");
    messages->addRow(rowOf({deadline, days, new QLabel(tr("days before"))}));
    alignWithText(addNote(messages, tr("The deadline of the book and the deadlines on the tab "
                                       "Writing Goals and Deadlines.")),
                  deadline);
    connect(deadline, &QCheckBox::toggled, days, &QWidget::setEnabled);
    whenLoaded([deadline, days]() { days->setEnabled(deadline->isChecked()); });

    QFormLayout* summary = addGroup(tr("Summary of the Session"));
    //: In Polish: Pokazuj &podsumowanie sesji
    addOption(summary, tr("Show a &summary of the session"), "stats.sessionSummary",
              //: In Polish: Przy zamknięciu książki albo programu: słowa, czas pisania i postęp
              //: celu dnia.
              tr("When the book or the program closes: the words, the time of writing and the "
                 "progress of the goal of the day."));

    QFormLayout* awards = addGroup(tr("Badges and Challenges"));
    addOption(awards, tr("&Badges"), "stats.badges",
              tr("For days in a row, for words and for goals reached."));
    addOption(awards, tr("C&hallenges"), "stats.challenges",
              tr("For those who like them, e.g. 50,000 words in a month."));
    pageLayout()->addStretch();
}

// =============================================================================
// Colors
// =============================================================================

StatsColorsPage::StatsColorsPage(QWidget* parent)
    : StatsSettingsPage(parent)
{
    //: In Polish: Kolory wykresów, paska celu dziennego i temperatury pracy. Każdy motyw
    //: programu ma własne kolory; program używa tych, które pasują do bieżącego motywu.
    addIntro(tr("The colors of the graphs, of the bar of the daily goal and of the temperature "
                "of work. Each theme of the program has its own; the program uses those of the "
                "theme in use."));
    QStringList themes = core::ThemeManager::getInstance().getAvailableThemes();
    // The light theme first, as the Theme page lists them
    themes.sort();
    std::stable_sort(themes.begin(), themes.end(), [](const QString& a, const QString& b) {
        return a == QLatin1String("Light") && b != QLatin1String("Light");
    });
    for (const QString& theme : themes) {
        addTheme(theme);
    }
    pageLayout()->addStretch();
}

void StatsColorsPage::addTheme(const QString& themeName)
{
    core::Theme theme;
    try {
        theme = core::ThemeManager::getInstance().loadTheme(themeName);
    } catch (const std::exception&) {
        return;
    }
    const bool dark = theme.palette.window.lightness() < 128;
    const QString title = themeName == QLatin1String("Light") ? tr("Colors of the Light Theme")
                          : themeName == QLatin1String("Dark") ? tr("Colors of the Dark Theme")
                                                                : tr("Colors of the Theme %1")
                                                                      .arg(themeName);
    QFormLayout* form = addGroup(title);
    const std::string name = themeName.toStdString();

    const std::vector<QString> labels{
        tr("Background of the graphs"),
        tr("Grid"),
        tr("Frame of the graphs"),
        tr("Line and fill of the graphs"),
        tr("Line of the goal"),
        tr("Goal reached"),
        tr("Temperature: a lot, fast"),
        tr("Temperature: a lot, slowly"),
        tr("Temperature: a little, fast"),
        tr("Temperature: a little, slowly"),
        tr("Times of writing: the fewest words"),
        tr("Times of writing: the most words"),
        tr("Texts on the graphs"),
    };
    const std::vector<std::string>& keys = StatsColors::keys();
    auto widgets = std::make_shared<std::vector<ColorConfigWidget*>>();
    auto* restoreRow = new QHBoxLayout();
    auto* restore = new QPushButton(tr("Restore Default Colors"));
    std::vector<std::function<void()>> restorers;
    for (std::size_t i = 0; i < keys.size() && i < labels.size(); ++i) {
        const std::string key = keys[i];
        const auto found = theme.editor.find(key);
        const QColor themeColor = found != theme.editor.end() ? found->second : QColor();
        auto* widget = new ColorConfigWidget(labels[i]);
        form->addRow(widget);
        widgets->push_back(widget);
        Binding binding;
        binding.key = [name, key]() { return core::ThemeManager::editorColorSetting(name, key); };
        binding.stored = [name, key, themeColor]() {
            const QColor stored(QString::fromStdString(
                core::SettingsManager::getInstance().get<std::string>(
                    core::ThemeManager::editorColorSetting(name, key), std::string())));
            return json((stored.isValid() ? stored : themeColor).name().toStdString());
        };
        binding.shown = [widget]() { return json(widget->color().name().toStdString()); };
        binding.show = [widget](const json& value) {
            widget->setColor(QColor(QString::fromStdString(value.get<std::string>())));
        };
        binding.store = [name, key, themeColor](const json& value) {
            // The theme's own color is not kept: the theme decides it again
            auto& settings = core::SettingsManager::getInstance();
            const std::string setting = core::ThemeManager::editorColorSetting(name, key);
            const QColor color(QString::fromStdString(value.get<std::string>()));
            if (color == themeColor) {
                settings.removeKey(setting);
            } else {
                settings.set<std::string>(setting, color.name().toStdString());
            }
        };
        bindCustom(std::move(binding));
        restorers.emplace_back([widget, themeColor]() { widget->setColor(themeColor); });
    }

    // The fill under the lines
    const int defaultFill = StatsColors::defaultFillOpacity(dark);
    QSpinBox* fill = numberBox(0, 60, tr(" %"));
    auto* fillLabel = new QLabel(tr("&Fill under the lines:"));
    fillLabel->setBuddy(fill);
    form->addRow(fillLabel, rowOf({fill}));
    const std::string fillKey = StatsColors::fillOpacitySetting(name);
    Binding fillBinding;
    fillBinding.key = [fillKey]() { return fillKey; };
    fillBinding.stored = [fillKey, defaultFill]() {
        return json(core::SettingsManager::getInstance().get<int>(fillKey, defaultFill));
    };
    fillBinding.shown = [fill]() { return json(fill->value()); };
    fillBinding.show = [fill](const json& value) { fill->setValue(value.get<int>()); };
    fillBinding.store = [fillKey, defaultFill](const json& value) {
        auto& settings = core::SettingsManager::getInstance();
        if (value.get<int>() == defaultFill) {
            settings.removeKey(fillKey);
        } else {
            settings.set<int>(fillKey, value.get<int>());
        }
    };
    bindCustom(std::move(fillBinding));
    restorers.emplace_back([fill, defaultFill]() { fill->setValue(defaultFill); });

    // The preview of the colors shown
    auto* preview = new StatsPreview([widgets, fill]() {
        StatsColors colors;
        const std::vector<QColor StatsColors::*> members{
            &StatsColors::background, &StatsColors::grid,       &StatsColors::frame,
            &StatsColors::line,       &StatsColors::goal,       &StatsColors::done,
            &StatsColors::muchFast,   &StatsColors::muchSlow,   &StatsColors::littleFast,
            &StatsColors::littleSlow, &StatsColors::hoursLow,   &StatsColors::hoursHigh,
            &StatsColors::text};
        for (std::size_t i = 0; i < members.size() && i < widgets->size(); ++i) {
            colors.*members[i] = (*widgets)[i]->color();
        }
        colors.fillOpacity = fill->value();
        return colors;
    });
    form->addRow(tr("Preview:"), preview);
    for (ColorConfigWidget* widget : *widgets) {
        connect(widget, &ColorConfigWidget::colorChanged, preview,
                qOverload<>(&QWidget::update));
    }
    connect(fill, qOverload<int>(&QSpinBox::valueChanged), preview,
            qOverload<>(&QWidget::update));

    restoreRow->addWidget(restore);
    restoreRow->addStretch();
    form->addRow(restoreRow);
    connect(restore, &QPushButton::clicked, this, [restorers]() {
        for (const auto& restoreOne : restorers) {
            restoreOne();
        }
    });
}

}  // namespace kalahari::gui
