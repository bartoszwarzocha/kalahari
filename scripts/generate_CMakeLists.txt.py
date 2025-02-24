import os
import json
import platform
from pathlib import Path, PurePosixPath

# === CONFIGURATION ===
DEFAULT_TRIPLETS = {
    "Windows": "x64-windows",
    "Linux": "x64-linux",
    "Darwin": "x64-osx"
}

# Funkcja pomocnicza do konwersji ścieżek na format POSIX
def to_posix_path(path: Path) -> str:
    return path.as_posix()

# Ścieżki bazowe
BASE_DIR = Path(__file__).resolve().parent.parent
VCPKG_JSON_PATH = BASE_DIR / "vcpkg.json"
OUTPUT_CMAKE_PATH = BASE_DIR / "CMakeLists.txt"

CMAKE_TEMPLATE = r'''
cmake_minimum_required(VERSION 3.21)
project(Serengeti LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(DEFINED ENV{{VCPKG_ROOT}})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{{VCPKG_ROOT}}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${{CMAKE_SOURCE_DIR}}/src/*.cpp" "${{CMAKE_SOURCE_DIR}}/include/*.hpp" "${{CMAKE_SOURCE_DIR}}/include/*.h")

add_executable(${{PROJECT_NAME}} ${{SRC_FILES}})

target_include_directories(${{PROJECT_NAME}} PRIVATE {include_dirs})
target_link_directories(${{PROJECT_NAME}} PRIVATE {lib_dirs})

{link_libraries}

set_target_properties(${{PROJECT_NAME}} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/lib"
)

message(STATUS "Project configured successfully.")
'''

def load_vcpkg_dependencies():
    if not VCPKG_JSON_PATH.exists():
        print(f"vcpkg.json not found at {to_posix_path(VCPKG_JSON_PATH)}.")
        return {}
    with VCPKG_JSON_PATH.open("r", encoding="utf-8") as file:
        data = json.load(file)
    return {dep.get("name").lower(): dep.get("features", []) for dep in data.get("dependencies", [])}

def find_include_dirs(base_dir: Path, patterns: list):
    return {to_posix_path(path.parent.resolve()) for pattern in patterns for path in base_dir.rglob(pattern)}

def find_libraries(lib_dir: Path, dep_name: str, components: list, is_debug: bool):
    libraries = []
    for lib in lib_dir.glob("*.lib"):
        lib_name = lib.stem.lower()
        matches_component = not components or any(comp in lib_name for comp in components)
        is_library_debug = "-gd" in lib_name or ("wx" in lib_name and "ud" in lib_name) or lib_name.endswith("d")

        dep_name_ = dep_name.replace("-", "_")
        if (dep_name in lib_name or dep_name_ in lib_name) and matches_component:
            if is_debug and is_library_debug:
                libraries.append(lib.stem)
            elif not is_debug and not is_library_debug:
                libraries.append(lib.stem)
    return libraries

def process_library(dep_name: str, components: list, release_lib_dir: Path, debug_lib_dir: Path):
    release_libs = find_libraries(release_lib_dir, dep_name, components, False)
    debug_libs = find_libraries(debug_lib_dir, dep_name, components, True)

    if not debug_libs:
        debug_libs = release_libs.copy()
    if not release_libs:
        release_libs = debug_libs.copy()

    return release_libs, debug_libs

def process_wxwidgets(release_lib_dir: Path, debug_lib_dir: Path, include_dir: Path, components: list):
    include_dirs = find_include_dirs(include_dir, ["wx/wx.h", "wx/setup.h"])

    for platform_dir in (release_lib_dir.parent / "lib").glob("*"):
        if platform_dir.is_dir() and any(tag in platform_dir.name.lower() for tag in ["msw", "gtk", "osx"]):
            include_dirs.add(to_posix_path(platform_dir.resolve()))

    release_libs = find_libraries(release_lib_dir, "wx", components, False)
    debug_libs = find_libraries(debug_lib_dir, "wx", components, True)

    if not debug_libs:
        debug_libs = release_libs.copy()
    if not release_libs:
        release_libs = debug_libs.copy()

    return release_libs, debug_libs, include_dirs

def scan_vcpkg_folders(vcpkg_installed_dir: Path, dependencies: dict):
    include_dirs, lib_dirs, link_commands = set(), set(), []

    include_path = vcpkg_installed_dir / "include"
    release_lib_dir = vcpkg_installed_dir / "lib"
    debug_lib_dir = vcpkg_installed_dir / "debug" / "lib"

    if include_path.exists():
        include_dirs.add(to_posix_path(include_path.resolve()))

    lib_dirs.update({to_posix_path(release_lib_dir.resolve()), to_posix_path(debug_lib_dir.resolve())})

    for dep_name, components in dependencies.items():
        dep_include_dir = include_path / dep_name
        if dep_include_dir.exists():
            include_dirs.add(to_posix_path(dep_include_dir.resolve()))

        if dep_name == "wxwidgets":
            release_libs, debug_libs, wx_includes = process_wxwidgets(release_lib_dir, debug_lib_dir, include_path, components)
            include_dirs.update(wx_includes)
        else:
            release_libs, debug_libs = process_library(dep_name, components, release_lib_dir, debug_lib_dir)

        link_commands.append(
            f"target_link_libraries(${{PROJECT_NAME}} PRIVATE\n"
            f"    $<$<CONFIG:Release>: {' '.join(release_libs)}>\n"
            f"    $<$<CONFIG:Debug>: {' '.join(debug_libs)}>\n)"
        )

    return include_dirs, lib_dirs, "\n".join(link_commands)

def generate_cmakelists(include_dirs, lib_dirs, link_libraries):
    try:
        cmake_content = CMAKE_TEMPLATE.format(
            include_dirs=" ".join(f'"{d}"' for d in sorted(include_dirs)),
            lib_dirs=" ".join(f'"{d}"' for d in sorted(lib_dirs)),
            link_libraries=link_libraries
        )
        with OUTPUT_CMAKE_PATH.open("w", encoding="utf-8") as file:
            file.write(cmake_content)
        print(f"CMakeLists.txt generated at {to_posix_path(OUTPUT_CMAKE_PATH)}")
    except KeyError as e:
        print(f"Template formatting error: {e}. Check placeholders in CMAKE_TEMPLATE.")

def process():
    triplet = DEFAULT_TRIPLETS.get(platform.system(), "x64-linux")

    local_dir = BASE_DIR / "vcpkg_installed" / triplet
    global_dir = Path(os.getenv("VCPKG_ROOT", "D:/vcpkg" if platform.system() == "Windows" else "/usr/local/vcpkg")) / "installed" / triplet
    vcpkg_dir = local_dir if local_dir.exists() else global_dir

    if not vcpkg_dir.exists():
        print(f"No vcpkg installation found for triplet '{triplet}'.")
        return

    dependencies = load_vcpkg_dependencies()
    if not dependencies:
        print("No dependencies found in vcpkg.json.")
        return

    include_dirs, lib_dirs, link_libraries = scan_vcpkg_folders(vcpkg_dir, dependencies)
    generate_cmakelists(include_dirs, lib_dirs, link_libraries)

def main():
    process()

if __name__ == "__main__":
    main()
