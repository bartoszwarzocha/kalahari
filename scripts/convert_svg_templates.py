#!/usr/bin/env python3
"""
Convert SVG icons to template format with color placeholders.

This script processes SVG files and replaces fill colors with placeholders:
- Elements with opacity > 0.5 (or no opacity) -> {COLOR_PRIMARY}
- Elements with opacity <= 0.5 -> {COLOR_SECONDARY}

After assigning colors, opacity attributes are REMOVED so the user
has full control over the final color appearance.

Usage:
    python convert_svg_templates.py [directory]

If no directory specified, processes resources/icons/twotone/
"""

import os
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


# Register SVG namespace to preserve it in output
ET.register_namespace('', 'http://www.w3.org/2000/svg')
ET.register_namespace('xlink', 'http://www.w3.org/1999/xlink')


def get_opacity(element):
    """Get opacity value from element, defaulting to 1.0."""
    # Check both opacity and fill-opacity
    opacity_str = element.get('opacity', element.get('fill-opacity', '1.0'))
    try:
        return float(opacity_str)
    except ValueError:
        return 1.0


def has_placeholder(svg_content):
    """Check if SVG already has color placeholders."""
    return '{COLOR_PRIMARY}' in svg_content or '{COLOR_SECONDARY}' in svg_content


def convert_svg_to_template(svg_path):
    """
    Convert SVG file to template format with color placeholders.

    Returns:
        tuple: (success: bool, message: str)
    """
    try:
        # Read original content
        with open(svg_path, 'r', encoding='utf-8') as f:
            original_content = f.read()

        # Skip if already has placeholders AND no opacity to remove
        if has_placeholder(original_content):
            if 'opacity=' not in original_content and 'fill-opacity=' not in original_content:
                return True, "Already converted (no opacity to remove)"
            # Has placeholders but still has opacity - need to remove it

        # Parse SVG
        try:
            root = ET.fromstring(original_content)
        except ET.ParseError as e:
            return False, f"XML parse error: {e}"

        # Elements to process (including groups)
        processable_tags = ['path', 'circle', 'rect', 'polygon', 'polyline', 'ellipse', 'line', 'g']

        # Track if we made any changes
        modified = False

        # Find and process all elements
        for elem in root.iter():
            local_name = elem.tag.split('}')[-1] if '}' in elem.tag else elem.tag

            if local_name not in processable_tags:
                continue

            # STEP 1: Read opacity BEFORE removing it
            opacity = get_opacity(elem)

            # STEP 2: Remove opacity/fill-opacity attributes
            if 'opacity' in elem.attrib:
                del elem.attrib['opacity']
                modified = True
            if 'fill-opacity' in elem.attrib:
                del elem.attrib['fill-opacity']
                modified = True

            # STEP 3: Determine color based on original opacity
            color_placeholder = '{COLOR_SECONDARY}' if opacity <= 0.5 else '{COLOR_PRIMARY}'

            # STEP 4: Get current fill and assign color
            fill = elem.get('fill', '')

            # Skip if fill is 'none' (transparent elements)
            if fill == 'none':
                continue

            # Replace or add fill attribute (only for drawable elements, not groups without fill)
            if fill and fill != 'none' and not fill.startswith('{COLOR_'):
                elem.set('fill', color_placeholder)
                modified = True
            elif not fill and local_name != 'g':
                # Add fill if element has path data or is a shape (but not empty groups)
                if elem.get('d') or local_name in ['circle', 'rect', 'polygon', 'polyline', 'ellipse', 'line']:
                    elem.set('fill', color_placeholder)
                    modified = True

        if not modified:
            return True, "No changes needed"

        # Convert back to string
        output = ET.tostring(root, encoding='unicode')

        # Clean up namespace prefixes that ElementTree adds
        output = output.replace('ns0:', '').replace(':ns0', '')
        output = re.sub(r'\s+xmlns:ns\d+="[^"]*"', '', output)

        # Ensure proper SVG namespace
        if 'xmlns=' not in output:
            output = output.replace('<svg', '<svg xmlns="http://www.w3.org/2000/svg"', 1)

        # Write back
        with open(svg_path, 'w', encoding='utf-8') as f:
            f.write(output)

        return True, "Converted successfully"

    except Exception as e:
        return False, f"Error: {e}"


def process_directory(dir_path):
    """Process all SVG files in directory."""
    dir_path = Path(dir_path)

    if not dir_path.exists():
        print(f"Error: Directory '{dir_path}' does not exist")
        return

    svg_files = list(dir_path.glob('*.svg'))

    if not svg_files:
        print(f"No SVG files found in '{dir_path}'")
        return

    print(f"Processing {len(svg_files)} SVG files in '{dir_path}'...")
    print("=" * 60)

    converted = 0
    skipped = 0
    failed = 0

    for svg_file in sorted(svg_files):
        success, message = convert_svg_to_template(svg_file)

        if success:
            if "Already" in message or "No changes" in message:
                print(f"  [SKIP] {svg_file.name}: {message}")
                skipped += 1
            else:
                print(f"  [OK]   {svg_file.name}: {message}")
                converted += 1
        else:
            print(f"  [FAIL] {svg_file.name}: {message}")
            failed += 1

    print("=" * 60)
    print(f"Results: {converted} converted, {skipped} skipped, {failed} failed")
    print(f"Total: {len(svg_files)} files processed")


def main():
    # Default directory
    default_dir = "resources/icons/twotone"

    # Get directory from command line or use default
    if len(sys.argv) > 1:
        target_dir = sys.argv[1]
    else:
        # Try to find project root
        script_dir = Path(__file__).parent
        project_root = script_dir.parent
        target_dir = project_root / default_dir

    process_directory(target_dir)


if __name__ == "__main__":
    main()
