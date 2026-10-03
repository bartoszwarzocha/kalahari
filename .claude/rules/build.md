# Build Commands

## Windows (PRIMARY)

```bash
# ALWAYS use this, NEVER cmake directly
scripts/build_windows.bat Debug
./build-windows/bin/kalahari-tests.exe
```

## Linux

```bash
scripts/build_linux.sh
./build-linux/bin/kalahari-tests
```

## macOS

```bash
scripts/build_macos.sh
```

## MCP Servers & Semantic Tools

- **Context7:** External library docs (`resolve-library-id` → `query-docs`)
- **Serena:** Semantic C++ navigation (`find_symbol`, `find_referencing_symbols`, symbol-level edits)
- Both live in a per-machine, git-ignored `.mcp.json`; copy `.mcp.json.example` to set them up.
  If they are missing, fall back to Grep/Glob/Read.
- **Native LSP (clangd):** non-functional on the Windows setup — use Serena or Grep/Glob/Read instead
