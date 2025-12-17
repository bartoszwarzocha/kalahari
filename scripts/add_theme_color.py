#!/usr/bin/env python3
"""
add_theme_color.py - Automatically adds a new color to the Kalahari theme system.

Usage:
    python scripts/add_theme_color.py <color_name> <dark_value> <light_value> [options]

Parameters:
    color_name:       camelCase name (e.g., "infoPrimary")
    dark_value:       hex color for Dark theme (e.g., "#6B9BD2")
    light_value:      hex color for Light theme (e.g., "#3D6A99")
    --description/-d: description for C++ comments (e.g., "Primary info color")
    --label/-l:       display label in Settings UI (e.g., "Primary Info")
    --add-to-settings/-s: also add to settings dialog UI

Examples:
    # Basic (5 files - theme system only):
    python scripts/add_theme_color.py infoPrimary "#6B9BD2" "#3D6A99" -d "Primary info color"

    # With Settings UI (9 files):
    python scripts/add_theme_color.py infoPrimary "#6B9BD2" "#3D6A99" -d "Primary info color" -l "Primary Info" -s
"""

import argparse
import json
import re
import shutil
import sys
from pathlib import Path
from typing import Optional


def get_project_root() -> Path:
    """Get project root directory (parent of scripts/)."""
    script_dir = Path(__file__).resolve().parent
    return script_dir.parent


def validate_camel_case(name: str) -> bool:
    """Validate that name is in camelCase format."""
    if not name:
        return False
    # Must start with lowercase letter, followed by alphanumeric
    pattern = r'^[a-z][a-zA-Z0-9]*$'
    return bool(re.match(pattern, name))


def validate_hex_color(color: str) -> bool:
    """Validate hex color format (#RRGGBB or #RGB)."""
    if not color:
        return False
    pattern = r'^#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})$'
    return bool(re.match(pattern, color))


def normalize_hex_color(color: str) -> str:
    """Normalize hex color to uppercase #RRGGBB format."""
    color = color.upper()
    if len(color) == 4:  # #RGB -> #RRGGBB
        return f"#{color[1]*2}{color[2]*2}{color[3]*2}"
    return color


def create_backup(file_path: Path) -> Path:
    """Create a backup of the file."""
    backup_path = file_path.with_suffix(file_path.suffix + '.bak')
    shutil.copy2(file_path, backup_path)
    return backup_path


def restore_from_backup(file_path: Path, backup_path: Path) -> None:
    """Restore file from backup."""
    if backup_path.exists():
        shutil.copy2(backup_path, file_path)


def cleanup_backup(backup_path: Path) -> None:
    """Remove backup file."""
    if backup_path.exists():
        backup_path.unlink()


def update_theme_json(file_path: Path, color_name: str, color_value: str) -> bool:
    """Add color to theme JSON file."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        # Add to colors section
        if 'colors' not in data:
            data['colors'] = {}

        # Check if color already exists
        if color_name in data['colors']:
            print(f"[WARN] Color '{color_name}' already exists in {file_path.name}")
            return True

        # Add after "text" entry (last standard color)
        data['colors'][color_name] = color_value

        # Write back with proper formatting
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)

        return True
    except Exception as e:
        print(f"[FAIL] Error updating {file_path.name}: {e}")
        return False


def update_theme_h(file_path: Path, color_name: str, description: str) -> bool:
    """Add color to Theme struct in theme.h."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Check if color already exists
        if f'QColor {color_name};' in content:
            print(f"[WARN] Color '{color_name}' already exists in {file_path.name}")
            return True

        # Generic pattern: find ANY QColor line followed by "} colors;"
        # This works regardless of which color is currently last
        pattern = r'(QColor \w+;\s*///< [^\n]*\n)(\s*\} colors;)'

        # Create the new color line with proper indentation
        comment = f"///< {description}" if description else "///< Custom color"
        new_line = f"        QColor {color_name}; {comment}\n"

        replacement = r'\1' + new_line + r'\2'

        new_content, count = re.subn(pattern, replacement, content)

        if count == 0:
            print(f"[FAIL] Could not find insertion point in {file_path.name}")
            return False

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        return True
    except Exception as e:
        print(f"[FAIL] Error updating {file_path.name}: {e}")
        return False


def update_theme_cpp(file_path: Path, color_name: str, dark_value: str, light_value: str) -> bool:
    """Add color parsing and serialization to theme.cpp."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Check if color already exists
        if f'colors.{color_name}' in content:
            print(f"[WARN] Color '{color_name}' already exists in {file_path.name}")
            return True

        # 1. Add to fromJson() - after infoHeader parsing (or text if infoHeader not present)
        from_json_pattern = r'(theme\.colors\.infoHeader = parseColor\(colors\.value\("infoHeader", "[^"]+"\)\);)'
        if not re.search(from_json_pattern, content):
            from_json_pattern = r'(theme\.colors\.text = parseColor\(colors\.value\("text", "[^"]+"\)\);)'

        new_from_json = (
            f'\\1\n        theme.colors.{color_name} = parseColor('
            f'colors.value("{color_name}", "{dark_value}"));'
        )

        new_content, count1 = re.subn(from_json_pattern, new_from_json, content)

        # 2. Add to toJson() - after infoHeader serialization (or text if not present)
        to_json_pattern = r'(\{"infoHeader", colorToHex\(colors\.infoHeader\)\})'
        if not re.search(to_json_pattern, content):
            to_json_pattern = r'(\{"text", colorToHex\(colors\.text\)\})'

        new_to_json = f'\\1,\n        {{"{color_name}", colorToHex(colors.{color_name})}}'

        new_content, count2 = re.subn(to_json_pattern, new_to_json, new_content)

        if count1 == 0 or count2 == 0:
            print(f"[FAIL] Could not find insertion points in {file_path.name} (fromJson: {count1}, toJson: {count2})")
            return False

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        return True
    except Exception as e:
        print(f"[FAIL] Error updating {file_path.name}: {e}")
        return False


def update_theme_manager_cpp(file_path: Path, color_name: str, dark_value: str, light_value: str) -> bool:
    """Add color handling to theme_manager.cpp."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Check if color already exists
        if f'colors.{color_name}' in content:
            print(f"[WARN] Color '{color_name}' already exists in {file_path.name}")
            return True

        success = True

        # 1. Add to constructor fallback (after infoHeader or text)
        fallback_pattern = r'(m_currentTheme\.colors\.infoHeader = QColor\("[^"]+"\);)'
        if not re.search(fallback_pattern, content):
            fallback_pattern = r'(m_currentTheme\.colors\.text = QColor\("[^"]+"\);)'

        new_fallback = f'\\1\n    m_currentTheme.colors.{color_name} = QColor("{light_value}");'

        new_content, count1 = re.subn(fallback_pattern, new_fallback, content)
        if count1 == 0:
            print(f"[WARN] Could not add fallback in {file_path.name}")
            success = False

        # 2. Add to applyColorOverrides() - after infoHeader or text handling
        override_pattern = r'(\} else if \(key == "infoHeader"\) \{\n\s*m_currentTheme\.colors\.infoHeader = color;\n\s*\})'
        if not re.search(override_pattern, content):
            override_pattern = r'(\} else if \(key == "text"\) \{\n\s*m_currentTheme\.colors\.text = color;\n\s*\})'

        new_override = (
            f'\\1 else if (key == "{color_name}") {{\n'
            f'            m_currentTheme.colors.{color_name} = color;\n'
            f'        }}'
        )

        new_content, count2 = re.subn(override_pattern, new_override, new_content)
        if count2 == 0:
            print(f"[WARN] Could not add applyColorOverrides handler in {file_path.name}")

        # 3. Add to setColorOverride() - after infoHeader or text handling
        set_override_pattern = r'(\} else if \(key == "infoHeader" \|\| key == "colors\.infoHeader"\) \{\n\s*m_currentTheme\.colors\.infoHeader = color;\n\s*\})'
        if not re.search(set_override_pattern, content):
            set_override_pattern = r'(\} else if \(key == "text" \|\| key == "colors\.text"\) \{\n\s*m_currentTheme\.colors\.text = color;\n\s*\})'

        new_set_override = (
            f'\\1\n    // Custom color: {color_name}\n'
            f'    else if (key == "{color_name}" || key == "colors.{color_name}") {{\n'
            f'        m_currentTheme.colors.{color_name} = color;\n'
            f'    }}'
        )

        new_content, count3 = re.subn(set_override_pattern, new_set_override, new_content)
        if count3 == 0:
            print(f"[WARN] Could not add setColorOverride handler in {file_path.name}")
            success = False

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        return success
    except Exception as e:
        print(f"[FAIL] Error updating {file_path.name}: {e}")
        return False


def update_settings_data_h(file_path: Path, color_name: str, description: str) -> bool:
    """Add color to SettingsData struct (in UI Colors section)."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Check if color already exists
        member_name = f"{color_name}Color"
        if f'QColor {member_name};' in content:
            print(f"[WARN] Color '{member_name}' already exists in {file_path.name}")
            return True

        # Find insertion point after infoHeaderColor (UI Colors section)
        pattern = r'(QColor infoHeaderColor;\s*///< [^\n]+)'

        comment = f"///< {description}" if description else "///< Custom UI color"
        new_line = f"\\1\n    QColor {member_name};        {comment}"

        new_content, count = re.subn(pattern, new_line, content)

        if count == 0:
            print(f"[FAIL] Could not find insertion point in {file_path.name}")
            return False

        # Also need to add to requiresVisualRefresh and operator!=
        # Add to requiresVisualRefresh comparison - after infoHeaderColor
        refresh_pattern = r'(infoHeaderColor != other\.infoHeaderColor \|\|)'
        refresh_new = f'\\1\n               {member_name} != other.{member_name} ||'
        new_content, _ = re.subn(refresh_pattern, refresh_new, new_content)

        # Add to operator!= comparison - after infoHeaderColor
        neq_pattern = r'(infoHeaderColor != other\.infoHeaderColor \|\|\n)'
        neq_new = f'\\1               {member_name} != other.{member_name} ||\n'
        new_content, _ = re.subn(neq_pattern, neq_new, new_content)

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        return True
    except Exception as e:
        print(f"[FAIL] Error updating {file_path.name}: {e}")
        return False


def update_settings_dialog_h(file_path: Path, color_name: str) -> bool:
    """Add ColorConfigWidget member to settings_dialog.h (UI Colors section)."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        widget_name = f"m_{color_name}ColorWidget"
        if widget_name in content:
            print(f"[WARN] Widget '{widget_name}' already exists in {file_path.name}")
            return True

        # Find insertion point after m_infoHeaderColorWidget
        pattern = r'(ColorConfigWidget\* m_infoHeaderColorWidget;)'

        new_line = f"\\1\n    ColorConfigWidget* {widget_name};"

        new_content, count = re.subn(pattern, new_line, content)

        if count == 0:
            print(f"[FAIL] Could not find insertion point in {file_path.name}")
            return False

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        return True
    except Exception as e:
        print(f"[FAIL] Error updating {file_path.name}: {e}")
        return False


def update_main_window_cpp(file_path: Path, color_name: str) -> bool:
    """Add color to collectCurrentSettings() in main_window.cpp."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        member_name = f"{color_name}Color"

        # Check if color already exists
        if f'settingsData.{member_name}' in content:
            print(f"[WARN] Color '{member_name}' already exists in {file_path.name}")
            return True

        # Find insertion point after infoHeaderColor (theme is already defined there)
        pattern = r'(settingsData\.infoHeaderColor = theme\.colors\.infoHeader;)'

        # Just add new line - theme is already defined
        new_line = f'\\1\n    settingsData.{member_name} = theme.colors.{color_name};'

        new_content, count = re.subn(pattern, new_line, content)

        if count == 0:
            print(f"[FAIL] Could not find insertion point in {file_path.name}")
            return False

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        return True
    except Exception as e:
        print(f"[FAIL] Error updating {file_path.name}: {e}")
        return False


def update_settings_dialog_cpp(file_path: Path, color_name: str, description: str,
                               dark_value: str, light_value: str, label: str = '') -> bool:
    """Add color widget to settings_dialog.cpp (in UI Colors group)."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        widget_name = f"m_{color_name}ColorWidget"
        member_name = f"{color_name}Color"

        if widget_name in content:
            print(f"[WARN] Widget '{widget_name}' already exists in {file_path.name}")
            return True

        # 1. Initialize in constructor - after m_infoHeaderColorWidget(nullptr) or m_brightTextColorWidget
        init_pattern = r'(, m_infoHeaderColorWidget\(nullptr\))'
        if not re.search(init_pattern, content):
            init_pattern = r'(, m_brightTextColorWidget\(nullptr\))'
        init_new = f"\\1\n    , {widget_name}(nullptr)"
        new_content, count1 = re.subn(init_pattern, init_new, content)

        # 2. Create widget in UI Colors group - after m_infoHeaderColorWidget
        # Find pattern in uiColorsGroup (NOT iconColorsGroup!)
        create_pattern = r'(m_infoHeaderColorWidget = new ColorConfigWidget\(tr\("Info Header"\), uiColorsGroup\);\n\s*m_infoHeaderColorWidget->setToolTip\(tr\("[^"]+"\)\);\n\s*uiColorsLayout->addWidget\(m_infoHeaderColorWidget\);)'

        # Use provided label, or fall back to formatted color_name
        display_label = label if label else color_name.replace('_', ' ').title()
        tooltip = description if description else f"{display_label} color"

        create_new = (
            f'\\1\n\n    {widget_name} = new ColorConfigWidget(tr("{display_label}"), uiColorsGroup);\n'
            f'    {widget_name}->setToolTip(tr("{tooltip}"));\n'
            f'    uiColorsLayout->addWidget({widget_name});'
        )

        new_content, count2 = re.subn(create_pattern, create_new, new_content)

        # 3. Populate from settings - after infoHeaderColor
        populate_pattern = r'(m_infoHeaderColorWidget->setColor\(settings\.infoHeaderColor\);)'
        populate_new = f"\\1\n    {widget_name}->setColor(settings.{member_name});"
        new_content, count3 = re.subn(populate_pattern, populate_new, new_content)

        # 4. Collect to settings - after infoHeaderColor
        collect_pattern = r'(settingsData\.infoHeaderColor = m_infoHeaderColorWidget->color\(\);)'
        collect_new = f"\\1\n    settingsData.{member_name} = {widget_name}->color();"
        new_content, count4 = re.subn(collect_pattern, collect_new, new_content)

        # 5. Add default value in onThemeComboChanged() - after defaultInfoHeader
        default_pattern = r'(std::string defaultInfoHeader = isDark \? "[^"]+" : "[^"]+";)'

        default_var = f"default{color_name[0].upper()}{color_name[1:]}"
        default_new = f"\\1\n    // {description if description else color_name}\n    std::string {default_var} = isDark ? \"{dark_value}\" : \"{light_value}\";"
        new_content, count5 = re.subn(default_pattern, default_new, new_content)

        # 6. Add widget color setting in onThemeComboChanged() - after infoHeader widget
        widget_set_pattern = r'(m_infoHeaderColorWidget->setColor\(QColor\(QString::fromStdString\(defaultInfoHeader\)\)\);)'

        widget_set_new = f"\\1\n    {widget_name}->setColor(QColor(QString::fromStdString({default_var})));"
        new_content, count6 = re.subn(widget_set_pattern, widget_set_new, new_content)

        if count1 == 0 or count2 == 0:
            print(f"[WARN] Some insertions failed in {file_path.name} "
                  f"(init: {count1}, create: {count2}, populate: {count3}, collect: {count4}, "
                  f"default: {count5}, themeSwitch: {count6})")

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        return count1 > 0 and count2 > 0
    except Exception as e:
        print(f"[FAIL] Error updating {file_path.name}: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description='Add a new color to the Kalahari theme system.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('color_name', help='camelCase color name (e.g., htmlHeading)')
    parser.add_argument('dark_value', help='Hex color for Dark theme (e.g., #1E3A5F)')
    parser.add_argument('light_value', help='Hex color for Light theme (e.g., #2B5278)')
    parser.add_argument('--description', '-d', default='', help='Description for C++ comments')
    parser.add_argument('--label', '-l', default='', help='Display label in settings UI (e.g., "Primary Info")')
    parser.add_argument('--add-to-settings', '-s', action='store_true',
                        help='Also add to settings dialog UI')

    args = parser.parse_args()

    # Validate inputs
    if not validate_camel_case(args.color_name):
        print(f"[FAIL] Invalid color name '{args.color_name}'. Must be camelCase (e.g., htmlHeading)")
        sys.exit(1)

    if not validate_hex_color(args.dark_value):
        print(f"[FAIL] Invalid dark color '{args.dark_value}'. Must be hex format (#RRGGBB or #RGB)")
        sys.exit(1)

    if not validate_hex_color(args.light_value):
        print(f"[FAIL] Invalid light color '{args.light_value}'. Must be hex format (#RRGGBB or #RGB)")
        sys.exit(1)

    # Normalize colors
    dark_value = normalize_hex_color(args.dark_value)
    light_value = normalize_hex_color(args.light_value)

    # Get project root
    project_root = get_project_root()

    # Define file paths
    files = {
        'dark_json': project_root / 'resources' / 'themes' / 'Dark.json',
        'light_json': project_root / 'resources' / 'themes' / 'Light.json',
        'theme_h': project_root / 'include' / 'kalahari' / 'core' / 'theme.h',
        'theme_cpp': project_root / 'src' / 'core' / 'theme.cpp',
        'theme_manager_cpp': project_root / 'src' / 'core' / 'theme_manager.cpp',
    }

    if args.add_to_settings:
        files.update({
            'settings_data_h': project_root / 'include' / 'kalahari' / 'gui' / 'settings_data.h',
            'settings_dialog_h': project_root / 'include' / 'kalahari' / 'gui' / 'settings_dialog.h',
            'settings_dialog_cpp': project_root / 'src' / 'gui' / 'settings_dialog.cpp',
            'main_window_cpp': project_root / 'src' / 'gui' / 'main_window.cpp',
        })

    # Verify all files exist
    for name, path in files.items():
        if not path.exists():
            print(f"[FAIL] File not found: {path}")
            sys.exit(1)

    print(f"Adding color '{args.color_name}' to theme system...")
    print(f"  Dark theme:  {dark_value}")
    print(f"  Light theme: {light_value}")
    if args.description:
        print(f"  Description: {args.description}")
    print()

    # Create backups
    backups = {}
    for name, path in files.items():
        backups[name] = create_backup(path)

    # Track success
    success_count = 0
    total_count = 0
    results = {}

    try:
        # Update theme JSON files
        total_count += 1
        if update_theme_json(files['dark_json'], args.color_name, dark_value):
            print(f"[OK] Updated {files['dark_json'].relative_to(project_root)}")
            success_count += 1
            results['dark_json'] = True
        else:
            results['dark_json'] = False

        total_count += 1
        if update_theme_json(files['light_json'], args.color_name, light_value):
            print(f"[OK] Updated {files['light_json'].relative_to(project_root)}")
            success_count += 1
            results['light_json'] = True
        else:
            results['light_json'] = False

        # Update theme.h
        total_count += 1
        if update_theme_h(files['theme_h'], args.color_name, args.description):
            print(f"[OK] Updated {files['theme_h'].relative_to(project_root)}")
            success_count += 1
            results['theme_h'] = True
        else:
            results['theme_h'] = False

        # Update theme.cpp
        total_count += 1
        if update_theme_cpp(files['theme_cpp'], args.color_name, dark_value, light_value):
            print(f"[OK] Updated {files['theme_cpp'].relative_to(project_root)}")
            success_count += 1
            results['theme_cpp'] = True
        else:
            results['theme_cpp'] = False

        # Update theme_manager.cpp
        total_count += 1
        if update_theme_manager_cpp(files['theme_manager_cpp'], args.color_name, dark_value, light_value):
            print(f"[OK] Updated {files['theme_manager_cpp'].relative_to(project_root)}")
            success_count += 1
            results['theme_manager_cpp'] = True
        else:
            results['theme_manager_cpp'] = False

        # Update settings dialog files if requested
        if args.add_to_settings:
            total_count += 1
            if update_settings_data_h(files['settings_data_h'], args.color_name, args.description):
                print(f"[OK] Updated {files['settings_data_h'].relative_to(project_root)}")
                success_count += 1
                results['settings_data_h'] = True
            else:
                results['settings_data_h'] = False

            total_count += 1
            if update_settings_dialog_h(files['settings_dialog_h'], args.color_name):
                print(f"[OK] Updated {files['settings_dialog_h'].relative_to(project_root)}")
                success_count += 1
                results['settings_dialog_h'] = True
            else:
                results['settings_dialog_h'] = False

            total_count += 1
            if update_settings_dialog_cpp(files['settings_dialog_cpp'], args.color_name, args.description,
                                          dark_value, light_value, args.label):
                print(f"[OK] Updated {files['settings_dialog_cpp'].relative_to(project_root)}")
                success_count += 1
                results['settings_dialog_cpp'] = True
            else:
                results['settings_dialog_cpp'] = False

            total_count += 1
            if update_main_window_cpp(files['main_window_cpp'], args.color_name):
                print(f"[OK] Updated {files['main_window_cpp'].relative_to(project_root)}")
                success_count += 1
                results['main_window_cpp'] = True
            else:
                results['main_window_cpp'] = False

        # Final status
        print()
        if success_count == total_count:
            print(f"Done! Added color '{args.color_name}' to {success_count} files.")
            # Clean up backups on success
            for backup_path in backups.values():
                cleanup_backup(backup_path)
        else:
            print(f"Completed with warnings. Updated {success_count}/{total_count} files.")
            print("Backup files (.bak) preserved for manual review.")

    except Exception as e:
        print(f"\n[FAIL] Unexpected error: {e}")
        print("Restoring from backups...")
        for name, path in files.items():
            if name in backups:
                restore_from_backup(path, backups[name])
        print("Files restored. Please check the error and try again.")
        sys.exit(1)


if __name__ == '__main__':
    main()
