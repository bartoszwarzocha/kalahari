---
name: kalahari-i18n
description: wxWidgets i18n/l10n specialist (wxLocale + gettext, multi-language support)
---

# Kalahari i18n/l10n Expert Skill

## Purpose

This skill provides specialized knowledge for **internationalization (i18n) and localization (l10n)** in Kalahari using wxWidgets wxLocale and GNU gettext. Ensures correct implementation of multi-language support across UI, plugins, and documentation.

## When to Activate

Claude should use this skill when:
- ✅ Implementing wxLocale in C++ code
- ✅ Marking strings for translation with _()
- ✅ Creating .po/.mo translation files
- ✅ Adding new UI languages
- ✅ Localizing plugins
- ✅ Handling plural forms and context-specific translations
- ✅ Testing translations

## Core Competencies

### 1. Kalahari i18n Stack

**Languages (Priority Order):**
1. 🇬🇧 **English** - PRIMARY (must be complete at all times, source language)
2. 🇵🇱 **Polish** - SECONDARY (MVP requirement, 100% complete before release)
3. 🇩🇪 **German** - Phase 2+ (post-MVP)
4. 🇷🇺 **Russian** - Phase 2+
5. 🇫🇷 **French** - Phase 2+
6. 🇪🇸 **Spanish** - Phase 2+

**Technologies:**
- **wxWidgets:** wxLocale + gettext (.po/.mo files)
- **Extraction:** xgettext (GNU gettext tools)
- **Editing:** Poedit, Lokalize, or text editor
- **Runtime:** wxLocale automatic loading of .mo files

**Directory Structure:**
```
/mnt/e/Python/Projekty/Kalahari/
├── locales/                    # All translation files
│   ├── en/                     # English (source)
│   │   └── kalahari.mo         # Compiled catalog
│   ├── pl/                     # Polish
│   │   ├── kalahari.po         # Translation file
│   │   └── kalahari.mo         # Compiled catalog
│   ├── de/                     # German (Phase 2+)
│   ├── ru/                     # Russian (Phase 2+)
│   ├── fr/                     # French (Phase 2+)
│   └── es/                     # Spanish (Phase 2+)
├── kalahari.pot                # Translation template (master)
└── src/                        # Source code with _() markers
```

### 2. wxLocale Setup

**Application Initialization:**
```cpp
/// @file application.cpp
#include <wx/wx.h>
#include <wx/intl.h>

class KalahariApp : public wxApp {
public:
    bool OnInit() override {
        // Initialize locale system
        m_locale = new wxLocale();

        // Set language (from user preference or system default)
        wxLanguage lang = wxLANGUAGE_DEFAULT;  // or wxLANGUAGE_ENGLISH, wxLANGUAGE_POLISH

        // Try to initialize with preferred language
        if (!m_locale->Init(lang)) {
            wxLogWarning(_("Failed to initialize locale"));
            m_locale->Init(wxLANGUAGE_ENGLISH);  // Fallback to English
        }

        // Add catalog lookup path
        m_locale->AddCatalogLookupPathPrefix("locales");

        // Load translation catalog
        if (!m_locale->AddCatalog("kalahari")) {
            wxLogWarning(_("Translation catalog not found"));
        }

        // Create main window
        MainWindow* window = new MainWindow();
        window->Show();

        return true;
    }

    int OnExit() override {
        delete m_locale;
        return 0;
    }

private:
    wxLocale* m_locale;
};
```

**Runtime Language Switching:**
```cpp
void MainWindow::OnLanguageChanged(wxCommandEvent& event) {
    wxLanguage newLang = static_cast<wxLanguage>(event.GetId());

    // Save preference
    wxConfig::Get()->Write("/Language", static_cast<int>(newLang));

    // Inform user to restart
    wxMessageBox(
        _("Language will be changed after restarting the application."),
        _("Language Change"),
        wxOK | wxICON_INFORMATION
    );
}
```

### 3. Marking Strings for Translation

**Basic Translation:**
```cpp
// Simple string
wxString msg = _("Welcome to Kalahari");

// With variables (use wxString::Format)
wxString welcome = wxString::Format(
    _("Welcome, %s!"),
    userName
);

// Multi-line strings
wxString help = _(
    "Kalahari is a powerful writing environment.\n"
    "Press F1 for help."
);
```

**Plural Forms:**
```cpp
// Plural handling
int count = 5;
wxString msg = wxString::Format(
    wxPLURAL(
        "%d chapter written",
        "%d chapters written",
        count
    ),
    count
);
```

**Context-Specific Translations (wxGETTEXT_IN_CONTEXT):**
```cpp
// Same word, different contexts
wxString fileMenu = wxGETTEXT_IN_CONTEXT("menu", "File");
wxString fileType = wxGETTEXT_IN_CONTEXT("document", "File");

// In .po file:
// msgctxt "menu"
// msgid "File"
// msgstr "Plik"  (Polish - menu)
//
// msgctxt "document"
// msgid "File"
// msgstr "Dokument"  (Polish - document type)
```

**What NOT to translate:**
```cpp
// ❌ DON'T translate technical identifiers
std::string fileExt = "kalahari";  // NOT _("kalahari")

// ❌ DON'T translate log messages (for developers)
spdlog::info("Plugin loaded: {}", pluginName);  // NOT _()

// ❌ DON'T translate API keys, URLs, technical constants
const char* API_URL = "https://api.example.com";  // NOT _()

// ✅ DO translate user-facing messages
wxLogError(_("Failed to load file"));
```

### 4. Workflow: Extracting and Compiling Translations

**Step 1: Extract strings from source code**
```bash
# Extract all _() strings to .pot template
xgettext --keyword=_ \
         --keyword=wxPLURAL:1,2 \
         --keyword=wxGETTEXT_IN_CONTEXT:1c,2 \
         --language=C++ \
         --from-code=UTF-8 \
         --output=kalahari.pot \
         --package-name=Kalahari \
         --package-version=1.0 \
         --msgid-bugs-address=support@kalahari.app \
         src/**/*.cpp \
         src/**/*.h

# Result: kalahari.pot (template file)
```

**Step 2: Create/Update translation files**
```bash
# Create new language (Polish)
msginit --input=kalahari.pot \
        --locale=pl_PL \
        --output=locales/pl/kalahari.po

# Update existing translation (merge new strings)
msgmerge --update \
         --backup=none \
         locales/pl/kalahari.po \
         kalahari.pot
```

**Step 3: Translate (manual)**
```
# Edit locales/pl/kalahari.po with Poedit or text editor

# Example .po file content:
#: src/main_window.cpp:42
msgid "Welcome to Kalahari"
msgstr "Witaj w Kalahari"

#: src/main_window.cpp:56
#, c-format
msgid "Welcome, %s!"
msgstr "Witaj, %s!"

#: src/chapter_manager.cpp:123
#, c-format
msgid "%d chapter written"
msgid_plural "%d chapters written"
msgstr[0] "%d rozdział napisany"
msgstr[1] "%d rozdziały napisane"
msgstr[2] "%d rozdziałów napisanych"
```

**Step 4: Compile .po → .mo**
```bash
# Compile Polish translation
msgfmt --output-file=locales/pl/kalahari.mo \
       --check \
       --statistics \
       locales/pl/kalahari.po

# Output: locales/pl/kalahari.mo (binary catalog)
```

**Step 5: Test**
```bash
# Run Kalahari with Polish locale
export LANG=pl_PL.UTF-8
./bin/kalahari

# Or use wxLocale selector in GUI
```

### 5. Plugin i18n

**Plugin Translation Structure:**
```
my-plugin.kplugin/
├── manifest.json
├── plugin.py
└── locales/
    ├── en/
    │   └── my-plugin.mo
    ├── pl/
    │   ├── my-plugin.po
    │   └── my-plugin.mo
    └── my-plugin.pot
```

**Python Plugin Code:**
```python
# plugin.py
import gettext
import os

# Setup translation
locale_dir = os.path.join(os.path.dirname(__file__), 'locales')
lang = os.getenv('LANG', 'en_US').split('_')[0]  # 'en', 'pl', etc.

try:
    translation = gettext.translation('my-plugin', locale_dir, languages=[lang])
    _ = translation.gettext
except FileNotFoundError:
    _ = lambda s: s  # Fallback to English

# Use _() for translatable strings
print(_("Plugin loaded successfully"))
```

**Extracting from Python:**
```bash
# Extract strings from Python plugin
xgettext --keyword=_ \
         --language=Python \
         --from-code=UTF-8 \
         --output=my-plugin/my-plugin.pot \
         my-plugin/**/*.py
```

### 6. Plural Forms

**Different Languages, Different Rules:**

| Language | Plural Forms | Example |
|----------|--------------|---------|
| **English** | 2 forms | 1 file, 2 files |
| **Polish** | 3 forms | 1 plik, 2 pliki, 5 plików |
| **Russian** | 3 forms | 1 файл, 2 файла, 5 файлов |
| **Arabic** | 6 forms | Complex rules |

**Polish Plural Rules (.po header):**
```
"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 "
"|| n%100>=20) ? 1 : 2);\n"
```

**Usage in .po:**
```
msgid "%d file selected"
msgid_plural "%d files selected"
msgstr[0] "%d plik wybrany"       # n == 1
msgstr[1] "%d pliki wybrane"      # n%10 in [2,3,4] && ...
msgstr[2] "%d plików wybranych"   # all other cases
```

### 7. Best Practices

**DO:**
- ✅ Use _() for all user-facing strings
- ✅ Extract full sentences, not fragments
- ✅ Provide context with comments
- ✅ Test with long translations (German often 30% longer)
- ✅ Use wxString::Format() for variables
- ✅ Include translator comments with `/// TRANSLATORS:`

**DON'T:**
- ❌ Concatenate translated strings
- ❌ Translate developer logs
- ❌ Hard-code UI dimensions (text length varies)
- ❌ Assume word order (English ≠ other languages)
- ❌ Use technical jargon without explanation

**Example - Good vs Bad:**
```cpp
// ❌ BAD: String concatenation
wxString msg = _("Error:") + " " + _("File not found");

// ✅ GOOD: Full sentence
wxString msg = _("Error: File not found");

// ❌ BAD: Assumed word order
wxString msg = wxString::Format(
    _("Delete") + " %s?",
    fileName
);

// ✅ GOOD: Variable in natural position
wxString msg = wxString::Format(
    _("Delete %s?"),
    fileName
);

// ❌ BAD: No context
_("Open");  // Menu item? Button? Past tense?

// ✅ GOOD: With context
wxGETTEXT_IN_CONTEXT("menu", "Open");
/// TRANSLATORS: Menu item to open a file
_("Open");
```

### 8. Testing Translations

**Pseudo-Translation (Testing Tool):**
```bash
# Create pseudo-localized version (shows untranslated strings)
msgen --output-file=locales/test/kalahari.po kalahari.pot

# Edit to wrap strings: "Text" → "[ţëẋţ]"
# Untranslated strings will remain unwrapped
```

**Translation Coverage:**
```bash
# Check translation statistics
msgfmt --statistics locales/pl/kalahari.po

# Output example:
# 345 translated messages, 12 fuzzy translations, 3 untranslated messages.
```

**Checklist:**
- [ ] All _() strings extracted to .pot
- [ ] All .po files updated with msgmerge
- [ ] No fuzzy translations in production build
- [ ] All .mo files compiled and deployed
- [ ] Tested with each supported language
- [ ] Long strings tested (German, Russian)
- [ ] Right-to-left tested (Arabic - if supported)

## Resources

- **Official Docs:** project_docs/09_i18n.md
- **wxWidgets i18n:** https://docs.wxwidgets.org/3.3/overview_i18n.html
- **GNU gettext:** https://www.gnu.org/software/gettext/manual/
- **Poedit:** https://poedit.net/ (GUI translation editor)

## Quick Reference

**wxLocale Language Codes:**
- `wxLANGUAGE_ENGLISH` - English
- `wxLANGUAGE_ENGLISH_US` - English (US)
- `wxLANGUAGE_POLISH` - Polish
- `wxLANGUAGE_GERMAN` - German
- `wxLANGUAGE_RUSSIAN` - Russian
- `wxLANGUAGE_FRENCH` - French
- `wxLANGUAGE_SPANISH` - Spanish
- `wxLANGUAGE_DEFAULT` - System default

**Translation Macros:**
- `_("text")` - Basic translation
- `wxPLURAL("singular", "plural", n)` - Plural forms
- `wxGETTEXT_IN_CONTEXT("context", "text")` - Contextual translation
- `wxTRANSLATE("text")` - Mark for extraction, translate later

**Gettext Tools:**
- `xgettext` - Extract strings from source
- `msginit` - Create new .po file
- `msgmerge` - Update .po from .pot
- `msgfmt` - Compile .po → .mo
- `msgcat` - Merge .po files
- `msggrep` - Search in .po files

**File Extensions:**
- `.pot` - Portable Object Template (master file)
- `.po` - Portable Object (translation file, human-readable)
- `.mo` - Machine Object (compiled catalog, binary)

---

**Skill Version:** 1.0
**Last Updated:** 2025-10-26
**Framework Compatibility:** Kalahari Phase 0+
