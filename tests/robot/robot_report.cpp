// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file robot_report.cpp
/// @brief The records of the robot and the pages made from them

#include "robot/robot_report.h"

#include <kalahari/version.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMap>
#include <QPixmap>
#include <QSysInfo>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <utility>

namespace kalahari::robot {

namespace {

/// How long a writer waits for the file of the records: the watchdog must not wait for ever
/// for a main thread that hangs in the middle of a record
constexpr std::chrono::seconds WRITE_WAIT{2};

/// The most places a finding of the page names; the others are counted
constexpr int PLACES_SHOWN = 12;

/// The most kinds of findings summary.md lists
constexpr int SUMMARY_ROWS = 60;

/// The most characters of a message in summary.md
constexpr int SUMMARY_TEXT = 220;

/// The name of the records file of a situation
const QString STEPS_FILE = QStringLiteral("steps.jsonl");

/// @brief The texts of @p list in a JSON array
QJsonArray toArray(const QStringList& list) {
    return QJsonArray::fromStringList(list);
}

/// @brief The texts of a JSON array
QStringList fromArray(const QJsonValue& value) {
    QStringList list;
    for (const QJsonValue& item : value.toArray()) {
        list << item.toString();
    }
    return list;
}

/// @brief The severity named @p name; a warning for a name the report does not know
Severity severityFrom(const QString& name) {
    if (name == QLatin1String("error")) {
        return Severity::Error;
    }
    return name == QLatin1String("note") ? Severity::Note : Severity::Warning;
}

QJsonObject toJson(const ShownWindow& window) {
    QJsonObject object;
    object[QStringLiteral("class")] = window.className;
    object[QStringLiteral("title")] = window.title;
    object[QStringLiteral("texts")] = window.texts;
    object[QStringLiteral("shot")] = window.shot;
    object[QStringLiteral("own")] = window.own;
    object[QStringLiteral("modal")] = window.modal;
    object[QStringLiteral("cancellable")] = window.cancellable;
    object[QStringLiteral("shownAfterMs")] = window.shownAfterMs;
    object[QStringLiteral("closedBy")] = window.closedBy;
    object[QStringLiteral("stayed")] = window.stayed;
    return object;
}

ShownWindow windowFromJson(const QJsonObject& object) {
    ShownWindow window;
    window.className = object.value(QStringLiteral("class")).toString();
    window.title = object.value(QStringLiteral("title")).toString();
    window.texts = object.value(QStringLiteral("texts")).toString();
    window.shot = object.value(QStringLiteral("shot")).toString();
    window.own = object.value(QStringLiteral("own")).toBool();
    window.modal = object.value(QStringLiteral("modal")).toBool();
    window.cancellable = object.value(QStringLiteral("cancellable")).toBool();
    window.shownAfterMs = object.value(QStringLiteral("shownAfterMs")).toInteger(-1);
    window.closedBy = object.value(QStringLiteral("closedBy")).toString();
    window.stayed = object.value(QStringLiteral("stayed")).toBool();
    return window;
}

QJsonObject toJson(const Finding& finding) {
    QJsonObject object;
    object[QStringLiteral("check")] = finding.check;
    object[QStringLiteral("severity")] = severityName(finding.severity);
    object[QStringLiteral("message")] = finding.message;
    return object;
}

Finding findingFromJson(const QJsonObject& object) {
    return {object.value(QStringLiteral("check")).toString(),
            severityFrom(object.value(QStringLiteral("severity")).toString()),
            object.value(QStringLiteral("message")).toString()};
}

// =============================================================================
// The pages
// =============================================================================

/// What the report found of one situation
struct SituationRun {
    QString id;                  ///< Its folder
    QJsonObject run;             ///< The record that started it
    QMap<int, StepRecord> steps; ///< Its steps by index, the setups (index 0) left out
    QList<StepRecord> setups;    ///< The records of building its window
    bool done = false;           ///< It ran to the end
    qint64 ms = 0;               ///< How long it ran
};

/// A finding with every place it was found
struct FoundAt {
    Finding finding;    ///< The finding
    QStringList places; ///< Anchor and text of each place: "anchor\ttext"
};

/// @brief The step @p index that began, from its first record
StepRecord begunStep(const QJsonObject& record) {
    StepRecord step;
    step.index = record.value(QStringLiteral("index")).toInt();
    step.command = record.value(QStringLiteral("command")).toString();
    step.label = record.value(QStringLiteral("label")).toString();
    step.menuPath = record.value(QStringLiteral("menuPath")).toString();
    step.outcome = QStringLiteral("unfinished");
    return step;
}

/// @brief A step that began and never ended: the program crashed or hung in it
void markUnfinished(SituationRun& situation, const QHash<int, QString>& phases,
                    const QHash<int, QString>& hangs) {
    for (auto it = situation.steps.begin(); it != situation.steps.end(); ++it) {
        StepRecord& step = it.value();
        if (step.outcome != QLatin1String("unfinished")) {
            continue;
        }
        if (hangs.contains(step.index)) {
            step.add(QStringLiteral("hang"), Severity::Error, hangs.value(step.index));
        } else {
            step.add(QStringLiteral("crash"), Severity::Error,
                     QStringLiteral("The run ended in this step (%1): the program crashed")
                         .arg(phases.value(step.index)));
        }
    }
}

/// @brief Take in one record of a situation's file
void readRecord(const QJsonObject& record, SituationRun& situation, QHash<int, QString>& phases,
                QHash<int, QString>& hangs) {
    const QString type = record.value(QStringLiteral("type")).toString();
    const int index = record.value(QStringLiteral("index")).toInt();
    if (type == QLatin1String("run")) {
        situation.run = record;
    } else if (type == QLatin1String("phase")) {
        phases.insert(index, record.value(QStringLiteral("phase")).toString());
        if (index > 0 && !situation.steps.contains(index)) {
            situation.steps.insert(index, begunStep(record));
        }
    } else if (type == QLatin1String("step")) {
        const StepRecord step = stepFromJson(record);
        if (step.index == 0) {
            situation.setups.append(step);
        } else {
            situation.steps.insert(step.index, step);
        }
    } else if (type == QLatin1String("hang")) {
        hangs.insert(index, record.value(QStringLiteral("message")).toString());
    } else if (type == QLatin1String("done")) {
        situation.done = true;
        situation.ms = record.value(QStringLiteral("ms")).toInteger();
    }
}

/// @brief The situation whose records are in @p file
SituationRun readSituation(const QString& id, const QString& file) {
    SituationRun situation;
    situation.id = id;
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly)) {
        return situation;
    }
    QHash<int, QString> phases;
    QHash<int, QString> hangs;
    while (!input.atEnd()) {
        const QJsonDocument line = QJsonDocument::fromJson(input.readLine());
        if (line.isObject()) {
            readRecord(line.object(), situation, phases, hangs);
        }
    }
    markUnfinished(situation, phases, hangs);
    return situation;
}

/// @brief The situations of the report folder @p folder, in the order of their names
QList<SituationRun> readSituations(const QString& folder) {
    QList<SituationRun> situations;
    const QDir dir(folder);
    for (const QString& id : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString file = dir.filePath(id + QLatin1Char('/') + STEPS_FILE);
        if (QFileInfo::exists(file)) {
            situations.append(readSituation(id, file));
        }
    }
    return situations;
}

/// The counts of a situation for the tables
struct Counts {
    int steps = 0;         ///< Commands
    int ran = 0;           ///< Ran
    int greyed = 0;        ///< Greyed out
    int preparation = 0;   ///< In preparation
    int skipped = 0;       ///< Not run on purpose
    int errors = 0;        ///< Findings that are errors
    int warnings = 0;      ///< Findings that are warnings
    int notes = 0;         ///< Findings that are notes
};

/// @brief Count the findings of @p step into @p counts
void countFindings(const StepRecord& step, Counts& counts) {
    for (const Finding& finding : step.findings) {
        if (finding.severity == Severity::Error) {
            ++counts.errors;
        } else if (finding.severity == Severity::Warning) {
            ++counts.warnings;
        } else {
            ++counts.notes;
        }
    }
}

/// @brief The counts of @p situation
Counts countsOf(const SituationRun& situation) {
    Counts counts;
    for (const StepRecord& step : situation.steps) {
        ++counts.steps;
        if (step.outcome == QLatin1String("ran")) {
            ++counts.ran;
        } else if (step.outcome == QLatin1String("greyed")) {
            ++counts.greyed;
        } else if (step.outcome == QLatin1String("in-preparation")) {
            ++counts.preparation;
        } else if (step.outcome == QLatin1String("skipped")) {
            ++counts.skipped;
        }
        countFindings(step, counts);
    }
    for (const StepRecord& setup : situation.setups) {
        countFindings(setup, counts);
    }
    return counts;
}

/// @brief The anchor of step @p index of situation @p id in index.html
QString anchorOf(const QString& id, int index) {
    return QStringLiteral("%1-%2").arg(id).arg(index);
}

/// @brief Add the findings of @p step at its place to @p found
void collectFindings(const QString& id, const StepRecord& step, QList<FoundAt>& found,
                     QHash<QString, qsizetype>& byKey) {
    const QString place = anchorOf(id, step.index) + QLatin1Char('\t') +
                          QStringLiteral("%1 #%2 %3").arg(id).arg(step.index).arg(step.command);
    for (const Finding& finding : step.findings) {
        const QString key = finding.check + QLatin1Char('\n') + finding.message;
        if (!byKey.contains(key)) {
            byKey.insert(key, found.size());
            found.append({finding, {}});
        }
        found[byKey.value(key)].places.append(place);
    }
}

/// @brief Every finding of the run with its places: the errors first, the most frequent first
QList<FoundAt> findingsOf(const QList<SituationRun>& situations) {
    QList<FoundAt> found;
    QHash<QString, qsizetype> byKey;
    for (const SituationRun& situation : situations) {
        for (const StepRecord& setup : situation.setups) {
            collectFindings(situation.id, setup, found, byKey);
        }
        for (const StepRecord& step : situation.steps) {
            collectFindings(situation.id, step, found, byKey);
        }
    }
    std::stable_sort(found.begin(), found.end(), [](const FoundAt& a, const FoundAt& b) {
        if (a.finding.severity != b.finding.severity) {
            return static_cast<int>(a.finding.severity) < static_cast<int>(b.finding.severity);
        }
        return a.places.size() > b.places.size();
    });
    return found;
}

/// @brief @p ms as minutes and seconds
QString durationText(qint64 ms) {
    const qint64 seconds = ms / 1000;
    return QStringLiteral("%1 min %2 s").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

/// @brief What the run was: its program, system, Qt, build and mode
QString runText(const QList<SituationRun>& situations) {
    const QJsonObject run = situations.isEmpty() ? QJsonObject() : situations.first().run;
    return QStringLiteral("Kalahari %1 · %2 · Qt %3 · %4 · %5 run · %6")
        .arg(run.value(QStringLiteral("version")).toString(),
             run.value(QStringLiteral("platform")).toString(),
             run.value(QStringLiteral("qt")).toString(),
             run.value(QStringLiteral("build")).toString(),
             run.value(QStringLiteral("mode")).toString(),
             run.value(QStringLiteral("started")).toString());
}

/// The look of index.html
const char* const PAGE_STYLE = R"(
body { font-family: system-ui, sans-serif; margin: 16px; color: #1f2328; background: #ffffff; }
h1 { margin-bottom: 4px; }
.meta { color: #59636e; margin-top: 0; }
table { border-collapse: collapse; margin: 8px 0 24px; }
th, td { border: 1px solid #d1d9e0; padding: 4px 8px; vertical-align: top; text-align: left; }
th { background: #f6f8fa; }
tr.error td:first-child { border-left: 4px solid #cf222e; }
tr.warning td:first-child { border-left: 4px solid #bf8700; }
tr.note td:first-child { border-left: 4px solid #8c959f; }
td.num { text-align: right; }
img { max-width: 320px; max-height: 200px; border: 1px solid #d1d9e0; margin: 2px; }
.small { color: #59636e; font-size: 90%; }
ul { margin: 0; padding-left: 18px; }
)";

/// @brief The table of the situations
QString situationsTable(const QList<SituationRun>& situations) {
    QString html = QStringLiteral(
        "<table><tr><th>Situation</th><th>Commands</th><th>Ran</th><th>Greyed</th>"
        "<th>In preparation</th><th>Skipped</th><th>Errors</th><th>Warnings</th><th>Notes</th>"
        "<th>Time</th></tr>\n");
    for (const SituationRun& situation : situations) {
        const Counts counts = countsOf(situation);
        html += QStringLiteral(
                    "<tr><td><a href=\"#%1\">%1</a><br><span class=\"small\">%2</span>%3</td>"
                    "<td class=\"num\">%4</td><td class=\"num\">%5</td><td class=\"num\">%6</td>"
                    "<td class=\"num\">%7</td><td class=\"num\">%8</td><td class=\"num\">%9</td>"
                    "<td class=\"num\">%10</td><td class=\"num\">%11</td><td>%12</td></tr>\n")
                    .arg(situation.id.toHtmlEscaped(),
                         situation.run.value(QStringLiteral("description")).toString().toHtmlEscaped(),
                         situation.done ? QString() : QStringLiteral("<br><b>did not end</b>"))
                    .arg(counts.steps)
                    .arg(counts.ran)
                    .arg(counts.greyed)
                    .arg(counts.preparation)
                    .arg(counts.skipped)
                    .arg(counts.errors)
                    .arg(counts.warnings)
                    .arg(counts.notes)
                    .arg(durationText(situation.ms));
    }
    return html + QStringLiteral("</table>\n");
}

/// @brief The links to the places of a finding
QString placesHtml(const QStringList& places) {
    QStringList links;
    for (qsizetype i = 0; i < places.size() && i < PLACES_SHOWN; ++i) {
        const QStringList parts = places.at(i).split(QLatin1Char('\t'));
        links << QStringLiteral("<a href=\"#%1\">%2</a>")
                     .arg(parts.value(0), parts.value(1).toHtmlEscaped());
    }
    if (places.size() > PLACES_SHOWN) {
        links << QStringLiteral("and %1 more").arg(places.size() - PLACES_SHOWN);
    }
    return links.join(QStringLiteral(", "));
}

/// @brief The table of the findings
QString findingsTable(const QList<FoundAt>& found) {
    if (found.isEmpty()) {
        return QStringLiteral("<p>The robot noticed nothing.</p>\n");
    }
    QString html = QStringLiteral(
        "<table><tr><th>Check</th><th>What happened</th><th>Times</th><th>Where</th></tr>\n");
    for (const FoundAt& item : found) {
        html += QStringLiteral("<tr class=\"%1\"><td>%2</td><td>%3</td><td class=\"num\">%4</td>"
                               "<td>%5</td></tr>\n")
                    .arg(severityName(item.finding.severity), item.finding.check.toHtmlEscaped(),
                         item.finding.message.toHtmlEscaped())
                    .arg(item.places.size())
                    .arg(placesHtml(item.places));
    }
    return html + QStringLiteral("</table>\n");
}

/// @brief A screenshot as a link to itself
QString shotHtml(const QString& shot, const QString& title) {
    if (shot.isEmpty()) {
        return {};
    }
    return QStringLiteral("<a href=\"%1\"><img loading=\"lazy\" src=\"%1\" alt=\"%2\" title=\"%2\"></a>")
        .arg(shot.toHtmlEscaped(), title.toHtmlEscaped());
}

/// @brief The windows of a step, with their screenshots
QString windowsHtml(const StepRecord& step) {
    QString html;
    for (const ShownWindow& window : step.windows) {
        const QString title = window.title.isEmpty() ? window.className : window.title;
        html += shotHtml(window.shot, title);
        html += QStringLiteral("<div class=\"small\">%1 (%2)%3</div>")
                    .arg(title.toHtmlEscaped(), window.className.toHtmlEscaped(),
                         window.closedBy.isEmpty()
                             ? QString()
                             : QStringLiteral(", closed by ") + window.closedBy.toHtmlEscaped());
    }
    return html + shotHtml(step.shot, QStringLiteral("The window after the step"));
}

/// @brief The findings and the messages of a step
QString notesHtml(const StepRecord& step) {
    QStringList items;
    for (const Finding& finding : step.findings) {
        items << QStringLiteral("<li><b>%1</b>: %2</li>")
                     .arg(finding.check.toHtmlEscaped(), finding.message.toHtmlEscaped());
    }
    for (const QString& line : step.log + step.qtMessages + step.opened) {
        items << QStringLiteral("<li class=\"small\">%1</li>").arg(line.toHtmlEscaped());
    }
    if (!step.settings.isEmpty()) {
        items << QStringLiteral("<li class=\"small\">Settings: %1</li>")
                     .arg(step.settings.join(QStringLiteral(", ")).toHtmlEscaped());
    }
    return items.isEmpty() ? QString() : QStringLiteral("<ul>") + items.join(QString()) +
                                             QStringLiteral("</ul>");
}

/// @brief The row of a step in the table of its situation
QString stepRow(const QString& id, const StepRecord& step) {
    const QString where = step.menuPath.isEmpty() ? step.label : step.menuPath;
    const QString severity = step.findings.isEmpty() ? QString()
                                                     : severityName(std::min_element(
                                                           step.findings.begin(), step.findings.end(),
                                                           [](const Finding& a, const Finding& b) {
                                                               return static_cast<int>(a.severity) <
                                                                      static_cast<int>(b.severity);
                                                           })->severity);
    return QStringLiteral("<tr id=\"%1\" class=\"%2\"><td class=\"num\">%3</td><td><b>%4</b><br>"
                          "<span class=\"small\">%5 %6</span></td><td>%7<br><span class=\"small\">%8"
                          "</span></td><td class=\"num\">%9<br><span class=\"small\">%10</span></td>"
                          "<td>%11</td><td>%12</td></tr>\n")
        .arg(anchorOf(id, step.index), severity)
        .arg(step.index)
        .arg(step.command.toHtmlEscaped(), where.toHtmlEscaped(), step.keys.toHtmlEscaped(),
             step.outcome.toHtmlEscaped(), step.reason.toHtmlEscaped())
        .arg(step.durationMs)
        .arg(step.stallMs > 0 ? QStringLiteral("held %1 ms").arg(step.stallMs) : QString())
        .arg(windowsHtml(step), notesHtml(step));
}

/// @brief The steps of a situation
QString stepsTable(const SituationRun& situation) {
    QString html = QStringLiteral("<h2 id=\"%1\">%1</h2>\n<p class=\"small\">%2</p>\n")
                       .arg(situation.id.toHtmlEscaped(),
                            situation.run.value(QStringLiteral("description")).toString().toHtmlEscaped());
    html += QStringLiteral("<table><tr><th>#</th><th>Command</th><th>Outcome</th><th>ms</th>"
                           "<th>Windows</th><th>Noticed</th></tr>\n");
    for (const StepRecord& setup : situation.setups) {
        html += stepRow(situation.id, setup);
    }
    for (const StepRecord& step : situation.steps) {
        html += stepRow(situation.id, step);
    }
    return html + QStringLiteral("</table>\n");
}

/// @brief index.html of the situations
QString indexPage(const QList<SituationRun>& situations) {
    QString html = QStringLiteral("<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
                                  "<title>Kalahari robot</title><style>") +
                   QLatin1String(PAGE_STYLE) +
                   QStringLiteral("</style></head><body>\n<h1>Kalahari robot</h1>\n<p class=\"meta\">%1</p>\n")
                       .arg(runText(situations).toHtmlEscaped());
    html += QStringLiteral("<h2>Situations</h2>\n") + situationsTable(situations);
    html += QStringLiteral("<h2>Findings</h2>\n") + findingsTable(findingsOf(situations));
    for (const SituationRun& situation : situations) {
        html += stepsTable(situation);
    }
    return html + QStringLiteral("</body></html>\n");
}

/// @brief @p text on one line of a Markdown table
QString markdownCell(const QString& text) {
    QString cell = text.simplified();
    if (cell.size() > SUMMARY_TEXT) {
        cell = cell.left(SUMMARY_TEXT - 1) + QChar(0x2026);
    }
    return cell.replace(QLatin1Char('|'), QStringLiteral("\\|"));
}

/// @brief summary.md of the situations
QString summaryPage(const QList<SituationRun>& situations) {
    QString md = QStringLiteral("### Robot: %1\n\n").arg(runText(situations));
    md += QStringLiteral("| Situation | Commands | Ran | Greyed | In preparation | Skipped | Errors | "
                         "Warnings | Notes | Time |\n|---|---|---|---|---|---|---|---|---|---|\n");
    for (const SituationRun& situation : situations) {
        const Counts counts = countsOf(situation);
        md += QStringLiteral("| %1%2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 | %10 | %11 |\n")
                  .arg(situation.id, situation.done ? QString() : QStringLiteral(" (did not end)"))
                  .arg(counts.steps)
                  .arg(counts.ran)
                  .arg(counts.greyed)
                  .arg(counts.preparation)
                  .arg(counts.skipped)
                  .arg(counts.errors)
                  .arg(counts.warnings)
                  .arg(counts.notes)
                  .arg(durationText(situation.ms));
    }
    const QList<FoundAt> found = findingsOf(situations);
    md += QStringLiteral("\n| Check | Severity | Times | First place | What happened |\n|---|---|---|---|---|\n");
    for (qsizetype i = 0; i < found.size() && i < SUMMARY_ROWS; ++i) {
        const FoundAt& item = found.at(i);
        md += QStringLiteral("| %1 | %2 | %3 | %4 | %5 |\n")
                  .arg(item.finding.check, severityName(item.finding.severity))
                  .arg(item.places.size())
                  .arg(markdownCell(item.places.value(0).section(QLatin1Char('\t'), 1)),
                       markdownCell(item.finding.message));
    }
    if (found.size() > SUMMARY_ROWS) {
        md += QStringLiteral("\n%1 more kinds of findings are in index.html.\n").arg(found.size() - SUMMARY_ROWS);
    }
    return md + QStringLiteral("\nEvery step with its screenshots: index.html in the artifact of the run.\n");
}

/// @brief Write @p text to @p path
bool writeText(const QString& path, const QString& text) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(text.toUtf8()) >= 0;
}

/// @brief The build of the program the robot runs
QString buildName() {
#ifdef NDEBUG
    return QStringLiteral("Release");
#else
    return QStringLiteral("Debug");
#endif
}

}  // namespace

QString severityName(Severity severity) {
    switch (severity) {
    case Severity::Error:
        return QStringLiteral("error");
    case Severity::Warning:
        return QStringLiteral("warning");
    case Severity::Note:
        break;
    }
    return QStringLiteral("note");
}

QJsonObject toJson(const StepRecord& step) {
    QJsonObject object;
    object[QStringLiteral("type")] = QStringLiteral("step");
    object[QStringLiteral("index")] = step.index;
    object[QStringLiteral("command")] = step.command;
    object[QStringLiteral("label")] = step.label;
    object[QStringLiteral("menuPath")] = step.menuPath;
    object[QStringLiteral("keys")] = step.keys;
    object[QStringLiteral("outcome")] = step.outcome;
    object[QStringLiteral("reason")] = step.reason;
    object[QStringLiteral("durationMs")] = step.durationMs;
    object[QStringLiteral("stallMs")] = step.stallMs;
    QJsonArray windows;
    for (const ShownWindow& window : step.windows) {
        windows.append(toJson(window));
    }
    object[QStringLiteral("windows")] = windows;
    object[QStringLiteral("log")] = toArray(step.log);
    object[QStringLiteral("qtMessages")] = toArray(step.qtMessages);
    object[QStringLiteral("opened")] = toArray(step.opened);
    object[QStringLiteral("settings")] = toArray(step.settings);
    object[QStringLiteral("shot")] = step.shot;
    QJsonArray findings;
    for (const Finding& finding : step.findings) {
        findings.append(toJson(finding));
    }
    object[QStringLiteral("findings")] = findings;
    return object;
}

StepRecord stepFromJson(const QJsonObject& record) {
    StepRecord step;
    step.index = record.value(QStringLiteral("index")).toInt();
    step.command = record.value(QStringLiteral("command")).toString();
    step.label = record.value(QStringLiteral("label")).toString();
    step.menuPath = record.value(QStringLiteral("menuPath")).toString();
    step.keys = record.value(QStringLiteral("keys")).toString();
    step.outcome = record.value(QStringLiteral("outcome")).toString();
    step.reason = record.value(QStringLiteral("reason")).toString();
    step.durationMs = record.value(QStringLiteral("durationMs")).toInteger();
    step.stallMs = record.value(QStringLiteral("stallMs")).toInteger();
    for (const QJsonValue& window : record.value(QStringLiteral("windows")).toArray()) {
        step.windows.append(windowFromJson(window.toObject()));
    }
    step.log = fromArray(record.value(QStringLiteral("log")));
    step.qtMessages = fromArray(record.value(QStringLiteral("qtMessages")));
    step.opened = fromArray(record.value(QStringLiteral("opened")));
    step.settings = fromArray(record.value(QStringLiteral("settings")));
    step.shot = record.value(QStringLiteral("shot")).toString();
    for (const QJsonValue& finding : record.value(QStringLiteral("findings")).toArray()) {
        step.findings.append(findingFromJson(finding.toObject()));
    }
    return step;
}

Report::Report(const QString& folder, const QString& situation, const QString& description)
    : m_folder(QDir(folder).filePath(situation)), m_situation(situation) {
    QDir(m_folder).removeRecursively();  // the records of an earlier run of the situation
    if (!QDir().mkpath(QDir(m_folder).filePath(QStringLiteral("shots")))) {
        return;
    }
    m_file.setFileName(QDir(m_folder).filePath(STEPS_FILE));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    QJsonObject run;
    run[QStringLiteral("type")] = QStringLiteral("run");
    run[QStringLiteral("situation")] = situation;
    run[QStringLiteral("description")] = description;
    run[QStringLiteral("started")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    run[QStringLiteral("platform")] = QSysInfo::prettyProductName();
    run[QStringLiteral("qt")] = QString::fromLatin1(qVersion());
    run[QStringLiteral("build")] = buildName();
    run[QStringLiteral("version")] = QString::fromLatin1(kalahari::VERSION);
    run[QStringLiteral("mode")] = qEnvironmentVariable("KALAHARI_ROBOT_MODE", QStringLiteral("quick"));
    write(run);
}

Report::~Report() {
    const std::unique_lock lock(m_mutex, WRITE_WAIT);
    if (lock.owns_lock()) {
        m_file.close();
    }
}

void Report::phase(const StepRecord& step, const QString& phase) {
    QJsonObject record;
    record[QStringLiteral("type")] = QStringLiteral("phase");
    record[QStringLiteral("index")] = step.index;
    record[QStringLiteral("command")] = step.command;
    record[QStringLiteral("label")] = step.label;
    record[QStringLiteral("menuPath")] = step.menuPath;
    record[QStringLiteral("phase")] = phase;
    write(record);
}

void Report::end(const StepRecord& step) {
    write(toJson(step));
}

void Report::hang(int index, const QString& command, const QString& message) {
    QJsonObject record;
    record[QStringLiteral("type")] = QStringLiteral("hang");
    record[QStringLiteral("index")] = index;
    record[QStringLiteral("command")] = command;
    record[QStringLiteral("message")] = message;
    write(record);
}

QString Report::saveShot(const QPixmap& shot, const QString& name) {
    const QString relative = QStringLiteral("%1/shots/%2.png").arg(m_situation, name);
    if (shot.isNull() || !shot.save(QDir(m_folder).filePath(QStringLiteral("shots/%1.png").arg(name)))) {
        return {};
    }
    return relative;
}

void Report::finish(int steps, qint64 ms) {
    QJsonObject record;
    record[QStringLiteral("type")] = QStringLiteral("done");
    record[QStringLiteral("steps")] = steps;
    record[QStringLiteral("ms")] = ms;
    write(record);
}

bool Report::writePages(const QString& folder) {
    const QList<SituationRun> situations = readSituations(folder);
    const QDir dir(folder);
    const bool index = writeText(dir.filePath(QStringLiteral("index.html")), indexPage(situations));
    const bool summary = writeText(dir.filePath(QStringLiteral("summary.md")), summaryPage(situations));
    return index && summary;
}

void Report::write(const QJsonObject& record) {
    const std::unique_lock lock(m_mutex, WRITE_WAIT);
    if (!lock.owns_lock() || !m_file.isOpen()) {
        const QByteArray line = QJsonDocument(record).toJson(QJsonDocument::Compact);
        std::fprintf(stderr, "Robot: a record could not be written: %s\n", line.constData());
        return;
    }
    m_file.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n');
    m_file.flush();
}

}  // namespace kalahari::robot
