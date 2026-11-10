/// @file session_summary_dialog.cpp
/// @brief The summary of the session of writing

#include "kalahari/gui/dialogs/session_summary_dialog.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QGridLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace kalahari::gui::dialogs {

namespace {

using stats_painter::number;

constexpr int DAY_MINUTES = 24 * 60;

/// @brief The setting of the summary
constexpr const char* SUMMARY_SETTING = "stats.sessionSummary";

/// @brief The time of the clock at @p minute of a writing day that starts at @p dayStart
QTime clockAt(int minute, int dayStart)
{
    const int clock = ((minute + dayStart) % DAY_MINUTES + DAY_MINUTES) % DAY_MINUTES;
    return QTime(clock / 60, clock % 60);
}

/// @brief The graph of the day: the words of each piece, on a fine grid
class DayGraph : public QWidget {
public:
    DayGraph(const SessionSummary& summary, QWidget* parent)
        : QWidget(parent)
        , m_summary(summary)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(captionHeight() * 2 + PLOT_HEIGHT);
    }

    QSize sizeHint() const override { return {420, captionHeight() * 2 + PLOT_HEIGHT}; }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        const StatsColors colors = StatsColors::current();
        QFont small = font();
        small.setPointSizeF(small.pointSizeF() * 0.85);
        painter.setFont(small);
        const QColor muted = SettingsPage::noteColor();
        const int caption = captionHeight();
        const double max = stats_painter::niceMax(
            m_summary.pieces.isEmpty()
                ? 0.0
                : *std::max_element(m_summary.pieces.cbegin(), m_summary.pieces.cend()));
        painter.setPen(muted);
        //: Above the graph of the summary. In Polish: Słowa na %1 min
        painter.drawText(QRect(0, 0, width() / 2, caption), Qt::AlignLeft | Qt::AlignVCenter,
                         SessionSummary::tr("Words per %1 min").arg(m_summary.pieceMinutes));
        painter.drawText(QRect(width() / 2, 0, width() / 2, caption),
                         Qt::AlignRight | Qt::AlignVCenter, number(qint64(max)));
        const QRectF plot(0.5, caption + 0.5, width() - 1.0, PLOT_HEIGHT - 1.0);
        painter.fillRect(plot, colors.background);
        stats_painter::drawGrid(painter, plot, 6, 3, colors.grid);
        stats_painter::drawArea(painter, plot.adjusted(1, 1, -1, -1), m_summary.pieces, max,
                                colors.line, colors.fill());
        painter.setPen(QPen(colors.frame, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(plot);
        painter.setPen(muted);
        const QLocale locale;
        const QRect bottom(0, int(plot.bottom()) + 1, width(), caption);
        painter.drawText(bottom, Qt::AlignLeft | Qt::AlignVCenter,
                         locale.toString(m_summary.graphFrom, QLocale::ShortFormat));
        painter.drawText(bottom, Qt::AlignRight | Qt::AlignVCenter,
                         locale.toString(m_summary.graphTo, QLocale::ShortFormat));
    }

private:
    static constexpr int PLOT_HEIGHT = 70;

    int captionHeight() const { return fontMetrics().height(); }

    SessionSummary m_summary;
};

}  // namespace

// =============================================================================
// SessionSummary
// =============================================================================

SessionSummary SessionSummary::of(const WritingStatsModel& model, const QDateTime& since,
                                  const QString& title)
{
    SessionSummary summary;
    const core::WritingStatistics statistics = model.goalStatistics();
    const core::WritingRules& rules = model.rules();
    summary.day = model.today();
    summary.bookTitle = title;
    const core::WritingTotals totals = statistics.day(summary.day);
    summary.words = totals.counted(rules.counting);
    // The words pasted are in them when they count
    summary.written = totals.written;
    summary.deleted = totals.deleted;
    summary.activeMs = totals.activeMs;
    summary.goal = model.dailyGoalToday();
    summary.streak = statistics.streak(summary.day);
    if (model.hasBook()) {
        summary.bookWords = model.bookWords();
        summary.bookTarget = model.goals().targetWords;
    }

    // The sessions of the day: the writing with no pause as long as SESSION_GAP_MINUTES
    const int dayStart = statistics.rules().dayStartMinutes;
    const QList<DayPiece> activity =
        WritingStatsModel::dayPieces(statistics, summary.day, core::ACTIVITY_SLOT_MINUTES);
    const int slotMinutes = activity.isEmpty() ? core::ACTIVITY_SLOT_MINUTES
                                               : DAY_MINUTES / int(activity.size());
    // A session that started on a day before: all of today is in it
    const int sinceMinute = since.isValid()
                                ? int(std::clamp<qint64>(model.minutesIntoDay(summary.day, since),
                                                         0, DAY_MINUTES))
                                : -1;
    int lastEnd = -DAY_MINUTES;
    int sessionStart = -1;
    for (int i = 0; i < activity.size(); ++i) {
        const DayPiece& piece = activity[i];
        if (piece.written == 0 && piece.deleted == 0 && piece.activeMs == 0) {
            continue;
        }
        const int start = i * slotMinutes;
        if (start - lastEnd >= SESSION_GAP_MINUTES) {
            ++summary.sessions;
            sessionStart = start;
            summary.sessionWords = 0;
        }
        summary.sessionWords += piece.counted(rules.counting);
        lastEnd = start + slotMinutes;
        if (sinceMinute >= 0 && lastEnd > sinceMinute) {
            summary.writtenSince = true;
        }
    }
    if (summary.sessions > 0) {
        summary.sessionFrom = clockAt(sessionStart, dayStart);
        summary.sessionTo = clockAt(lastEnd, dayStart);
    }

    // The graph of the day, in the hours of the bar
    auto& settings = core::SettingsManager::getInstance();
    const int pieceSetting = settings.get<int>("stats.bar.pieceMinutes", 10);
    const QList<DayPiece> graph =
        WritingStatsModel::dayPieces(statistics, summary.day, pieceSetting);
    const int piece = graph.isEmpty() ? pieceSetting : DAY_MINUTES / int(graph.size());
    summary.pieceMinutes = piece;
    const int nowMinute =
        int(std::clamp<qint64>(model.minutesIntoDay(summary.day, model.now()), 0, DAY_MINUTES));
    const auto [from, to] = WritingStatsModel::shownMinutes(
        graph, piece, dayStart, std::clamp(settings.get<int>("stats.bar.fromHour", 6), 0, 23),
        std::clamp(settings.get<int>("stats.bar.toHour", 24), 1, 24), nowMinute);
    for (int i = from / piece; i < std::min<int>(int(graph.size()), (to + piece - 1) / piece);
         ++i) {
        summary.pieces.append(std::max(0, graph[i].counted(rules.counting)));
    }
    summary.graphFrom = clockAt(from, dayStart);
    summary.graphTo = clockAt(to, dayStart);
    return summary;
}

QString SessionSummary::heading() const
{
    //: In Polish: Dzisiaj: %1
    return tr("Today: %1").arg(stats_painter::words(words));
}

QString SessionSummary::subheading() const
{
    const QLocale locale;
    QStringList parts;
    if (!bookTitle.isEmpty()) {
        //: In Polish: Książka „%1”
        parts << tr("Book “%1”").arg(bookTitle);
    }
    parts << locale.toString(day, QStringLiteral("dddd, d MMMM yyyy"));
    if (sessions > 0) {
        parts << (sessions == 1
                      //: In Polish: 1 sesja
                      ? tr("1 session")
                      //: In Polish: %n sesje / %n sesji
                      : tr("%n sessions", nullptr, sessions));
    }
    return parts.join(QStringLiteral(", "));
}

QList<std::pair<QString, QString>> SessionSummary::details() const
{
    const QLocale locale;
    QList<std::pair<QString, QString>> details;
    if (sessions > 0) {
        //: In Polish: Ta sesja, %1–%2
        details.append({tr("This session, %1–%2")
                            .arg(locale.toString(sessionFrom, QLocale::ShortFormat),
                                 locale.toString(sessionTo, QLocale::ShortFormat)),
                        stats_painter::words(sessionWords)});
    }
    //: In Polish: Napisane i usunięte / %1 i %2
    details.append({tr("Written and deleted"),
                    tr("%1 and %2").arg(number(written), number(deleted))});
    if (activeMs >= 60'000) {
        const double perHour = words * 3'600'000.0 / double(activeMs);
        //: In Polish: Tempo / %1 sł./godz.
        details.append({tr("Pace"), tr("%1 words an hour").arg(number(std::lround(perHour)))});
    }
    //: In Polish: Dni z rzędu
    details.append({tr("Days in a row"), number(streak)});
    if (!bookTitle.isEmpty()) {
        details.append({tr("Book"),
                        bookTarget > 0
                            //: In Polish: %1 z %2 słów
                            ? tr("%1 of %2 words").arg(number(bookWords), number(bookTarget))
                            : stats_painter::words(bookWords)});
    }
    return details;
}

QString SessionSummary::text() const
{
    QStringList lines{heading(), subheading(), QString()};
    //: In Polish: Słowa
    lines << QStringLiteral("%1: %2").arg(tr("Words"), number(words));
    //: In Polish: Czas pisania
    lines << QStringLiteral("%1: %2").arg(tr("Time of writing"),
                                         stats_painter::hoursMinutes(activeMs));
    if (goal > 0) {
        //: In Polish: Cel dzienny
        lines << QStringLiteral("%1: %2%").arg(tr("Daily goal")).arg(
                     std::lround(100.0 * words / goal));
    }
    for (const auto& [name, value] : details()) {
        lines << QStringLiteral("%1: %2").arg(name, value);
    }
    return lines.join(QLatin1Char('\n'));
}

// =============================================================================
// SessionSummaryDialog
// =============================================================================

SessionSummaryDialog::SessionSummaryDialog(const SessionSummary& summary, QWidget* parent)
    : KalahariDialog(parent)
    , m_summary(summary)
{
    //: In Polish: Podsumowanie sesji pisania
    setWindowTitle(tr("Summary of the Writing Session"));
    setHeading(summary.heading(), summary.subheading());
    setHeadingIcon(QStringLiteral("view.statistics"));

    auto* graph = new DayGraph(summary, this);
    graph->setObjectName(QStringLiteral("summaryGraph"));
    contentLayout()->addWidget(graph);

    // The big numbers: the words, the time of writing and the goal of the day
    auto* numbers = new QGridLayout();
    numbers->setHorizontalSpacing(18);
    const auto addNumber = [this, numbers](int column, const QString& caption,
                                           const QString& value) {
        auto* name = new QLabel(caption, this);
        name->setStyleSheet(
            QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
        auto* big = new QLabel(value, this);
        big->setObjectName(QStringLiteral("summaryNumber%1").arg(column));
        QFont font = big->font();
        font.setPointSizeF(font.pointSizeF() * 1.5);
        font.setWeight(QFont::Light);
        big->setFont(font);
        numbers->addWidget(name, 0, column);
        numbers->addWidget(big, 1, column);
    };
    addNumber(0, SessionSummary::tr("Words"), number(summary.words));
    addNumber(1, SessionSummary::tr("Time of writing"),
              stats_painter::hoursMinutes(summary.activeMs));
    if (summary.goal > 0) {
        addNumber(2, SessionSummary::tr("Daily goal"),
                  QStringLiteral("%1%").arg(std::lround(100.0 * summary.words / summary.goal)));
    }
    numbers->setColumnStretch(3, 1);
    contentLayout()->addLayout(numbers);

    // The details, a name and a value in each row
    auto* details = new QGridLayout();
    details->setHorizontalSpacing(12);
    int row = 0;
    for (const auto& [name, value] : summary.details()) {
        auto* nameLabel = new QLabel(name, this);
        nameLabel->setStyleSheet(
            QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
        auto* valueLabel = new QLabel(value, this);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        details->addWidget(nameLabel, row, 0);
        details->addWidget(valueLabel, row, 1, Qt::AlignRight);
        ++row;
    }
    details->setColumnStretch(0, 1);
    contentLayout()->addLayout(details);

    //: In Polish: &Pokazuj podsumowanie przy zamknięciu książki
    m_showAgain = new QCheckBox(tr("&Show the summary when the book closes"), this);
    m_showAgain->setObjectName(QStringLiteral("summaryShowAgain"));
    m_showAgain->setChecked(core::SettingsManager::getInstance().get<bool>(SUMMARY_SETTING, true));
    connect(m_showAgain, &QCheckBox::toggled, this, [](bool on) {
        core::SettingsManager::getInstance().set<bool>(SUMMARY_SETTING, on);
        core::Logger::getInstance().info("Session summary {}", on ? "on" : "off");
    });
    contentLayout()->addWidget(m_showAgain);
    contentLayout()->addStretch(1);

    //: In Polish: &Kopiuj
    QPushButton* copy = addSideButton(tr("&Copy"));
    copy->setObjectName(QStringLiteral("summaryCopy"));
    connect(copy, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_summary.text());
    });
    setAcceptText(tr("OK"));
    cancelButton()->hide();
    acceptButton()->setFocus();
}

}  // namespace kalahari::gui::dialogs
