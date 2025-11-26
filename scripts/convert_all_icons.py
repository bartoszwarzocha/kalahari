#!/usr/bin/env python3
"""
Convert filled SVG icons to use color placeholders.

This script processes SVG files in the filled icons directory and adds
{COLOR_PRIMARY} placeholder to path elements that don't have fill="none".

Usage:
    python scripts/convert_filled_icons.py
"""

import os
import re
from pathlib import Path


def convert_svg_file(filepath: Path) -> bool:
    """
    Convert a single SVG file to use color placeholders.

    Args:
        filepath: Path to the SVG file

    Returns:
        True if file was modified, False otherwise
    """
    try:
        content = filepath.read_text(encoding='utf-8')
        original = content

        # Pattern to match path elements
        # We need to add fill="{COLOR_PRIMARY}" to paths that:
        # 1. Don't have fill attribute at all
        # 2. Have fill with a color (not "none")

        def replace_path(match):
            path_tag = match.group(0)

            # Skip if already has placeholder
            if '{COLOR_PRIMARY}' in path_tag or '{COLOR_SECONDARY}' in path_tag:
                return path_tag

            # Check if has fill="none" - skip these
            if 'fill="none"' in path_tag or "fill='none'" in path_tag:
                return path_tag

            # Check if has fill attribute with a color
            fill_match = re.search(r'fill="([^"]*)"', path_tag)
            if fill_match:
                fill_value = fill_match.group(1)
                if fill_value != 'none':
                    # Replace the fill value with placeholder
                    return re.sub(r'fill="[^"]*"', 'fill="{COLOR_PRIMARY}"', path_tag)
                return path_tag

            # No fill attribute - add one before the closing >
            # Handle self-closing tags <path ... />
            if path_tag.endswith('/>'):
                return path_tag[:-2] + ' fill="{COLOR_PRIMARY}"/>'
            # Handle opening tags <path ...>
            elif path_tag.endswith('>'):
                return path_tag[:-1] + ' fill="{COLOR_PRIMARY}">'

            return path_tag

        # Match <path ...> or <path ... />
        content = re.sub(r'<path[^>]*/?>', replace_path, content)

        # Also handle <rect>, <circle>, <polygon>, <ellipse> if present
        for tag in ['rect', 'circle', 'polygon', 'ellipse', 'line', 'polyline']:
            content = re.sub(rf'<{tag}[^>]*/?>', replace_path, content)

        if content != original:
            filepath.write_text(content, encoding='utf-8')
            return True

        return False

    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return False


def main():
    # Get project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    # Process ALL icon theme folders
    icons_dir = project_root / 'resources' / 'icons'
    theme_folders = ['filled', 'outlined', 'rounded', 'twotone']

    total_modified = 0
    total_files = 0

    for theme in theme_folders:
        theme_dir = icons_dir / theme

        if not theme_dir.exists():
            print(f"Skipping (not found): {theme_dir}")
            continue

        modified = 0
        count = 0

        for svg_file in theme_dir.glob('*.svg'):
            count += 1
            if convert_svg_file(svg_file):
                modified += 1

        total_files += count
        total_modified += modified
        print(f"{theme}: {modified}/{count} converted")

    print(f"\nTotal Results:")
    print(f"  Total files: {total_files}")
    print(f"  Modified: {total_modified}")
    print(f"  Already OK: {total_files - total_modified}")


if __name__ == '__main__':
    main()
