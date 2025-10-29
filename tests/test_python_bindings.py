#!/usr/bin/env python3
"""
Test script for kalahari_api Python bindings.

This script verifies that:
1. kalahari_api module can be imported
2. Logger bindings work correctly
3. All log levels (info, error, debug, warning) work

Run from build directory:
  cd build-linux
  python3 ../tests/test_python_bindings.py

Or with explicit path:
  PYTHONPATH=/path/to/build/lib python3 tests/test_python_bindings.py
"""

import sys
import os

def add_build_paths():
    """Add build directory to Python path for module discovery."""
    # Try common build directory names
    possible_paths = [
        './lib',
        './lib/python',
        '../build-linux/lib',
        '../build-linux/lib/python',
        '../build-linux/bin',
        '../build-windows/lib',
        '../build-windows/lib/python',
        '../build-windows/bin',
    ]

    for path in possible_paths:
        abs_path = os.path.abspath(path)
        if os.path.exists(abs_path):
            sys.path.insert(0, abs_path)
            print(f"✓ Added to path: {abs_path}")

    # Also try current working directory
    sys.path.insert(0, '.')

def main():
    print("=" * 60)
    print("Testing kalahari_api Python bindings")
    print("=" * 60)
    print()

    # Set up Python path
    print("1. Setting up Python module search paths...")
    add_build_paths()
    print(f"   Python path: {sys.path[:3]}...")
    print()

    # Import kalahari_api
    print("2. Importing kalahari_api module...")
    try:
        import kalahari_api
        print("   ✅ Successfully imported kalahari_api")
    except ImportError as e:
        print(f"   ❌ Failed to import kalahari_api: {e}")
        print()
        print("   Troubleshooting:")
        print("   1. Build the project first:")
        print("      ./scripts/build_linux.sh")
        print("   2. Verify kalahari_api.so exists:")
        print("      ls build-linux/lib/python/kalahari_api.so")
        print("      ls build-linux/bin/kalahari_api.so")
        sys.exit(1)
    except Exception as e:
        print(f"   ❌ Runtime error: {e}")
        sys.exit(1)

    print()

    # Test Logger bindings
    print("3. Testing Logger bindings...")
    try:
        print("   - Testing Logger.info()...")
        kalahari_api.Logger.info("Hello from Python! 🐍 (info level)")
        print("     ✅ Logger.info() works")

        print("   - Testing Logger.debug()...")
        kalahari_api.Logger.debug("This is a debug message from Python")
        print("     ✅ Logger.debug() works")

        print("   - Testing Logger.warn()...")
        kalahari_api.Logger.warn("This is a warning from Python")
        print("     ✅ Logger.warn() works")

        print("   - Testing Logger.error()...")
        kalahari_api.Logger.error("This is an error message from Python (test only)")
        print("     ✅ Logger.error() works")

    except AttributeError as e:
        print(f"   ❌ Logger method not found: {e}")
        print(f"   Available in module: {dir(kalahari_api)}")
        sys.exit(1)
    except Exception as e:
        print(f"   ❌ Logger test failed: {e}")
        sys.exit(1)

    print()

    # Print module information
    print("4. Module information...")
    print(f"   Module: {kalahari_api}")
    print(f"   Module dir: {dir(kalahari_api)}")
    print(f"   Module docstring: {kalahari_api.__doc__}")
    print()

    # Success
    print("=" * 60)
    print("✅ All tests passed!")
    print("=" * 60)
    print()
    print("Summary:")
    print("  ✓ kalahari_api module imported successfully")
    print("  ✓ Logger.info() works")
    print("  ✓ Logger.debug() works")
    print("  ✓ Logger.warn() works")
    print("  ✓ Logger.error() works")
    print()

if __name__ == "__main__":
    main()
