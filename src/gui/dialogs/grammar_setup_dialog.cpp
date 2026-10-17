/// @file grammar_setup_dialog.cpp
/// @brief The window of the preparation of grammar checking: what is downloaded, how it goes,
///        that it is ready, or why it stopped

#include "kalahari/gui/dialogs/grammar_setup_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/gui/grammar_components.h"
#include "kalahari/gui/grammar_setup.h"
#include "kalahari/gui/issue_colors.h"
#include "kalahari/gui/settings/settings_page.h"

#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace kalahari::gui::dialogs {

namespace {

constexpr int WIDTH = 600;            ///< The window, as the mockup has it
constexpr int HEIGHT = 440;
constexpr int ICON_SIZE = 16;         ///< The icons of the lines
constexpr int STEP_SPACING = 10;      ///< Between the steps
constexpr int ROW_SPACING = 6;        ///< Between the rows of what is downloaded
constexpr int DETAILS_LINES = 4;      ///< Height of the details, in lines
constexpr int WAVE_WIDTH = 26;        ///< The underlines of the legend
constexpr int WAVE_HEIGHT = 8;
constexpr qint64 MEGABYTE = 1'000'000;
constexpr qint64 TOTAL_ROUNDING = 10'000'000;  ///< "About 300 MB": to tens of megabytes

/// @brief Megabytes, rounded
qint64 megabytes(qint64 bytes) {
    return (bytes + MEGABYTE / 2) / MEGABYTE;
}

/// @brief A muted text (a note)
QLabel* noteLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    return label;
}

/// @brief A text that wraps
QLabel* textLabel(const QString& text = QString()) {
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

/// @brief A wave of the underlines, for the legend
QPixmap wavePixmap(const QColor& color, qreal ratio) {
    QPixmap pixmap(QSize(WAVE_WIDTH, WAVE_HEIGHT) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.4));
    QPainterPath wave(QPointF(0, WAVE_HEIGHT / 2.0 + 1));
    for (int x = 2; x <= WAVE_WIDTH; x += 2) {
        wave.lineTo(QPointF(x, (x / 2) % 2 == 1 ? 2.0 : WAVE_HEIGHT - 2.0));
    }
    painter.drawPath(wave);
    return pixmap;
}

/// @brief A sentence with the underlines of the editor: [grammar] and {spelling}
QTextEdit* underlineSample(const QString& marked, QWidget* parent) {
    auto* sample = new QTextEdit(parent);
    sample->setReadOnly(true);
    sample->setFrameShape(QFrame::NoFrame);
    sample->setFocusPolicy(Qt::NoFocus);
    sample->setTextInteractionFlags(Qt::NoTextInteraction);
    sample->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sample->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sample->viewport()->setAutoFillBackground(false);
    sample->setAccessibleName(QString(marked).remove(QLatin1Char('[')).remove(QLatin1Char(']'))
                                  .remove(QLatin1Char('*')));

    // The colors of the editor's waves on paper like the window
    const bool darkPaper = parent->palette().color(QPalette::Window).lightness() < 128;
    QTextCursor cursor(sample->document());
    QTextCharFormat plain;
    QTextCharFormat grammar;
    grammar.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    grammar.setUnderlineColor(issueColor(editor::TextIssueKind::Grammar, darkPaper));
    QTextCharFormat spelling = grammar;
    spelling.setUnderlineColor(issueColor(editor::TextIssueKind::Spelling, darkPaper));

    // Braces are kept for the names of kinds in translated texts: a misspelled word is between
    // two asterisks
    QString piece;
    QTextCharFormat format = plain;
    bool misspelled = false;
    for (const QChar character : marked) {
        if (character == QLatin1Char('[') || character == QLatin1Char(']') ||
            character == QLatin1Char('*')) {
            cursor.insertText(piece, format);
            piece.clear();
            misspelled = character == QLatin1Char('*') && !misspelled;
            format = character == QLatin1Char('[') ? grammar : misspelled ? spelling : plain;
            continue;
        }
        piece += character;
    }
    cursor.insertText(piece, format);

    // As high as its text, at any width
    const auto fit = [sample]() {
        sample->document()->setTextWidth(sample->viewport()->width());
        sample->setFixedHeight(static_cast<int>(std::ceil(sample->document()->size().height())) +
                               2 * sample->frameWidth());
    };
    QObject::connect(sample->document()->documentLayout(),
                     &QAbstractTextDocumentLayout::documentSizeChanged, sample, fit);
    fit();
    return sample;
}

}  // namespace

GrammarSetupDialog::GrammarSetupDialog(GrammarSetup* setup, QWidget* parent)
    : KalahariDialog(parent)
    , m_setup(setup)
{
    setObjectName(QStringLiteral("grammarSetupDialog"));
    setHeadingIcon(QStringLiteral("tools.grammar"));
    setCompactHeading(true);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(createPlanPage());
    m_pages->addWidget(createStepsPage());
    m_pages->addWidget(createDonePage());
    contentLayout()->addWidget(m_pages);

    // Cancel of the steps breaks the preparation off; the button of the dialog only closes it
    m_stopButton = buttonBox()->addButton(tr("&Cancel"), QDialogButtonBox::ActionRole);
    m_stopButton->setAutoDefault(false);
    connect(m_stopButton, &QPushButton::clicked, this, [this]() { m_setup->cancel(); });
    m_copyButton = addSideButton(tr("&Copy"));
    connect(m_copyButton, &QPushButton::clicked, this, &GrammarSetupDialog::copyFailure);

    connect(m_setup, &GrammarSetup::progress, this, &GrammarSetupDialog::updateSteps);
    connect(m_setup, &GrammarSetup::finished, this, &GrammarSetupDialog::onFinished);

    resize(WIDTH, HEIGHT);
    showState();
}

// =============================================================================
// The pages
// =============================================================================

QWidget* GrammarSetupDialog::createPlanPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(textLabel(tr("Grammar checking needs two free components. The program "
                                   "downloads them from their official sites and prepares "
                                   "them itself.")));

    // What is downloaded, and how much
    auto* table = new QGridLayout();
    table->setHorizontalSpacing(2 * ROW_SPACING);
    table->setVerticalSpacing(ROW_SPACING);
    QFont bold = font();
    bold.setBold(true);
    const auto addRow = [table, &bold](int row, const QString& name, const QString& purpose,
                                       QLabel* size) {
        auto* nameLabel = new QLabel(name);
        nameLabel->setFont(bold);
        table->addWidget(nameLabel, row, 0);
        table->addWidget(textLabel(purpose), row, 1);
        size->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->addWidget(size, row, 2);
    };
    m_languageToolSize = new QLabel();
    addRow(0,
           tr("LanguageTool %1").arg(QLatin1String(GrammarComponents::LANGUAGE_TOOL_VERSION)),
           tr("checks grammar, style and punctuation"), m_languageToolSize);
    m_javaSize = new QLabel();
    addRow(1, tr("Java %1 (Eclipse Temurin)").arg(GrammarComponents::JAVA_VERSION),
           tr("needed to run LanguageTool"), m_javaSize);
    m_totalSize = new QLabel();
    m_totalSize->setFont(bold);
    addRow(2, tr("Total to download"), QString(), m_totalSize);
    table->setColumnStretch(1, 1);
    layout->addLayout(table);

    // Whether it can be done: room, folder, internet
    const auto addCheck = [layout](QLabel*& icon, QLabel* text) {
        auto* row = new QHBoxLayout();
        icon = new QLabel();
        icon->setFixedSize(ICON_SIZE, ICON_SIZE);
        row->addWidget(icon, 0, Qt::AlignTop);
        row->addWidget(text, 1);
        layout->addLayout(row);
        return row;
    };
    m_room = textLabel();
    QHBoxLayout* roomRow = addCheck(m_roomIcon, m_room);
    m_checkAgainButton = new QPushButton(tr("Check &Again"));
    m_checkAgainButton->setAutoDefault(false);
    roomRow->addWidget(m_checkAgainButton, 0, Qt::AlignTop);
    connect(m_checkAgainButton, &QPushButton::clicked, this, &GrammarSetupDialog::updatePlan);

    m_folder = textLabel();
    m_folder->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addCheck(m_folderIcon, m_folder);

    QLabel* internetIcon = nullptr;
    addCheck(internetIcon, textLabel(tr("The internet is needed only for the download. The "
                                        "grammar is checked on this computer - the text of "
                                        "the book does not leave it.")));
    setIcon(internetIcon, QStringLiteral("common.check"));

    layout->addWidget(noteLabel(tr("Licenses: LanguageTool - LGPL 2.1, Java (Eclipse Temurin) "
                                   "- GPL 2 with the Classpath Exception.")));
    layout->addStretch(1);
    return page;
}

QWidget* GrammarSetupDialog::createStepsPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    // Why it stopped (after a failure)
    auto* failureRow = new QHBoxLayout();
    m_failureIcon = new QLabel();
    m_failureIcon->setFixedSize(ICON_SIZE, ICON_SIZE);
    setIcon(m_failureIcon, QStringLiteral("common.warning"));
    failureRow->addWidget(m_failureIcon, 0, Qt::AlignTop);
    m_failure = textLabel();
    QFont bold = font();
    bold.setBold(true);
    m_failure->setFont(bold);
    failureRow->addWidget(m_failure, 1);
    layout->addLayout(failureRow);

    const std::array<QString, 4> titles = {
        tr("Downloading Java %1 (Eclipse Temurin)").arg(GrammarComponents::JAVA_VERSION),
        tr("Downloading LanguageTool %1")
            .arg(QLatin1String(GrammarComponents::LANGUAGE_TOOL_VERSION)),
        tr("Unpacking and checking the files"),
        tr("First start and a trial check"),
    };
    auto* steps = new QVBoxLayout();
    steps->setSpacing(STEP_SPACING);
    for (std::size_t index = 0; index < m_steps.size(); ++index) {
        StepRow& step = m_steps.at(index);
        auto* stepLayout = new QVBoxLayout();
        stepLayout->setSpacing(2);
        auto* line = new QHBoxLayout();
        step.icon = new QLabel();
        step.icon->setFixedSize(ICON_SIZE, ICON_SIZE);
        line->addWidget(step.icon);
        step.title = new QLabel(titles.at(index));
        line->addWidget(step.title, 1);
        step.value = new QLabel();
        step.value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(step.value);
        stepLayout->addLayout(line);
        step.bar = new QProgressBar();
        step.bar->setTextVisible(false);
        step.bar->setMaximumHeight(fontMetrics().height() / 2);
        step.bar->setAccessibleName(titles.at(index));
        stepLayout->addWidget(step.bar);
        steps->addLayout(stepLayout);
    }
    layout->addLayout(steps);

    // While it goes
    m_backgroundNote = noteLabel(tr("You can download in the background and go on writing - "
                                    "the status bar shows the progress."));
    layout->addWidget(m_backgroundNote);
    m_cancelNote = noteLabel(tr("Cancel stops the download. The components downloaded "
                                "already stay for the next try."));
    layout->addWidget(m_cancelNote);

    // After a failure: what to do, and the details
    m_hint = textLabel();
    layout->addWidget(m_hint);
    m_detailsToggle = new QToolButton();
    m_detailsToggle->setText(tr("&Details"));
    m_detailsToggle->setCheckable(true);
    m_detailsToggle->setAutoRaise(true);
    m_detailsToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_detailsToggle->setArrowType(Qt::RightArrow);
    layout->addWidget(m_detailsToggle, 0, Qt::AlignLeft);
    m_details = new QPlainTextEdit();
    m_details->setReadOnly(true);
    m_details->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_details->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_details->setFixedHeight(m_details->fontMetrics().lineSpacing() * DETAILS_LINES +
                              2 * m_details->frameWidth() +
                              static_cast<int>(2 * m_details->document()->documentMargin()));
    m_details->setAccessibleName(tr("Details"));
    m_details->hide();
    layout->addWidget(m_details);
    connect(m_detailsToggle, &QToolButton::toggled, this, [this](bool shown) {
        m_detailsToggle->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
        m_details->setVisible(shown);
    });

    layout->addStretch(1);
    return page;
}

QWidget* GrammarSetupDialog::createDonePage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* readyRow = new QHBoxLayout();
    auto* readyIcon = new QLabel();
    readyIcon->setPixmap(core::ArtProvider::getInstance()
                             .getIcon(QStringLiteral("common.checkCircle"))
                             .pixmap(QSize(2 * ICON_SIZE, 2 * ICON_SIZE)));
    readyRow->addWidget(readyIcon);
    auto* ready = textLabel(tr("Grammar checking is ready and turned on."));
    QFont bold = font();
    bold.setBold(true);
    ready->setFont(bold);
    readyRow->addWidget(ready, 1);
    layout->addLayout(readyRow);

    layout->addWidget(textLabel(tr("The program checks grammar, style and punctuation in the "
                                   "language of the book, on this computer.")));

    // What the underlines look like: [grammar] and *spelling*
    layout->addWidget(underlineSample(
        tr("Anna [smiled smiled] and put the *leter* into her bag.",
           "A sample: [a grammar issue] and *a misspelled word*, marked so"),
        page));
    auto* legend = new QHBoxLayout();
    const bool darkPaper = palette().color(QPalette::Window).lightness() < 128;
    const qreal ratio = devicePixelRatioF();
    const auto addLegend = [legend, ratio](const QColor& color, const QString& text) {
        auto* wave = new QLabel();
        wave->setPixmap(wavePixmap(color, ratio));
        legend->addWidget(wave);
        legend->addWidget(new QLabel(text));
        legend->addSpacing(2 * STEP_SPACING);
    };
    addLegend(issueColor(editor::TextIssueKind::Grammar, darkPaper),
              tr("grammar, style and punctuation"));
    addLegend(issueColor(editor::TextIssueKind::Spelling, darkPaper), tr("spelling"));
    legend->addStretch(1);
    layout->addLayout(legend);

    layout->addWidget(textLabel(tr("Right-click an underline (or press the menu key) to see "
                                   "the suggestion. F7 goes to the next issue.")));
    layout->addWidget(noteLabel(tr("Settings of the grammar: Edit > Settings..., group "
                                   "Editor > Grammar.")));
    layout->addStretch(1);
    return page;
}

// =============================================================================
// What the window shows
// =============================================================================

void GrammarSetupDialog::showState() {
    switch (m_setup->state()) {
    case GrammarSetup::State::Running:
        showPage(Page::Progress);
        break;
    case GrammarSetup::State::Finished:
        showPage(Page::Done);
        break;
    case GrammarSetup::State::Failed:
        showPage(Page::Failed);
        break;
    case GrammarSetup::State::Idle:
    case GrammarSetup::State::Cancelled:
        showPage(Page::Plan);
        break;
    }
}

void GrammarSetupDialog::showPage(Page page) {
    m_page = page;
    const bool steps = page == Page::Progress || page == Page::Failed;
    m_pages->setCurrentIndex(page == Page::Plan ? 0 : steps ? 1 : 2);

    QString title;
    int number = 2;
    switch (page) {
    case Page::Plan:
        title = tr("What Will Be Downloaded");
        number = 1;
        setAcceptText(tr("&Download and Prepare"));
        cancelButton()->setText(tr("Cancel"));
        updatePlan();
        break;
    case Page::Progress:
        title = tr("Downloading and Preparing");
        setAcceptText(tr("Download in &Background"));
        break;
    case Page::Done:
        title = tr("Ready");
        number = 3;
        setAcceptText(tr("&Finish"));
        break;
    case Page::Failed:
        title = m_setup->step() == GrammarSetup::Step::Java ||
                        m_setup->step() == GrammarSetup::Step::LanguageTool
                    ? tr("Download Stopped")
                    : tr("Preparation Stopped");
        setAcceptText(tr("&Try Again"));
        cancelButton()->setText(tr("Close"));
        break;
    }
    setHeading(title, tr("Step %1 of %2").arg(number).arg(3));
    setWindowTitle(tr("Preparing Grammar Checking"));

    // The buttons of the page: the one that does its job, and the one that closes it
    cancelButton()->setVisible(page == Page::Plan || page == Page::Failed);
    m_stopButton->setVisible(page == Page::Progress);
    m_copyButton->setVisible(page == Page::Failed);
    acceptButton()->setDefault(true);

    m_failureIcon->setVisible(page == Page::Failed);
    m_failure->setVisible(page == Page::Failed);
    m_backgroundNote->setVisible(page == Page::Progress);
    m_cancelNote->setVisible(page == Page::Progress);
    m_hint->setVisible(page == Page::Failed);
    m_detailsToggle->setVisible(page == Page::Failed);
    m_details->setVisible(page == Page::Failed && m_detailsToggle->isChecked());
    if (steps) {
        updateSteps();
    }
}

void GrammarSetupDialog::updatePlan() {
    const bool javaThere = GrammarComponents::javaDownloaded();
    const bool languageToolThere = GrammarComponents::languageToolDownloaded() ||
                                   !GrammarComponents::languageToolArchive().isEmpty();
    const qint64 javaBytes = javaThere ? 0 : GrammarComponents::JAVA_DOWNLOAD_BYTES;
    const qint64 languageToolBytes =
        languageToolThere ? 0 : GrammarComponents::LANGUAGE_TOOL_DOWNLOAD_BYTES;
    m_javaSize->setText(javaThere ? tr("downloaded already", "Java")
                                  : GrammarComponents::sizeText(javaBytes));
    m_languageToolSize->setText(languageToolThere
                                    ? tr("downloaded already", "LanguageTool")
                                    : GrammarComponents::sizeText(languageToolBytes));
    const qint64 total = javaBytes + languageToolBytes;
    if (total == 0) {
        m_totalSize->setText(tr("nothing"));
    } else if (javaBytes > 0 && languageToolBytes > 0) {
        const qint64 rounded = (total + TOTAL_ROUNDING / 2) / TOTAL_ROUNDING * TOTAL_ROUNDING;
        m_totalSize->setText(tr("about %1").arg(GrammarComponents::sizeText(rounded)));
    } else {
        m_totalSize->setText(GrammarComponents::sizeText(total));
    }

    // Room for the components and the file of LanguageTool while it is unpacked
    const qint64 free = GrammarSetup::freeBytes();
    const bool room = free >= GrammarComponents::NEEDED_FREE_BYTES;
    const QString disk = GrammarSetup::disk();
    const QString needed = GrammarComponents::sizeText(GrammarComponents::INSTALLED_BYTES);
    const QString freeText = GrammarComponents::sizeText(free);
    m_room->setText(room ? tr("There is enough room on the disk %1: the components take about "
                              "%2 (free: %3).")
                               .arg(disk, needed, freeText)
                         : tr("There is not enough room on the disk %1: the components take "
                              "about %2, and %3 is free. Free some room and click Check Again.")
                               .arg(disk, needed, freeText));
    setIcon(m_roomIcon, room ? QStringLiteral("common.check") : QStringLiteral("common.warning"));
    m_checkAgainButton->setVisible(!room);

    const bool writable = GrammarSetup::canWrite();
    const QString folder = QDir::toNativeSeparators(GrammarComponents::folder());
    m_folder->setText(writable ? tr("No administrator rights are needed. Folder:") +
                                     QLatin1Char('\n') + folder
                               : tr("The program cannot write in the folder %1. Check the "
                                    "permissions of the folder, or install the components "
                                    "yourself and choose them in the advanced settings.")
                                     .arg(folder));
    setIcon(m_folderIcon,
            writable ? QStringLiteral("common.check") : QStringLiteral("common.warning"));

    acceptButton()->setEnabled(room && writable);
}

void GrammarSetupDialog::updateSteps() {
    if (m_page != Page::Progress && m_page != Page::Failed) {
        return;
    }
    const bool failed = m_page == Page::Failed;
    const auto current = static_cast<std::size_t>(m_setup->step());
    for (std::size_t index = 0; index < m_steps.size(); ++index) {
        StepRow& row = m_steps.at(index);
        const auto step = static_cast<GrammarSetup::Step>(index);
        const bool download = step == GrammarSetup::Step::Java ||
                              step == GrammarSetup::Step::LanguageTool;
        const bool done = m_setup->isDone(step);
        const bool active = !done && index == current && (m_setup->isRunning() || failed);
        const qint64 received = megabytes(m_setup->received(step));
        const qint64 total = megabytes(m_setup->total(step));

        QString value;
        QString icon;
        if (done) {
            value = tr("done");
            icon = QStringLiteral("common.check");
        } else if (active && failed) {
            value = download && received > 0
                        ? tr("stopped at %1 of %2 MB").arg(received).arg(total)
                        : tr("failed");
            icon = QStringLiteral("common.error");
        } else if (active) {
            value = download ? tr("%1 of %2 MB").arg(received).arg(total) : tr("in progress...");
            icon = QStringLiteral("common.arrowForward");
        } else {
            value = tr("waiting");
        }
        row.value->setText(value);
        setIcon(row.icon, icon);
        // The steps not reached yet are greyed out
        row.title->setEnabled(done || active);
        row.value->setEnabled(done || active);

        // A bar for the step being done: how much is downloaded, or that it goes on
        const bool bar = active && !failed;
        row.bar->setVisible(bar);
        if (bar && download) {
            const qint64 bytes = m_setup->total(step);
            row.bar->setRange(0, 100);
            row.bar->setValue(
                bytes > 0 ? static_cast<int>(std::min<qint64>(
                                100, m_setup->received(step) * 100 / bytes))
                          : 0);
        } else if (bar) {
            row.bar->setRange(0, 0);
        }
    }

    if (!failed) {
        return;
    }
    m_failure->setText(message());
    QString hint;
    switch (m_setup->failure()) {
    case GrammarSetup::Failure::NoInternet:
        hint = tr("Check the connection to the internet and click Try Again.");
        if (m_setup->step() == GrammarSetup::Step::LanguageTool &&
            m_setup->isDone(GrammarSetup::Step::Java)) {
            hint += QLatin1Char(' ') + tr("Java is downloaded already, so the program "
                                          "downloads only LanguageTool.");
        }
        break;
    case GrammarSetup::Failure::ServerDown:
    case GrammarSetup::Failure::Damaged:
    case GrammarSetup::Failure::NoRoom:
    case GrammarSetup::Failure::NoFolder:
    case GrammarSetup::Failure::NotUnpacked:
    case GrammarSetup::Failure::NotStarted:
    case GrammarSetup::Failure::None:
        break;
    }
    m_hint->setText(hint);
    m_hint->setVisible(!hint.isEmpty());
    m_details->setPlainText(details());
}

QString GrammarSetupDialog::message() const {
    const bool java = m_setup->step() == GrammarSetup::Step::Java;
    switch (m_setup->failure()) {
    case GrammarSetup::Failure::NoInternet:
        return java ? tr("Java could not be downloaded: no connection to the internet.")
                    : tr("LanguageTool could not be downloaded: no connection to the internet.");
    case GrammarSetup::Failure::ServerDown: {
        const QString server = m_setup->errorUrl().host();
        return java ? tr("Java could not be downloaded: the server %1 does not answer. Try "
                         "again later.")
                          .arg(server)
                    : tr("LanguageTool could not be downloaded: the server %1 does not "
                         "answer. Try again later.")
                          .arg(server);
    }
    case GrammarSetup::Failure::Damaged:
        return java ? tr("The downloaded file of Java is damaged. The program removed it - "
                         "click Try Again.")
                    : tr("The downloaded file of LanguageTool is damaged. The program "
                         "removed it - click Try Again.");
    case GrammarSetup::Failure::NoRoom:
        return tr("There is not enough room on the disk %1. Free about %2 and click Try "
                  "Again.")
            .arg(GrammarSetup::disk(),
                 GrammarComponents::sizeText(GrammarComponents::INSTALLED_BYTES));
    case GrammarSetup::Failure::NoFolder:
        return tr("The program cannot write in the folder %1. Check the permissions of the "
                  "folder, or install the components yourself and choose them in the "
                  "advanced settings.")
            .arg(QDir::toNativeSeparators(GrammarComponents::folder()));
    case GrammarSetup::Failure::NotUnpacked:
        return tr("The downloaded files could not be unpacked. Details show why.");
    case GrammarSetup::Failure::NotStarted:
        return tr("LanguageTool did not start. Details show the message of Java.");
    case GrammarSetup::Failure::None:
        break;
    }
    return QString();
}

QString GrammarSetupDialog::details() const {
    QStringList lines;
    if (!m_setup->errorText().isEmpty()) {
        lines.append(m_setup->errorText());
    }
    if (!m_setup->errorUrl().isEmpty()) {
        lines.append(m_setup->errorUrl().toString());
        const GrammarSetup::Step step = m_setup->step();
        if ((step == GrammarSetup::Step::Java || step == GrammarSetup::Step::LanguageTool) &&
            m_setup->received(step) > 0) {
            lines.append(tr("Downloaded %1 of %2 MB.")
                             .arg(megabytes(m_setup->received(step)))
                             .arg(megabytes(m_setup->total(step))));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

// =============================================================================
// The buttons
// =============================================================================

void GrammarSetupDialog::accept() {
    switch (m_page) {
    case Page::Plan:
    case Page::Failed:
        start();
        break;
    case Page::Progress:
    case Page::Done:
        // The preparation goes on in the background (the status bar shows it)
        KalahariDialog::accept();
        break;
    }
}

void GrammarSetupDialog::start() {
    if (m_page == Page::Plan) {
        updatePlan();
        if (!acceptButton()->isEnabled()) {
            return;  // no room or no folder meanwhile
        }
    }
    m_detailsToggle->setChecked(false);
    m_setup->start();
    showState();
}

void GrammarSetupDialog::onFinished() {
    if (m_setup->state() == GrammarSetup::State::Cancelled) {
        reject();
        return;
    }
    showState();
}

void GrammarSetupDialog::copyFailure() {
    QStringList parts = {windowTitle(), message()};
    if (!m_hint->text().isEmpty()) {
        parts.append(m_hint->text());
    }
    if (!details().isEmpty()) {
        parts.append(details());
    }
    QGuiApplication::clipboard()->setText(parts.join(QStringLiteral("\n\n")));
}

void GrammarSetupDialog::setIcon(QLabel* label, const QString& iconId) const {
    if (iconId.isEmpty()) {
        label->clear();
        return;
    }
    label->setPixmap(core::ArtProvider::getInstance().getIcon(iconId).pixmap(
        QSize(ICON_SIZE, ICON_SIZE)));
}

}  // namespace kalahari::gui::dialogs
