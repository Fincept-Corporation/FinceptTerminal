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

