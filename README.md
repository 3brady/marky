<p align="center">
  <img src="resources/icons/marky.svg" width="96" alt="marky logo">
</p>

<h1 align="center">marky</h1>

<p align="center">
  A fast, native Markdown editor for Linux with a live preview.<br>
  Written in C++ with Qt 6. No Electron, no embedded browser.
</p>

![marky screenshot](docs/screenshot.png)

Open a project folder, pick a note from the file list, edit it on the left and watch the rendered result on the right. marky starts instantly, uses very little memory and stays out of your way.

## Features

- **Three panes:** a file list, an editor and a live preview.
- **Syntax highlighting** in the editor for headings, lists, quotes, code, links and emphasis, with matching light and dark colors.
- **Live preview** that updates a moment after you stop typing, so large files stay responsive.
- **Two-way scroll sync:** scroll either pane and the other follows.
- **GitHub-flavored Markdown:** tables, task lists, strikethrough, autolinks, fenced code and more.
- **Linked notes:** `[text](other.md)` and `[text](other.md#section)` open inside marky, `#section` links jump within a note, and everything else opens in your browser. Links can't leave the project folder.
- **Images:** local images are shown and scaled to fit.
- **Export to PDF** from the File menu. Always rendered in light colors, even when you work in dark mode.
- **Dark mode** and **zoom**, both remembered between runs. On first start marky follows your system theme.
- **Safe editing:** unsaved changes are marked in the title bar, you are asked before switching files or quitting, and saving is atomic so a crash can't corrupt your file.
- **Notices outside changes:** if another program edits the open file, marky reloads it (or asks first, if you have unsaved edits).

## Installation

marky is built from source. It takes a minute.

### Requirements

A C++20 compiler, CMake 3.21 or newer, and Qt 6 (Widgets). On Ubuntu:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev git
```

### Build and install

The Markdown parser ([md4c](https://github.com/mity/md4c)) is included as a git submodule, so clone with `--recurse-submodules`:

```bash
git clone --recurse-submodules https://github.com/3brady/marky
cd marky
cmake -S . -B build -G Ninja
cmake --build build
cmake --install build --prefix ~/.local
```

That installs the `marky` command, a desktop launcher and an icon under `~/.local`, so marky shows up in your app menu and in "Open with" for Markdown files. If the `marky` command isn't found afterwards, open a new terminal.

To try it without installing, run `./build/marky ~/notes` instead of the last step.

If you cloned without `--recurse-submodules`, run `git submodule update --init` first.

### Uninstall

```bash
xargs rm < build/install_manifest.txt
```

## Usage

```bash
marky                  # open the current folder
marky ~/notes          # open a folder
marky ~/notes/todo.md  # open a file (its folder becomes the project)
```

Launched from the app menu, marky reopens the last project you used.

The file panel lists the Markdown files (`.md`, `.markdown`) in the project folder and its subfolders. It stays pinned to that folder, so there is no way to browse above it from inside the app.

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New file (created in the selected folder) |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save as (a new name, in the same folder) |
| `Ctrl+Q` | Quit |
| `Ctrl+=` / `Ctrl+-` / `Ctrl+0` | Zoom in / out / reset |
| `Ctrl` + mouse wheel | Zoom |

Dark mode and PDF export are in the View and File menus.

## How it works

```
Markdown text ──md4c──▶ AST ──renderer──▶ QTextDocument ──▶ preview
```

- **`src/markdown/`** is plain C++ with no Qt. It wraps md4c and builds an AST of its own, which also records the source line where each block starts. md4c doesn't report positions, so they are derived from the text pointers it passes to its callbacks. Scroll sync uses these lines.
- **`src/render/`** turns the AST into a `QTextDocument`. It also returns a list of (source line, position in the document) pairs, which is how the two panes are matched up when scrolling. The same document is what gets printed to PDF.
- **`src/app/`** is the window, menus, file panel, saving, theming and the editor's syntax highlighter.

Keeping the AST between the parser and the renderer means either side can be replaced without touching the other.

```
libs/md4c/       the Markdown parser (git submodule)
src/markdown/    parser wrapper and AST (no Qt)
src/render/      AST -> QTextDocument
src/app/         main window, theme, syntax highlighter
tests/           parser tests
resources/       desktop launcher and icon
docs/            documentation assets
```

## Tests

The parser has its own test suite and needs no extra dependencies:

```bash
ctest --test-dir build --output-on-failure
```

Each test feeds Markdown to the parser and compares the resulting tree with the expected one. On a failure it prints the input, the expected tree and the actual tree.

## Known limitations

- Remote images (`https://...`) are not downloaded; their alt text is shown instead.
- Inline HTML (`<br>`, `<details>`, ...) is shown as plain text.
- Table column alignment and ordered-list start numbers are ignored.
- Developed and tested on Ubuntu.


## Contributing

The project is licensed under MIT license. \
Bug reports and pull requests are welcome. Please run the tests before sending a change, and keep `src/markdown/` free of Qt.

## Acknowledgements

Markdown parsing is done by [md4c](https://github.com/mity/md4c) by Martin Mitáš, which is MIT licensed.