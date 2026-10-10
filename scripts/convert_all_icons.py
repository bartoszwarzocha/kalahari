#!/usr/bin/env python3
"""
Convert SVG icons to use color placeholders.

This script processes the SVG files of every icon theme folder and gives
{COLOR_PRIMARY} to the shapes that are drawn: those with a color of their own,
or with no fill when no group around them has fill="none".

Usage:
    python scripts/convert_all_icons.py
"""

import os
import re
from pathlib import Path


# A tag: closing slash, name, attributes, self-closing slash
TAG_PATTERN = re.compile(r'<(/?)([A-Za-z][\w:.-]*)([^>]*?)(/?)>')
FILL_PATTERN = re.compile(r'\bfill\s*=\s*["\']([^"\']*)["\']')
DRAWABLE_TAGS = {'path', 'rect', 'circle', 'polygon', 'ellipse', 'line', 'polyline'}


def convert_drawable(tag: str, own_fill, inherited_fill) -> str:
    """
    Give one shape the color placeholder.

    Args:
        tag: The shape's tag
        own_fill: Its fill attribute, or None
        inherited_fill: The fill of the nearest group that sets one, or None

    Returns:
        The tag with the placeholder, or as it was
    """
    # Skip if already has placeholder
    if '{COLOR_PRIMARY}' in tag or '{COLOR_SECONDARY}' in tag:
        return tag

    if own_fill is not None:
        if own_fill == 'none':
            return tag
        # A color: the placeholder takes its place
        return FILL_PATTERN.sub('fill="{COLOR_PRIMARY}"', tag, count=1)

    # No fill of its own: a group's fill="none" keeps it invisible, as the square that
    # Material icons draw in <g fill="none"> to mark their size
    if inherited_fill == 'none':
        return tag

    # Add one before the closing > (self-closing <path ... /> or opening <path ...>)
    if tag.endswith('/>'):
        return tag[:-2].rstrip() + ' fill="{COLOR_PRIMARY}"/>'
    return tag[:-1] + ' fill="{COLOR_PRIMARY}">'


def convert_svg_content(content: str) -> str:
    """
    Give the shapes of an SVG the color placeholder.

    Args:
        content: The SVG

    Returns:
        The SVG with the placeholders
    """
    # The fill each open element sets (None: it sets none)
    open_fills = []

    def inherited_fill():
        for fill in reversed(open_fills):
            if fill is not None:
                return fill
        return None

    def replace_tag(match):
        closing, name, attributes, self_closing = match.groups()
        tag = match.group(0)
        if closing:
            if open_fills:
                open_fills.pop()
            return tag
        fill_match = FILL_PATTERN.search(attributes)
        own_fill = fill_match.group(1) if fill_match else None
        if name in DRAWABLE_TAGS:
            tag = convert_drawable(tag, own_fill, inherited_fill())
        if not self_closing:
            open_fills.append(own_fill)
        return tag

    return TAG_PATTERN.sub(replace_tag, content)


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
        converted = convert_svg_content(content)

        if converted != content:
            filepath.write_text(converted, encoding='utf-8')
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
