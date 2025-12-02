#!/usr/bin/env python3
"""Scan Python scripts and update requirements.txt with missing packages."""
import ast
import os
import re
from pathlib import Path

SCRIPTS_DIR = Path("resources/scripts")
REQUIREMENTS_FILE = Path("resources/requirements.txt")
STDLIB_MODULES = set([
    'abc', 'aifc', 'argparse', 'array', 'ast', 'asynchat', 'asyncio', 'asyncore',
    'base64', 'bdb', 'binascii', 'binhex', 'bisect', 'builtins', 'bz2',
    'calendar', 'cgi', 'cgitb', 'chunk', 'cmath', 'cmd', 'code', 'codecs',
    'codeop', 'collections', 'colorsys', 'compileall', 'concurrent', 'configparser',
    'contextlib', 'copy', 'copyreg', 'cProfile', 'csv', 'ctypes', 'curses',
    'dataclasses', 'datetime', 'dbm', 'decimal', 'difflib', 'dis', 'distutils',
    'doctest', 'email', 'encodings', 'enum', 'errno', 'faulthandler', 'fcntl',
    'filecmp', 'fileinput', 'fnmatch', 'formatter', 'fractions', 'ftplib',
    'functools', 'gc', 'getopt', 'getpass', 'gettext', 'glob', 'gzip',
    'hashlib', 'heapq', 'hmac', 'html', 'http', 'imaplib', 'imghdr', 'imp',
    'importlib', 'inspect', 'io', 'ipaddress', 'itertools', 'json', 'keyword',
    'lib2to3', 'linecache', 'locale', 'logging', 'lzma', 'mailbox', 'mailcap',
    'marshal', 'math', 'mimetypes', 'mmap', 'modulefinder', 'multiprocessing',
    'netrc', 'numbers', 'operator', 'optparse', 'os', 'ossaudiodev', 'parser',
    'pathlib', 'pdb', 'pickle', 'pickletools', 'pipes', 'pkgutil', 'platform',
    'plistlib', 'poplib', 'posix', 'posixpath', 'pprint', 'profile', 'pstats',
    'pty', 'pwd', 'py_compile', 'pyclbr', 'pydoc', 'queue', 'quopri', 'random',
    're', 'readline', 'reprlib', 'resource', 'rlcompleter', 'runpy', 'sched',
    'secrets', 'select', 'selectors', 'shelve', 'shlex', 'shutil', 'signal',
    'site', 'smtpd', 'smtplib', 'sndhdr', 'socket', 'socketserver', 'sqlite3',
    'ssl', 'stat', 'statistics', 'string', 'stringprep', 'struct', 'subprocess',
    'sunau', 'symbol', 'symtable', 'sys', 'sysconfig', 'syslog', 'tabnanny',
    'tarfile', 'telnetlib', 'tempfile', 'termios', 'test', 'textwrap', 'threading',
    'time', 'timeit', 'token', 'tokenize', 'trace', 'traceback', 'tracemalloc',
    'tty', 'turtle', 'turtledemo', 'types', 'typing', 'unicodedata', 'unittest',
    'urllib', 'uu', 'uuid', 'venv', 'warnings', 'wave', 'weakref', 'webbrowser',
    'winreg', 'winsound', 'wsgiref', 'xdrlib', 'xml', 'xmlrpc', 'zipapp',
    'zipfile', 'zipimport', 'zlib'
])

def extract_imports(file_path):
    """Extract all import statements from a Python file."""
    imports = set()
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            tree = ast.parse(f.read(), filename=str(file_path))

        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                for alias in node.names:
                    imports.add(alias.name.split('.')[0])
            elif isinstance(node, ast.ImportFrom):
                if node.module:
                    imports.add(node.module.split('.')[0])
    except Exception as e:
        print(f"Warning: Could not parse {file_path}: {e}")

    return imports

def scan_all_scripts():
    """Scan all Python scripts in resources/scripts directory."""
    all_imports = set()

    for py_file in SCRIPTS_DIR.rglob("*.py"):
        imports = extract_imports(py_file)
        all_imports.update(imports)

    # Filter out standard library modules
    third_party = {imp for imp in all_imports if imp not in STDLIB_MODULES}

    return sorted(third_party)

def read_requirements():
    """Read existing requirements.txt."""
    if not REQUIREMENTS_FILE.exists():
        return set()

    packages = set()
    with open(REQUIREMENTS_FILE, 'r') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                # Extract package name (before ==, >=, etc.)
                pkg = re.split(r'[=<>!]', line)[0].strip()
                packages.add(pkg.lower())

    return packages

def update_requirements(new_packages):
    """Update requirements.txt with new packages."""
    existing = read_requirements()
    to_add = [pkg for pkg in new_packages if pkg.lower() not in existing]

    if to_add:
        print(f"Adding {len(to_add)} new packages to requirements.txt:")
        for pkg in to_add:
            print(f"  - {pkg}")

        with open(REQUIREMENTS_FILE, 'a') as f:
            for pkg in to_add:
                f.write(f"{pkg}\n")
    else:
        print("All packages already in requirements.txt")

if __name__ == "__main__":
    print("Scanning Python scripts for dependencies...")
    packages = scan_all_scripts()
    print(f"Found {len(packages)} third-party packages")
    update_requirements(packages)
