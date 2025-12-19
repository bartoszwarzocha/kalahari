#!/usr/bin/env python3
"""Initialize SQLite databases for example projects.

This script creates project.db files for all example projects
that don't have them yet.

OpenSpec #00041: SQLite Project Database
"""

import sqlite3
import os
from pathlib import Path

EXAMPLES_DIR = Path(__file__).parent.parent / "examples"

SCHEMA = """
-- Book metadata (key-value)
CREATE TABLE IF NOT EXISTS book_metadata (
    key TEXT PRIMARY KEY,
    value TEXT
);

-- Chapters metadata
CREATE TABLE IF NOT EXISTS chapters (
    id TEXT PRIMARY KEY,
    path TEXT NOT NULL,
    title TEXT,
    status TEXT DEFAULT 'draft',
    word_count INTEGER DEFAULT 0,
    character_count INTEGER DEFAULT 0,
    order_index INTEGER,
    created_at TEXT,
    modified_at TEXT
);

CREATE INDEX IF NOT EXISTS idx_chapters_order ON chapters(order_index);

-- Chapter history
CREATE TABLE IF NOT EXISTS chapter_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    chapter_id TEXT NOT NULL,
    action TEXT NOT NULL,
    author TEXT,
    timestamp TEXT,
    FOREIGN KEY (chapter_id) REFERENCES chapters(id) ON DELETE CASCADE
);

-- Character library
CREATE TABLE IF NOT EXISTS characters (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    color TEXT,
    notes TEXT,
    created_at TEXT,
    modified_at TEXT
);

-- Location library
CREATE TABLE IF NOT EXISTS locations (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    notes TEXT,
    created_at TEXT,
    modified_at TEXT
);

-- Item library
CREATE TABLE IF NOT EXISTS items (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    notes TEXT,
    created_at TEXT,
    modified_at TEXT
);

-- Session statistics
CREATE TABLE IF NOT EXISTS session_stats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    document_id TEXT,
    words_written INTEGER DEFAULT 0,
    words_deleted INTEGER DEFAULT 0,
    active_minutes INTEGER DEFAULT 0,
    hour INTEGER
);

CREATE INDEX IF NOT EXISTS idx_stats_timestamp ON session_stats(timestamp);

-- Paragraph styles
CREATE TABLE IF NOT EXISTS paragraph_styles (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    base_style TEXT,
    properties TEXT
);

-- Character styles
CREATE TABLE IF NOT EXISTS character_styles (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    properties TEXT
);

-- Project settings
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT
);
"""


def create_database(db_path: Path) -> bool:
    """Create an empty project database with schema."""
    try:
        conn = sqlite3.connect(str(db_path))
        conn.executescript(SCHEMA)
        conn.execute("PRAGMA journal_mode=WAL")
        conn.commit()
        conn.close()
        return True
    except Exception as e:
        print(f"  Error: {e}")
        return False


def main():
    print("Initializing example project databases...")
    print(f"Examples directory: {EXAMPLES_DIR}")
    print()

    if not EXAMPLES_DIR.exists():
        print(f"ERROR: Examples directory not found: {EXAMPLES_DIR}")
        return 1

    # Find all .klh files
    klh_files = list(EXAMPLES_DIR.rglob("*.klh"))

    if not klh_files:
        print("No .klh project files found")
        return 0

    created = 0
    skipped = 0

    for klh_path in klh_files:
        project_dir = klh_path.parent
        db_path = project_dir / "project.db"

        print(f"Project: {project_dir.name}")

        if db_path.exists():
            print(f"  Database already exists: {db_path}")
            skipped += 1
            continue

        print(f"  Creating database: {db_path}")
        if create_database(db_path):
            print(f"  OK - Database created")
            created += 1
        else:
            print(f"  FAILED")

    print()
    print(f"Summary: {created} created, {skipped} skipped")
    return 0


if __name__ == "__main__":
    exit(main())
