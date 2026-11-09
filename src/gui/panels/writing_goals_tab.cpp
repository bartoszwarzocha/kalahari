/// @file writing_goals_tab.cpp
/// @brief The tab Writing Goals and Deadlines

#include "kalahari/gui/panels/writing_goals_tab.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDateEdit>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include <cmath>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief The settings of the writer's goals (WritingRules::fromSettings())
constexpr const char* DAILY_GOAL_SETTING = "stats.dailyGoal";
constexpr const char* WRITING_DAYS_SETTING = "stats.writingDays";
constexpr const char* WEEKLY_GOAL_SETTING = "stats.weeklyGoal";

/// @brief Words of a day or a week at most
constexpr int MAX_GOAL_WORDS = 1'000'000;

/// @brief Words of a book at most
constexpr int MAX_BOOK_WORDS = 10'000'000;

/// @brief The page is not wider than this: the lines stay short on a wide window
constexpr int MAX_PAGE_WIDTH = 1000;

/// @brief The columns of the list of the deadlines
enum Column { NameColumn, DateColumn, PartColumn, GoalColumn, StateColumn, ColumnCount };

/// @brief Set @p box to @p value unless the writer is typing in it
void showValue(QSpinBox* box, int value)
{
    if (box->hasFocus() || box->value() == value) {
        return;
    }
    const QSignalBlocker blocker(box);
    box->setValue(value);
}

/// @brief A spin box of words
QSpinBox* wordsBox(int maximum, int step, QWidget* parent)
{
    auto* box = new QSpinBox(parent);
    box->setRange(0, maximum);
    box->setSingleStep(step);
    box->setGroupSeparatorShown(true);
    box->setAlignment(Qt::AlignRight);
    return box;
}

/// @brief A row of a group: its widgets from the left
QHBoxLayout* rowIn(QVBoxLayout* layout, const QList<QWidget*>& widgets)
{
    auto* row = new QHBoxLayout();
    for (QWidget* widget : widgets) {
        row->addWidget(widget);
    }
    row->addStretch(1);
    layout->addLayout(row);
    return row;
}

}  // namespace

WritingGoalsTab::WritingGoalsTab(WritingStatsModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("writingGoalsTab"));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    outer->addWidget(scroll);

    auto* page = new QWidget();
    page->setBackgroundRole(QPalette::Window);
    page->setAutoFillBackground(true);
    scroll->setWidget(page);
    auto* pageRow = new QHBoxLayout(page);
    auto* content = new QWidget(page);
    content->setMaximumWidth(MAX_PAGE_WIDTH);
    pageRow->addWidget(content, 1);
    pageRow->addStretch(0);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);

    // The heading: what the tab is and where its goals are kept
    m_title = new QLabel(content);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.2);
    m_title->setFont(titleFont);
    layout->addWidget(m_title);
    m_intro = new QLabel(content);
    m_intro->setWordWrap(true);
    m_intro->setTextFormat(Qt::PlainText);
    m_notes.append(m_intro);
    layout->addWidget(m_intro);

    auto* columns = new QWidget(content);
    m_columns = new QBoxLayout(QBoxLayout::LeftToRight, columns);
    m_columns->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(columns);
    m_left = new QWidget(columns);
    auto* left = new QVBoxLayout(m_left);
    left->setContentsMargins(0, 0, 0, 0);
    m_right = new QWidget(columns);
    auto* right = new QVBoxLayout(m_right);
    right->setContentsMargins(0, 0, 0, 0);
    m_columns->addWidget(m_left, 1);
    m_columns->addWidget(m_right, 1);

    // The daily goal and the writing days: the writer's own
    m_dailyGroup = new QGroupBox(m_left);
    auto* daily = new QVBoxLayout(m_dailyGroup);
    m_dailyLabel = new QLabel(m_dailyGroup);
    m_dailyGoal = wordsBox(MAX_GOAL_WORDS, 100, m_dailyGroup);
    m_dailyGoal->setObjectName(QStringLiteral("dailyGoal"));
    m_dailyLabel->setBuddy(m_dailyGoal);
    m_dailyWords = new QLabel(m_dailyGroup);
    rowIn(daily, {m_dailyLabel, m_dailyGoal, m_dailyWords});
    m_daysLabel = new QLabel(m_dailyGroup);
    QList<QWidget*> dayRow{m_daysLabel};
    for (int day = 0; day < 7; ++day) {
        auto* button = new QToolButton(m_dailyGroup);
        button->setObjectName(QStringLiteral("writingDay%1").arg(day + 1));
        button->setCheckable(true);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_dayButtons[std::size_t(day)] = button;
        dayRow.append(button);
        connect(button, &QToolButton::toggled, this, [this]() {
            core::SettingsManager::getInstance().set<int>(WRITING_DAYS_SETTING,
                                                          writingDaysOfButtons());
        });
    }
    m_daysLabel->setBuddy(m_dayButtons.front());
    rowIn(daily, dayRow);
    m_daysNote = new QLabel(m_dailyGroup);
    m_daysNote->setWordWrap(true);
    m_notes.append(m_daysNote);
    daily->addWidget(m_daysNote);
    m_fromDeadline = new QCheckBox(m_dailyGroup);
    m_fromDeadline->setObjectName(QStringLiteral("dailyGoalFromDeadline"));
    m_fromDeadlineNote = new QLabel(m_dailyGroup);
    m_notes.append(m_fromDeadlineNote);
    rowIn(daily, {m_fromDeadline, m_fromDeadlineNote});
    left->addWidget(m_dailyGroup);

    // The weekly goal: the daily goals of the writing days, or the writer's own
    m_weeklyGroup = new QGroupBox(m_left);
    auto* weekly = new QVBoxLayout(m_weeklyGroup);
    m_weeklySum = new QRadioButton(m_weeklyGroup);
    m_weeklySum->setObjectName(QStringLiteral("weeklyGoalSum"));
    rowIn(weekly, {m_weeklySum});
    m_weeklyOwn = new QRadioButton(m_weeklyGroup);
    m_weeklyOwn->setObjectName(QStringLiteral("weeklyGoalOwn"));
    m_weeklyGoal = wordsBox(MAX_GOAL_WORDS, 500, m_weeklyGroup);
    m_weeklyGoal->setObjectName(QStringLiteral("weeklyGoal"));
    m_weeklyGoal->setMinimum(1);
    m_weeklyWords = new QLabel(m_weeklyGroup);
    rowIn(weekly, {m_weeklyOwn, m_weeklyGoal, m_weeklyWords});
    auto* weeklyChoice = new QButtonGroup(m_weeklyGroup);
    weeklyChoice->addButton(m_weeklySum);
    weeklyChoice->addButton(m_weeklyOwn);
    left->addWidget(m_weeklyGroup);
    left->addStretch(1);

    // The goal of the book: its length and its deadline, and what they need
    m_bookGroup = new QGroupBox(m_right);
    auto* book = new QVBoxLayout(m_bookGroup);
    m_lengthLabel = new QLabel(m_bookGroup);
    m_length = wordsBox(MAX_BOOK_WORDS, 1000, m_bookGroup);
    m_length->setObjectName(QStringLiteral("bookLength"));
    m_lengthLabel->setBuddy(m_length);
    m_lengthWords = new QLabel(m_bookGroup);
    rowIn(book, {m_lengthLabel, m_length, m_lengthWords});
    m_hasDeadline = new QCheckBox(m_bookGroup);
    m_hasDeadline->setObjectName(QStringLiteral("bookHasDeadline"));
    m_deadline = new QDateEdit(m_bookGroup);
    m_deadline->setObjectName(QStringLiteral("bookDeadline"));
    m_deadline->setCalendarPopup(true);
    rowIn(book, {m_hasDeadline, m_deadline});
    m_calculation = new QLabel(m_bookGroup);
    m_calculation->setObjectName(QStringLiteral("bookCalculation"));
    m_calculation->setWordWrap(true);
    m_calculation->setTextFormat(Qt::RichText);
    m_calculation->setTextInteractionFlags(Qt::TextSelectableByMouse);
    book->addWidget(m_calculation);
    right->addWidget(m_bookGroup);
    right->addStretch(1);

    // The deadlines of the parts of the book
    m_deadlinesGroup = new QGroupBox(content);
    auto* deadlines = new QVBoxLayout(m_deadlinesGroup);
    m_deadlines = new QTreeWidget(m_deadlinesGroup);
    m_deadlines->setObjectName(QStringLiteral("deadlines"));
    m_deadlines->setColumnCount(ColumnCount);
    m_deadlines->setRootIsDecorated(false);
    m_deadlines->setUniformRowHeights(true);
    m_deadlines->setAllColumnsShowFocus(true);
    m_deadlines->setSelectionMode(QAbstractItemView::SingleSelection);
    m_deadlines->header()->setStretchLastSection(false);
    m_deadlines->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_deadlines->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_deadlines->header()->setSectionsMovable(false);
    // A few rows high; the page grows no higher than its content
    m_deadlines->setMinimumHeight(m_deadlines->fontMetrics().height() * 7);
    m_deadlines->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_deadlines->installEventFilter(this);
    deadlines->addWidget(m_deadlines);
    m_add = new QPushButton(m_deadlinesGroup);
    m_add->setObjectName(QStringLiteral("addDeadline"));
    m_change = new QPushButton(m_deadlinesGroup);
    m_change->setObjectName(QStringLiteral("changeDeadline"));
    m_remove = new QPushButton(m_deadlinesGroup);
    m_remove->setObjectName(QStringLiteral("removeDeadline"));
    rowIn(deadlines, {m_add, m_change, m_remove});
    m_deadlinesNote = new QLabel(m_deadlinesGroup);
    m_deadlinesNote->setWordWrap(true);
    m_notes.append(m_deadlinesNote);
    deadlines->addWidget(m_deadlinesNote);
    layout->addWidget(m_deadlinesGroup);

    m_settings = new QPushButton(content);
    m_settings->setObjectName(QStringLiteral("statsSettings"));
    rowIn(layout, {m_settings});
    layout->addStretch(1);

    // A change works at once: the writer's goals in the settings, the book's in the book
    connect(m_dailyGoal, &QSpinBox::valueChanged, this, [](int value) {
        core::SettingsManager::getInstance().set<int>(DAILY_GOAL_SETTING, value);
    });
    auto storeWeekly = [this]() {
        core::SettingsManager::getInstance().set<int>(
            WEEKLY_GOAL_SETTING, m_weeklyOwn->isChecked() ? m_weeklyGoal->value() : 0);
    };
    connect(m_weeklyOwn, &QRadioButton::toggled, this, storeWeekly);
    connect(m_weeklyGoal, &QSpinBox::valueChanged, this, [this, storeWeekly]() {
        if (!m_weeklyOwn->isChecked()) {
            const QSignalBlocker blocker(m_weeklyOwn);
            m_weeklyOwn->setChecked(true);
        }
        storeWeekly();
    });
    connect(m_fromDeadline, &QCheckBox::toggled, this, [this](bool on) {
        core::BookGoals goals = m_model->goals();
        goals.dailyGoalFromDeadline = on;
        storeGoals(goals);
    });
    connect(m_length, &QSpinBox::valueChanged, this, [this](int value) {
        core::BookGoals goals = m_model->goals();
        goals.targetWords = value;
        storeGoals(goals);
    });
    connect(m_hasDeadline, &QCheckBox::toggled, this, [this](bool on) {
        core::BookGoals goals = m_model->goals();
        goals.deadline = on ? m_deadline->date() : QDate();
        storeGoals(goals);
    });
    connect(m_deadline, &QDateEdit::dateChanged, this, [this](const QDate& date) {
        core::BookGoals goals = m_model->goals();
        goals.deadline = date;
        if (!m_hasDeadline->isChecked()) {
            const QSignalBlocker blocker(m_hasDeadline);
            m_hasDeadline->setChecked(true);
        }
        storeGoals(goals);
    });
    connect(m_deadlines, &QTreeWidget::itemSelectionChanged, this,
            &WritingGoalsTab::updateDeadlineButtons);
    connect(m_deadlines, &QTreeWidget::itemActivated, this, &WritingGoalsTab::changeDeadline);
    connect(m_add, &QPushButton::clicked, this, &WritingGoalsTab::addDeadline);
    connect(m_change, &QPushButton::clicked, this, &WritingGoalsTab::changeDeadline);
    connect(m_remove, &QPushButton::clicked, this, &WritingGoalsTab::removeDeadline);
    connect(m_settings, &QPushButton::clicked, this, &WritingGoalsTab::settingsRequested);

    // What changes in another place shows here at once
    connect(m_model, &WritingStatsModel::changed, this, &WritingGoalsTab::refresh);
    connect(m_model, &WritingStatsModel::goalsChanged, this, &WritingGoalsTab::refresh);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &WritingGoalsTab::updateColors);

    // The keys go to the first field when the tab comes to the front
    setFocusProxy(m_dailyGoal);
    retranslate();
    updateColors();
    refresh();
}

void WritingGoalsTab::setBookSource(BookSource source)
{
    m_source = std::move(source);
    refresh();
}

QString WritingGoalsTab::tabTitle()
{
    //: The tab in the middle of the window. In Polish: Cele i terminy pisania
    return tr("Writing Goals and Deadlines");
}

QString WritingGoalsTab::words(qint64 count)
{
    // The plural forms are those of the language; in English "%1 words" also for 1, rare here
    //: %1: a number of words. In Polish: %1 słowo / %1 słowa / %1 słów
    return tr("%1 words", nullptr, int(std::clamp<qint64>(count, -2'000'000'000, 2'000'000'000)))
        .arg(stats_painter::number(count));
}

void WritingGoalsTab::retranslate()
{
    m_title->setText(tabTitle());
    //: In Polish: Cel dzienny
    m_dailyGroup->setTitle(tr("Daily Goal"));
    //: Before the words of the daily goal. In Polish: &Pisać co najmniej
    m_dailyLabel->setText(tr("&Write at least"));
    //: After the words of the daily goal. In Polish: słów w dniu pisania
    m_dailyWords->setText(tr("words on a writing day"));
    //: In Polish: 0 – bez celu dziennego
    m_dailyGoal->setToolTip(tr("0: no daily goal"));
    //: In Polish: &Dni pisania:
    m_daysLabel->setText(tr("Writing &days:"));
    // The days of the week from Monday, short, with their full names for the screen readers
    const std::array<QString, 7> shortNames{
        //: Monday, short, on a button. In Polish: Pn
        tr("Mo"),
        //: Tuesday, short. In Polish: Wt
        tr("Tu"),
        //: Wednesday, short. In Polish: Śr
        tr("We"),
        //: Thursday, short. In Polish: Cz
        tr("Th"),
        //: Friday, short. In Polish: Pt
        tr("Fr"),
        //: Saturday, short. In Polish: So
        tr("Sa"),
        //: Sunday, short. In Polish: Nd
        tr("Su")};
    const QLocale locale;
    for (int day = 0; day < 7; ++day) {
        QToolButton* button = m_dayButtons[std::size_t(day)];
        button->setText(shortNames[std::size_t(day)]);
        button->setToolTip(locale.dayName(day + 1));
        button->setAccessibleName(locale.dayName(day + 1));
    }
    //: In Polish: Dni wolne nie przerywają serii dni z rzędu.
    m_daysNote->setText(tr("Days off do not break the days in a row."));
    //: In Polish: Ustal cel dzienny z terminu &książki
    m_fromDeadline->setText(tr("Set the daily goal from the deadline of the &book"));
    //: In Polish: Cel tygodniowy
    m_weeklyGroup->setTitle(tr("Weekly Goal"));
    //: In Polish: &Własny:
    m_weeklyOwn->setText(tr("&Own:"));
    //: After the words of the weekly goal. In Polish: słów
    m_weeklyWords->setText(tr("words"));
    //: In Polish: Cel książki
    m_bookGroup->setTitle(tr("Goal of the Book"));
    //: In Polish: Docel&owa długość
    m_lengthLabel->setText(tr("Target &length"));
    //: After the length of the book. In Polish: słów
    m_lengthWords->setText(tr("words"));
    //: In Polish: 0 – bez docelowej długości
    m_length->setToolTip(tr("0: no target length"));
    //: In Polish: &Termin:
    m_hasDeadline->setText(tr("Deadli&ne:"));
    m_deadline->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
    //: In Polish: Termin książki
    m_deadline->setAccessibleName(tr("Deadline of the book"));
    //: In Polish: Terminy
    m_deadlinesGroup->setTitle(tr("Deadlines"));
    m_deadlines->setHeaderLabels({
        //: In Polish: Termin
        tr("Deadline"),
        //: In Polish: Data
        tr("Date"),
        //: In Polish: Czego dotyczy
        tr("What It Is About"),
        //: In Polish: Cel
        tr("Goal"),
        //: In Polish: Stan
        tr("State")});
    //: In Polish: Terminy książki
    m_deadlines->setAccessibleName(tr("Deadlines of the book"));
    //: In Polish: Dod&aj termin...
    m_add->setText(tr("&Add Deadline..."));
    //: In Polish: &Zmień...
    m_change->setText(tr("C&hange..."));
    //: In Polish: &Usuń
    m_remove->setText(tr("&Remove"));
    //: In Polish: Program przypomina o terminie na dole okna (Ustawienia > Statystyki i cele >
    //: Powiadomienia).
    m_deadlinesNote->setText(tr("The program reminds you of a deadline at the bottom of the window "
                                "(Settings > Statistics and Goals > Notifications)."));
    //: In Polish: Ustaw&ienia statystyk...
    m_settings->setText(tr("Statistics S&ettings..."));
    m_settings->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("tools.statsSettings")));
    m_add->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("tools.writingGoals")));
}

void WritingGoalsTab::updateColors()
{
    const QString style = QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name());
    for (QLabel* note : m_notes) {
        note->setStyleSheet(style);
    }
}

void WritingGoalsTab::updateColumns()
{
    const int spacing = m_columns->spacing();
    const int wanted = m_left->minimumSizeHint().width() + m_right->minimumSizeHint().width() +
                       spacing + 8;
    const auto direction = width() >= wanted * 5 / 4 ? QBoxLayout::LeftToRight
                                                     : QBoxLayout::TopToBottom;
    if (m_columns->direction() != direction) {
        m_columns->setDirection(direction);
    }
}

void WritingGoalsTab::refresh()
{
    const core::WritingRules& rules = m_model->rules();

    // The writer's goals
    showValue(m_dailyGoal, rules.dailyGoal);
    for (int day = 0; day < 7; ++day) {
        QToolButton* button = m_dayButtons[std::size_t(day)];
        const bool on = (rules.writingDays & (1 << day)) != 0;
        if (button->isChecked() != on) {
            const QSignalBlocker blocker(button);
            button->setChecked(on);
        }
    }
    {
        const QSignalBlocker sum(m_weeklySum);
        const QSignalBlocker own(m_weeklyOwn);
        (rules.weeklyGoal > 0 ? m_weeklyOwn : m_weeklySum)->setChecked(true);
    }
    core::WritingRules sumRules = rules;
    sumRules.weeklyGoal = 0;
    //: The weekly goal that adds up the daily goals. In Polish: &Suma celów dni pisania: %1
    m_weeklySum->setText(tr("The &sum of the daily goals: %1").arg(words(sumRules.weeklyGoalWords())));
    showValue(m_weeklyGoal, rules.weeklyGoal > 0 ? rules.weeklyGoal
                                                 : std::max(1, sumRules.weeklyGoalWords()));

    // The goals of the book
    const bool book = m_model->hasBook();
    const QString title = book && m_source.title ? m_source.title() : QString();
    if (book) {
        //: %1: the title of the book. In Polish: Cel dzienny i dni pisania są Twoje, wspólne dla
        //: wszystkich książek. Cel książki i terminy zapisują się w książce „%1”. Zmiany działają
        //: od razu.
        m_intro->setText(tr("The daily goal and the writing days are yours, the same for all the "
                            "books. The goal of the book and the deadlines are kept in the book "
                            "“%1”. The changes work at once.")
                             .arg(title));
    } else {
        //: In Polish: Cel dzienny i dni pisania są Twoje, wspólne dla wszystkich książek. Cel
        //: książki i terminy ustawisz po otwarciu książki. Zmiany działają od razu.
        m_intro->setText(tr("The daily goal and the writing days are yours, the same for all the "
                            "books. Open a book to set its goal and deadlines. The changes work "
                            "at once."));
    }
    const core::BookGoals& goals = m_model->goals();
    m_bookGroup->setEnabled(book);
    m_deadlinesGroup->setEnabled(book);
    showValue(m_length, goals.targetWords);
    {
        const QSignalBlocker blocker(m_hasDeadline);
        m_hasDeadline->setChecked(goals.deadline.isValid());
    }
    if (goals.deadline.isValid() && !m_deadline->hasFocus() && m_deadline->date() != goals.deadline) {
        const QSignalBlocker blocker(m_deadline);
        m_deadline->setDate(goals.deadline);
    } else if (!goals.deadline.isValid() && !m_deadline->hasFocus()) {
        const QSignalBlocker blocker(m_deadline);
        m_deadline->setDate(m_model->today().addMonths(3));
    }

    // The daily goal from the deadline: with a length and a deadline still ahead
    const QDate today = m_model->today();
    const bool deadlineAhead = goals.targetWords > 0 && goals.deadline.isValid() &&
                               goals.deadline >= today;
    {
        const QSignalBlocker blocker(m_fromDeadline);
        m_fromDeadline->setChecked(book && goals.dailyGoalFromDeadline);
    }
    m_fromDeadline->setEnabled(book && (deadlineAhead || goals.dailyGoalFromDeadline));
    if (!book) {
        m_fromDeadlineNote->clear();
    } else if (!deadlineAhead) {
        //: In Polish: (potrzebuje długości i terminu książki)
        m_fromDeadlineNote->setText(tr("(needs the length and the deadline of the book)"));
    } else {
        const int wordsToday = m_model->bookStatistics().day(today).counted(rules.counting);
        const int needed = core::dailyGoalForDeadline(rules, goals,
                                                      m_model->bookWords() - wordsToday, today);
        m_fromDeadlineNote->setText(
            rules.isWritingDay(today)
                //: In Polish: (dziś: %1)
                ? tr("(today: %1)").arg(words(needed))
                //: In Polish: (w dniu pisania: %1)
                : tr("(on a writing day: %1)").arg(words(needed)));
    }
    m_dailyGoal->setEnabled(!(book && goals.dailyGoalFromDeadline && deadlineAhead));

    showCalculation();
    showDeadlines();
    updateColumns();
}

void WritingGoalsTab::showCalculation()
{
    if (!m_model->hasBook()) {
        //: In Polish: Otwórz książkę, aby ustawić jej długość i termin.
        m_calculation->setText(tr("Open a book to set its length and deadline."));
        return;
    }
    const core::WritingRules& rules = m_model->rules();
    const core::BookGoals& goals = m_model->goals();
    const QDate today = m_model->today();
    const QLocale locale;
    const int bookWords = m_model->bookWords();
    const int target = goals.targetWords;
    if (target <= 0) {
        //: %1: the words of the book. In Polish: Książka ma %1. Ustaw jej długość, aby zobaczyć,
        //: ile zostało.
        m_calculation->setText(
            tr("The book has %1. Set its length to see how much is left.").arg(words(bookWords)));
        return;
    }
    const int left = std::max(0, target - bookWords);
    if (left == 0) {
        //: In Polish: Książka ma %1: osiągnęła docelową długość.
        m_calculation->setText(tr("The book has %1: it has reached its length.").arg(words(bookWords)));
        return;
    }
    QString text;
    if (goals.deadline.isValid() && goals.deadline >= today) {
        const int days = core::writingDaysBetween(rules, today, goals.deadline);
        //: %1: the words of the book, %2: the words left, %3: the writing days to the
        //: deadline, %4: the words a writing day needs. In Polish: Jest %1, zostało %2 i %3 do
        //: terminu, czyli <b>%4 w dniu pisania</b>.
        text = tr("The book has %1, %2 are left and %3 to the deadline, that is <b>%4 on a "
                  "writing day</b>.")
                   .arg(words(bookWords), words(left),
                        //: In Polish: %1 dzień pisania / %1 dni pisania
                        tr("%1 writing days", nullptr, days).arg(stats_painter::number(days)),
                        words(core::wordsPerWritingDay(left, days)));
    } else if (goals.deadline.isValid()) {
        //: In Polish: Jest %1 i zostało %2. Termin minął.
        text = tr("The book has %1 and %2 are left. The deadline has passed.")
                   .arg(words(bookWords), words(left));
    } else {
        //: In Polish: Jest %1 i zostało %2.
        text = tr("The book has %1 and %2 are left.").arg(words(bookWords), words(left));
    }
    const double average =
        m_model->bookStatistics().averagePerWritingDay(today.addDays(-27), today);
    const QDate forecast = core::forecastDate(rules, today, left, average);
    text += QLatin1Char(' ');
    if (forecast.isValid()) {
        //: %1: the average words of a writing day, %2: a day. In Polish: W obecnym tempie
        //: (średnio %1) skończysz ok. %2.
        text += tr("At the present pace (an average of %1) you will finish about %2.")
                    .arg(words(qint64(std::lround(average))),
                         locale.toString(forecast, QStringLiteral("d MMMM yyyy")));
    } else {
        //: In Polish: Prognoza pojawi się po kilku dniach pisania.
        text += tr("The forecast shows after a few days of writing.");
    }
    m_calculation->setText(text);
}

void WritingGoalsTab::showDeadlines()
{
    // The deadline chosen stays chosen
    const QTreeWidgetItem* current = m_deadlines->currentItem();
    const QString chosenKey = current != nullptr && current->isSelected()
                                  ? current->data(NameColumn, Qt::UserRole + 1).toString()
                                  : QString();

    const core::BookGoals& goals = m_model->goals();
    const QDate today = m_model->today();
    const int next = goals.nextDeadline(today);
    const QLocale locale;
    const QList<dialogs::BookPart> parts =
        m_model->hasBook() && m_source.parts ? m_source.parts() : QList<dialogs::BookPart>();

    const QSignalBlocker blocker(m_deadlines);
    m_deadlines->clear();
    QTreeWidgetItem* again = nullptr;
    for (int index = 0; index < goals.deadlines.size(); ++index) {
        const core::WritingDeadline& deadline = goals.deadlines[index];
        auto* item = new QTreeWidgetItem(m_deadlines);
        item->setText(NameColumn, deadline.name);
        item->setToolTip(NameColumn, deadline.name);
        item->setText(DateColumn, deadline.date.isValid()
                                      ? locale.toString(deadline.date, QLocale::ShortFormat)
                                      : QString());
        QString part;
        if (deadline.elementId.isEmpty()) {
            //: What a deadline is about. In Polish: cała książka
            part = tr("the whole book");
        } else {
            const auto found = std::find_if(parts.cbegin(), parts.cend(),
                                            [&deadline](const dialogs::BookPart& candidate) {
                                                return candidate.id == deadline.elementId;
                                            });
            //: In Polish: część, której nie ma już w książce
            part = found != parts.cend() ? found->title : tr("a part no longer in the book");
        }
        item->setText(PartColumn, part);
        item->setText(GoalColumn, deadline.targetWords > 0 ? words(deadline.targetWords) : QString());
        const int done = deadline.elementId.isEmpty()
                             ? m_model->bookWords()
                             : (m_source.words ? m_source.words(deadline.elementId) : 0);
        QString state = stats_painter::number(done);
        if (deadline.targetWords > 0) {
            state = QStringLiteral("%1 (%2%)").arg(
                state, QString::number(std::lround(100.0 * done / deadline.targetWords)));
        }
        if (deadline.date.isValid() && deadline.date < today) {
            //: The state of a deadline that has passed. In Polish: %1, po terminie
            state = tr("%1, passed").arg(state);
        }
        item->setText(StateColumn, state);
        item->setTextAlignment(GoalColumn, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(StateColumn, Qt::AlignRight | Qt::AlignVCenter);
        item->setData(NameColumn, Qt::UserRole, index);
        const QString key = deadline.name + QChar(0x1f) + deadline.date.toString(Qt::ISODate) +
                            QChar(0x1f) + deadline.elementId + QChar(0x1f) +
                            QString::number(deadline.targetWords);
        item->setData(NameColumn, Qt::UserRole + 1, key);
        if (index == next) {
            // The next deadline stands out
            for (int column = 0; column < ColumnCount; ++column) {
                QFont font = item->font(column);
                font.setBold(true);
                item->setFont(column, font);
            }
            //: %1: the name of a deadline. In Polish: %1 (najbliższy termin)
            item->setToolTip(NameColumn, tr("%1 (the next deadline)").arg(deadline.name));
        }
        if (!chosenKey.isEmpty() && key == chosenKey) {
            again = item;
        }
    }
    if (again != nullptr) {
        m_deadlines->setCurrentItem(again);
    }
    updateDeadlineButtons();
}

void WritingGoalsTab::updateDeadlineButtons()
{
    const bool chosen = m_deadlines->currentItem() != nullptr &&
                        m_deadlines->currentItem()->isSelected();
    m_add->setEnabled(m_model->hasBook());
    m_change->setEnabled(chosen);
    m_remove->setEnabled(chosen);
}

void WritingGoalsTab::storeGoals(const core::BookGoals& goals)
{
    if (!m_model->setGoals(goals)) {
        core::Logger::getInstance().warn("WritingGoalsTab: The goals of the book were not kept");
    }
}

int WritingGoalsTab::writingDaysOfButtons() const
{
    int days = 0;
    for (int day = 0; day < 7; ++day) {
        if (m_dayButtons[std::size_t(day)]->isChecked()) {
            days |= 1 << day;
        }
    }
    return days;
}

void WritingGoalsTab::addDeadline()
{
    if (!m_model->hasBook()) {
        return;
    }
    core::WritingDeadline fresh;
    fresh.date = m_model->today().addMonths(1);
    dialogs::DeadlineDialog dialog(fresh, m_source.parts ? m_source.parts() : QList<dialogs::BookPart>(),
                                   this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    core::BookGoals goals = m_model->goals();
    const core::WritingDeadline added = dialog.deadline();
    goals.deadlines.append(added);
    storeGoals(goals);
    core::Logger::getInstance().info("WritingGoalsTab: Deadline added, due {}",
                                     added.date.toString(Qt::ISODate).toStdString());
    // The new one chosen, where the sorting put it
    for (int row = 0; row < m_deadlines->topLevelItemCount(); ++row) {
        QTreeWidgetItem* item = m_deadlines->topLevelItem(row);
        const int index = item->data(NameColumn, Qt::UserRole).toInt();
        if (index >= 0 && index < m_model->goals().deadlines.size() &&
            m_model->goals().deadlines[index] == added) {
            m_deadlines->setCurrentItem(item);
            break;
        }
    }
    m_deadlines->setFocus();
}

void WritingGoalsTab::changeDeadline()
{
    QTreeWidgetItem* item = m_deadlines->currentItem();
    if (!m_model->hasBook() || item == nullptr) {
        return;
    }
    const int index = item->data(NameColumn, Qt::UserRole).toInt();
    core::BookGoals goals = m_model->goals();
    if (index < 0 || index >= goals.deadlines.size()) {
        return;
    }
    dialogs::DeadlineDialog dialog(goals.deadlines[index],
                                   m_source.parts ? m_source.parts() : QList<dialogs::BookPart>(),
                                   this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const core::WritingDeadline changed = dialog.deadline();
    goals.deadlines[index] = changed;
    storeGoals(goals);
    core::Logger::getInstance().info("WritingGoalsTab: Deadline changed");
    for (int row = 0; row < m_deadlines->topLevelItemCount(); ++row) {
        QTreeWidgetItem* shown = m_deadlines->topLevelItem(row);
        const int at = shown->data(NameColumn, Qt::UserRole).toInt();
        if (at >= 0 && at < m_model->goals().deadlines.size() &&
            m_model->goals().deadlines[at] == changed) {
            m_deadlines->setCurrentItem(shown);
            break;
        }
    }
    m_deadlines->setFocus();
}

void WritingGoalsTab::removeDeadline()
{
    QTreeWidgetItem* item = m_deadlines->currentItem();
    if (!m_model->hasBook() || item == nullptr) {
        return;
    }
    const int index = item->data(NameColumn, Qt::UserRole).toInt();
    core::BookGoals goals = m_model->goals();
    if (index < 0 || index >= goals.deadlines.size()) {
        return;
    }
    //: In Polish: Usuń termin
    if (!dialogs::MessageDialog::confirm(
            this, tr("Remove Deadline"),
            //: %1: the name of the deadline. In Polish: Usunąć termin „%1”?
            tr("Remove the deadline “%1”?").arg(goals.deadlines[index].name),
            //: In Polish: &Usuń
            tr("&Remove"))) {
        return;
    }
    goals.deadlines.removeAt(index);
    storeGoals(goals);
    core::Logger::getInstance().info("WritingGoalsTab: Deadline removed");
    if (m_deadlines->topLevelItemCount() > 0) {
        m_deadlines->setCurrentItem(
            m_deadlines->topLevelItem(std::min(index, m_deadlines->topLevelItemCount() - 1)));
        m_deadlines->setFocus();
    } else {
        m_add->setFocus();
    }
}

void WritingGoalsTab::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
        refresh();
        // The title of its tab, in the middle of the window
        for (QWidget* up = parentWidget(); up != nullptr; up = up->parentWidget()) {
            if (auto* tabs = qobject_cast<QTabWidget*>(up)) {
                if (const int index = tabs->indexOf(this); index >= 0) {
                    tabs->setTabText(index, tabTitle());
                    tabs->setTabToolTip(index, tabTitle());
                }
                break;
            }
        }
    }
}

void WritingGoalsTab::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateColumns();
}

bool WritingGoalsTab::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_deadlines && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Delete && key->modifiers() == Qt::NoModifier) {
            removeDeadline();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace kalahari::gui
