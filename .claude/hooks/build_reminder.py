"""PostToolUse hook (Edit|Write) - reminds to build after changing C++ sources."""

from _common import emit_context, read_input

CPP_SUFFIXES = (".cpp", ".h", ".hpp", ".cmake", "CMakeLists.txt")


def main() -> None:
    path = read_input().get("tool_input", {}).get("file_path", "")
    if path.endswith(CPP_SUFFIXES):
        emit_context(
            "PostToolUse",
            "[BUILD REMINDER] C++/CMake file changed - build before marking the task complete "
            "(Windows: scripts/build_windows.bat Debug, Linux: scripts/build_linux.sh).",
        )


if __name__ == "__main__":
    main()
