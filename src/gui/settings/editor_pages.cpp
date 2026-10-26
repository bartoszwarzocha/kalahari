/// @file editor_pages.cpp
/// @brief Settings pages: Editor > General, Colors, Cursor, Pages and Margins
///
/// The editors follow these settings by themselves (EditorPanel subscribes to them).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/gui/utils/chapter_cursor.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/settings_schema.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace kalahari {
namespace gui {

namespace {

/// Size of the font of the scene break preview, over the dialog's font
constexpr qreal SCENE_BREAK_PREVIEW_SCALE = 1.3;

/// Width of the scene break preview, in letters M of its font
constexpr qreal SCENE_BREAK_PREVIEW_EMS = 6.0;

/// Length of the short line of the scene break preview, as a share of its width
constexpr qreal SCENE_BREAK_PREVIEW_LINE_SHARE = 0.5;

/// A scene break as the editor draws it, in the editor's font
class SceneBreakPreview : public QWidget {
public:
    explicit SceneBreakPreview(QWidget* parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFamily(font().family());
    }

    void setLook(editor::SceneBreakLook look) {
        m_look = look;
        update();
    }

    void setFamily(const QString& family) {
        m_font = font();
        m_font.setFamily(family);
        if (m_font.pointSizeF() > 0) {
            m_font.setPointSizeF(m_font.pointSizeF() * SCENE_BREAK_PREVIEW_SCALE);
        }
        updateGeometry();
        update();
    }

    QSize sizeHint() const override {
        const QFontMetricsF metrics(m_font);
        return QSize(static_cast<int>(std::ceil(metrics.horizontalAdvance(QLatin1Char('M')) *
                                                SCENE_BREAK_PREVIEW_EMS)),
                     static_cast<int>(std::ceil(metrics.height())));
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::WindowText));
        if (const QString text = editor::sceneBreakText(m_look); !text.isEmpty()) {
            painter.setFont(m_font);
            painter.drawText(rect(), Qt::AlignCenter, text);
        } else if (m_look == editor::SceneBreakLook::Line) {
            const qreal length = std::round(width() * SCENE_BREAK_PREVIEW_LINE_SHARE);
            const qreal left = std::round((width() - length) / 2.0);
            const qreal middle = std::round(height() / 2.0);
            painter.drawLine(QPointF(left, middle), QPointF(left + length, middle));
        }
    }

private:
    editor::SceneBreakLook m_look = editor::SceneBreakLook::Asterisks;
    QFont m_font;
};

/// Height of a page of the mirror margins preview, in lines of the dialog's font
constexpr qreal SPREAD_PREVIEW_PAGE_LINES = 7.0;

/// Lines of text on the full height of a page of the mirror margins preview
constexpr qreal SPREAD_PREVIEW_TEXT_LINES = 28.0;

/// Size of the page numbers of the mirror margins preview, over the dialog's font
constexpr qreal SPREAD_PREVIEW_NUMBER_SCALE = 0.8;

/// Pages 2 and 3 of a chapter side by side, as an open book: with mirror margins the inner
/// ones meet at the binding between them. To the scale of the page format, in the dialog's
/// colors
class SpreadPreview : public QWidget {
public:
    explicit SpreadPreview(QWidget* parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    /// The page format and its margins, in millimetres
    void setPage(const QSizeF& size, double top, double bottom, double inner, double outer) {
        if (size.width() > 0.0 && size.height() > 0.0) {
            m_size = size;
        }
        m_top = top;
        m_bottom = bottom;
        m_inner = inner;
        m_outer = outer;
        updateGeometry();
        update();
    }

    QSize sizeHint() const override {
        const QSizeF page = pageSize();
        return QSize(static_cast<int>(std::ceil(page.width() * 2.0)) + 1,
                     static_cast<int>(std::ceil(page.height())) + 1);
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        const QSizeF page = pageSize();
        const qreal scale = page.height() / m_size.height();
        const QColor text = palette().color(QPalette::Text);
        QColor lines = text;
        lines.setAlphaF(0.35F);
        QColor edge = text;
        edge.setAlphaF(0.45F);
        QFont numberFont = font();
        if (numberFont.pointSizeF() > 0) {
            numberFont.setPointSizeF(numberFont.pointSizeF() * SPREAD_PREVIEW_NUMBER_SCALE);
        }
        painter.setFont(numberFont);
        const qreal pitch = page.height() / SPREAD_PREVIEW_TEXT_LINES;

        for (int side = 0; side < 2; ++side) {
            // Page 2 is a left page: its inner margin is on the right, at the binding
            const QRectF sheet(0.5 + side * page.width(), 0.5, page.width(), page.height());
            const double left = side == 0 ? m_outer : m_inner;
            const double right = side == 0 ? m_inner : m_outer;
            painter.fillRect(sheet, palette().color(QPalette::Base));
            const QRectF frame(sheet.left() + left * scale, sheet.top() + m_top * scale,
                               sheet.width() - (left + right) * scale,
                               sheet.height() - (m_top + m_bottom) * scale);
            if (frame.width() > 0.0 && frame.height() > 0.0) {
                for (qreal y = frame.top(); y + pitch / 2.0 <= frame.bottom(); y += pitch) {
                    painter.fillRect(QRectF(frame.left(), y, frame.width(), pitch * 0.4), lines);
                }
            }
            painter.setPen(edge);
            painter.drawRect(sheet);
            painter.drawText(QRectF(sheet.left(), sheet.bottom() - m_bottom * scale,
                                    sheet.width(), m_bottom * scale),
                             Qt::AlignCenter | Qt::TextDontClip, QString::number(side + 2));
        }
        // The binding
        painter.setPen(text);
        painter.drawLine(QPointF(0.5 + page.width(), 0.5),
                         QPointF(0.5 + page.width(), 0.5 + page.height()));
    }

private:
    /// The size of a page on the screen, in pixels
    QSizeF pageSize() const {
        const qreal height = std::round(fontMetrics().height() * SPREAD_PREVIEW_PAGE_LINES);
        return QSizeF(std::round(height * m_size.width() / m_size.height()), height);
    }

    QSizeF m_size{210.0, 297.0};
    double m_top = 25.4;
    double m_bottom = 25.4;
    double m_inner = 30.0;
    double m_outer = 20.0;
};

/// The line the numbers of the margin numbers preview start at: line 12 of a chapter...
constexpr int NUMBERS_PREVIEW_LINE = 12;

/// ...in its paragraph 4
constexpr int NUMBERS_PREVIEW_PARAGRAPH = 4;

/// Width of the text of the margin numbers preview, in letters M of its font
constexpr qreal NUMBERS_PREVIEW_TEXT_EMS = 10.0;

/// Space between the numbers and the text of the margin numbers preview, in letters M of
/// the text's font
constexpr qreal NUMBERS_PREVIEW_GAP_EMS = 0.6;

/// The margin numbers on the light and on the dark paper, at two lines of one paragraph in
/// the editor's font. The numbers have their font, their color and their size next to the
/// text, as in the editor. The papers side by side; in a narrow window one under the other
class MarginNumbersPreview : public QWidget {
public:
    /// @param lines The two lines of the paragraph
    /// @param captions The names of the light and of the dark paper
    MarginNumbersPreview(const QStringList& lines, const QStringList& captions,
                         QWidget* parent = nullptr)
        : QWidget(parent), m_lines(lines), m_captions(captions) {
        QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);
    }

    /// The numbers as the fields show them
    void setNumbers(editor::MarginNumbering numbering, int interval, const QString& family,
                    qreal points, const QColor& color) {
        m_numbering = numbering;
        m_interval = std::max(1, interval);
        m_numbersFamily = family;
        m_numbersPoints = points;
        m_color = color;
        updateGeometry();
        update();
    }

    /// The editor's text and paper, as stored
    void setText(const QString& family, qreal points, const std::pair<QColor, QColor>& light,
                 const std::pair<QColor, QColor>& dark) {
        m_textFamily = family;
        m_textPoints = points;
        m_light = light;
        m_dark = dark;
        updateGeometry();
        update();
    }

    QSize sizeHint() const override {
        const Geometry geometry = measure();
        return QSize(sideBySideWidth(geometry), rowHeight(geometry));
    }

    QSize minimumSizeHint() const override {
        const Geometry geometry = measure();
        return QSize(static_cast<int>(std::ceil(geometry.box.width())) + 1, rowHeight(geometry));
    }

    bool hasHeightForWidth() const override { return true; }

    int heightForWidth(int width) const override {
        const Geometry geometry = measure();
        const int row = rowHeight(geometry);
        return width >= sideBySideWidth(geometry)
            ? row
            : row * 2 + static_cast<int>(std::round(geometry.spacing / 2.0));
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        const Geometry geometry = measure();
        const QFontMetricsF textMetrics(geometry.textFont);
        const QFontMetricsF numberMetrics(geometry.numbersFont);
        const bool sideBySide = width() >= sideBySideWidth(geometry);
        for (int paper = 0; paper < 2; ++paper) {
            const auto& [background, foreground] = paper == 0 ? m_light : m_dark;
            const QRectF box = sideBySide
                ? geometry.box.translated(0.5 + paper * (geometry.box.width() + geometry.spacing), 0.5)
                : geometry.box.translated(
                      0.5, 0.5 + paper * (rowHeight(geometry) + std::round(geometry.spacing / 2.0)));
            painter.save();
            // Greyed out with its fields: the papers keep their own colors
            if (!isEnabled()) {
                painter.setOpacity(0.5);
            }
            painter.fillRect(box, background);
            painter.setPen(palette().color(QPalette::Mid));
            painter.drawRect(box);
            painter.setClipRect(box);
            for (int line = 0; line < 2; ++line) {
                const qreal baseline =
                    box.top() + geometry.padding + line * textMetrics.lineSpacing() + textMetrics.ascent();
                const qreal textLeft = box.left() + geometry.textLeft;
                painter.setFont(geometry.textFont);
                painter.setPen(foreground);
                painter.drawText(QPointF(textLeft, baseline), m_lines.value(line));
                if (const QString number = numberAt(line); !number.isEmpty()) {
                    painter.setFont(geometry.numbersFont);
                    painter.setPen(m_color);
                    painter.drawText(QPointF(textLeft - geometry.gap -
                                                 numberMetrics.horizontalAdvance(number),
                                             baseline),
                                     number);
                }
            }
            painter.restore();
            painter.setFont(font());
            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(QRectF(box.left(), box.bottom(), box.width(), geometry.caption),
                             Qt::AlignLeft | Qt::AlignBottom, m_captions.value(paper));
        }
    }

private:
    /// The fonts and the places of the preview
    struct Geometry {
        QFont textFont;
        QFont numbersFont;
        qreal padding = 0.0;   ///< Around the text, inside the paper
        qreal gap = 0.0;       ///< Between the numbers and the text
        qreal textLeft = 0.0;  ///< From the left edge of the paper
        QRectF box;            ///< One paper
        qreal spacing = 0.0;   ///< Between the papers
        qreal caption = 0.0;   ///< Height of the name under a paper
    };

    Geometry measure() const {
        Geometry geometry;
        geometry.textFont = font();
        geometry.textFont.setFamily(m_textFamily);
        geometry.numbersFont = font();
        geometry.numbersFont.setFamily(m_numbersFamily);
        // As large next to the text as in the editor
        if (geometry.textFont.pointSizeF() > 0 && m_textPoints > 0) {
            geometry.numbersFont.setPointSizeF(
                std::max(1.0, geometry.textFont.pointSizeF() * m_numbersPoints / m_textPoints));
        }
        const QFontMetricsF textMetrics(geometry.textFont);
        const qreal em = textMetrics.horizontalAdvance(QLatin1Char('M'));
        geometry.padding = std::round(em * 0.4);
        geometry.gap = std::round(em * NUMBERS_PREVIEW_GAP_EMS);
        geometry.textLeft = geometry.padding +
            std::ceil(QFontMetricsF(geometry.numbersFont).horizontalAdvance(QStringLiteral("000"))) +
            geometry.gap;
        geometry.box = QRectF(0.0, 0.0,
                              std::ceil(geometry.textLeft + em * NUMBERS_PREVIEW_TEXT_EMS +
                                        geometry.padding),
                              std::ceil(geometry.padding * 2.0 + textMetrics.lineSpacing() * 2.0));
        geometry.spacing = std::round(em);
        geometry.caption = fontMetrics().height() + 2.0;
        return geometry;
    }

    /// The width of the papers side by side
    static int sideBySideWidth(const Geometry& geometry) {
        return static_cast<int>(std::ceil(geometry.box.width() * 2.0 + geometry.spacing)) + 1;
    }

    /// The height of a paper with its name
    static int rowHeight(const Geometry& geometry) {
        return static_cast<int>(std::ceil(geometry.box.height() + geometry.caption)) + 1;
    }

    /// The number at a line of the preview; none where the interval leaves it out
    QString numberAt(int line) const {
        const auto atInterval = [this](int from) {
            return (from + m_interval - 1) / m_interval * m_interval;
        };
        switch (m_numbering) {
            case editor::MarginNumbering::Lines: {
                const int number = atInterval(NUMBERS_PREVIEW_LINE) + line;
                return number % m_interval == 0 ? QString::number(number) : QString();
            }
            case editor::MarginNumbering::Paragraphs:
                // One paragraph: its number at its first line
                return line == 0 ? QString::number(atInterval(NUMBERS_PREVIEW_PARAGRAPH)) : QString();
            case editor::MarginNumbering::None:
                break;
        }
        return QString();
    }

    QStringList m_lines;
    QStringList m_captions;
    editor::MarginNumbering m_numbering = editor::MarginNumbering::None;
    int m_interval = 1;
    QString m_numbersFamily;
    qreal m_numbersPoints = 8.0;
    QColor m_color;
    QString m_textFamily;
    qreal m_textPoints = 14.0;
    std::pair<QColor, QColor> m_light;  ///< Background, text
    std::pair<QColor, QColor> m_dark;   ///< Background, text
};

}  // namespace

// ============================================================================
// Editor > General
// ============================================================================

EditorGeneralPage::EditorGeneralPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* font = addGroup(tr("Editor Font"));
    auto* family = new QFontComboBox();
    family->setFontFilters(QFontComboBox::AllFonts);
    addField(font, tr("Font Family:"), family, "editor.fontFamily");
    auto* size = new QSpinBox();
    size->setRange(8, 32);
    size->setSuffix(tr(" pt"));
    addField(font, tr("Font Size:"), size, "editor.fontSize");

    QFormLayout* behavior = addGroup(tr("Editor Behavior"));
    // What Edit > Paste does with text of other programs: their headings, quotes, emphasis and
    // lines in the look of the place it goes to, also their fonts, sizes and colors, or only
    // the text. The Edit menu has a paste for each of them too
    auto* pasteMode = new QComboBox();
    pasteMode->addItem(tr("Merge Formatting"), QString::fromLatin1(editor::PASTE_MODE_MERGE));
    pasteMode->addItem(tr("Keep Source Formatting"),
                       QString::fromLatin1(editor::PASTE_MODE_SOURCE));
    pasteMode->addItem(tr("Keep Text Only"), QString::fromLatin1(editor::PASTE_MODE_TEXT));
    addField(behavior, tr("Paste Command:"), pasteMode, editor::PASTE_MODE_KEY);
    QLabel* pasteNote = addNote(behavior, QString());
    // With the keys Paste has now (the user may change them)
    const auto showPasteKeys = [pasteNote]() {
        const Command* command = CommandRegistry::getInstance().getCommand("edit.paste");
        const QString keys = command != nullptr && !command->shortcut.isEmpty()
            ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
            : QString();
        const QString paste = keys.isEmpty()
            ? tr("Edit > Paste pastes text of other programs this way.")
            : tr("Edit > Paste (%1) pastes text of other programs this way.").arg(keys);
        pasteNote->setText(
            paste + QLatin1Char(' ') +
            tr("The Edit menu has a paste for each way too. Merge Formatting keeps the headings, "
               "quotes, emphasis and lines, and gives the text the look of the place it is "
               "pasted to. Text copied in Kalahari keeps all its formatting, except with Paste "
               "Text Only."));
    };
    showPasteKeys();
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, pasteNote,
            showPasteKeys);
    // A chapter opens where its cursor was when it was closed last, or at its start
    auto* opensAt = new QComboBox();
    opensAt->addItem(tr("Where You Last Edited"),
                     QString::fromLatin1(utils::CHAPTER_OPENS_WHERE_LEFT));
    opensAt->addItem(tr("At the Beginning"), QString::fromLatin1(utils::CHAPTER_OPENS_AT_START));
    addField(behavior, tr("Open a Chapter:"), opensAt, utils::CHAPTER_OPENS_AT_KEY);
    addNote(behavior, tr("The place of the cursor is kept in the book for every chapter, also "
                         "after the program is closed."));

    // A view setting: the chapter files are not changed
    QFormLayout* typography = addGroup(tr("Typography"));
    auto* lineHeight = new QDoubleSpinBox();
    lineHeight->setRange(1.0, 3.0);
    lineHeight->setSingleStep(0.1);
    lineHeight->setDecimals(1);
    lineHeight->setToolTip(tr("Multiple of the font's line height (1.0 = single spacing)"));
    addField(typography, tr("Line Spacing:"), lineHeight, "editor.lineHeight");
    auto* paragraphSpacing = new LengthSpinBox(LengthUnit::Pixels, 0, 48);
    addField(typography, tr("Space After Paragraph:"), paragraphSpacing, "editor.paragraphSpacing");
    auto* indent = new QCheckBox(tr("Indent First Line:"));
    auto* indentSize = new LengthSpinBox(LengthUnit::Pixels, 0, 96);
    typography->addRow(indent, indentSize);
    bind(indent, "editor.firstLineIndent");
    bind(indentSize, "editor.indentSize");
    const auto syncIndent = [indent, indentSize]() { indentSize->setEnabled(indent->isChecked()); };
    connect(indent, &QCheckBox::toggled, this, syncIndent);
    whenLoaded(syncIndent);

    // How far apart the tab stops are: every first-line indent of the paragraph's style, or
    // every few spaces of its font
    auto* tabStops = new QComboBox();
    tabStops->setObjectName(QStringLiteral("tabStops"));
    tabStops->addItem(tr("Paragraph Indent"), QString::fromLatin1(editor::TAB_STOPS_INDENT));
    tabStops->addItem(tr("Number of Spaces"), QString::fromLatin1(editor::TAB_STOPS_SPACES));
    auto* tabSize = new QSpinBox();
    tabSize->setObjectName(QStringLiteral("tabSize"));
    tabSize->setRange(1, 16);
    auto* tabRow = new QWidget();
    auto* tabLayout = new QHBoxLayout(tabRow);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(tabStops);
    tabLayout->addWidget(tabSize);
    tabLayout->addStretch();
    auto* tabLabel = new QLabel(tr("Tab Size:"));
    tabLabel->setBuddy(tabStops);
    typography->addRow(tabLabel, tabRow);
    bind(tabStops, "editor.tabStops");
    bind(tabSize, "editor.tabSize");
    addNote(typography,
            tr("Tab types a tab character, also at the start of a paragraph. With Paragraph "
               "Indent the tab stops are as far apart as the first-line indent of the "
               "paragraph's style; a style without one, as a heading, has them as far apart as "
               "the indent above."));
    const auto syncTabs = [tabStops, tabSize]() {
        tabSize->setEnabled(tabStops->currentData().toString() ==
                            QString::fromLatin1(editor::TAB_STOPS_SPACES));
        tabSize->setSuffix(tr(" space(s)", nullptr, tabSize->value()));
    };
    connect(tabStops, &QComboBox::currentIndexChanged, this, syncTabs);
    connect(tabSize, &QSpinBox::valueChanged, this, syncTabs);
    whenLoaded(syncTabs);

    // How the scene breaks look, shown next to the choice in the editor's font
    auto* sceneBreak = new QComboBox();
    const auto addLook = [sceneBreak](const QString& text, editor::SceneBreakLook look) {
        sceneBreak->addItem(text, editor::sceneBreakLookName(look));
    };
    addLook(tr("* * * (three asterisks)"), editor::SceneBreakLook::Asterisks);
    addLook(tr("%1 (asterism)").arg(QChar(0x2042)), editor::SceneBreakLook::Asterism);
    addLook(tr("%1 (fleuron)").arg(QChar(0x2766)), editor::SceneBreakLook::Fleuron);
    addLook(tr("Short line"), editor::SceneBreakLook::Line);
    addLook(tr("Blank line"), editor::SceneBreakLook::Blank);
    auto* preview = new SceneBreakPreview();
    preview->setObjectName(QStringLiteral("sceneBreakPreview"));
    auto* sceneBreakRow = new QWidget();
    auto* sceneBreakLayout = new QHBoxLayout(sceneBreakRow);
    sceneBreakLayout->setContentsMargins(0, 0, 0, 0);
    sceneBreakLayout->addWidget(sceneBreak);
    sceneBreakLayout->addWidget(preview);
    sceneBreakLayout->addStretch();
    auto* sceneBreakLabel = new QLabel(tr("Scene Break Look:"));
    sceneBreakLabel->setBuddy(sceneBreak);
    typography->addRow(sceneBreakLabel, sceneBreakRow);
    bind(sceneBreak, "editor.sceneBreak.style");
    connect(sceneBreak, &QComboBox::currentIndexChanged, preview, [sceneBreak, preview]() {
        preview->setLook(editor::sceneBreakLookFromName(sceneBreak->currentData().toString()));
    });
    connect(family, &QFontComboBox::currentFontChanged, preview,
            [preview](const QFont& font) { preview->setFamily(font.family()); });
    whenLoaded([sceneBreak, family, preview]() {
        preview->setLook(editor::sceneBreakLookFromName(sceneBreak->currentData().toString()));
        preview->setFamily(family->currentFont().family());
    });
    addNote(typography, tr("Choose from: * * *, %1, %2, a short line, a blank line. Changes only "
                           "the look: the file keeps the scene break.")
                            .arg(QString(QChar(0x2042)), QString(QChar(0x2766))));

    // Turned on and off with View > Typewriter Scrolling
    QFormLayout* typewriter = addGroup(tr("Typewriter Scrolling"));
    QLabel* typewriterNote = addNote(typewriter, QString());
    // With the keys the command has now (the user may change them)
    const auto showTypewriterKeys = [typewriterNote]() {
        const Command* command = CommandRegistry::getInstance().getCommand("view.typewriter");
        const QString keys = command != nullptr && !command->shortcut.isEmpty()
            ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
            : QString();
        typewriterNote->setText(
            keys.isEmpty()
                ? tr("View > Typewriter Scrolling keeps the line you write at one height of the "
                     "view, in the Continuous and the Page Layout view.")
                : tr("View > Typewriter Scrolling (%1) keeps the line you write at one height of "
                     "the view, in the Continuous and the Page Layout view.")
                      .arg(keys));
    };
    showTypewriterKeys();
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, typewriterNote,
            showTypewriterKeys);
    auto* focus = new QSpinBox();
    focus->setRange(10, 90);
    focus->setSingleStep(5);
    focus->setSuffix(tr(" % from the top"));
    focus->setToolTip(tr("Where the line with the cursor stays (50% = middle)"));
    // Stored as a fraction of the view's height
    addField(typewriter, tr("Cursor Line Height:"), focus, "editor.typewriter.focusPosition", 100.0);
    addCheckBox(typewriter, tr("Smooth scrolling"), "editor.typewriter.smoothScroll")
        ->setToolTip(tr("Glide to the next line instead of jumping"));

    // The counts of the whole program: the status bar, the panels, Distraction-Free writing
    QFormLayout* wordCount = addGroup(tr("Word Count"));
    addCheckBox(wordCount, tr("Count standalone dashes as words"),
                "editor.wordCount.dashesAsWords");
    addNote(wordCount, tr("A dash separated by spaces from the words around it, as at the start "
                          "of a line of dialogue. Microsoft Word counts it as a word, LibreOffice "
                          "does not."));

    pageLayout()->addStretch();
}

// ============================================================================
// Editor > Colors
// ============================================================================

EditorColorsPage::EditorColorsPage(QWidget* parent)
    : SettingsPage(parent)
{
    addNote(pageLayout(), tr("Configure editor colors for light and dark mode.\n"
                             "Editor color mode is independent from the application theme."));

    auto* darkMode = new QCheckBox(tr("Use dark mode for editor"));
    darkMode->setToolTip(tr("Toggle between light and dark editor colors"));
    pageLayout()->addWidget(darkMode);
    bind(darkMode, "editor.darkMode");

    const auto color = [this](QFormLayout* form, const QString& label, const char* key) {
        auto* widget = new ColorConfigWidget(label);
        form->addRow(widget);
        bind(widget, key);
    };
    QFormLayout* light = addGroup(tr("Light Mode Colors"));
    color(light, tr("Background"), "editor.colors.backgroundLight");
    color(light, tr("Text"), "editor.colors.textLight");
    color(light, tr("Inactive (Focus mode)"), "editor.colors.inactiveLight");
    color(light, tr("Formatting marks"), "editor.colors.formattingMarksLight");
    QFormLayout* dark = addGroup(tr("Dark Mode Colors"));
    color(dark, tr("Background"), "editor.colors.backgroundDark");
    color(dark, tr("Text"), "editor.colors.textDark");
    color(dark, tr("Inactive (Focus mode)"), "editor.colors.inactiveDark");
    color(dark, tr("Formatting marks"), "editor.colors.formattingMarksDark");

    pageLayout()->addStretch();
}

// ============================================================================
// Editor > Cursor
// ============================================================================

EditorCursorPage::EditorCursorPage(QWidget* parent)
    : SettingsPage(parent)
{
    addNote(pageLayout(), tr("Configure the appearance of the text cursor in the editor."));

    QFormLayout* style = addGroup(tr("Cursor Style"));
    auto* shape = new QComboBox();
    shape->addItem(tr("Line (|)"), static_cast<int>(editor::CursorStyle::Line));
    shape->addItem(tr("Block"), static_cast<int>(editor::CursorStyle::Block));
    shape->addItem(tr("Underline (_)"), static_cast<int>(editor::CursorStyle::Underline));
    shape->setToolTip(tr("Select the cursor shape:\n"
                         "- Line: vertical bar (|)\n"
                         "- Block: rectangle on character\n"
                         "- Underline: line under character (_)"));
    addField(style, tr("Style:"), shape, "editor.cursor.style");
    auto* lineWidth = new LengthSpinBox(LengthUnit::Pixels, 1, 5);
    lineWidth->setToolTip(tr("Width of the line cursor (1-5 pixels)"));
    addField(style, tr("Cursor width:"), lineWidth, "editor.cursor.lineWidth");
    // The width is used by the line cursor only
    const auto showWidth = [style, shape, lineWidth]() {
        style->setRowVisible(lineWidth,
                             shape->currentData().toInt() == static_cast<int>(editor::CursorStyle::Line));
    };
    connect(shape, &QComboBox::currentIndexChanged, this, showWidth);
    whenLoaded(showWidth);

    QFormLayout* color = addGroup(tr("Cursor Color"));
    QCheckBox* custom = addCheckBox(color, tr("Use custom color"), "editor.cursor.useCustomColor");
    custom->setToolTip(tr("When unchecked, cursor uses the text color.\n"
                          "When checked, cursor uses a custom color."));
    auto* customColor = new ColorConfigWidget(tr("Custom Color"));
    customColor->setToolTip(tr("Custom cursor color (only used when 'Use custom color' is checked)"));
    color->addRow(customColor);
    bind(customColor, "editor.cursor.customColor");
    const auto syncColor = [custom, customColor]() { customColor->setEnabled(custom->isChecked()); };
    connect(custom, &QCheckBox::toggled, this, syncColor);
    whenLoaded(syncColor);

    QFormLayout* blink = addGroup(tr("Blinking"));
    QCheckBox* blinking = addCheckBox(blink, tr("Enable cursor blinking"), "editor.cursor.blinking");
    blinking->setToolTip(tr("Enable or disable cursor blinking animation"));
    auto* interval = new QSpinBox();
    interval->setRange(100, 2000);
    interval->setSingleStep(50);
    interval->setToolTip(tr("Blink interval (100-2000 ms)"));
    QLabel* intervalLabel = addField(blink, tr("Blink interval (ms):"), interval, "editor.cursor.blinkInterval");
    const auto syncInterval = [blinking, interval, intervalLabel]() {
        interval->setEnabled(blinking->isChecked());
        intervalLabel->setEnabled(blinking->isChecked());
    };
    connect(blinking, &QCheckBox::toggled, this, syncInterval);
    whenLoaded(syncInterval);

    pageLayout()->addStretch();
}

// ============================================================================
// Editor > Pages and Margins
// ============================================================================

EditorPagesPage::EditorPagesPage(QWidget* parent)
    : SettingsPage(parent)
{
    addNote(pageLayout(), tr("Configure the page format, the page margins, the margin numbering "
                             "and the text frame border."));

    // Every view: the continuous views show one endless page
    QFormLayout* page = addGroup(tr("Page"));
    auto* format = new QComboBox();
    format->addItem(tr("A4 (210 x 297 mm)"), QStringLiteral("A4"));
    format->addItem(tr("A5 (148 x 210 mm)"), QStringLiteral("A5"));
    format->addItem(tr("B5 (176 x 250 mm)"), QStringLiteral("B5"));
    format->addItem(tr("6 x 9 in (152 x 229 mm)"), QStringLiteral("6x9"));
    format->addItem(tr("Letter (8.5 x 11 in)"), QStringLiteral("Letter"));
    format->addItem(tr("Legal (8.5 x 14 in)"), QStringLiteral("Legal"));
    format->addItem(tr("Custom"), QStringLiteral("Custom"));
    addField(page, tr("Format:"), format, "editor.page.size");
    const auto sizeField = []() {
        return new LengthSpinBox(LengthUnit::Millimeters, 50.0, 500.0);
    };
    LengthSpinBox* width = sizeField();
    LengthSpinBox* height = sizeField();
    addField(page, tr("Width:"), width, "editor.page.customWidth");
    addField(page, tr("Height:"), height, "editor.page.customHeight");
    // The width and height are set for a custom format only
    const auto syncSize = [format, width, height]() {
        const bool custom = format->currentData().toString() == QStringLiteral("Custom");
        width->setEnabled(custom);
        height->setEnabled(custom);
    };
    connect(format, &QComboBox::currentIndexChanged, this, syncSize);
    whenLoaded(syncSize);
    auto* gap = new LengthSpinBox(LengthUnit::Pixels, 0, 100);
    gap->setToolTip(tr("Space between the pages and around them, at 100% zoom"));
    addField(page, tr("Gap between pages:"), gap, "editor.page.gap");
    addCheckBox(page, tr("Show page numbers"), "editor.page.showNumbers");

    const auto marginField = [](const QString& toolTip) {
        auto* field = new LengthSpinBox(LengthUnit::Millimeters, 0.0, 100.0);
        field->setToolTip(toolTip);
        return field;
    };
    QFormLayout* margins = addGroup(tr("Page Margins"));
    LengthSpinBox* top = marginField(tr("Top margin of the page (up to 100 mm)"));
    LengthSpinBox* bottom = marginField(tr("Bottom margin of the page (up to 100 mm)"));
    LengthSpinBox* left = marginField(tr("Left margin of the page (up to 100 mm)"));
    LengthSpinBox* right = marginField(tr("Right margin of the page (up to 100 mm)"));
    left->setObjectName(QStringLiteral("pageLeft"));
    right->setObjectName(QStringLiteral("pageRight"));
    addField(margins, tr("Top:"), top, "editor.margins.pageTop");
    addField(margins, tr("Bottom:"), bottom, "editor.margins.pageBottom");
    QLabel* leftLabel = addField(margins, tr("Left:"), left, "editor.margins.pageLeft");
    QLabel* rightLabel = addField(margins, tr("Right:"), right, "editor.margins.pageRight");
    // Mirror margins: the inner one at the binding, the outer one at the edge, instead of the
    // left and the right one
    QCheckBox* mirror = addCheckBox(margins, tr("Mirror margins (for book binding)"),
                                    "editor.margins.mirrorEnabled");
    mirror->setObjectName(QStringLiteral("mirrorMargins"));
    LengthSpinBox* inner = marginField(tr("Inner margin of the page (up to 100 mm)"));
    LengthSpinBox* outer = marginField(tr("Outer margin of the page (up to 100 mm)"));
    inner->setObjectName(QStringLiteral("pageInner"));
    outer->setObjectName(QStringLiteral("pageOuter"));
    addField(margins, tr("Inner (binding):"), inner, "editor.margins.pageInner");
    addField(margins, tr("Outer (edge):"), outer, "editor.margins.pageOuter");
    auto* spread = new SpreadPreview();
    spread->setObjectName(QStringLiteral("mirrorMarginsPreview"));
    spread->setAccessibleName(tr("Preview of pages 2 and 3"));
    auto* spreadRow = new QWidget();
    auto* spreadLayout = new QHBoxLayout(spreadRow);
    spreadLayout->setContentsMargins(0, 0, 0, 0);
    spreadLayout->addWidget(spread, 0, Qt::AlignTop);
    addNote(spreadLayout, tr("Pages 2 and 3 of a chapter. Page 1 is a right page, so its inner "
                             "margin is on the left. A chapter can start on a left page: "
                             "\"Starts on a left page\" in the Properties panel."));
    margins->addRow(new QLabel(tr("Preview:")), spreadRow);
    const auto syncMirror = [margins, mirror, left, leftLabel, right, rightLabel, inner, outer,
                             spreadRow]() {
        const bool mirrored = mirror->isChecked();
        for (QWidget* field : {static_cast<QWidget*>(left), static_cast<QWidget*>(leftLabel),
                               static_cast<QWidget*>(right), static_cast<QWidget*>(rightLabel)}) {
            field->setEnabled(!mirrored);
        }
        margins->setRowVisible(inner, mirrored);
        margins->setRowVisible(outer, mirrored);
        margins->setRowVisible(spreadRow, mirrored);
    };
    connect(mirror, &QCheckBox::toggled, this, syncMirror);
    whenLoaded(syncMirror);
    // The preview follows the fields: the format, its size, the margins
    const auto syncSpread = [format, width, height, top, bottom, inner, outer, spread]() {
        editor::PageLayout layout;
        layout.pageSize = editor::PageLayout::pageSizeFromId(format->currentData().toString());
        layout.customWidth = width->storedValue();
        layout.customHeight = height->storedValue();
        spread->setPage(layout.pageSizeMm(), top->storedValue(), bottom->storedValue(),
                        inner->storedValue(), outer->storedValue());
    };
    connect(format, &QComboBox::currentIndexChanged, this, syncSpread);
    for (LengthSpinBox* field : {width, height, top, bottom, inner, outer}) {
        connect(field, &QDoubleSpinBox::valueChanged, this, syncSpread);
    }
    whenLoaded(syncSpread);

    addMarginNumbersGroup();

    QFormLayout* frame = addGroup(tr("Text Frame Border"));
    QCheckBox* showFrame = addCheckBox(frame, tr("Show text frame border"), "editor.textFrameBorder.show");
    showFrame->setToolTip(tr("Display a visible border around the text content area.\n"
                             "Useful for visualizing margin boundaries."));
    auto* frameColor = new ColorConfigWidget(tr("Border color"));
    frameColor->setToolTip(tr("Color of the text frame border"));
    frame->addRow(frameColor);
    bind(frameColor, "editor.textFrameBorder.color");
    auto* frameWidth = new LengthSpinBox(LengthUnit::Pixels, 1, 5);
    frameWidth->setToolTip(tr("Border width (1-5 pixels)"));
    QLabel* frameWidthLabel = addField(frame, tr("Border width:"), frameWidth, "editor.textFrameBorder.width");
    const auto syncFrame = [showFrame, frameColor, frameWidth, frameWidthLabel]() {
        const bool shown = showFrame->isChecked();
        frameColor->setEnabled(shown);
        frameWidth->setEnabled(shown);
        frameWidthLabel->setEnabled(shown);
    };
    connect(showFrame, &QCheckBox::toggled, this, syncFrame);
    whenLoaded(syncFrame);

    pageLayout()->addStretch();
}

void EditorPagesPage::addMarginNumbersGroup() {
    // Only on the screen. View > Margin Numbering and the Views toolbar change the same settings
    QFormLayout* numbers = addGroup(tr("Margin Numbering"));
    numbers->parentWidget()->setObjectName(QStringLiteral("marginNumbersGroup"));
    const std::vector<std::pair<QRadioButton*, const char*>> choices = {
        {new QRadioButton(tr("None")), editor::MARGIN_NUMBERING_NONE},
        {new QRadioButton(tr("Lines")), editor::MARGIN_NUMBERING_LINES},
        {new QRadioButton(tr("Paragraphs")), editor::MARGIN_NUMBERING_PARAGRAPHS}};
    auto* choicesRow = new QWidget();
    auto* choicesLayout = new QHBoxLayout(choicesRow);
    choicesLayout->setContentsMargins(0, 0, 0, 0);
    for (const auto& [button, name] : choices) {
        button->setObjectName(QStringLiteral("marginNumbers_%1").arg(QLatin1String(name)));
        choicesLayout->addWidget(button);
    }
    choicesLayout->addStretch();
    auto* choicesLabel = new QLabel(tr("Number:"));
    choicesLabel->setBuddy(choices.front().first);
    numbers->addRow(choicesLabel, choicesRow);

    auto& settings = core::SettingsManager::getInstance();
    const auto chosen = [choices]() {
        for (const auto& [button, name] : choices) {
            if (button->isChecked()) {
                return editor::marginNumberingFromName(QLatin1String(name));
            }
        }
        return editor::MarginNumbering::None;
    };
    Binding mode;
    mode.key = []() { return std::string("editor.marginNumbers.mode"); };
    mode.stored = [&settings]() {
        return nlohmann::json(settings.get<std::string>("editor.marginNumbers.mode"));
    };
    mode.shown = [choices]() {
        for (const auto& [button, name] : choices) {
            if (button->isChecked()) {
                return nlohmann::json(name);
            }
        }
        return nlohmann::json(editor::MARGIN_NUMBERING_NONE);
    };
    mode.show = [choices](const nlohmann::json& value) {
        // A name the program does not know shows as None, as the editor takes it
        const editor::MarginNumbering numbering = editor::marginNumberingFromName(
            QString::fromStdString(value.is_string() ? value.get<std::string>() : std::string()));
        for (const auto& [button, name] : choices) {
            button->setChecked(editor::marginNumberingFromName(QLatin1String(name)) == numbering);
        }
    };
    mode.store = [&settings](const nlohmann::json& value) {
        settings.set<std::string>("editor.marginNumbers.mode", value.get<std::string>());
    };
    bindCustom(std::move(mode));

    // The fields of the look, which the button Restore Defaults shows again
    std::vector<Binding*> look;
    auto* interval = new QSpinBox();
    interval->setObjectName(QStringLiteral("marginNumbersInterval"));
    interval->setRange(1, 100);
    QLabel* intervalLabel =
        addField(numbers, tr("Interval:"), interval, "editor.marginNumbers.interval");
    look.push_back(&lastBinding());
    QLabel* intervalNote = addNote(numbers, QString());

    QCheckBox* restart = addCheckBox(numbers, tr("Restart on every page"),
                                     "editor.marginNumbers.restartEachPage");
    restart->setObjectName(QStringLiteral("marginNumbersRestart"));
    look.push_back(&lastBinding());
    auto* restartNoteRow = new QVBoxLayout();
    restartNoteRow->setContentsMargins(0, 0, 0, 0);
    alignWithText(addNote(restartNoteRow, tr("Line numbers in the Page Layout view only. In the "
                                             "Continuous view the numbers run through the whole "
                                             "chapter.")),
                  restart);
    numbers->addRow(restartNoteRow);

    auto* font = new QFontComboBox();
    font->setObjectName(QStringLiteral("marginNumbersFont"));
    font->setFontFilters(QFontComboBox::AllFonts);
    QLabel* fontLabel = addField(numbers, tr("Font:"), font, "editor.marginNumbers.font");
    look.push_back(&lastBinding());
    auto* size = new QSpinBox();
    size->setObjectName(QStringLiteral("marginNumbersSize"));
    size->setRange(6, 24);
    size->setSuffix(tr(" pt"));
    QLabel* sizeLabel = addField(numbers, tr("Size:"), size, "editor.marginNumbers.size");
    look.push_back(&lastBinding());
    addNote(numbers, tr("At 100% zoom; the numbers grow and shrink with the text."));

    auto* color = new ColorConfigWidget(tr("Number color"));
    color->setObjectName(QStringLiteral("marginNumbersColor"));
    numbers->addRow(color);
    bind(color, "editor.marginNumbers.color");
    look.push_back(&lastBinding());
    addNote(numbers, tr("The same on the light and on the dark paper."));

    auto* preview = new MarginNumbersPreview(
        {tr("Fog settled on the river"), tr("long before dawn")},
        {tr("Light paper"), tr("Dark paper")});
    preview->setObjectName(QStringLiteral("marginNumbersPreview"));
    preview->setAccessibleName(tr("Preview of the margin numbers"));
    auto* previewLabel = new QLabel(tr("Preview:"));
    numbers->addRow(previewLabel, preview);

    auto* restore = new QPushButton(tr("Restore Defaults"));
    restore->setObjectName(QStringLiteral("marginNumbersDefaults"));
    restore->setToolTip(tr("The default interval, font, size and color; the numbering stays "
                           "as chosen. Apply and OK store them."));
    auto* restoreRow = new QHBoxLayout();
    restoreRow->addWidget(restore);
    restoreRow->addStretch();
    numbers->addRow(restoreRow);
    addNote(numbers, tr("The numbering buttons on the Views toolbar and the commands of "
                        "View > Margin Numbering change the same settings as the fields above."));

    // The preview: the numbers as the fields show them, the text and the paper as stored
    const auto showPreview = [chosen, interval, font, size, color, preview]() {
        preview->setNumbers(chosen(), interval->value(), font->currentFont().family(),
                            size->value(), color->color());
    };
    whenLoaded([&settings, preview]() {
        const auto colorOf = [&settings](const char* key) {
            return QColor(QString::fromStdString(settings.get<std::string>(key)));
        };
        preview->setText(QString::fromStdString(settings.get<std::string>("editor.fontFamily")),
                         settings.get<int>("editor.fontSize"),
                         {colorOf("editor.colors.backgroundLight"), colorOf("editor.colors.textLight")},
                         {colorOf("editor.colors.backgroundDark"), colorOf("editor.colors.textDark")});
    });

    // With None the fields are greyed out; Restart on every page counts lines only
    const auto syncNumbers = [chosen, interval, intervalLabel, intervalNote, restart, font,
                              fontLabel, size, sizeLabel, color, preview, previewLabel, restore,
                              showPreview]() {
        const editor::MarginNumbering numbering = chosen();
        const bool on = numbering != editor::MarginNumbering::None;
        for (QWidget* field : std::initializer_list<QWidget*>{
                 interval, intervalLabel, font, fontLabel, size, sizeLabel, color, preview,
                 previewLabel, restore}) {
            field->setEnabled(on);
        }
        restart->setEnabled(numbering == editor::MarginNumbering::Lines);
        intervalNote->setText(numbering == editor::MarginNumbering::Paragraphs
                                  ? tr("1: a number at every paragraph, 5: at every fifth one. "
                                       "Headings, scene breaks and empty paragraphs are not "
                                       "counted.")
                                  : tr("1: a number at every line, 5: at every fifth one."));
        showPreview();
    };
    for (const auto& [button, name] : choices) {
        connect(button, &QRadioButton::toggled, this, syncNumbers);
    }
    whenLoaded(syncNumbers);
    connect(interval, &QSpinBox::valueChanged, this, showPreview);
    connect(font, &QFontComboBox::currentFontChanged, this, showPreview);
    connect(size, &QSpinBox::valueChanged, this, showPreview);
    connect(color, &ColorConfigWidget::colorChanged, this, showPreview);
    connect(restore, &QPushButton::clicked, this, [look, showPreview]() {
        for (Binding* binding : look) {
            if (const nlohmann::json* value = core::settings_schema::defaultValue(binding->key())) {
                binding->show(*value);
            }
        }
        showPreview();
    });
}

} // namespace gui
} // namespace kalahari
