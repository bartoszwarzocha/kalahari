/// @file editor_appearance.cpp
/// @brief Visual appearance configuration implementation (OpenSpec #00042 Phase 5)

#include <kalahari/editor/editor_appearance.h>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSizeF>

namespace kalahari::editor {

// =============================================================================
// EditorColors
// =============================================================================

EditorColors EditorColors::lightTheme()
{
    EditorColors colors;
    // Light theme is the default - just return default-constructed
    return colors;
}

EditorColors EditorColors::darkTheme()
{
    EditorColors colors;

    // Background colors
    colors.editorBackground = QColor(30, 30, 35);
    colors.pageBackground = QColor(45, 45, 50);
    colors.pageShadow = QColor(0, 0, 0, 100);
    colors.marginArea = QColor(40, 40, 45);

    // Text colors
    colors.text = QColor(220, 220, 220);
    colors.textSecondary = QColor(150, 150, 150);
    colors.textDimmed = QColor(100, 100, 100);

    // Selection & cursor
    colors.selection = QColor(66, 133, 244, 100);
    colors.selectionBorder = QColor(100, 150, 255);
    colors.cursor = QColor(255, 255, 255);
    colors.cursorLine = QColor(255, 255, 255, 20);

    // UI elements
    colors.ruler = QColor(60, 60, 65);
    colors.rulerMarker = QColor(100, 100, 105);
    colors.scrollbar = QColor(80, 80, 85);
    colors.scrollbarHover = QColor(100, 100, 105);

    // Accents
    colors.accent = QColor(100, 150, 255);
    colors.accentSecondary = QColor(80, 200, 120);
    colors.warning = QColor(255, 200, 50);
    colors.error = QColor(255, 100, 100);

    // Focus mode
    colors.focusHighlight = QColor(50, 50, 55);
    colors.focusDimOverlay = QColor(30, 30, 35, 200);

    return colors;
}

EditorColors EditorColors::sepiaTheme()
{
    EditorColors colors;

    // Warm, paper-like colors
    colors.editorBackground = QColor(60, 55, 50);
    colors.pageBackground = QColor(253, 246, 230);  // Warm cream
    colors.pageShadow = QColor(60, 50, 40, 80);
    colors.marginArea = QColor(245, 238, 220);

    // Text colors - warm browns
    colors.text = QColor(60, 50, 40);
    colors.textSecondary = QColor(120, 100, 80);
    colors.textDimmed = QColor(180, 160, 140);

    // Selection & cursor
    colors.selection = QColor(200, 170, 100, 80);
    colors.selectionBorder = QColor(180, 150, 80);
    colors.cursor = QColor(60, 50, 40);
    colors.cursorLine = QColor(60, 50, 40, 15);

    // UI elements
    colors.ruler = QColor(220, 210, 190);
    colors.rulerMarker = QColor(180, 160, 140);
    colors.scrollbar = QColor(200, 180, 160);
    colors.scrollbarHover = QColor(160, 140, 120);

    // Accents
    colors.accent = QColor(160, 120, 60);
    colors.accentSecondary = QColor(100, 140, 80);
    colors.warning = QColor(200, 160, 40);
    colors.error = QColor(180, 80, 60);

    // Focus mode
    colors.focusHighlight = QColor(255, 250, 235);
    colors.focusDimOverlay = QColor(253, 246, 230, 200);

    return colors;
}

EditorColors EditorColors::fromJson(const QJsonObject& json)
{
    EditorColors colors;

    auto readColor = [&json](const QString& key, QColor& target) {
        if (json.contains(key)) {
            target = QColor(json[key].toString());
        }
    };

    readColor("editorBackground", colors.editorBackground);
    readColor("pageBackground", colors.pageBackground);
    readColor("pageShadow", colors.pageShadow);
    readColor("marginArea", colors.marginArea);
    readColor("text", colors.text);
    readColor("textSecondary", colors.textSecondary);
    readColor("textDimmed", colors.textDimmed);
    readColor("selection", colors.selection);
    readColor("selectionBorder", colors.selectionBorder);
    readColor("cursor", colors.cursor);
    readColor("cursorLine", colors.cursorLine);
    readColor("ruler", colors.ruler);
    readColor("rulerMarker", colors.rulerMarker);
    readColor("scrollbar", colors.scrollbar);
    readColor("scrollbarHover", colors.scrollbarHover);
    readColor("accent", colors.accent);
    readColor("accentSecondary", colors.accentSecondary);
    readColor("warning", colors.warning);
    readColor("error", colors.error);
    readColor("focusHighlight", colors.focusHighlight);
    readColor("focusDimOverlay", colors.focusDimOverlay);

    // Dual-mode colors (Continuous View)
    if (json.contains("continuous")) {
        QJsonObject cont = json["continuous"].toObject();
        if (cont.contains("backgroundLight"))
            colors.continuous.backgroundLight = QColor(cont["backgroundLight"].toString());
        if (cont.contains("textLight"))
            colors.continuous.textLight = QColor(cont["textLight"].toString());
        if (cont.contains("backgroundDark"))
            colors.continuous.backgroundDark = QColor(cont["backgroundDark"].toString());
        if (cont.contains("textDark"))
            colors.continuous.textDark = QColor(cont["textDark"].toString());
    }

    // Dual-mode colors (Focus View)
    if (json.contains("focus")) {
        QJsonObject foc = json["focus"].toObject();
        if (foc.contains("inactiveLight"))
            colors.focus.inactiveLight = QColor(foc["inactiveLight"].toString());
        if (foc.contains("inactiveDark"))
            colors.focus.inactiveDark = QColor(foc["inactiveDark"].toString());
    }

    return colors;
}

QJsonObject EditorColors::toJson() const
{
    QJsonObject json;

    auto writeColor = [&json](const QString& key, const QColor& color) {
        json[key] = color.name(QColor::HexArgb);
    };

    writeColor("editorBackground", editorBackground);
    writeColor("pageBackground", pageBackground);
    writeColor("pageShadow", pageShadow);
    writeColor("marginArea", marginArea);
    writeColor("text", text);
    writeColor("textSecondary", textSecondary);
    writeColor("textDimmed", textDimmed);
    writeColor("selection", selection);
    writeColor("selectionBorder", selectionBorder);
    writeColor("cursor", cursor);
    writeColor("cursorLine", cursorLine);
    writeColor("ruler", ruler);
    writeColor("rulerMarker", rulerMarker);
    writeColor("scrollbar", scrollbar);
    writeColor("scrollbarHover", scrollbarHover);
    writeColor("accent", accent);
    writeColor("accentSecondary", accentSecondary);
    writeColor("warning", warning);
    writeColor("error", error);
    writeColor("focusHighlight", focusHighlight);
    writeColor("focusDimOverlay", focusDimOverlay);

    // Dual-mode colors (Continuous View)
    QJsonObject cont;
    cont["backgroundLight"] = continuous.backgroundLight.name(QColor::HexArgb);
    cont["textLight"] = continuous.textLight.name(QColor::HexArgb);
    cont["backgroundDark"] = continuous.backgroundDark.name(QColor::HexArgb);
    cont["textDark"] = continuous.textDark.name(QColor::HexArgb);
    json["continuous"] = cont;

    // Dual-mode colors (Focus View)
    QJsonObject foc;
    foc["inactiveLight"] = focus.inactiveLight.name(QColor::HexArgb);
    foc["inactiveDark"] = focus.inactiveDark.name(QColor::HexArgb);
    json["focus"] = foc;

    return json;
}

// =============================================================================
// VisualElements
// =============================================================================

VisualElements VisualElements::fromJson(const QJsonObject& json)
{
    VisualElements elem;

    if (json.contains("showHorizontalRuler")) elem.showHorizontalRuler = json["showHorizontalRuler"].toBool();
    if (json.contains("showVerticalRuler")) elem.showVerticalRuler = json["showVerticalRuler"].toBool();
    if (json.contains("rulerHeight")) elem.rulerHeight = json["rulerHeight"].toInt();
    if (json.contains("rulerWidth")) elem.rulerWidth = json["rulerWidth"].toInt();
    if (json.contains("showLineNumbers")) elem.showLineNumbers = json["showLineNumbers"].toBool();
    if (json.contains("relativeLineNumbers")) elem.relativeLineNumbers = json["relativeLineNumbers"].toBool();
    if (json.contains("showMarginGuide")) elem.showMarginGuide = json["showMarginGuide"].toBool();
    if (json.contains("marginGuideColumn")) elem.marginGuideColumn = json["marginGuideColumn"].toInt();
    if (json.contains("showIndentGuides")) elem.showIndentGuides = json["showIndentGuides"].toBool();
    if (json.contains("highlightCurrentLine")) elem.highlightCurrentLine = json["highlightCurrentLine"].toBool();
    if (json.contains("highlightCurrentParagraph")) elem.highlightCurrentParagraph = json["highlightCurrentParagraph"].toBool();
    if (json.contains("showPageShadows")) elem.showPageShadows = json["showPageShadows"].toBool();
    if (json.contains("showPageBorders")) elem.showPageBorders = json["showPageBorders"].toBool();
    if (json.contains("showPageNumbers")) elem.showPageNumbers = json["showPageNumbers"].toBool();
    if (json.contains("showScrollbar")) elem.showScrollbar = json["showScrollbar"].toBool();
    if (json.contains("autoHideScrollbar")) elem.autoHideScrollbar = json["autoHideScrollbar"].toBool();
    if (json.contains("scrollbarWidth")) elem.scrollbarWidth = json["scrollbarWidth"].toInt();
    if (json.contains("showMinimap")) elem.showMinimap = json["showMinimap"].toBool();
    if (json.contains("minimapWidth")) elem.minimapWidth = json["minimapWidth"].toInt();

    return elem;
}

QJsonObject VisualElements::toJson() const
{
    QJsonObject json;

    json["showHorizontalRuler"] = showHorizontalRuler;
    json["showVerticalRuler"] = showVerticalRuler;
    json["rulerHeight"] = rulerHeight;
    json["rulerWidth"] = rulerWidth;
    json["showLineNumbers"] = showLineNumbers;
    json["relativeLineNumbers"] = relativeLineNumbers;
    json["showMarginGuide"] = showMarginGuide;
    json["marginGuideColumn"] = marginGuideColumn;
    json["showIndentGuides"] = showIndentGuides;
    json["highlightCurrentLine"] = highlightCurrentLine;
    json["highlightCurrentParagraph"] = highlightCurrentParagraph;
    json["showPageShadows"] = showPageShadows;
    json["showPageBorders"] = showPageBorders;
    json["showPageNumbers"] = showPageNumbers;
    json["showScrollbar"] = showScrollbar;
    json["autoHideScrollbar"] = autoHideScrollbar;
    json["scrollbarWidth"] = scrollbarWidth;
    json["showMinimap"] = showMinimap;
    json["minimapWidth"] = minimapWidth;

    return json;
}

// =============================================================================
// EditorTypography
// =============================================================================

EditorTypography EditorTypography::fromJson(const QJsonObject& json)
{
    EditorTypography typo;

    if (json.contains("textFont")) {
        QJsonObject fontObj = json["textFont"].toObject();
        typo.textFont = QFont(
            fontObj["family"].toString("Georgia"),
            fontObj["size"].toInt(14)
        );
    }
    if (json.contains("lineHeight")) typo.lineHeight = json["lineHeight"].toDouble();
    if (json.contains("paragraphSpacing")) typo.paragraphSpacing = json["paragraphSpacing"].toDouble();
    if (json.contains("firstLineIndent")) typo.firstLineIndent = json["firstLineIndent"].toBool();
    if (json.contains("indentSize")) typo.indentSize = json["indentSize"].toDouble();

    return typo;
}

QJsonObject EditorTypography::toJson() const
{
    QJsonObject json;

    QJsonObject fontObj;
    fontObj["family"] = textFont.family();
    fontObj["size"] = textFont.pointSize();
    json["textFont"] = fontObj;

    json["lineHeight"] = lineHeight;
    json["paragraphSpacing"] = paragraphSpacing;
    json["firstLineIndent"] = firstLineIndent;
    json["indentSize"] = indentSize;

    return json;
}

// =============================================================================
// PageLayout
// =============================================================================

QSizeF PageLayout::pageSizePixels(qreal dpi) const
{
    qreal mmToPixels = dpi / 25.4;

    qreal width = 0;
    qreal height = 0;

    switch (pageSize) {
        case PageSize::A4:
            width = 210.0;
            height = 297.0;
            break;
        case PageSize::A5:
            width = 148.0;
            height = 210.0;
            break;
        case PageSize::Letter:
            width = 215.9;
            height = 279.4;
            break;
        case PageSize::Legal:
            width = 215.9;
            height = 355.6;
            break;
        case PageSize::Custom:
            width = customWidth;
            height = customHeight;
            break;
    }

    return QSizeF(width * mmToPixels * zoomLevel,
                  height * mmToPixels * zoomLevel);
}

QSizeF PageLayout::textAreaPixels(qreal dpi) const
{
    QSizeF page = pageSizePixels(dpi);
    qreal mmToPixels = dpi / 25.4 * zoomLevel;

    return QSizeF(
        page.width() - (margins.left() + margins.right()) * mmToPixels,
        page.height() - (margins.top() + margins.bottom()) * mmToPixels
    );
}

PageLayout PageLayout::fromJson(const QJsonObject& json)
{
    PageLayout layout;

    if (json.contains("pageSize")) {
        QString size = json["pageSize"].toString();
        if (size == "A4") layout.pageSize = PageSize::A4;
        else if (size == "A5") layout.pageSize = PageSize::A5;
        else if (size == "Letter") layout.pageSize = PageSize::Letter;
        else if (size == "Legal") layout.pageSize = PageSize::Legal;
        else if (size == "Custom") layout.pageSize = PageSize::Custom;
    }
    if (json.contains("customWidth")) layout.customWidth = json["customWidth"].toDouble();
    if (json.contains("customHeight")) layout.customHeight = json["customHeight"].toDouble();
    if (json.contains("margins")) {
        QJsonObject m = json["margins"].toObject();
        layout.margins = QMarginsF(
            m["left"].toDouble(25.4),
            m["top"].toDouble(25.4),
            m["right"].toDouble(25.4),
            m["bottom"].toDouble(25.4)
        );
    }
    if (json.contains("zoomLevel")) layout.zoomLevel = json["zoomLevel"].toDouble();
    if (json.contains("pageGap")) layout.pageGap = json["pageGap"].toDouble();
    if (json.contains("centerPages")) layout.centerPages = json["centerPages"].toBool();

    return layout;
}

QJsonObject PageLayout::toJson() const
{
    QJsonObject json;

    QString sizeStr;
    switch (pageSize) {
        case PageSize::A4: sizeStr = "A4"; break;
        case PageSize::A5: sizeStr = "A5"; break;
        case PageSize::Letter: sizeStr = "Letter"; break;
        case PageSize::Legal: sizeStr = "Legal"; break;
        case PageSize::Custom: sizeStr = "Custom"; break;
    }
    json["pageSize"] = sizeStr;
    json["customWidth"] = customWidth;
    json["customHeight"] = customHeight;

    QJsonObject m;
    m["left"] = margins.left();
    m["top"] = margins.top();
    m["right"] = margins.right();
    m["bottom"] = margins.bottom();
    json["margins"] = m;

    json["zoomLevel"] = zoomLevel;
    json["pageGap"] = pageGap;
    json["centerPages"] = centerPages;

    return json;
}

// =============================================================================
// EditorAppearance
// =============================================================================

EditorAppearance EditorAppearance::defaultAppearance()
{
    return EditorAppearance();
}

EditorAppearance EditorAppearance::darkAppearance()
{
    EditorAppearance appearance;
    appearance.colors = EditorColors::darkTheme();
    return appearance;
}

EditorAppearance EditorAppearance::sepiaAppearance()
{
    EditorAppearance appearance;
    appearance.colors = EditorColors::sepiaTheme();
    return appearance;
}

EditorAppearance EditorAppearance::minimalAppearance()
{
    EditorAppearance appearance;

    // Disable most visual elements
    appearance.elements.showHorizontalRuler = false;
    appearance.elements.showVerticalRuler = false;
    appearance.elements.showLineNumbers = false;
    appearance.elements.showMarginGuide = false;
    appearance.elements.showIndentGuides = false;
    appearance.elements.highlightCurrentLine = false;
    appearance.elements.showPageShadows = false;
    appearance.elements.showPageBorders = false;
    appearance.elements.autoHideScrollbar = true;

    return appearance;
}

EditorAppearance EditorAppearance::typewriterAppearance()
{
    EditorAppearance appearance = sepiaAppearance();

    // Typewriter-like typography
    appearance.typography.textFont = QFont("Courier New", 14);
    appearance.typography.lineHeight = 1.8;
    appearance.typography.firstLineIndent = false;

    // Enable typewriter mode
    appearance.typewriter.enabled = true;
    appearance.typewriter.focusPosition = 0.4;
    appearance.typewriter.smoothScroll = true;

    return appearance;
}

EditorAppearance EditorAppearance::highContrastAppearance()
{
    EditorAppearance appearance;
    EditorColors& colors = appearance.colors;
    colors.editorBackground = QColor(0, 0, 0);
    colors.pageBackground = QColor(255, 255, 255);
    colors.pageShadow = QColor(0, 0, 0, 0);
    colors.marginArea = QColor(255, 255, 255);
    colors.text = QColor(0, 0, 0);
    colors.textSecondary = QColor(0, 0, 0);
    colors.textDimmed = QColor(100, 100, 100);
    colors.selection = QColor(0, 0, 255, 100);
    colors.selectionBorder = QColor(0, 0, 255);
    colors.cursor = QColor(0, 0, 0);
    colors.cursorLine = QColor(255, 255, 0, 50);
    colors.ruler = QColor(0, 0, 0);
    colors.scrollbar = QColor(0, 0, 0);
    colors.accent = QColor(0, 0, 255);
    colors.warning = QColor(255, 255, 0);
    colors.error = QColor(255, 0, 0);
    appearance.elements.showPageShadows = false;
    appearance.elements.showPageBorders = true;
    appearance.typography.textFont = QFont("Arial", 16);
    appearance.typography.lineHeight = 2.0;
    return appearance;
}

bool EditorAppearance::isSystemHighContrastEnabled()
{
    // Check GTK_THEME for Linux high contrast
    QString gtkTheme = qEnvironmentVariable("GTK_THEME");
    if (!gtkTheme.isEmpty()) {
        return gtkTheme.toLower().contains("highcontrast");
    }
    return false;
}

EditorAppearance EditorAppearance::systemAwareAppearance()
{
    return isSystemHighContrastEnabled() ? highContrastAppearance() : defaultAppearance();
}

EditorAppearance EditorAppearance::fromJson(const QJsonObject& json)
{
    EditorAppearance appearance;

    // Editor color mode (light/dark toggle)
    if (json.contains("colorMode")) {
        QString mode = json["colorMode"].toString();
        appearance.colorMode = (mode == "light")
            ? EditorColorMode::Light
            : EditorColorMode::Dark;
    }

    if (json.contains("colors")) {
        appearance.colors = EditorColors::fromJson(json["colors"].toObject());
    }
    if (json.contains("elements")) {
        appearance.elements = VisualElements::fromJson(json["elements"].toObject());
    }
    if (json.contains("typography")) {
        appearance.typography = EditorTypography::fromJson(json["typography"].toObject());
    }
    if (json.contains("pageLayout")) {
        appearance.pageLayout = PageLayout::fromJson(json["pageLayout"].toObject());
    }

    // Mode-specific settings
    if (json.contains("typewriter")) {
        QJsonObject tw = json["typewriter"].toObject();
        appearance.typewriter.enabled = tw["enabled"].toBool();
        appearance.typewriter.focusPosition = tw["focusPosition"].toDouble(0.4);
        appearance.typewriter.smoothScroll = tw["smoothScroll"].toBool(true);
        appearance.typewriter.scrollDuration = tw["scrollDuration"].toInt(150);
    }

    if (json.contains("focusMode")) {
        QJsonObject fm = json["focusMode"].toObject();
        appearance.focusMode.enabled = fm["enabled"].toBool();
        appearance.focusMode.dimOpacity = fm["dimOpacity"].toDouble(0.3);
        appearance.focusMode.highlightBackground = fm["highlightBackground"].toBool();
        appearance.focusMode.fadeTransition = fm["fadeTransition"].toBool(true);
        appearance.focusMode.transitionDuration = fm["transitionDuration"].toInt(200);
    }

    if (json.contains("distractionFree")) {
        QJsonObject df = json["distractionFree"].toObject();
        appearance.distractionFree.enabled = df["enabled"].toBool();
        appearance.distractionFree.fullscreen = df["fullscreen"].toBool(true);
        appearance.distractionFree.hideAllUI = df["hideAllUI"].toBool(true);
        appearance.distractionFree.showWordCount = df["showWordCount"].toBool(true);
        appearance.distractionFree.textWidth = df["textWidth"].toDouble(0.6);
        appearance.distractionFree.uiFadeTimeout = df["uiFadeTimeout"].toInt(2000);
    }

    // Margins
    if (json.contains("pageMargins") && json["pageMargins"].isObject()) {
        QJsonObject pm = json["pageMargins"].toObject();
        if (pm.contains("top")) appearance.pageMargins.top = pm["top"].toDouble();
        if (pm.contains("bottom")) appearance.pageMargins.bottom = pm["bottom"].toDouble();
        if (pm.contains("left")) appearance.pageMargins.left = pm["left"].toDouble();
        if (pm.contains("right")) appearance.pageMargins.right = pm["right"].toDouble();
        if (pm.contains("mirrorEnabled")) appearance.pageMargins.mirrorEnabled = pm["mirrorEnabled"].toBool();
        if (pm.contains("inner")) appearance.pageMargins.inner = pm["inner"].toDouble();
        if (pm.contains("outer")) appearance.pageMargins.outer = pm["outer"].toDouble();
    }
    if (json.contains("viewMargins") && json["viewMargins"].isObject()) {
        QJsonObject vm = json["viewMargins"].toObject();
        if (vm.contains("vertical")) appearance.viewMargins.vertical = vm["vertical"].toDouble();
        if (vm.contains("horizontal")) appearance.viewMargins.horizontal = vm["horizontal"].toDouble();
    }

    // Text frame border
    if (json.contains("textFrameBorder") && json["textFrameBorder"].isObject()) {
        QJsonObject tfb = json["textFrameBorder"].toObject();
        if (tfb.contains("show")) appearance.textFrameBorder.show = tfb["show"].toBool();
        if (tfb.contains("color") && tfb["color"].isArray()) {
            QJsonArray colorArray = tfb["color"].toArray();
            if (colorArray.size() >= 3) {
                appearance.textFrameBorder.color = QColor(
                    colorArray[0].toInt(),
                    colorArray[1].toInt(),
                    colorArray[2].toInt()
                );
            }
        }
        if (tfb.contains("width")) appearance.textFrameBorder.width = tfb["width"].toInt();
    }

    return appearance;
}

QJsonObject EditorAppearance::toJson() const
{
    QJsonObject json;

    // Editor color mode
    json["colorMode"] = (colorMode == EditorColorMode::Light) ? "light" : "dark";

    json["colors"] = colors.toJson();
    json["elements"] = elements.toJson();
    json["typography"] = typography.toJson();
    json["pageLayout"] = pageLayout.toJson();

    // Mode-specific settings
    QJsonObject tw;
    tw["enabled"] = typewriter.enabled;
    tw["focusPosition"] = typewriter.focusPosition;
    tw["smoothScroll"] = typewriter.smoothScroll;
    tw["scrollDuration"] = typewriter.scrollDuration;
    json["typewriter"] = tw;

    QJsonObject fm;
    fm["enabled"] = focusMode.enabled;
    fm["dimOpacity"] = focusMode.dimOpacity;
    fm["highlightBackground"] = focusMode.highlightBackground;
    fm["fadeTransition"] = focusMode.fadeTransition;
    fm["transitionDuration"] = focusMode.transitionDuration;
    json["focusMode"] = fm;

    QJsonObject df;
    df["enabled"] = distractionFree.enabled;
    df["fullscreen"] = distractionFree.fullscreen;
    df["hideAllUI"] = distractionFree.hideAllUI;
    df["showWordCount"] = distractionFree.showWordCount;
    df["textWidth"] = distractionFree.textWidth;
    df["uiFadeTimeout"] = distractionFree.uiFadeTimeout;
    json["distractionFree"] = df;

    // Margins
    QJsonObject pm;
    pm["top"] = pageMargins.top;
    pm["bottom"] = pageMargins.bottom;
    pm["left"] = pageMargins.left;
    pm["right"] = pageMargins.right;
    pm["mirrorEnabled"] = pageMargins.mirrorEnabled;
    pm["inner"] = pageMargins.inner;
    pm["outer"] = pageMargins.outer;
    json["pageMargins"] = pm;

    QJsonObject vm;
    vm["vertical"] = viewMargins.vertical;
    vm["horizontal"] = viewMargins.horizontal;
    json["viewMargins"] = vm;

    // Text frame border
    QJsonObject tfb;
    tfb["show"] = textFrameBorder.show;
    QJsonArray colorArray;
    colorArray.append(textFrameBorder.color.red());
    colorArray.append(textFrameBorder.color.green());
    colorArray.append(textFrameBorder.color.blue());
    tfb["color"] = colorArray;
    tfb["width"] = textFrameBorder.width;
    json["textFrameBorder"] = tfb;

    return json;
}

EditorAppearance EditorAppearance::loadFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return defaultAppearance();
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) {
        return defaultAppearance();
    }

    return fromJson(doc.object());
}

bool EditorAppearance::saveToFile(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

EditorAppearance EditorAppearance::with(std::function<void(EditorAppearance&)> modifier) const
{
    EditorAppearance copy = *this;
    modifier(copy);
    return copy;
}

EditorAppearance EditorAppearance::lerp(const EditorAppearance& a,
                                         const EditorAppearance& b,
                                         qreal t)
{
    // For now, just switch at t=0.5
    // Future: proper color interpolation for smooth theme transitions
    return t < 0.5 ? a : b;
}

}  // namespace kalahari::editor
