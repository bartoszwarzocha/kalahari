/// @file appearance_pages.cpp
/// @brief Settings pages: Appearance > Theme and Icons

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

using json = nlohmann::json;

// ============================================================================
// Appearance > Theme
// ============================================================================

ThemePage::ThemePage(QWidget* parent)
    : SettingsPage(parent)
    , m_themeCombo(new QComboBox())
{
    auto* themeRow = new QHBoxLayout();
    auto* themeLabel = new QLabel(tr("Theme:"));
    themeLabel->setBuddy(m_themeCombo);
    m_themeCombo->addItem(tr("Light"), QStringLiteral("Light"));
    m_themeCombo->addItem(tr("Dark"), QStringLiteral("Dark"));
    themeRow->addWidget(themeLabel);
    themeRow->addWidget(m_themeCombo, 1);
    pageLayout()->addLayout(themeRow);
    // Stored by apply(): reloading the theme stores its name
    bind(m_themeCombo, "appearance.theme").store = [](const json&) {};

    const auto group = [this](const QString& title) {
        auto* box = new QGroupBox(title);
        pageLayout()->addWidget(box);
        return new QVBoxLayout(box);
    };
    // Section headings inside a group, in the theme's text color
    const QString headingStyle = QStringLiteral("font-weight: bold; margin-top: 8px; color: %1;")
        .arg(core::ThemeManager::getInstance().getCurrentTheme().palette.windowText.name());
    const auto heading = [&headingStyle](QVBoxLayout* layout, const QString& text) {
        auto* label = new QLabel(text);
        label->setStyleSheet(headingStyle);
        layout->addWidget(label);
    };
    using core::Theme;

    QVBoxLayout* icons = group(tr("Icon Colors"));
    m_primaryColor = addColor(icons, tr("Primary"), tr("Primary icon color used for main icon elements"),
                              "icons.themes.", ".colorPrimary",
                              [](const Theme& t) { return t.colors.primary; });
    m_secondaryColor = addColor(icons, tr("Secondary"), tr("Secondary icon color used for icon accents"),
                                "icons.themes.", ".colorSecondary",
                                [](const Theme& t) { return t.colors.secondary; });
    connect(m_primaryColor, &ColorConfigWidget::colorChanged, this, &ThemePage::iconColorsChanged);
    connect(m_secondaryColor, &ColorConfigWidget::colorChanged, this, &ThemePage::iconColorsChanged);

    QVBoxLayout* ui = group(tr("UI Colors"));
    addColor(ui, tr("Tooltip Background"), tr("Background color for tooltips"),
             "themes.", ".ui.toolTipBase", [](const Theme& t) { return t.palette.toolTipBase; });
    addColor(ui, tr("Tooltip Text"), tr("Text color for tooltips"),
             "themes.", ".ui.toolTipText", [](const Theme& t) { return t.palette.toolTipText; });
    addColor(ui, tr("Placeholder Text"), tr("Color for placeholder text in input fields"),
             "themes.", ".ui.placeholderText", [](const Theme& t) { return t.palette.placeholderText; });
    addColor(ui, tr("Bright Text"), tr("High contrast text color for dark backgrounds"),
             "themes.", ".ui.brightText", [](const Theme& t) { return t.palette.brightText; });
    addColor(ui, tr("Info Header"), tr("Color for information panel headers"),
             "themes.", ".colors.infoHeader", [](const Theme& t) { return t.colors.infoHeader; });
    addColor(ui, tr("Dashboard Secondary"), tr("Secondary dashboard accent color"),
             "themes.", ".colors.dashboardSecondary",
             [](const Theme& t) { return t.colors.dashboardSecondary; });
    addColor(ui, tr("Dashboard Primary"), tr("Primary dashboard accent color"),
             "themes.", ".colors.dashboardPrimary",
             [](const Theme& t) { return t.colors.dashboardPrimary; });
    addColor(ui, tr("Info Primary"), tr("Primary color for info panels"),
             "themes.", ".colors.infoPrimary", [](const Theme& t) { return t.colors.infoPrimary; });
    addColor(ui, tr("Info Secondary"), tr("Secondary color for info panels"),
             "themes.", ".colors.infoSecondary", [](const Theme& t) { return t.colors.infoSecondary; });

    QVBoxLayout* palette = group(tr("Palette Colors"));
    const auto paletteColor = [&](const QString& label, const QString& toolTip, const char* key,
                                  QColor Theme::Palette::*field) {
        addColor(palette, label, toolTip, "themes.", std::string(".palette.") + key,
                 [field](const Theme& t) { return t.palette.*field; });
    };
    heading(palette, tr("Basic Colors"));
    paletteColor(tr("Window"), tr("General background color for windows and panels"),
                 "window", &Theme::Palette::window);
    paletteColor(tr("Window Text"), tr("General text color used throughout the interface"),
                 "windowText", &Theme::Palette::windowText);
    paletteColor(tr("Base"), tr("Background color for input fields and text editors"),
                 "base", &Theme::Palette::base);
    paletteColor(tr("Alternate Base"), tr("Alternating row background color in lists and tables"),
                 "alternateBase", &Theme::Palette::alternateBase);
    paletteColor(tr("Text"), tr("Text color for input fields and text editors"),
                 "text", &Theme::Palette::text);
    heading(palette, tr("Button Colors"));
    paletteColor(tr("Button"), tr("Background color for buttons"), "button", &Theme::Palette::button);
    paletteColor(tr("Button Text"), tr("Text color for buttons"), "buttonText", &Theme::Palette::buttonText);
    heading(palette, tr("Selection Colors"));
    paletteColor(tr("Highlight"), tr("Background color for selected items"),
                 "highlight", &Theme::Palette::highlight);
    paletteColor(tr("Highlighted Text"), tr("Text color for selected items"),
                 "highlightedText", &Theme::Palette::highlightedText);
    heading(palette, tr("3D Effect Colors"));
    paletteColor(tr("Light"), tr("Lightest color for 3D effects (bevels, shadows)"),
                 "light", &Theme::Palette::light);
    paletteColor(tr("Midlight"), tr("Color between Light and Button for 3D effects"),
                 "midlight", &Theme::Palette::midlight);
    paletteColor(tr("Mid"), tr("Medium color for borders and dividers"), "mid", &Theme::Palette::mid);
    paletteColor(tr("Dark"), tr("Darker color for 3D effects"), "dark", &Theme::Palette::dark);
    paletteColor(tr("Shadow"), tr("Darkest color for shadows"), "shadow", &Theme::Palette::shadow);
    heading(palette, tr("Link Colors"));
    paletteColor(tr("Link"), tr("Color for hyperlinks"), "link", &Theme::Palette::link);
    paletteColor(tr("Link Visited"), tr("Color for visited hyperlinks"),
                 "linkVisited", &Theme::Palette::linkVisited);

    QVBoxLayout* log = group(tr("Log Panel Colors"));
    const auto logColor = [&](const QString& label, const QString& toolTip, const char* key,
                              QColor Theme::LogColors::*field) {
        addColor(log, label, toolTip, "themes.", std::string(".log.") + key,
                 [field](const Theme& t) { return t.log.*field; });
    };
    logColor(tr("Trace"), tr("Color for TRACE level log messages (diagnostic mode only)"),
             "trace", &Theme::LogColors::trace);
    logColor(tr("Debug"), tr("Color for DEBUG level log messages (diagnostic mode only)"),
             "debug", &Theme::LogColors::debug);
    logColor(tr("Info"), tr("Color for INFO level log messages"), "info", &Theme::LogColors::info);
    logColor(tr("Warning"), tr("Color for WARNING level log messages"), "warning", &Theme::LogColors::warning);
    logColor(tr("Error"), tr("Color for ERROR level log messages"), "error", &Theme::LogColors::error);
    logColor(tr("Critical"), tr("Color for CRITICAL level log messages"),
             "critical", &Theme::LogColors::critical);
    logColor(tr("Background"), tr("Background color of the log panel"),
             "background", &Theme::LogColors::background);

    auto* reset = new QPushButton(tr("Reset to Theme Defaults"));
    reset->setToolTip(tr("Reset all colors to the default values for the selected theme"));
    connect(reset, &QPushButton::clicked, this, &ThemePage::resetToThemeDefaults);
    pageLayout()->addWidget(reset);
    pageLayout()->addStretch();

    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, &ThemePage::onThemeChanged);
}

ColorConfigWidget* ThemePage::addColor(QLayout* section, const QString& label, const QString& toolTip,
                                       const std::string& keyPrefix, const std::string& keySuffix,
                                       std::function<QColor(const core::Theme&)> themeColor) {
    auto* widget = new ColorConfigWidget(label);
    widget->setToolTip(toolTip);
    section->addWidget(widget);

    // The key follows the theme chosen in the combo box; a color that is not stored
    // shows the theme file's value
    Binding& binding = bind(widget, keyPrefix + "Light" + keySuffix);
    binding.key = [this, keyPrefix, keySuffix]() { return keyPrefix + themeName() + keySuffix; };
    binding.stored = [this, keyPrefix, keySuffix, themeColor]() {
        const std::string key = keyPrefix + themeName() + keySuffix;
        auto& settings = core::SettingsManager::getInstance();
        if (settings.hasKey(key)) {
            return settings.get<json>(key);
        }
        try {
            const core::Theme theme =
                core::ThemeManager::getInstance().loadTheme(QString::fromStdString(themeName()));
            return json(themeColor(theme).name().toStdString());
        } catch (const std::exception& e) {
            core::Logger::getInstance().warn("ThemePage: Cannot load theme '{}': {}", themeName(), e.what());
            return json(core::ThemeManager::getInstance().getCurrentTheme().colors.primary.name().toStdString());
        }
    };
    binding.store = [binding = &binding](const json& value) {
        core::SettingsManager::getInstance().set<json>(binding->key(), value);
    };
    m_colors.push_back({widget, std::move(themeColor), &binding});
    return widget;
}

std::string ThemePage::themeName() const {
    return m_themeCombo->currentData().toString().toStdString();
}

void ThemePage::onThemeChanged() {
    for (ColorField& color : m_colors) {
        reload(*color.binding);
    }
    emit iconColorsChanged();
}

void ThemePage::resetToThemeDefaults() {
    core::Theme defaults;
    try {
        defaults = core::ThemeManager::getInstance().loadTheme(m_themeCombo->currentData().toString());
    } catch (const std::exception& e) {
        core::Logger::getInstance().warn("ThemePage: Cannot load theme '{}': {}", themeName(), e.what());
        return;
    }
    for (const ColorField& color : m_colors) {
        color.widget->setColor(color.themeColor(defaults));
    }
    emit iconColorsChanged();
}

std::vector<std::string> ThemePage::apply() {
    std::vector<std::string> written = SettingsPage::apply();
    if (!written.empty()) {
        // Theme switch and colors in one pass, from the stored colors: the same result
        // as after a restart
        core::ThemeManager::getInstance().reloadTheme(m_themeCombo->currentData().toString());
    }
    return written;
}

std::pair<QColor, QColor> ThemePage::iconColors() const {
    return {m_primaryColor->color(), m_secondaryColor->color()};
}

// ============================================================================
// Appearance > Icons
// ============================================================================

IconsPage::IconsPage(QWidget* parent)
    : SettingsPage(parent)
    , m_iconStyle(new QComboBox())
    , m_previewLayout(nullptr)
    , m_primary(core::ArtProvider::getInstance().getPrimaryColor())
    , m_secondary(core::ArtProvider::getInstance().getSecondaryColor())
{
    auto& artProvider = core::ArtProvider::getInstance();

    QFormLayout* style = addGroup(tr("Icon Style"));
    m_iconStyle->addItem(tr("Two-tone (Default)"), QStringLiteral("twotone"));
    m_iconStyle->addItem(tr("Filled"), QStringLiteral("filled"));
    m_iconStyle->addItem(tr("Outlined"), QStringLiteral("outlined"));
    m_iconStyle->addItem(tr("Rounded"), QStringLiteral("rounded"));
    addField(style, tr("Icon Style:"), m_iconStyle, "appearance.iconTheme");
    // ArtProvider stores the style and redraws the icons
    lastBinding().store = [&artProvider](const json& value) {
        artProvider.setIconTheme(QString::fromStdString(value.get<std::string>()));
    };

    auto* preview = new QWidget();
    preview->setAutoFillBackground(true);
    QPalette previewPalette = preview->palette();
    previewPalette.setColor(QPalette::Window, palette().color(QPalette::Base));
    preview->setPalette(previewPalette);
    preview->setFixedHeight(52);
    m_previewLayout = new QHBoxLayout(preview);
    m_previewLayout->setContentsMargins(12, 8, 12, 8);
    m_previewLayout->setSpacing(16);
    m_previewLayout->setAlignment(Qt::AlignCenter);
    style->addRow(tr("Preview:"), preview);
    connect(m_iconStyle, &QComboBox::currentIndexChanged, this, &IconsPage::updatePreview);

    QFormLayout* sizes = addGroup(tr("Icon Sizes"));
    const auto size = [&](const QString& label, core::IconContext context, int core::IconSizeConfig::*field,
                          const char* key, int maximum) {
        auto* spin = new QSpinBox();
        spin->setRange(12, maximum);
        spin->setSingleStep(2);
        spin->setSuffix(tr(" px"));
        addField(sizes, label, spin, key);
        // The icon registry holds the sizes in use; ArtProvider stores them and redraws
        Binding& binding = lastBinding();
        binding.stored = [field]() { return json(core::IconRegistry::getInstance().getSizes().*field); };
        binding.store = [&artProvider, context](const json& value) {
            artProvider.setIconSize(context, value.get<int>());
        };
    };
    size(tr("Toolbar:"), core::IconContext::Toolbar, &core::IconSizeConfig::toolbar, "icons.sizes.toolbar", 48);
    size(tr("Menu:"), core::IconContext::Menu, &core::IconSizeConfig::menu, "icons.sizes.menu", 32);
    size(tr("Navigator/Tree:"), core::IconContext::TreeView, &core::IconSizeConfig::treeView,
         "icons.sizes.treeView", 32);
    size(tr("Tab Bar:"), core::IconContext::TabBar, &core::IconSizeConfig::tabBar, "icons.sizes.tabBar", 32);
    size(tr("Buttons:"), core::IconContext::Button, &core::IconSizeConfig::button, "icons.sizes.button", 32);
    size(tr("Status Bar:"), core::IconContext::StatusBar, &core::IconSizeConfig::statusBar,
         "icons.sizes.statusBar", 24);
    size(tr("Combo Boxes:"), core::IconContext::ComboBox, &core::IconSizeConfig::comboBox,
         "icons.sizes.comboBox", 24);

    pageLayout()->addStretch();
}

void IconsPage::setPreviewColors(const QColor& primary, const QColor& secondary) {
    m_primary = primary;
    m_secondary = secondary;
    updatePreview();
}

void IconsPage::updatePreview() {
    utils::clearLayout(m_previewLayout);

    const QString iconTheme = m_iconStyle->currentData().toString();
    constexpr int ICON_SIZE = 24;  // Qt scales for the screen's DPI
    const QStringList sampleIcons = {
        QStringLiteral("file.new"), QStringLiteral("file.open"), QStringLiteral("file.save"),
        QStringLiteral("edit.undo"), QStringLiteral("edit.redo"), QStringLiteral("edit.copy")};

    for (const QString& cmdId : sampleIcons) {
        // Drawn in the colors shown on the Theme page, not the cached theme colors
        const QIcon icon = core::IconRegistry::getInstance().getIconWithColors(
            cmdId, iconTheme, ICON_SIZE, m_primary, m_secondary);
        if (icon.isNull()) {
            continue;
        }
        auto* iconLabel = new QLabel();
        iconLabel->setPixmap(icon.pixmap(ICON_SIZE, ICON_SIZE));
        iconLabel->setFixedSize(ICON_SIZE, ICON_SIZE);
        iconLabel->setToolTip(cmdId);
        m_previewLayout->addWidget(iconLabel);
    }
}

} // namespace gui
} // namespace kalahari
