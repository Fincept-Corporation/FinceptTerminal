from textual.app import ComposeResult
from textual.widgets import MarkdownViewer
from textual.containers import Container

EXAMPLE_MARKDOWN = """\
# FinScript Code Editor (Under Development)

Get your code running here !!


## Features

Markdown syntax and extensions are supported.

- Typography *emphasis*, **strong**, `inline code` etc.
- Headers
- Lists (bullet and ordered)
- Syntax highlighted code blocks
- Tables!

## Table

Tables are displayed in a DataTable widget.

| Name            | Type   | Default | Description                        |
| --------------- | ------ | ------- | ---------------------------------- |
| `show_header`   | `bool` | `True`  | Show the table header              |
| `fixed_rows`    | `int`  | `0`     | Number of fixed rows               |
| `fixed_columns` | `int`  | `0`     | Number of fixed columns            |
| `zebra_stripes` | `bool` | `False` | Display alternating colors on rows |
| `header_height` | `int`  | `1`     | Height of header row               |
| `show_cursor`   | `bool` | `True`  | Show a cell cursor                 |


## Example Code

Code blocks are syntax highlighted, with guidelines.

```finscript (example.fincept)
if ema(RITES.NS, 21) > wma(RITES.NS, 50) {
    buy "EMA 21 crosses WMA 50 Buy"
}

plot ema(RITES.NS, 7), "EMA (7)"
plot wma(RITES.NS, 21), "WMA (21)"
```
"""


class FinceptLangScreen(Container):
    def compose(self) -> ComposeResult:
        yield MarkdownViewer(EXAMPLE_MARKDOWN, show_table_of_contents=True)

# #!/usr/bin/env python
# """
# A VS Code–like code editor built with Textual with enhanced debugging.
#
# Features:
#   - A file explorer (left) that displays files and folders from the current directory.
#   - A code editor (center) that shows a file’s contents.
#   - A toolbar with buttons to create, delete, rename, save, and run files.
#   - An output panel (bottom) that displays output (and debugging messages) from running the current file.
#
# Keyboard shortcuts:
#   • Ctrl+S to save the file.
#   • Ctrl+R to run the current file.
# """
#
# import asyncio
# import os
# import sys
#
# from textual.app import App, ComposeResult
# from textual.containers import Horizontal, Vertical
# from textual.widgets import Header, Footer, Button, TextArea, Static, Tree, Input
# from textual.widgets.tree import TreeNode
# from textual import events
#
# # ── CSS Styling ─────────────────────────────────────────────
# CSS = """
# Screen {
#     layout: vertical;
# }
#
# #toolbar {
#     height: 3;
#     background: #333;
# }
#
# #toolbar Button {
#     margin: 0 1;
# }
#
# #main_container {
#     layout: horizontal;
#     height: 60%;
# }
#
# #file_explorer {
#     width: 30%;
#     border: solid #444;
#     overflow: auto;
# }
#
# #editor_container {
#     width: 70%;
#     border: solid #444;
# }
#
# #editor {
#     height: 100%;
# }
#
# #output {
#     height: 20%;
#     border: solid #444;
#     overflow: auto;
#     padding: 1;
# }
#
# #file_prompt {
#     layer: modal;
#     align: center middle;
#     width: 50%;
#     height: 20%;
#     background: #222;
#     border: solid #444;
#     padding: 1;
# }
# """
#
#
# # ── Modal Prompt for File Names ─────────────────────────────
# class FilePrompt(Static):
#     """
#     A simple modal prompt that asks for a file name.
#     When OK is pressed, it calls the provided callback with the entered text.
#     """
#
#     def __init__(self, prompt_text: str, default_text: str = "", callback=None) -> None:
#         super().__init__(id="file_prompt")
#         self.prompt_text = prompt_text
#         self.default_text = default_text
#         self.callback = callback
#
#     def compose(self) -> ComposeResult:
#         yield Static(self.prompt_text, id="prompt_label")
#         yield Input(value=self.default_text, id="prompt_input")
#         with Horizontal():
#             yield Button("OK", id="prompt_ok")
#             yield Button("Cancel", id="prompt_cancel")
#
#     async def on_button_pressed(self, event: Button.Pressed) -> None:
#         if event.button.id == "prompt_ok":
#             input_widget = self.query_one("#prompt_input", Input)
#             self.log(f"[DEBUG] FilePrompt OK pressed, input: {input_widget.value}")
#             if self.callback:
#                 await self.callback(input_widget.value)
#             self.remove()  # Dismiss the prompt
#         elif event.button.id == "prompt_cancel":
#             self.log("[DEBUG] FilePrompt Cancel pressed")
#             self.remove()
#
#
# # ── Main Application ─────────────────────────────────────────
# class CodeEditorApp(App):
#     CSS = CSS
#
#     def __init__(self):
#         super().__init__()
#         self.current_file: str | None = None  # Path of the currently open file
#
#     def compose(self) -> ComposeResult:
#         yield Header()
#         with Horizontal(id="toolbar"):
#             yield Button("New File", id="new_file")
#             yield Button("Delete File", id="delete_file")
#             yield Button("Rename File", id="rename_file")
#             yield Button("Save", id="save_file")
#             yield Button("Run", id="run_file")
#         with Horizontal(id="main_container"):
#             yield Tree("Files", id="file_explorer")
#             with Vertical(id="editor_container"):
#                 yield TextArea(id="editor")
#         yield Static("", id="output")
#         yield Footer()
#
#     def on_mount(self) -> None:
#         self.log("[DEBUG] App mounted; starting refresh_file_tree()")
#         self.refresh_file_tree()
#         file_tree = self.query_one("#file_explorer", Tree)
#         file_tree.focus()
#         self.log("[DEBUG] Focus set to file_explorer")
#
#     def refresh_file_tree(self) -> None:
#         """Rebuild the file tree from the current directory using absolute paths."""
#         self.log("[DEBUG] refresh_file_tree() called")
#         tree: Tree = self.query_one("#file_explorer", Tree)
#         for child in list(tree.root.children):
#             self.log(f"[DEBUG] Removing child node: {child.label}")
#             tree.root.remove_child(child)
#         tree.root.label = "Files"
#         current_dir = os.path.abspath(".")
#         self.log(f"[DEBUG] Scanning directory: {current_dir}")
#         self.add_files_to_tree(tree.root, current_dir)
#         tree.root.expand()
#         tree.refresh()
#         self.log("[DEBUG] File tree refreshed")
#
#     def add_files_to_tree(self, node: TreeNode, path: str) -> None:
#         """Recursively add files and directories from 'path' to the tree node."""
#         self.log(f"[DEBUG] Adding files from path: {path}")
#         try:
#             entries = sorted(os.scandir(path), key=lambda e: (not e.is_dir(), e.name.lower()))
#             for entry in entries:
#                 if entry.name.startswith("."):
#                     continue  # Skip hidden files/directories.
#                 abs_path = os.path.abspath(entry.path)
#                 self.log(f"[DEBUG] Found entry: {entry.name} -> {abs_path}")
#                 if entry.is_dir():
#                     child = node.add(entry.name, data=abs_path)
#                     self.add_files_to_tree(child, abs_path)
#                 else:
#                     node.add(entry.name, data=abs_path)
#         except Exception as e:
#             self.log(f"[DEBUG] Error scanning {path}: {e}")
#
#     async def load_file_from_node(self, node: TreeNode) -> None:
#         """Load the entire file (if applicable) into the editor."""
#         self.log(f"[DEBUG] load_file_from_node() called for node: {node.label}")
#         output = self.query_one("#output", Static)
#         if node.data and os.path.isfile(node.data):
#             self.current_file = node.data
#             self.log(f"[DEBUG] Attempting to load file: {self.current_file}")
#             try:
#                 with open(self.current_file, "r", encoding="utf-8") as f:
#                     content = f.read()
#                 self.log(f"[DEBUG] File loaded; length: {len(content)} characters")
#                 editor = self.query_one("#editor", TextArea)
#                 editor.value = content  # Load the file's contents into the editor.
#                 output.update(f"[DEBUG] Loaded file: {self.current_file}")
#             except Exception as e:
#                 error_msg = f"[DEBUG] Error reading file: {e}"
#                 self.log(error_msg)
#                 output.update(error_msg)
#         else:
#             msg = "[DEBUG] Selected node is not a file."
#             self.log(msg)
#             output.update(msg)
#
#     async def on_tree_node_selected(self, event: Tree.NodeSelected) -> None:
#         """When a node is selected, load its file (if applicable)."""
#         self.log(f"[DEBUG] on_tree_node_selected: {event.node.label}")
#         await self.load_file_from_node(event.node)
#
#     async def on_button_pressed(self, event: Button.Pressed) -> None:
#         button_id = event.button.id
#         self.log(f"[DEBUG] Button pressed: {button_id}")
#         output = self.query_one("#output", Static)
#         if button_id == "new_file":
#             self.log("[DEBUG] New File button pressed")
#             prompt = FilePrompt("Enter new file name:", callback=self.create_new_file)
#             await self.mount(prompt)
#         elif button_id == "delete_file":
#             self.log("[DEBUG] Delete File button pressed")
#             if self.current_file and os.path.isfile(self.current_file):
#                 try:
#                     os.remove(self.current_file)
#                     self.log(f"[DEBUG] Deleted file: {self.current_file}")
#                     self.current_file = None
#                     editor = self.query_one("#editor", TextArea)
#                     editor.value = ""
#                     self.refresh_file_tree()
#                     output.update("[DEBUG] File deleted.")
#                 except Exception as e:
#                     err = f"[DEBUG] Error deleting file: {e}"
#                     self.log(err)
#                     output.update(err)
#         elif button_id == "rename_file":
#             self.log("[DEBUG] Rename File button pressed")
#             if self.current_file and os.path.isfile(self.current_file):
#                 current_name = os.path.basename(self.current_file)
#                 prompt = FilePrompt("Enter new file name:", default_text=current_name, callback=self.rename_file)
#                 await self.mount(prompt)
#         elif button_id == "save_file":
#             self.log("[DEBUG] Save File button pressed")
#             if self.current_file:
#                 editor = self.query_one("#editor", TextArea)
#                 try:
#                     with open(self.current_file, "w", encoding="utf-8") as f:
#                         f.write(editor.value)
#                     self.log(f"[DEBUG] File saved: {self.current_file}")
#                     output.update("[DEBUG] File saved.")
#                 except Exception as e:
#                     err = f"[DEBUG] Error saving file: {e}"
#                     self.log(err)
#                     output.update(err)
#         elif button_id == "run_file":
#             self.log("[DEBUG] Run File button pressed")
#             await self.run_current_file()
#
#     async def create_new_file(self, filename: str) -> None:
#         """Create a new file and load it into the editor."""
#         self.log(f"[DEBUG] create_new_file() called with filename: {filename}")
#         output = self.query_one("#output", Static)
#         if not filename:
#             self.log("[DEBUG] Filename is empty; aborting create_new_file()")
#             return
#         if not filename.endswith(".py"):
#             filename += ".py"
#         try:
#             with open(filename, "w", encoding="utf-8") as f:
#                 f.write("")  # Create an empty file.
#             self.current_file = os.path.abspath(filename)
#             self.log(f"[DEBUG] Created new file: {self.current_file}")
#             editor = self.query_one("#editor", TextArea)
#             editor.value = ""
#             self.refresh_file_tree()
#             output.update(f"[DEBUG] Created file {filename}")
#         except Exception as e:
#             err = f"[DEBUG] Error creating file: {e}"
#             self.log(err)
#             output.update(err)
#
#     async def rename_file(self, new_filename: str) -> None:
#         """Rename the currently open file."""
#         self.log(f"[DEBUG] rename_file() called with new_filename: {new_filename}")
#         output = self.query_one("#output", Static)
#         if not new_filename or not self.current_file:
#             self.log("[DEBUG] Invalid new filename or no current file; aborting rename_file()")
#             return
#         dir_path = os.path.dirname(self.current_file)
#         new_path = os.path.join(dir_path, new_filename)
#         try:
#             os.rename(self.current_file, new_path)
#             self.log(f"[DEBUG] File renamed from {self.current_file} to {new_path}")
#             self.current_file = new_path
#             self.refresh_file_tree()
#             output.update("[DEBUG] File renamed.")
#         except Exception as e:
#             err = f"[DEBUG] Error renaming file: {e}"
#             self.log(err)
#             output.update(err)
#
#     async def run_current_file(self) -> None:
#         """
#         Save the current file (using the editor's contents) and execute it.
#         The output (or errors) are captured and displayed in the output panel.
#         """
#         self.log("[DEBUG] run_current_file() called")
#         output = self.query_one("#output", Static)
#         if self.current_file and os.path.isfile(self.current_file):
#             editor = self.query_one("#editor", TextArea)
#             try:
#                 with open(self.current_file, "w", encoding="utf-8") as f:
#                     f.write(editor.value)
#                 self.log(f"[DEBUG] File saved before running: {self.current_file}")
#             except Exception as e:
#                 err = f"[DEBUG] Error saving file before running: {e}"
#                 self.log(err)
#                 output.update(err)
#                 return
#             try:
#                 self.log(f"[DEBUG] Starting subprocess for: {self.current_file}")
#                 process = await asyncio.create_subprocess_exec(
#                     sys.executable,
#                     self.current_file,
#                     stdout=asyncio.subprocess.PIPE,
#                     stderr=asyncio.subprocess.PIPE,
#                 )
#                 stdout, stderr = await process.communicate()
#                 self.log("[DEBUG] Subprocess finished execution")
#                 output_text = stdout.decode("utf-8", errors="replace") + stderr.decode("utf-8", errors="replace")
#                 self.log(f"[DEBUG] Output captured (length {len(output_text)} characters)")
#                 output.update(output_text)
#             except Exception as e:
#                 err = f"[DEBUG] Error running file: {e}"
#                 self.log(err)
#                 output.update(err)
#         else:
#             msg = "[DEBUG] No file selected to run."
#             self.log(msg)
#             output.update(msg)
#
#     async def on_key(self, event: events.Key) -> None:
#         """Handle global key bindings."""
#         self.log(f"[DEBUG] Key pressed: {event.key}")
#         if event.key == "ctrl+r":
#             await self.run_current_file()
#         elif event.key == "ctrl+s":
#             if self.current_file:
#                 editor = self.query_one("#editor", TextArea)
#                 try:
#                     with open(self.current_file, "w", encoding="utf-8") as f:
#                         f.write(editor.value)
#                     self.log(f"[DEBUG] File saved via shortcut: {self.current_file}")
#                     output = self.query_one("#output", Static)
#                     output.update("[DEBUG] File saved.")
#                 except Exception as e:
#                     err = f"[DEBUG] Error saving file via shortcut: {e}"
#                     self.log(err)
#                     output = self.query_one("#output", Static)
#                     output.update(err)
#
#
# if __name__ == "__main__":
#     CodeEditorApp().run()
