from pathlib import Path


WINDOWS_ONLY_TOKENS = ("_snprintf_s", "OutputDebugStringA", "CreateFileA")


def test_terminalshell_windows_debug_calls_are_guarded():
    source = Path(__file__).resolve().parents[1] / "src" / "app" / "TerminalShell.cpp"
    guarded_depth = 0
    violations = []

    for line_number, line in enumerate(source.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.strip()
        if stripped.startswith("#ifdef Q_OS_WIN"):
            guarded_depth += 1
            continue
        if stripped.startswith("#else") and guarded_depth:
            guarded_depth -= 1
            continue
        if stripped.startswith("#endif") and guarded_depth:
            guarded_depth -= 1
            continue

        if guarded_depth == 0 and any(token in line for token in WINDOWS_ONLY_TOKENS):
            violations.append((line_number, line.strip()))

    assert violations == []
