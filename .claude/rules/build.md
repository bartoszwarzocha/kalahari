# Build Commands

## Windows (PRIMARY)

```bash
# ALWAYS use this, NEVER cmake directly
scripts/build_windows.bat Debug
```

## Linux

```bash
scripts/build_linux.sh
```

## MCP Servers & Semantic Tools

- **Context7:** External library docs (`resolve-library-id` → `query-docs`)
- **Serena:** Semantic C++ navigation (`find_symbol`, `find_referencing_symbols`, symbol-level edits)
- **Native LSP (clangd):** non-functional on this Windows setup — use Serena or Grep/Glob/Read instead
