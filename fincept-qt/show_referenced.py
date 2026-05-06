import os

with open('py_files_rel.txt', 'r') as f:
    all_py_files = set(line.strip() for line in f if line.strip())

with open('cpp_refs.txt', 'r') as f:
    cpp_quoted_refs = set(line.strip() for line in f if line.strip())

cpp_stems = set()
for ref in cpp_quoted_refs:
    filename = os.path.basename(ref)
    if filename.endswith('.py'):
        stem = filename[:-3]
        cpp_stems.add(stem)

directly_referenced = set()
for py_file in all_py_files:
    if py_file in cpp_quoted_refs:
        directly_referenced.add(py_file)
    else:
        filename = os.path.basename(py_file)
        if filename in cpp_quoted_refs or filename[:-3] in cpp_stems:
            directly_referenced.add(py_file)

print("Directly referenced from C++ (count: {}):\n".format(len(directly_referenced)))
for ref in sorted(directly_referenced):
    print(f"  {ref}")
