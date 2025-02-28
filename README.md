# 📝 **Usage Guide for Scripts and Project Configuration – Kalahari**  

**Guide** for using scripts and configuring the **Kalahari** project. The documentation covers dependency management, project building, and CI/CD integration.

---

## 📂 **Script Structure and Location**  
### ✅ **Final and Consistent Script Names and Locations:**  
- **In the project's root directory:**  
  - `init_project.py` – Initializes and builds the project on Windows/Linux/macOS.  
- **Configuration files:**  
  - `vcpkg.json` – List of dependencies and their versions.  
  - `CMakeLists.txt` – Main project configuration.  
- **In the `.github/workflows/` directory:**  
  - `ci.yml` – CI/CD configuration using GitHub Actions.  

---

## 🐍 **1️⃣ Python Script – `init_project.py`**  
### 🎯 **Purpose:**  
Automatically manage dependencies specified in `vcpkg.json`, configure CMake, and build the project.  

**Note!** For Linux, ensure all required build components are installed:  
```bash
sudo apt-get install build-essential flex bison cmake ninja-build
```

### ✅ **Script Functions:**  
- Reads the `vcpkg.json` file and analyzes the `dependencies` section.  
- Checks which packages are already installed.  
- Installs missing dependencies using `vcpkg`.  
- Configures CMake.  
- Builds the project (VS for Windows, Code::Blocks for Linux, Xcode for macOS).  

### 🚀 **How to Use:**  
```bash
python init_project.py                         # For every platform or with a specific triplet...
python init_project.py --triplet x64-windows   # Windows
python init_project.py --triplet x64-linux     # Linux
python init_project.py --triplet x64-osx       # macOS
```

💪 **Result:** Dependencies will appear in the `vcpkg_installed` directory. Binaries will be in `build/Release` or `build/Debug`.

---

## 🚀 **2️⃣ CI/CD – GitHub Actions (`.github/workflows/ci.yml`)**  
### 🎯 **Purpose:**  
Automatically build and test the project on **Windows**, **Linux**, and **macOS**.  

### 🔍 **What Does the Pipeline Do?**  
- Builds the project after each **push** and **pull request**.  
- Installs dependencies from `vcpkg.json`.  
- Compiles the project in **Release** mode.  
- (Optional) Runs tests if defined.  

### 🚀 **How It Works:**  
💪 The pipeline runs automatically after pushing code to the repository.  
🔎 Results can be found in the **Actions** tab on GitHub.  

---

## 🏗️ **Example Workflow:**  
### 🚀 **Developer Locally:**  
1. **Installing dependencies:**  
   ```bash
   python -X utf8 generate_dependencies.py --triplet x64-windows
   ```
2. **Building the project (Windows):**  
   ```bash
   init_project_win.bat Release
   ```
3. **Building the project (Linux/macOS):**  
   ```bash
   ./init_project_unix.sh Debug
   ```
4. **Checking CI/CD results:**  
   - After **push**, check the **Actions** tab on GitHub.  

---

## 📝 **FAQ:**  

### ❓ **Where are the executable files located?**  
🤞 After the build, they can be found in: `build/{Release|Debug}`.  

### ❓ **How to fix `UnicodeEncodeError` in Python?**  
🤞 Run the script with the `-X utf8` option or set encoding in the script.  

### ❓ **Why is `hunspell` not detected even after installation?**  
🤞 If `find_package(hunspell)` returns an error:  
1. Check if the `hunspellConfig.cmake` file exists in `vcpkg_installed`.  
2. If the file is named `hunspell-<version>.lib`, add the following in `CMakeLists.txt`:  
   ```cmake
   find_library(HUNSPELL_LIBRARY NAMES hunspell hunspell-1.7 PATHS ${CMAKE_PREFIX_PATH}/lib)
   ```
3. Add the path to `CMAKE_PREFIX_PATH`:  
   ```cmake
   list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
   ```
🤞 This ensures that CMake finds the correct configuration files.  

### ❓ **Does CI/CD require manual intervention?**  
🤞 No. The pipeline runs automatically after a **push** or **pull request**.  

---