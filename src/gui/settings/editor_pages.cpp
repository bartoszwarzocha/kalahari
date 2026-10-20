/// @file editor_pages.cpp
/// @brief Settings pages: Editor > General, Colors, Cursor, Pages and Margins
///
/// The editors follow these settings by themselves (EditorPanel subscribes to them).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/gui/utils/chapter_cursor.h"
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
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

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
    // Text of other programs: their headings, quotes, emphasis and lines in the look of the
    // place it goes to, also their fonts, sizes and colors, or only the text
    auto* pasteMode = new QComboBox();
    pasteMode->addItem(tr("Merge Formatting"), QString::fromLatin1(editor::PASTE_MODE_MERGE));
    pasteMode->addItem(tr("Keep Source Formatting"),
                       QString::fromLatin1(editor::PASTE_MODE_SOURCE));
    pasteMode->addItem(tr("Keep Text Only"), QString::fromLatin1(editor::PASTE_MODE_TEXT));
    addField(behavior, tr("Paste from Other Programs:"), pasteMode, editor::PASTE_MODE_KEY);
    addNote(behavior, tr("Merge Formatting keeps the headings, quotes, emphasis and lines, and "
                         "gives the text the look of the place it is pasted to. Text copied in "
                         "Kalahari keeps all its formatting."));
    // A chapter opens where its cursor was when it was closed last, or at its start
    auto* opensAt = new QComboBox();
    opensAt->addItem(tr("Where You Last Edited"),
                     QString::fromLatin1(utils::CHAPTER_OPENS_WHERE_LEFT));
    opensAt->addItem(tr("At the Beginning"), QString::fromLatin1(utils::CHAPTER_OPENS_AT_START));
    addField(behavior, tr("Open a Chapter:"), opensAt, utils::CHAPTER_OPENS_AT_KEY);
    addNote(behavior, tr("The place of the cursor is kept in the book for every chapter, also "
                         "after the program is closed."));
    // The editor does not use these options yet
    auto* tabSize = new QSpinBox();
    tabSize->setRange(2, 8);
    tabSize->setSuffix(tr(" spaces"));
    markNotUsedYet(tabSize, addField(behavior, tr("Tab Size:"), tabSize, "editor.tabSize"));
    markNotUsedYet(addCheckBox(behavior, tr("Show Line Numbers"), "editor.lineNumbers"));
    markNotUsedYet(addCheckBox(behavior, tr("Enable Word Wrap"), "editor.wordWrap"));

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
    addNote(pageLayout(), tr("Configure the page format, the page margins and the text frame border."));

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
    addField(margins, tr("Top:"), marginField(tr("Top margin of the page (up to 100 mm)")),
             "editor.margins.pageTop");
    addField(margins, tr("Bottom:"), marginField(tr("Bottom margin of the page (up to 100 mm)")),
             "editor.margins.pageBottom");
    addField(margins, tr("Left:"), marginField(tr("Left margin of the page (up to 100 mm)")),
             "editor.margins.pageLeft");
    addField(margins, tr("Right:"), marginField(tr("Right margin of the page (up to 100 mm)")),
             "editor.margins.pageRight");
    // The pages do not use mirror margins yet: the inner and outer margins stay hidden
    markNotUsedYet(addCheckBox(margins, tr("Mirror margins (for book binding)"), "editor.margins.mirrorEnabled"));
    LengthSpinBox* inner = marginField(tr("Inner margin of the page (up to 100 mm)"));
    LengthSpinBox* outer = marginField(tr("Outer margin of the page (up to 100 mm)"));
    addField(margins, tr("Inner (binding):"), inner, "editor.margins.pageInner");
    addField(margins, tr("Outer (edge):"), outer, "editor.margins.pageOuter");
    margins->setRowVisible(inner, false);
    margins->setRowVisible(outer, false);

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

} // namespace gui
} // namespace kalahari
