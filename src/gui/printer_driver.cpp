// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file printer_driver.cpp
/// @brief The settings of the driver of a printer: its paper sources and qualities, the
///        window of the driver (Printer Properties...) and its settings for a print
///
/// On Windows the settings of a driver are a DEVMODE: DeviceCapabilities lists the paper
/// sources and the qualities, DocumentProperties gives the settings at first and shows the
/// window of the driver, and the print engine of Qt takes the settings as QPrintDialog gives
/// them (QWin32PrintEngine::setGlobalDevMode). Elsewhere the system does not let the program
/// do it: the lists stay empty and the window of the driver is not there.

#include "kalahari/gui/printer_driver.h"

#include <QByteArray>
#include <QList>
#include <QPrinter>
#include <QSize>
#include <QString>
#include <QWidget>

#ifdef Q_OS_WIN
#if !defined(NOMINMAX)
#define NOMINMAX  // std::min and std::max, not the macros of windows.h
#endif
#include <windows.h>
#include <winspool.h>

#ifdef _MSC_VER
#pragma warning(push, 0)  // A private header of Qt, not the program's code
#endif
#include <QtPrintSupport/private/qprintengine_win_p.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#endif

namespace kalahari::gui::printer_driver {

using Dialog = dialogs::PrintBookDialog;

#ifdef Q_OS_WIN

namespace {

/// The room for the name of a paper source that DeviceCapabilities gives, in characters
constexpr int BIN_NAME_LENGTH = 24;

/// Whether @p bytes hold the settings of a driver: a DEVMODE with what the driver adds
bool holdsDevMode(const QByteArray& bytes) {
    if (bytes.size() < static_cast<qsizetype>(sizeof(DEVMODEW))) {
        return false;
    }
    const auto* mode = reinterpret_cast<const DEVMODEW*>(bytes.constData());
    return mode->dmSize > 0 &&
           static_cast<qsizetype>(mode->dmSize) + mode->dmDriverExtra <= bytes.size();
}

/// The DEVMODE of the settings of a driver in @p bytes
DEVMODEW* devModeOf(QByteArray& bytes) {
    return reinterpret_cast<DEVMODEW*>(bytes.data());
}

/// Whether the settings @p mode say @p field
bool has(const DEVMODEW& mode, DWORD field) {
    return (mode.dmFields & field) != 0;
}

/// The settings @p mode with @p field said
void mark(DEVMODEW& mode, DWORD field) {
    mode.dmFields |= field;
}

/// A printer of the system open for its driver, closed when it goes
class DriverPrinter {
public:
    explicit DriverPrinter(const QString& name)
        : m_name(name.toStdWString())
    {
        if (OpenPrinterW(m_name.data(), &m_handle, nullptr) == FALSE) {
            m_handle = nullptr;
        }
    }

    ~DriverPrinter()
    {
        if (m_handle != nullptr) {
            ClosePrinter(m_handle);
        }
    }

    DriverPrinter(const DriverPrinter&) = delete;
    DriverPrinter& operator=(const DriverPrinter&) = delete;
    DriverPrinter(DriverPrinter&&) = delete;
    DriverPrinter& operator=(DriverPrinter&&) = delete;

    /// The settings its driver takes at first; empty: it does not say
    [[nodiscard]] QByteArray defaultSettings()
    {
        QByteArray settings = settingsRoom();
        if (settings.isEmpty() ||
            DocumentPropertiesW(nullptr, m_handle, m_name.data(), devModeOf(settings), nullptr,
                                static_cast<DWORD>(DM_OUT_BUFFER)) != IDOK) {
            return {};
        }
        return settings;
    }

    /// Show the window of its driver over @p window with @p settings: the settings chosen,
    /// empty when the writer cancelled it
    [[nodiscard]] QByteArray showWindow(HWND window, QByteArray settings)
    {
        QByteArray chosen = settingsRoom();
        if (chosen.isEmpty() ||
            DocumentPropertiesW(window, m_handle, m_name.data(), devModeOf(chosen),
                                devModeOf(settings),
                                static_cast<DWORD>(DM_IN_BUFFER | DM_IN_PROMPT |
                                                   DM_OUT_BUFFER)) != IDOK) {
            return {};
        }
        return chosen;
    }

private:
    /// Room for the settings of its driver; empty: it does not say how much
    [[nodiscard]] QByteArray settingsRoom()
    {
        if (m_handle == nullptr) {
            return {};
        }
        const LONG size =
            DocumentPropertiesW(nullptr, m_handle, m_name.data(), nullptr, nullptr, 0);
        if (size <= 0) {
            return {};
        }
        return QByteArray(std::max(static_cast<qsizetype>(size),
                                   static_cast<qsizetype>(sizeof(DEVMODEW))),
                          '\0');
    }

    std::wstring m_name;
    HANDLE m_handle = nullptr;
};

/// The paper sources of the printer @p name as its driver lists them, without those it does
/// not name
QList<Dialog::PaperSource> sourcesOf(const std::wstring& name) {
    QList<Dialog::PaperSource> sources;
    const int count = DeviceCapabilitiesW(name.c_str(), nullptr, DC_BINS, nullptr, nullptr);
    if (count <= 0 ||
        DeviceCapabilitiesW(name.c_str(), nullptr, DC_BINNAMES, nullptr, nullptr) != count) {
        return sources;
    }
    std::vector<WORD> ids(static_cast<size_t>(count));
    std::vector<wchar_t> names(static_cast<size_t>(count) * BIN_NAME_LENGTH, L'\0');
    if (DeviceCapabilitiesW(name.c_str(), nullptr, DC_BINS,
                            reinterpret_cast<LPWSTR>(ids.data()), nullptr) != count ||
        DeviceCapabilitiesW(name.c_str(), nullptr, DC_BINNAMES, names.data(), nullptr) !=
            count) {
        return sources;
    }
    for (size_t i = 0; i < ids.size(); ++i) {
        // A name as long as its room has no end of its own
        const wchar_t* start = names.data() + i * BIN_NAME_LENGTH;
        const wchar_t* end = std::find(start, start + BIN_NAME_LENGTH, L'\0');
        const QString shown =
            QString::fromWCharArray(start, static_cast<qsizetype>(end - start)).trimmed();
        if (!shown.isEmpty()) {
            sources.append(Dialog::PaperSource{static_cast<int>(ids.at(i)), shown});
        }
    }
    return sources;
}

/// The qualities of the printer @p name as its driver lists them: the dots per inch across
/// and down
QList<QSize> qualitiesOf(const std::wstring& name) {
    QList<QSize> qualities;
    const int count =
        DeviceCapabilitiesW(name.c_str(), nullptr, DC_ENUMRESOLUTIONS, nullptr, nullptr);
    if (count <= 0) {
        return qualities;
    }
    // Two numbers each: across, then down
    std::vector<LONG> values(static_cast<size_t>(count) * 2, 0);
    if (DeviceCapabilitiesW(name.c_str(), nullptr, DC_ENUMRESOLUTIONS,
                            reinterpret_cast<LPWSTR>(values.data()), nullptr) != count) {
        return qualities;
    }
    for (size_t i = 0; i + 1 < values.size(); i += 2) {
        const QSize quality(static_cast<int>(values.at(i)), static_cast<int>(values.at(i + 1)));
        if (quality.width() > 0 && quality.height() > 0 && !qualities.contains(quality)) {
            qualities.append(quality);
        }
    }
    return qualities;
}

/// The quality of the settings @p mode in dots per inch; invalid: the driver's own (a level
/// such as "draft" or "high", not dots)
QSize qualityOf(const DEVMODEW& mode) {
    if (!has(mode, DM_PRINTQUALITY) || mode.dmPrintQuality <= 0) {
        return {};
    }
    const int down = has(mode, DM_YRESOLUTION) && mode.dmYResolution > 0 ? mode.dmYResolution
                                                                          : mode.dmPrintQuality;
    return {mode.dmPrintQuality, down};
}

/// The paper of the settings @p mode; invalid: they do not say
QPageSize paperOf(const DEVMODEW& mode) {
    if (has(mode, DM_PAPERSIZE) && mode.dmPaperSize > 0) {
        const QPageSize::PageSizeId id = QPageSize::id(static_cast<int>(mode.dmPaperSize));
        if (id != QPageSize::Custom) {
            return QPageSize(id);
        }
    }
    // A paper of the driver's own: its size, in tenths of a millimeter
    if (has(mode, DM_PAPERWIDTH) && has(mode, DM_PAPERLENGTH) && mode.dmPaperWidth > 0 &&
        mode.dmPaperLength > 0) {
        return QPageSize(QSizeF(mode.dmPaperWidth / 10.0, mode.dmPaperLength / 10.0),
                         QPageSize::Millimeter);
    }
    return {};
}

/// The way of both sides of the settings of a driver @p duplex, as the window has it
Dialog::Duplex duplexOf(short duplex) {
    switch (duplex) {
        case DMDUP_VERTICAL:
            return Dialog::Duplex::LongEdge;
        case DMDUP_HORIZONTAL:
            return Dialog::Duplex::ShortEdge;
        default:
            break;
    }
    return Dialog::Duplex::None;
}

/// @p mode with the paper source @p source and the quality @p quality, where the window
/// chose them
void putSourceAndQuality(DEVMODEW& mode, int source, const QSize& quality) {
    if (source >= 0) {
        mode.dmDefaultSource = static_cast<short>(source);
        mark(mode, DM_DEFAULTSOURCE);
    }
    if (quality.isValid()) {
        mode.dmPrintQuality = static_cast<short>(quality.width());
        mode.dmYResolution = static_cast<short>(quality.height());
        mark(mode, DM_PRINTQUALITY);
        mark(mode, DM_YRESOLUTION);
    }
}

/// @p mode with the choices of the window @p choice, which its window shows at first. Not
/// the way the sheet lies: the window of the print chooses it from the pages (a change in
/// the window of the driver does not count)
void putChoices(DEVMODEW& mode, const Dialog::Choice& choice) {
    if (const int paper = choice.paper.isValid() ? choice.paper.windowsId() : 0; paper > 0) {
        mode.dmPaperSize = static_cast<short>(paper);
        mark(mode, DM_PAPERSIZE);
    }
    mode.dmCopies = static_cast<short>(choice.copies);
    mark(mode, DM_COPIES);
    mode.dmCollate = static_cast<short>(choice.collate ? DMCOLLATE_TRUE : DMCOLLATE_FALSE);
    mark(mode, DM_COLLATE);
    mode.dmColor = static_cast<short>(choice.color ? DMCOLOR_COLOR : DMCOLOR_MONOCHROME);
    mark(mode, DM_COLOR);
    // Both sides by hand: the printer prints on one side
    short duplex = DMDUP_SIMPLEX;
    if (choice.duplex == Dialog::Duplex::LongEdge) {
        duplex = DMDUP_VERTICAL;
    } else if (choice.duplex == Dialog::Duplex::ShortEdge) {
        duplex = DMDUP_HORIZONTAL;
    }
    mode.dmDuplex = duplex;
    mark(mode, DM_DUPLEX);
    putSourceAndQuality(mode, choice.source, choice.quality);
}

/// What the window of the driver chose: its settings @p settings, which it showed as
/// @p shown
Dialog::DriverChoice driverChoiceOf(QByteArray settings, const DEVMODEW& shown) {
    Dialog::DriverChoice chosen;
    const DEVMODEW& mode = *devModeOf(settings);
    chosen.paper = paperOf(mode);
    if (has(mode, DM_COPIES) && mode.dmCopies > 0) {
        chosen.copies = mode.dmCopies;
    }
    if (has(mode, DM_COLLATE)) {
        chosen.collate = mode.dmCollate == DMCOLLATE_TRUE;
    }
    if (has(mode, DM_COLOR)) {
        chosen.color = mode.dmColor == DMCOLOR_COLOR;
    }
    // Only a change: one side is also both sides by hand in the window
    if (has(mode, DM_DUPLEX) && mode.dmDuplex != shown.dmDuplex) {
        chosen.duplex = duplexOf(mode.dmDuplex);
    }
    if (has(mode, DM_DEFAULTSOURCE)) {
        chosen.source = mode.dmDefaultSource;
    }
    chosen.quality = qualityOf(mode);
    chosen.settings = std::move(settings);
    return chosen;
}

}  // namespace

bool hasDriverWindow() {
    return true;
}

void addSourcesAndQualities(Dialog::Printer& printer) {
    const std::wstring name = printer.name.toStdWString();
    printer.sources = sourcesOf(name);
    printer.qualities = qualitiesOf(name);
    DriverPrinter driver(printer.name);
    QByteArray settings = driver.defaultSettings();
    if (!holdsDevMode(settings)) {
        return;
    }
    const DEVMODEW& mode = *devModeOf(settings);
    if (has(mode, DM_DEFAULTSOURCE)) {
        printer.source = mode.dmDefaultSource;
    }
    printer.quality = qualityOf(mode);
}

std::optional<Dialog::DriverChoice> showDriverWindow(const Dialog::Choice& choice,
                                                     QWidget* parent) {
    DriverPrinter driver(choice.printer);
    QByteArray settings =
        holdsDevMode(choice.driverSettings) ? choice.driverSettings : driver.defaultSettings();
    if (!holdsDevMode(settings)) {
        return std::nullopt;
    }
    putChoices(*devModeOf(settings), choice);
    const DEVMODEW shown = *devModeOf(settings);
    const HWND window =
        parent != nullptr ? reinterpret_cast<HWND>(parent->window()->winId()) : nullptr;
    QByteArray chosen = driver.showWindow(window, settings);
    if (!holdsDevMode(chosen)) {
        return std::nullopt;
    }
    return driverChoiceOf(std::move(chosen), shown);
}

void PrintSettings::Release::operator()(void* memory) const {
    GlobalFree(static_cast<HGLOBAL>(memory));
}

bool PrintSettings::apply(QPrinter& printer, const Dialog::Choice& choice) {
    if (m_memory != nullptr || printer.outputFormat() != QPrinter::NativeFormat ||
        (choice.driverSettings.isEmpty() && choice.source < 0 && !choice.quality.isValid())) {
        return false;
    }
    QByteArray settings = holdsDevMode(choice.driverSettings)
                              ? choice.driverSettings
                              : DriverPrinter(choice.printer).defaultSettings();
    if (!holdsDevMode(settings)) {
        return false;
    }
    putSourceAndQuality(*devModeOf(settings), choice.source, choice.quality);
    HGLOBAL memory = GlobalAlloc(GHND, static_cast<SIZE_T>(settings.size()));
    if (memory == nullptr) {
        return false;
    }
    void* bytes = GlobalLock(memory);
    if (bytes == nullptr) {
        GlobalFree(memory);
        return false;
    }
    std::memcpy(bytes, settings.constData(), static_cast<size_t>(settings.size()));
    GlobalUnlock(memory);
    m_memory.reset(memory);
    // As QPrintDialog gives the printer the settings of its window: the engine of a printer
    // of the system is a QWin32PrintEngine, which keeps the memory locked while it lives and
    // leaves it to be freed after it
    static_cast<QWin32PrintEngine*>(printer.printEngine())->setGlobalDevMode(nullptr, memory);
    return true;
}

#else

bool hasDriverWindow() {
    return false;
}

void addSourcesAndQualities(Dialog::Printer& /*printer*/) {}

std::optional<Dialog::DriverChoice> showDriverWindow(const Dialog::Choice& /*choice*/,
                                                     QWidget* /*parent*/) {
    return std::nullopt;
}

void PrintSettings::Release::operator()(void* /*memory*/) const {}

bool PrintSettings::apply(QPrinter& /*printer*/, const Dialog::Choice& /*choice*/) {
    return false;
}

#endif

}  // namespace kalahari::gui::printer_driver
