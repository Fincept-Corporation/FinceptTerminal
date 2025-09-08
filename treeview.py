import os


def create_tree(directory, prefix="", only_python=True):
    """Create a tree view of directory structure"""
    items = []

    try:
        all_items = sorted(os.listdir(directory))

        # Filter for directories and Python files
        for item in all_items:
            item_path = os.path.join(directory, item)
            if os.path.isdir(item_path):
                items.append((item, True))  # (name, is_directory)
            elif only_python and item.endswith('.py'):
                items.append((item, False))
    except PermissionError:
        return

    for i, (item, is_dir) in enumerate(items):
        is_last = i == len(items) - 1
        current_prefix = "└── " if is_last else "├── "
        print(f"{prefix}{current_prefix}{item}")

        if is_dir:
            extension = "    " if is_last else "│   "
            create_tree(os.path.join(directory, item), prefix + extension, only_python)


def show_project_tree(root_path="."):
    """Display project tree structure"""
    print(f"Project Structure: {os.path.basename(os.path.abspath(root_path))}")
    create_tree(root_path)


if __name__ == "__main__":
    show_project_tree()