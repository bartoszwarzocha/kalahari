---
name: kalahari-i18n
description: USE ME when adding translations, using _() macro, working with .po/.mo files, wxLocale setup, or multi-language support. Phase 2+ feature (not MVP). Contains wxLocale + gettext workflow.
---

# Kalahari i18n/l10n Expert Skill

## Quick Activation Triggers

USE this skill when you see:
- "translation", "i18n", "l10n", "locale", "language"
- "_() macro", ".po file", ".mo file", "gettext"
- "wxLocale", "msgfmt", "xgettext", "msgmerge"
- "Polish", "German", "Russian", "multi-language"
- "internationalization", "localization"

**NOTE:** i18n is Phase 2+ feature, NOT in MVP (Phase 0-1)

## Critical Patterns (Kalahari-Specific)

### 1. wxLocale Setup (Application Init)

```cpp
class KalahariApp : public wxApp {
public:
    bool OnInit() override {
        m_locale = new wxLocale();

        // Language from settings or system default
        wxLanguage lang = wxLANGUAGE_DEFAULT;  // or wxLANGUAGE_POLISH

        if (!m_locale->Init(lang)) {
            wxLogWarning(_("Failed to initialize locale"));
            m_locale->Init(wxLANGUAGE_ENGLISH);
        }

        m_locale->AddCatalogLookupPathPrefix("locales");
        if (!m_locale->AddCatalog("kalahari")) {
            wxLogWarning(_("Translation catalog not found"));
        }

        return true;
    }

private:
    wxLocale* m_locale;
};
```

### 2. Marking Strings for Translation

```cpp
// Simple translation
wxString msg = _("Welcome to Kalahari");

// With variables (use wxString::Format)
wxString welcome = wxString::Format(_("Welcome, %s!"), userName);

// Plural forms
int count = 5;
wxString chapters = wxString::Format(
    wxPLURAL("%d chapter written", "%d chapters written", count),
    count
);

// Context-specific (same word, different meanings)
wxString fileMenu = wxGETTEXT_IN_CONTEXT("menu", "File");
wxString fileDoc = wxGETTEXT_IN_CONTEXT("document", "File");
```

### 3. What NOT to Translate

```cpp
// ❌ DON'T translate technical identifiers
std::string fileExt = "kalahari";  // NOT _("kalahari")

// ❌ DON'T translate log messages (for developers)
spdlog::info("Plugin loaded: {}", pluginName);  // NOT _()

// ❌ DON'T translate API keys, URLs
const char* API_URL = "https://api.example.com";  // NOT _()

// ✅ DO translate user-facing messages
wxLogError(_("Failed to load file"));
```

### 4. Translation Workflow

**Step 1: Extract strings**
```bash
xgettext --keyword=_ \
         --keyword=wxPLURAL:1,2 \
         --keyword=wxGETTEXT_IN_CONTEXT:1c,2 \
         --language=C++ \
         --output=kalahari.pot \
         src/**/*.cpp src/**/*.h
```

**Step 2: Create/Update .po**
```bash
# Create new language
msginit --input=kalahari.pot --locale=pl_PL --output=locales/pl/kalahari.po

# Update existing
msgmerge --update locales/pl/kalahari.po kalahari.pot
```

**Step 3: Translate** (edit .po file manually or with Poedit)

**Step 4: Compile**
```bash
msgfmt --output-file=locales/pl/kalahari.mo locales/pl/kalahari.po
```

### 5. Directory Structure

```
/mnt/e/Python/Projekty/Kalahari/
├── locales/
│   ├── en/kalahari.mo          # English
│   ├── pl/kalahari.po          # Polish (editable)
│   │      kalahari.mo          # Polish (compiled)
│   ├── de/                     # German (Phase 2+)
│   ├── ru/                     # Russian (Phase 2+)
│   └── ...
└── kalahari.pot                # Translation template
```

### 6. Plural Forms (Language-Specific)

**English (2 forms):**
```
msgid "%d file"
msgid_plural "%d files"
msgstr[0] "%d file"
msgstr[1] "%d files"
```

**Polish (3 forms):**
```
msgid "%d file"
msgid_plural "%d files"
msgstr[0] "%d plik"      # 1
msgstr[1] "%d pliki"     # 2-4
msgstr[2] "%d plików"    # 5+
```

### 7. Plugin Translation

```python
# plugin.py
import gettext
import os

locale_dir = os.path.join(os.path.dirname(__file__), 'locales')
lang = os.getenv('LANG', 'en_US').split('_')[0]

try:
    translation = gettext.translation('my-plugin', locale_dir, languages=[lang])
    _ = translation.gettext
except FileNotFoundError:
    _ = lambda s: s  # Fallback

print(_("Plugin loaded successfully"))
```

### 8. Best Practices

**DO:**
- ✅ Use _() for ALL user-facing strings
- ✅ Extract full sentences, not fragments
- ✅ Test with long translations (German is 30% longer)
- ✅ Use wxString::Format() for variables

**DON'T:**
- ❌ Concatenate translated strings
- ❌ Translate developer logs
- ❌ Assume word order (varies by language)
- ❌ Hard-code UI dimensions

## Supported Languages

**MVP (Phase 0-1):**
- 🇬🇧 English (PRIMARY, 100% complete)
- 🇵🇱 Polish (SECONDARY, 100% before release)

**Phase 2+:**
- 🇩🇪 German, 🇷🇺 Russian, 🇫🇷 French, 🇪🇸 Spanish

## Resources

- **Kalahari Docs**: project_docs/09_i18n.md
- **wxWidgets i18n**: https://docs.wxwidgets.org/3.3/overview_i18n.html
- **GNU gettext**: https://www.gnu.org/software/gettext/manual/
- **Poedit**: https://poedit.net/

## Translation Macros

- `_("text")` - Basic translation
- `wxPLURAL("singular", "plural", n)` - Plural forms
- `wxGETTEXT_IN_CONTEXT("context", "text")` - Contextual
- `wxTRANSLATE("text")` - Mark for extraction

## Gettext Tools

- `xgettext` - Extract from source
- `msginit` - Create new .po
- `msgmerge` - Update .po from .pot
- `msgfmt` - Compile .po → .mo
