# 📝 **Instrukcja użycia skryptów oraz konfiguracji projektu Serengeti**

**Instrukcja** korzystania ze skryptów oraz konfiguracji projektu **Serengeti**. Dokumentacja obejmuje zarządzanie zależnościami, budowanie projektu oraz integrację CI/CD.

---

## 📂 **Struktura i lokalizacja skryptów**
### ✅ **Ostateczne i spójne nazwy oraz lokalizacje skryptów:**
- **W katalogu głównym projektu:**
  - `init_project.py` – Inicjalizacja i budowanie projektu na Windows/Linux/macOS.
- **Pliki konfiguracyjne:**
  - `vcpkg.json` – Lista zależności i ich wersji.
  - `CMakeLists.txt` – Główna konfiguracja projektu.
- **W katalogu `.github/workflows/`:**
  - `ci.yml` – Konfiguracja CI/CD przy użyciu GitHub Actions.

---

## 🐍 **1️⃣ Skrypt Python – `init_project.py`**
### 🎯 **Cel:**  
Automatyczne zarządzanie zależnościami określonymi w `vcpkg.json`, konfigurowanie CMake oraz budowanie projektu.

**Uwaga!** Dla linux upewnij się, że zainstalowane są wszystkie skłądniki wymagane do budowania:
```bash
sudo apt-get install build-essential flex bison cmake ninja-build
```

### ✅ **Funkcje skryptu:**
- Odczytuje plik `vcpkg.json` i analizuje sekcję `dependencies`.
- Sprawdza, które pakiety są już zainstalowane.
- Instaluje brakujące zależności przy użyciu `vcpkg`.
- konfiguruje CMake.
- buduje projekt (VS (Windows), Code::Blocks (Linux), Xcode (macOS)).

### 🚀 **Jak używać:**
```bash
python init_project.py                         # For every platform or with specific triplet...
python init_project.py --triplet x64-windows   # Windows
python init_project.py --triplet x64-linux     # Linux
python init_project.py --triplet x64-osx       # macOS
```

✅ **Wynik:** Zależności pojawią się w katalogu `vcpkg_installed`. Pliki binarne w `build/Release` lub `build/Debug`.

---

## 🚀 **2️⃣ CI/CD – GitHub Actions (`.github/workflows/ci.yml`)**
### 🎯 **Cel:**  
Automatyczne budowanie i testowanie projektu na **Windows**, **Linux** oraz **macOS**.

### 🔍 **Co robi pipeline?**
- Buduje projekt po każdym **push** i **pull request**.
- Instaluje zależności z `vcpkg.json`.
- Kompiluje projekt w trybie **Release**.
- (Opcjonalnie) Uruchamia testy, jeśli są zdefiniowane.

### 🚀 **Jak działa:**
✅ Pipeline uruchamia się automatycznie po przesłaniu kodu do repozytorium.  
🔎 Wyniki znajdziesz w zakładce **Actions** na GitHub.

---

## 🏗️ **Przykładowy przebieg pracy:**
### 🚀 **Deweloper lokalnie:**
1. **Instalacja zależności:**
   ```bash
   python -X utf8 generate_dependencies.py --triplet x64-windows
   ```
2. **Budowa projektu (Windows):**
   ```bash
   init_project_win.bat Release
   ```
3. **Budowa projektu (Linux/macOS):**
   ```bash
   ./init_project_unix.sh Debug
   ```
4. **Przejrzenie wyników w CI/CD:**
   - Po **push** sprawdź zakładkę **Actions** na GitHub.

---

## 📝 **FAQ:**

### ❓ **Gdzie są pliki wykonywalne?**
➡️ Po budowie znajdują się w katalogu: `build/{Release|Debug}`.

### ❓ **Jak rozwiązać błąd z `UnicodeEncodeError` w Python?**
➡️ Uruchom skrypt z opcją `-X utf8` lub ustaw kodowanie w skrypcie.

### ❓ **Dlaczego `hunspell` nie jest wykrywany pomimo instalacji?**
➡️ Jeśli `find_package(hunspell)` zwraca błąd:
1. Sprawdź, czy w `vcpkg_installed` istnieje plik `hunspellConfig.cmake`.  
2. Jeśli plik ma nazwę w formacie `hunspell-<wersja>.lib`, w `CMakeLists.txt` dodaj:
   ```cmake
   find_library(HUNSPELL_LIBRARY NAMES hunspell hunspell-1.7 PATHS ${CMAKE_PREFIX_PATH}/lib)
   ```
3. Dodaj ścieżkę do `CMAKE_PREFIX_PATH`:
   ```cmake
   list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
   ```
➡️ To zapewni, że CMake odnajdzie właściwe pliki konfiguracyjne.

### ❓ **Czy CI/CD wymaga ręcznej interwencji?**
➡️ Nie. Pipeline działa automatycznie po **push** lub **pull request**.

---

