#!/usr/bin/env python3
import re
import os
from pathlib import Path

# Read Sources.cmake
sources_file = r'D:\Repositories\TombEngine\TombEngine\Sources.cmake'
base_dir = r'D:\Repositories\TombEngine\TombEngine'

with open(sources_file, 'r') as f:
    content = f.read()

# Extract all .cpp and .h file references
referenced_files = re.findall(r'^\s+([A-Za-z0-9_./]+\.(cpp|h))\s*$', content, re.MULTILINE)
referenced_files = [f[0] for f in referenced_files]
referenced_files = sorted(set(referenced_files))

print(f"Total unique files referenced in Sources.cmake: {len(referenced_files)}\n")

# Check which files exist
missing_files = []
for file in referenced_files:
    full_path = os.path.join(base_dir, file)
    if not os.path.exists(full_path):
        missing_files.append(file)

# Get all actual files in directories referenced by Sources.cmake
actual_files = set()
dirs_to_check = set()

# Get all directories referenced in Sources.cmake
for file in referenced_files:
    dir_path = os.path.dirname(file)
    if dir_path:
        dirs_to_check.add(dir_path)

# Expand to parent directories
expanded_dirs = set()
for d in dirs_to_check:
    parts = d.split('/')
    for i in range(len(parts)):
        expanded_dirs.add('/'.join(parts[:i+1]))

print(f"Directories to check: {len(expanded_dirs)}")
print()

# Find all .cpp and .h files in those directories
for dir_name in sorted(expanded_dirs):
    full_dir = os.path.join(base_dir, dir_name)
    if os.path.isdir(full_dir):
        for root, dirs, files in os.walk(full_dir):
            for file in files:
                if file.endswith('.cpp') or file.endswith('.h'):
                    rel_path = os.path.relpath(os.path.join(root, file), base_dir)
                    rel_path = rel_path.replace('\\', '/')
                    actual_files.add(rel_path)

print(f"Total actual files found in referenced directories: {len(actual_files)}\n")

# Find files on disk not in Sources.cmake
not_in_cmake = actual_files - set(referenced_files)

# Report missing files
if missing_files:
    print("=" * 80)
    print("FILES LISTED IN Sources.cmake BUT DON'T EXIST ON DISK:")
    print("=" * 80)
    for file in sorted(missing_files):
        print(f"  {file}")
    print(f"\nTotal missing: {len(missing_files)}\n")
else:
    print("All files listed in Sources.cmake exist on disk.\n")

# Report files not in cmake
if not_in_cmake:
    print("=" * 80)
    print("FILES ON DISK BUT NOT LISTED IN Sources.cmake:")
    print("=" * 80)
    for file in sorted(not_in_cmake):
        print(f"  {file}")
    print(f"\nTotal not in cmake: {len(not_in_cmake)}\n")
else:
    print("All files in referenced directories are listed in Sources.cmake.\n")
