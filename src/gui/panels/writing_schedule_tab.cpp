// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_schedule_tab.cpp
/// @brief The tab Writing Schedule

#include "kalahari/gui/panels/writing_schedule_tab.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/text_plan_dialog.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include <algorithm>

namespace kalahari::gui {

namespace {

constexpr const char* SCALE_DAYS = "days";
constexpr const char* SCALE_WEEKS = "weeks";
constexpr const char* SCALE_MONTHS = "months";

/// @brief The days of the record before today that give the pace of the forecast, as on the
/// tab Writing Goals and Deadlines
constexpr int PACE_DAYS = 27;

ScheduleView::Scale scaleOf(const std::string& name)
{
    if (name == SCALE_DAYS) {
        return ScheduleView::Scale::Days;
    }
    if (name == SCALE_MONTHS) {
        return ScheduleView::Scale::Months;
    }
    return ScheduleView::Scale::Weeks;
}

const char* nameOf(ScheduleView::Scale scale)
{
    switch (scale) {
    case ScheduleView::Scale::Days:
        return SCALE_DAYS;
    case ScheduleView::Scale::Weeks:
        return SCALE_WEEKS;
    case ScheduleView::Scale::Months:
        return SCALE_MONTHS;
    }
    return SCALE_WEEKS;
}

QToolButton* scaleButton(QWidget* parent, const char* name)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setCheckable(true);
    button->setAutoRaise(false);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setFocusPolicy(Qt::StrongFocus);
    return button;
}

}  // namespace

WritingScheduleTab::WritingScheduleTab(WritingStatsModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("writingScheduleTab"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 4);

    // The heading and the buttons of the chart
    auto* header = new QHBoxLayout();
    m_title = new QLabel(this);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.2);
    m_title->setFont(titleFont);
    header->addWidget(m_title);
    header->addStretch(1);
    m_days = scaleButton(this, "scheduleDays");
    m_weeks = scaleButton(this, "scheduleWeeks");
    m_months = scaleButton(this, "scheduleMonths");
    auto* scales = new QButtonGroup(this);
    scales->setExclusive(true);
    for (QToolButton* button : {m_days, m_weeks, m_months}) {
        scales->addButton(button);
        header->addWidget(button);
    }
    m_today = new QPushButton(this);
    m_today->setObjectName(QStringLiteral("scheduleToday"));
    header->addWidget(m_today);
    m_daysOff = new QCheckBox(this);
    m_daysOff->setObjectName(QStringLiteral("scheduleDaysOff"));
    header->addWidget(m_daysOff);
    m_goals = new QPushButton(this);
    m_goals->setObjectName(QStringLiteral("scheduleGoals"));
    m_goals->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("tools.writingGoals")));
    header->addWidget(m_goals);
    m_settings = new QToolButton(this);
    m_settings->setObjectName(QStringLiteral("scheduleSettings"));
    m_settings->setAutoRaise(true);
    m_settings->setFocusPolicy(Qt::StrongFocus);
    m_settings->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("dock.settings"),
                                                                  core::IconContext::Panel));
    header->addWidget(m_settings);
    layout->addLayout(header);

    m_intro = new QLabel(this);
    m_intro->setObjectName(QStringLiteral("scheduleIntro"));
    m_intro->setWordWrap(true);
    m_intro->setTextFormat(Qt::PlainText);
    m_intro->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_intro);

    m_view = new ScheduleView(this);
    layout->addWidget(m_view, 1);

    m_details = new QLabel(this);
    m_details->setObjectName(QStringLiteral("scheduleDetails"));
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::PlainText);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(m_details);

    // What the writer chose last time
    auto& settings = core::SettingsManager::getInstance();
    m_view->setScale(scaleOf(settings.get<std::string>(SCALE_SETTING, SCALE_WEEKS)));
    m_view->setDaysOffShown(settings.get<bool>(DAYS_OFF_SETTING, true));
    m_days->setChecked(m_view->scale() == ScheduleView::Scale::Days);
    m_weeks->setChecked(m_view->scale() == ScheduleView::Scale::Weeks);
    m_months->setChecked(m_view->scale() == ScheduleView::Scale::Months);
    m_daysOff->setChecked(m_view->daysOffShown());

    connect(m_days, &QToolButton::clicked, this,
            [this]() { chooseScale(ScheduleView::Scale::Days); });
    connect(m_weeks, &QToolButton::clicked, this,
            [this]() { chooseScale(ScheduleView::Scale::Weeks); });
    connect(m_months, &QToolButton::clicked, this,
            [this]() { chooseScale(ScheduleView::Scale::Months); });
    connect(m_today, &QPushButton::clicked, this, [this]() {
        m_view->showDate(m_model->today());
        m_view->setFocus();
    });
    connect(m_daysOff, &QCheckBox::toggled, this, [this](bool shown) {
        m_view->setDaysOffShown(shown);
        core::SettingsManager::getInstance().set<bool>(DAYS_OFF_SETTING, shown);
    });
    connect(m_goals, &QPushButton::clicked, this, &WritingScheduleTab::goalsRequested);
    connect(m_settings, &QToolButton::clicked, this, &WritingScheduleTab::settingsRequested);
    connect(m_view, &ScheduleView::planEdited, this, &WritingScheduleTab::storePlan);
    connect(m_view, &ScheduleView::editRequested, this, &WritingScheduleTab::editPlan);
    connect(m_view, &ScheduleView::currentChanged, this, [this]() {
        showDetails();
        emit editStatesChanged();
    });

    // What the model has, while the tab is in sight; when it is hidden, once it is shown
    const auto follow = [this]() {
        if (isVisible()) {
            refresh();
        } else {
            m_stale = true;
        }
    };
    connect(m_model, &WritingStatsModel::changed, this, follow);
    connect(m_model, &WritingStatsModel::goalsChanged, this, follow);
    connect(m_model, &WritingStatsModel::planChanged, this, [this, follow]() {
        if (!m_storing) {
            // Another book or volume: its own changes to undo
            m_undo.clear();
            m_redo.clear();
            emit editStatesChanged();
        }
        follow();
    });

    retranslate();
    refresh();
}

void WritingScheduleTab::setRowsSource(std::function<QList<ScheduleRow>()> source)
{
    m_rowsSource = std::move(source);
    refresh();
}

QString WritingScheduleTab::tabTitle()
{
    //: The tab of the schedule of the book. In Polish: Harmonogram
    return tr("Schedule");
}

void WritingScheduleTab::refresh()
{
    m_stale = false;
    m_view->setRows(m_rowsSource && m_model->hasBook() ? m_rowsSource() : QList<ScheduleRow>());
    takePlan();
    showFacts();
    retranslate();
}

void WritingScheduleTab::takePlan()
{
    m_view->setReadOnly(isReadOnly());
    if (m_view->plan() != m_model->plan()) {
        m_view->setPlan(m_model->plan());
    }
}

void WritingScheduleTab::showFacts()
{
    ScheduleFacts facts;
    facts.today = m_model->today();
    facts.rules = m_model->rules();
    if (m_model->hasBook()) {
        const core::BookGoals& goals = m_model->goals();
        facts.bookTargetWords = goals.targetWords;
        facts.bookDeadline = goals.deadline;
        facts.deadlines = goals.deadlines;
        const core::WritingStatistics statistics = m_model->volumeStatistics();
        if (goals.targetWords > 0) {
            const int left = std::max(0, goals.targetWords - m_model->bookWords());
            const double average =
                statistics.averagePerWritingDay(facts.today.addDays(-PACE_DAYS), facts.today);
            facts.forecast = core::forecastDate(facts.rules, facts.today, left, average);
        }
        const QMap<QDate, QList<core::WritingActivity>>& record = statistics.record();
        for (auto day = record.cbegin(); day != record.cend(); ++day) {
            for (const core::WritingActivity& entry : day.value()) {
                const int written = statistics.counted(entry).written;
                if (written > 0 && !entry.elementId.isEmpty()) {
                    facts.written[entry.elementId][day.key()] += written;
                }
            }
        }
    }
    m_view->setFacts(facts);
    showDetails();
}

void WritingScheduleTab::showDetails()
{
    const QString text = m_view->describe(m_view->currentRow());
    if (m_details->text() != text) {
        m_details->setText(text);
    }
}

bool WritingScheduleTab::isReadOnly() const
{
    return !m_model->hasBook() || m_model->isBookReadOnly();
}

void WritingScheduleTab::storePlan(const core::WritingPlan& plan)
{
    const core::WritingPlan before = m_model->plan();
    if (isReadOnly() || plan == before) {
        takePlan();
        return;
    }
    m_storing = true;
    const bool kept = m_model->setPlan(plan);
    m_storing = false;
    if (kept && m_model->plan() != before) {
        m_undo.append(before);
        while (m_undo.size() > UNDO_LIMIT) {
            m_undo.removeFirst();
        }
        m_redo.clear();
        emit editStatesChanged();
    }
    takePlan();
    showDetails();
}

void WritingScheduleTab::undo()
{
    if (m_undo.isEmpty() || isReadOnly()) {
        return;
    }
    const core::WritingPlan now = m_model->plan();
    const core::WritingPlan before = m_undo.takeLast();
    m_storing = true;
    m_model->setPlan(before);
    m_storing = false;
    m_redo.append(now);
    takePlan();
    showDetails();
    emit editStatesChanged();
}

void WritingScheduleTab::redo()
{
    if (m_redo.isEmpty() || isReadOnly()) {
        return;
    }
    const core::WritingPlan now = m_model->plan();
    const core::WritingPlan after = m_redo.takeLast();
    m_storing = true;
    m_model->setPlan(after);
    m_storing = false;
    m_undo.append(now);
    takePlan();
    showDetails();
    emit editStatesChanged();
}

void WritingScheduleTab::editPlan(const QString& elementId)
{
    const int row = m_view->rowOf(elementId);
    if (row < 0 || elementId.isEmpty()) {
        return;
    }
    const ScheduleRow& text = m_view->rows().at(row);
    core::TextPlan plan;
    if (const core::TextPlan* found = m_model->plan().find(elementId)) {
        plan = *found;
    }
    plan.elementId = elementId;
    dialogs::TextPlanDialog dialog(text.title, plan, text.words, m_model->rules(),
                                   m_model->today(), window());
    if (isReadOnly()) {
        dialog.acceptButton()->setEnabled(false);
    }
    if (dialog.exec() != QDialog::Accepted || isReadOnly()) {
        m_view->setFocus();
        return;
    }
    core::WritingPlan changed = m_model->plan();
    if (dialog.isRemoved()) {
        changed.remove(elementId);
    } else {
        changed.set(dialog.plan());
    }
    storePlan(changed);
    m_view->setFocus();
}

void WritingScheduleTab::chooseScale(ScheduleView::Scale scale)
{
    m_view->setScale(scale);
    core::SettingsManager::getInstance().set<std::string>(SCALE_SETTING, nameOf(scale));
}

// =============================================================================
// Edit > Undo, Redo, Delete
// =============================================================================

bool WritingScheduleTab::takesEdit(EditCommand command) const
{
    return command == EditCommand::Undo || command == EditCommand::Redo
           || command == EditCommand::Delete;
}

bool WritingScheduleTab::canEditHere(EditCommand command) const
{
    if (isReadOnly()) {
        return false;
    }
    switch (command) {
    case EditCommand::Undo:
        return canUndo();
    case EditCommand::Redo:
        return canRedo();
    case EditCommand::Delete: {
        const QString text = m_view->currentText();
        return !text.isEmpty() && m_model->plan().find(text) != nullptr;
    }
    default:
        return false;
    }
}

void WritingScheduleTab::editHere(EditCommand command)
{
    switch (command) {
    case EditCommand::Undo:
        undo();
        break;
    case EditCommand::Redo:
        redo();
        break;
    case EditCommand::Delete: {
        core::WritingPlan changed = m_model->plan();
        if (changed.remove(m_view->currentText())) {
            storePlan(changed);
        }
        break;
    }
    default:
        break;
    }
}

QMetaObject::Connection WritingScheduleTab::followEditStates(QObject* receiver,
                                                             std::function<void()> changed)
{
    return connect(this, &WritingScheduleTab::editStatesChanged, receiver, std::move(changed));
}

// =============================================================================
// The texts
// =============================================================================

void WritingScheduleTab::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_stale) {
        refresh();
    }
}

void WritingScheduleTab::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
        showDetails();
    } else if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        m_view->refreshColors();
        m_settings->setIcon(core::ArtProvider::getInstance().getIcon(
            QStringLiteral("dock.settings"), core::IconContext::Panel));
        m_goals->setIcon(
            core::ArtProvider::getInstance().getIcon(QStringLiteral("tools.writingGoals")));
    }
}

void WritingScheduleTab::retranslate()
{
    //: The heading of the tab. In Polish: Harmonogram pracy
    m_title->setText(tr("Writing Schedule"));
    //: The scale of the chart: a day is wide. In Polish: Dn&i
    m_days->setText(tr("Day&s"));
    //: In Polish: T&ygodnie
    m_weeks->setText(tr("Wee&ks"));
    //: In Polish: &Miesiące
    m_months->setText(tr("Mo&nths"));
    //: Scrolls the chart to today. In Polish: D&ziś
    m_today->setText(tr("Toda&y"));
    //: Shades the days the writer does not write on. In Polish: Zaznacz dni wo&lne
    m_daysOff->setText(tr("Ma&rk days off"));
    //: Opens the tab Writing Goals and Deadlines. In Polish: Cele i te&rminy...
    m_goals->setText(tr("Goals and Deadlines..."));
    //: In Polish: Kolory harmonogramu
    const QString colors = tr("Colors of the Schedule");
    m_settings->setToolTip(colors);
    m_settings->setAccessibleName(colors);
    QString intro;
    if (!m_model->hasBook()) {
        //: In Polish: Otwórz książkę, aby zaplanować pracę nad jej tekstami.
        intro = tr("Open a book to plan the work on its texts.");
    } else if (m_model->isBookReadOnly()) {
        //: In Polish: Książka jest otwarta tylko do odczytu: jej plan się nie zmienia.
        intro = tr("The book is open read-only: its plan does not change.");
    } else {
        //: In Polish: Przeciągnięcie paska przesuwa go, a przeciągnięcie jego prawej krawędzi
        //: zmienia ostatni dzień; przeciągnięcie w pustym wierszu planuje tekst. Klawisze:
        //: strzałki w górę i w dół wybierają tekst, Alt+strzałka w lewo i Alt+strzałka w
        //: prawo przesuwają jego plan o dzień, Alt+Shift+strzałka w lewo i Alt+Shift+strzałka
        //: w prawo zmieniają ostatni dzień pisania, Enter otwiera plan tekstu, Delete go
        //: usuwa.
        intro = tr("Dragging a bar moves it, dragging its right edge changes its last day; "
                   "dragging in an empty row plans the text. Keys: Up and Down choose a text, "
                   "Alt+Left and Alt+Right move its plan by a day, Alt+Shift+Left and "
                   "Alt+Shift+Right change the last day of writing, Enter opens the plan of "
                   "the text, Delete removes it.");
    }
    m_intro->setText(intro);
}

}  // namespace kalahari::gui
