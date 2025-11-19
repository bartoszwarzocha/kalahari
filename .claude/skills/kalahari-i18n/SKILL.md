---
name: kalahari-i18n
description: USE ME when adding translations, using tr() macro, working with .ts/.qm files, QTranslator setup, or multi-language support. Phase 2+ feature (not MVP). Contains Qt i18n + Qt Linguist workflow.
---

# Kalahari i18n/l10n Expert Skill (Qt6)

## Quick Activation Triggers

USE this skill when you see:
- "translation", "i18n", "l10n", "locale", "language"
- "tr() macro", ".ts file", ".qm file", "Qt Linguist"
- "QTranslator", "lupdate", "lrelease"
- "Polish", "German", "Russian", "multi-language"
- "internationalization", "localization"

**NOTE:** i18n is Phase 2+ feature, NOT in MVP (Phase 0-1)

## Critical Patterns (Kalahari-Specific, Qt6)

### 1. QTranslator Setup (Application Init)

```cpp
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <spdlog/spdlog.h>

class KalahariApp : public QApplication {
    Q_OBJECT

public:
    KalahariApp(int &argc, char **argv) : QApplication(argc, argv) {
        setApplicationName("Kalahari");
        setOrganizationName("Kalahari Project");

        // Load translation from settings or system default
        QString locale = QLocale::system().name();  // e.g., "pl_PL", "de_DE"
        loadTranslation(locale);
    }

    void loadTranslation(const QString& locale) {
        // Remove old translator
        if (m_translator) {
            removeTranslator(m_translator);
            delete m_translator;
        }

        // Load new translator
        m_translator = new QTranslator(this);
        QString qmFile = QString("kalahari_%1.qm").arg(locale);

        // Try resource first, then file system
        if (m_translator->load(qmFile, ":/translations")) {
            installTranslator(m_translator);
            spdlog::info("Loaded translation: {}", locale.toStdString());
            emit languageChanged();
        } else {
            spdlog::warn("Translation not found: {}", locale.toStdString());
        }
    }

signals:
    void languageChanged();

private:
    QTranslator* m_translator = nullptr;
};
```

### 2. Marking Strings for Translation

```cpp
#include <QString>

// ✅ Simple translation (Q_OBJECT macro required in class)
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow() {
        setWindowTitle(tr("Kalahari - Writer's IDE"));
    }
};

// ✅ With variables (use arg())
QString userName = "John";
QString welcome = tr("Welcome, %1!").arg(userName);

// ✅ Multiple variables
int wordCount = 1234;
int time = 45;
QString stats = tr("%1 words in %2 minutes").arg(wordCount).arg(time);

// ✅ Plural forms (automatic based on language)
int count = 5;
QString chapters = tr("%n chapter(s) written", "", count);

// ✅ Context-specific (same word, different meanings)
// Context is automatically the class name for tr()
class FileMenu : public QMenu {
    Q_OBJECT
public:
    FileMenu() {
        setTitle(tr("File"));  // Context: "FileMenu"
    }
};

class FileDialog : public QDialog {
    Q_OBJECT
public:
    FileDialog() {
        setWindowTitle(tr("File"));  // Context: "FileDialog"
    }
};

// ✅ Comments for translators (appear in Qt Linguist)
//: This is the main menu bar
menuBar()->addMenu(tr("&File"));

//: %1 is the document title
statusBar()->showMessage(tr("Opened: %1").arg(docTitle));
```

### 3. What NOT to Translate

```cpp
// ❌ DON'T translate technical identifiers
std::string fileExt = "kalahari";  // NOT tr("kalahari")

// ❌ DON'T translate log messages (for developers)
spdlog::info("Plugin loaded: {}", pluginName);  // NOT tr()

// ❌ DON'T translate API keys, URLs
const char* API_URL = "https://api.example.com";  // NOT tr()

// ✅ DO translate user-facing messages
QMessageBox::critical(this, tr("Error"), tr("Failed to load file"));
```

### 4. Translation Workflow (Qt Tools)

**Step 1: Extract strings with lupdate**
```bash
# Update .ts files from C++ sources
lupdate src/*.cpp include/**/*.h -ts translations/kalahari_pl.ts
lupdate src/*.cpp include/**/*.h -ts translations/kalahari_de.ts
lupdate src/*.cpp include/**/*.h -ts translations/kalahari_fr.ts

# With CMake (automatic):
cmake --build . --target lupdate
```

**What lupdate does:**
- Scans C++ files for tr() calls
- Extracts strings + context (class names automatically)
- Updates .ts XML files
- Preserves existing translations
- Marks obsolete strings

**Step 2: Translate with Qt Linguist (Visual Tool)**
```bash
# Open translation file in visual editor
linguist translations/kalahari_pl.ts
```

**Qt Linguist Features:**
- Visual string browser with context
- Translation memory (reuse previous translations)
- Glossary support
- Validation (missing translations, placeholders)
- Fuzzy matching suggestions
- Source code preview

**Step 3: Compile with lrelease**
```bash
# Compile .ts → .qm (binary format for runtime)
lrelease translations/kalahari_pl.ts -qm translations/kalahari_pl.qm

# With CMake (automatic):
cmake --build . --target lrelease
```

**What lrelease does:**
- Compiles .ts XML → .qm binary
- Optimizes for fast runtime loading
- Includes only finished translations
- Warnings for incomplete translations

**Step 4: Embed in Resources**
```xml
<!-- translations/translations.qrc -->
<!DOCTYPE RCC>
<RCC version="1.0">
<qresource prefix="/translations">
    <file>kalahari_pl.qm</file>
    <file>kalahari_de.qm</file>
    <file>kalahari_fr.qm</file>
</qresource>
</RCC>
```

```cmake
# CMakeLists.txt
qt6_add_translation(QM_FILES
    translations/kalahari_pl.ts
    translations/kalahari_de.ts
)

qt6_add_resources(RESOURCES
    translations/translations.qrc
)

add_executable(kalahari ${SOURCES} ${QM_FILES} ${RESOURCES})
```

### 5. Directory Structure (Qt)

```
/mnt/e/Python/Projekty/Kalahari/
├── translations/
│   ├── kalahari_pl.ts          # Polish (editable XML)
│   │   kalahari_pl.qm          # Polish (compiled binary)
│   ├── kalahari_de.ts          # German (editable)
│   │   kalahari_de.qm          # German (compiled)
│   ├── kalahari_fr.ts          # French
│   ├── translations.qrc        # Qt resource file
│   └── ...
└── src/
    └── main.cpp
```

### 6. Plural Forms (Qt Automatic)

**Example .ts file (Polish - 3 forms):**
```xml
<?xml version="1.0" encoding="utf-8"?>
<TS version="2.1" language="pl_PL">
<context>
    <name>MainWindow</name>
    <message numerus="yes">
        <source>%n chapter(s) written</source>
        <translation>
            <numerusform>Napisano %n rozdział</numerusform>    <!-- 1 -->
            <numerusform>Napisano %n rozdziały</numerusform>   <!-- 2-4 -->
            <numerusform>Napisano %n rozdziałów</numerusform>  <!-- 5+ -->
        </translation>
    </message>
</context>
</TS>
```

**Qt handles plural forms automatically:**
- English: 2 forms (1 file, 2 files)
- Polish: 3 forms (1 plik, 2-4 pliki, 5+ plików)
- Russian: 3 forms (different rules)
- Arabic: 6 forms (0, 1, 2, 3-10, 11-99, 100+)

### 7. Runtime Language Switching

```cpp
class LanguageSettingsPanel : public QWidget {
    Q_OBJECT

public:
    LanguageSettingsPanel(QWidget* parent = nullptr) : QWidget(parent) {
        QComboBox* languageCombo = new QComboBox(this);
        languageCombo->addItem(QIcon(":/flags/en.png"), "English", "en");
        languageCombo->addItem(QIcon(":/flags/pl.png"), "Polski", "pl");
        languageCombo->addItem(QIcon(":/flags/de.png"), "Deutsch", "de");

        connect(languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &LanguageSettingsPanel::onLanguageChanged);
    }

private slots:
    void onLanguageChanged(int index) {
        QString locale = languageCombo->itemData(index).toString();

        // Save to settings
        QSettings settings;
        settings.setValue("language", locale);

        // Apply immediately
        qobject_cast<KalahariApp*>(qApp)->loadTranslation(locale);

        // Widgets need to retranslate
        QMessageBox::information(this, tr("Language Changed"),
            tr("Language has been changed. Some elements will update immediately, "
               "others will update when you reopen dialogs."));
    }
};

// Widgets react to language change
class MainWindow : public QMainWindow {
    Q_OBJECT

protected:
    void changeEvent(QEvent* event) override {
        if (event->type() == QEvent::LanguageChange) {
            retranslateUi();
        }
        QMainWindow::changeEvent(event);
    }

private:
    void retranslateUi() {
        setWindowTitle(tr("Kalahari - Writer's IDE"));
        m_fileMenu->setTitle(tr("&File"));
        m_newAction->setText(tr("&New Project..."));
    }
};
```

### 8. Plugin Translation (Python)

```python
# plugin.py
from PyQt6.QtCore import QCoreApplication, QTranslator, QLocale

class MyPlugin:
    def on_load(self, api):
        # Load plugin-specific translations
        self.translator = QTranslator()
        locale = QLocale.system().name()
        qm_file = f"plugin_myplugin_{locale}.qm"

        if self.translator.load(qm_file, ":/plugin/translations"):
            QCoreApplication.instance().installTranslator(self.translator)

        # Use tr() via QCoreApplication
        print(QCoreApplication.translate("MyPlugin", "Plugin loaded successfully"))
```

### 9. Best Practices

**DO:**
- ✅ Use tr() for ALL user-facing strings
- ✅ Extract full sentences, not fragments
- ✅ Test with long translations (German is 30% longer)
- ✅ Use arg() for variables, not QString::sprintf()
- ✅ Add translator comments with //: prefix
- ✅ Implement changeEvent(QEvent::LanguageChange) for dynamic switching

**DON'T:**
- ❌ Concatenate translated strings
- ❌ Translate developer logs (spdlog)
- ❌ Assume word order (varies by language)
- ❌ Hard-code UI dimensions (use layouts)
- ❌ Use _() macro (that's gettext, not Qt!)

## Supported Languages

**MVP (Phase 0-1):**
- 🇬🇧 English (PRIMARY, 100% complete)
- 🇵🇱 Polish (SECONDARY, 100% before release)

**Phase 2+:**
- 🇩🇪 German, 🇷🇺 Russian, 🇫🇷 French, 🇪🇸 Spanish, 🇮🇹 Italian

## Resources

- **Kalahari Docs**: project_docs/09_i18n.md
- **Qt i18n**: https://doc.qt.io/qt-6/internationalization.html
- **Qt Linguist**: https://doc.qt.io/qt-6/qtlinguist-index.html
- **Qt Linguist Manual**: https://doc.qt.io/qt-6/linguist-translators.html

## Qt i18n Functions

- `tr("text")` - Basic translation (class member function)
- `tr("%n item(s)", "", count)` - Plural forms
- `QCoreApplication::translate("Context", "text")` - Global translate
- `QT_TR_NOOP("text")` - Mark for extraction (not translated immediately)

## Qt i18n Tools

- `lupdate` - Extract strings from C++ sources to .ts
- `lrelease` - Compile .ts → .qm
- `linguist` - Visual translation editor (GUI)

## Migration from wxWidgets/gettext

| wxWidgets/gettext | Qt i18n |
|-------------------|---------|
| `_("text")` | `tr("text")` |
| `wxPLURAL("s", "p", n)` | `tr("%n item(s)", "", n)` |
| `wxGETTEXT_IN_CONTEXT(ctx, txt)` | Class name as context (automatic) |
| `wxLocale` | `QTranslator` |
| `.po` / `.mo` | `.ts` / `.qm` |
| `xgettext` | `lupdate` |
| `msgfmt` | `lrelease` |
| Poedit | Qt Linguist |

**Key Differences:**
- Qt: Context is class name (automatic from tr())
- Qt: Plurals handled via single tr() call with %n
- Qt: .ts is XML (human-readable, version-controllable)
- Qt: .qm is optimized binary (fast loading)
- Qt: Integrated CMake workflow (qt6_add_translation)
