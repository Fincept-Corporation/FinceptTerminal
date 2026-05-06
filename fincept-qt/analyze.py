import re
import os

# Read all Python files (relative paths)
with open('py_files_rel.txt', 'r') as f:
    all_py_files = set(line.strip() for line in f if line.strip())

print(f"Total Python files: {len(all_py_files)}")

# Read C++ references (quoted strings that appear to be .py file paths)
with open('cpp_refs.txt', 'r') as f:
    cpp_quoted_refs = set(line.strip() for line in f if line.strip())

# Extract stems (filenames without .py and without path)
cpp_stems = set()
for ref in cpp_quoted_refs:
    filename = os.path.basename(ref)
    if filename.endswith('.py'):
        stem = filename[:-3]
        cpp_stems.add(stem)

print(f"C++ quoted references (with paths): {len(cpp_quoted_refs)}")
print(f"C++ stems (no path, no .py): {len(cpp_stems)}")

# A Python file is "directly referenced" if:
# 1. Its relative path appears in cpp_quoted_refs, OR
# 2. Its filename (with .py) appears in cpp_quoted_refs, OR
# 3. Its stem (without .py) appears in cpp_stems

directly_referenced = set()
for py_file in all_py_files:
    # Check if path appears directly
    if py_file in cpp_quoted_refs:
        directly_referenced.add(py_file)
    else:
        # Check if filename appears
        filename = os.path.basename(py_file)
        if filename in cpp_quoted_refs or filename[:-3] in cpp_stems:
            directly_referenced.add(py_file)

print(f"Directly referenced from C++: {len(directly_referenced)}")

# Now we need to find Python imports
# Map each Python file to the modules it imports
import_map = {}

for py_file in all_py_files:
    full_path = f"scripts/{py_file}"
    if not os.path.exists(full_path):
        continue
    
    try:
        with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except:
        continue
    
    # Find all imports
    import_patterns = [
        r'^\s*from\s+([a-zA-Z_][a-zA-Z0-9_]*(?:\.[a-zA-Z_][a-zA-Z0-9_]*)*)\s+import',
        r'^\s*import\s+([a-zA-Z_][a-zA-Z0-9_]*(?:\.[a-zA-Z_][a-zA-Z0-9_]*)*)',
    ]
    
    imports = set()
    for line in content.split('\n'):
        for pattern in import_patterns:
            match = re.match(pattern, line)
            if match:
                module_path = match.group(1)
                imports.add(module_path)
    
    import_map[py_file] = imports

# Transitive closure: starting from directly_referenced, mark all imports as indirectly used
indirectly_used = set()
to_check = set(directly_referenced)
visited = set()

while to_check:
    current = to_check.pop()
    if current in visited:
        continue
    visited.add(current)
    
    imports = import_map.get(current, set())
    
    for imp in imports:
        # Resolve this import to a file in scripts/
        imp_path = imp.replace('.', '/')
        
        candidates = [
            f"{imp_path}.py",
            f"{imp_path}/__init__.py",
        ]
        
        for candidate in candidates:
            if candidate in all_py_files:
                if candidate not in directly_referenced and candidate not in indirectly_used:
                    indirectly_used.add(candidate)
                    to_check.add(candidate)
                break

print(f"Indirectly used (imported by referenced): {len(indirectly_used)}")

# Orphaned = all - directly_referenced - indirectly_used
referenced_total = directly_referenced | indirectly_used
orphaned = all_py_files - referenced_total

print(f"Orphaned: {len(orphaned)}")
print()

# Group orphans by top-level directory
orphan_groups = {}
for orphan in sorted(orphaned):
    parts = orphan.split('/')
    if len(parts) > 1:
        top_dir = parts[0]
    else:
        top_dir = "root"
    
    if top_dir not in orphan_groups:
        orphan_groups[top_dir] = []
    orphan_groups[top_dir].append(orphan)

# Write results
with open('results.txt', 'w') as f:
    f.write(f"Total Python files: {len(all_py_files)}\n")
    f.write(f"Directly referenced from C++: {len(directly_referenced)}\n")
    f.write(f"Indirectly used: {len(indirectly_used)}\n")
    f.write(f"Orphaned: {len(orphaned)}\n")
    f.write("\nOrphans by directory:\n")
    for top_dir in sorted(orphan_groups.keys()):
        count = len(orphan_groups[top_dir])
        f.write(f"  {top_dir}: {count}\n")

# Also save detailed orphan lists
with open('orphans_by_dir.txt', 'w') as f:
    for top_dir in sorted(orphan_groups.keys()):
        orphans = orphan_groups[top_dir]
        f.write(f"\n{top_dir} ({len(orphans)} orphans):\n")
        for orphan in sorted(orphans)[:100]:  # First 100 per group
            f.write(f"  {orphan}\n")
        if len(orphans) > 100:
            f.write(f"  ... and {len(orphans) - 100} more\n")

print("Results saved to results.txt and orphans_by_dir.txt")
