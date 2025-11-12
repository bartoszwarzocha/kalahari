/// @file command.cpp
/// @brief Command Registry data structures implementation

#include "kalahari/gui/command.h"
#include <wx/image.h>
#include <wx/filename.h>
#include <cctype>
#include <algorithm>

namespace kalahari {
namespace gui {

// ============================================================================
// IconSet Implementation
// ============================================================================

IconSet::IconSet(const wxString& path) {
    // Load image from file
    wxImage image(path);
    if (!image.IsOk()) {
        return; // Failed to load, all bitmaps remain invalid
    }

    // Create 3 scaled versions
    // Note: wxImage::Scale() handles aspect ratio and smoothing
    icon16 = wxBitmap(image.Scale(16, 16, wxIMAGE_QUALITY_HIGH));
    icon24 = wxBitmap(image.Scale(24, 24, wxIMAGE_QUALITY_HIGH));
    icon32 = wxBitmap(image.Scale(32, 32, wxIMAGE_QUALITY_HIGH));
}

// ============================================================================
// KeyboardShortcut Implementation
// ============================================================================

wxString KeyboardShortcut::toString() const {
    if (isEmpty()) {
        return wxEmptyString;
    }

    wxString result;

    // Add modifiers
    if (ctrl) {
        result += "Ctrl+";
    }
    if (alt) {
        result += "Alt+";
    }
    if (shift) {
        result += "Shift+";
    }

    // Add key
    // Special keys (F1-F12, WXK_* constants)
    if (keyCode >= WXK_F1 && keyCode <= WXK_F12) {
        result += wxString::Format("F%d", keyCode - WXK_F1 + 1);
    }
    else if (keyCode == WXK_RETURN) {
        result += "Enter";
    }
    else if (keyCode == WXK_ESCAPE) {
        result += "Esc";
    }
    else if (keyCode == WXK_TAB) {
        result += "Tab";
    }
    else if (keyCode == WXK_BACK) {
        result += "Backspace";
    }
    else if (keyCode == WXK_DELETE) {
        result += "Delete";
    }
    else if (keyCode == WXK_INSERT) {
        result += "Insert";
    }
    else if (keyCode == WXK_HOME) {
        result += "Home";
    }
    else if (keyCode == WXK_END) {
        result += "End";
    }
    else if (keyCode == WXK_PAGEUP) {
        result += "PageUp";
    }
    else if (keyCode == WXK_PAGEDOWN) {
        result += "PageDown";
    }
    else if (keyCode == WXK_UP) {
        result += "Up";
    }
    else if (keyCode == WXK_DOWN) {
        result += "Down";
    }
    else if (keyCode == WXK_LEFT) {
        result += "Left";
    }
    else if (keyCode == WXK_RIGHT) {
        result += "Right";
    }
    else if (keyCode >= 32 && keyCode < 127) {
        // Printable ASCII (convert to uppercase for display)
        result += wxString::Format("%c", std::toupper(keyCode));
    }
    else {
        // Unknown key code - show as number
        result += wxString::Format("Key%d", keyCode);
    }

    return result;
}

KeyboardShortcut KeyboardShortcut::fromString(const wxString& str) {
    KeyboardShortcut result;

    if (str.IsEmpty()) {
        return result;
    }

    // Split by '+' separator
    wxArrayString parts = wxSplit(str, '+');
    if (parts.IsEmpty()) {
        return result;
    }

    // Parse modifiers and key
    for (size_t i = 0; i < parts.size(); ++i) {
        wxString part = parts[i].Trim(true).Trim(false).Lower();

        if (part == "ctrl" || part == "cmd") {
            result.ctrl = true;
        }
        else if (part == "alt") {
            result.alt = true;
        }
        else if (part == "shift") {
            result.shift = true;
        }
        else if (i == parts.size() - 1) {
            // Last part is the key

            // Special keys
            if (part.StartsWith("f") && part.Length() > 1) {
                // F1-F12
                long fNum;
                if (part.Mid(1).ToLong(&fNum) && fNum >= 1 && fNum <= 12) {
                    result.keyCode = WXK_F1 + (fNum - 1);
                }
            }
            else if (part == "enter" || part == "return") {
                result.keyCode = WXK_RETURN;
            }
            else if (part == "esc" || part == "escape") {
                result.keyCode = WXK_ESCAPE;
            }
            else if (part == "tab") {
                result.keyCode = WXK_TAB;
            }
            else if (part == "backspace") {
                result.keyCode = WXK_BACK;
            }
            else if (part == "delete" || part == "del") {
                result.keyCode = WXK_DELETE;
            }
            else if (part == "insert" || part == "ins") {
                result.keyCode = WXK_INSERT;
            }
            else if (part == "home") {
                result.keyCode = WXK_HOME;
            }
            else if (part == "end") {
                result.keyCode = WXK_END;
            }
            else if (part == "pageup" || part == "pgup") {
                result.keyCode = WXK_PAGEUP;
            }
            else if (part == "pagedown" || part == "pgdn") {
                result.keyCode = WXK_PAGEDOWN;
            }
            else if (part == "up") {
                result.keyCode = WXK_UP;
            }
            else if (part == "down") {
                result.keyCode = WXK_DOWN;
            }
            else if (part == "left") {
                result.keyCode = WXK_LEFT;
            }
            else if (part == "right") {
                result.keyCode = WXK_RIGHT;
            }
            else if (part.Length() == 1) {
                // Single character (A-Z, 0-9, etc.)
                result.keyCode = std::toupper(part[0].GetValue());
            }
        }
    }

    return result;
}

} // namespace gui
} // namespace kalahari
