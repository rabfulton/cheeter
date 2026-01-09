# Cheeter

Cheeter is a context-aware cheatsheet overlay daemon for Linux. It runs in the background and, when triggered (default: `ctrl + alt + c`), detects the currently active application and displays a relevant PDF cheatsheet in an overlay.

## Features

- **Context-Aware**: Automatically detects the active window (via X11 or AT-SPI) to show the correct sheets.
- **Overlay UI**: Minimalist, borderless window that appears in the center of the screen.
- **Fast**: Written in C using GLib and GTK+.
- **Configurable**: Simple text-based configuration and mapping files.
- **PDF and PNG Support**: Renders cheatsheets directly using Poppler.

## Dependencies

To build Cheeter, you need a C compiler and the following development libraries:

- `glib-2.0`
- `gtk+-3.0`
- `poppler-glib`
- `at-spi2-core` (or `at-spi-2.0`)
- `libx11` (for X11 backend)

**Arch Linux:**
```bash
sudo pacman -S glib2 gtk3 poppler-glib at-spi2-core libx11
```

**Debian/Ubuntu:**
```bash
sudo apt install libglib2.0-dev libgtk-3-dev libpoppler-glib-dev libatspi-dev libx11-dev
```

## Building

### Manual Build

```bash
make
```
This produces `cheeter` (CLI) and `cheeterd` (Daemon).

### Arch Linux (PKGBUILD)

A `PKGBUILD` is included for generating an Arch package:

```bash
makepkg -si
```

## Installation

You can install the binaries, systemd unit, and desktop entry using:

```bash
sudo make install
```
(Defaults to `/usr/local`. Use `PREFIX=/usr` to override).

### Enable Autostart

To start the daemon automatically with your user session:

```bash
systemctl --user enable --now cheeter.service
```

Alternatively, the `cheeter.desktop` file is installed to `share/applications` and should be visible in your desktop environment's startup settings if configured.

## Usage

4.  **Trigger**: Press the hotkey (default `Super+/` on X11) to toggle the overlay.
    *   You can also toggle via CLI: `cheeter toggle`.

## Controls

| Key | Action |
| :--- | :--- |
| `n` / `Right Arrow` | Next Page |
| `p` / `Left Arrow` | Previous Page |
| `Escape` | Close Overlay |

## Usage
1.  **Start the Daemon**: Ensure `cheeterd` is running (manually or via systemd).
2.  **Add Cheatsheets**: Place your cheatsheets in `~/.local/share/cheeter/sheets/`.
    *   Supported formats: `.pdf`, `.png`, `.jpg`, `.jpeg`.
    *   Example: `cp ~/Downloads/vim-cheat.png ~/.local/share/cheeter/sheets/vim.png`
3.  **Mappings**: Cheeter tries to resolve sheets automatically.
    *   **Terminal Detection**: Cheeter automatically detects applications running *inside* your terminal (e.g., `vim`, `nano`, `python`) so you can simply name your sheet `vim.pdf` or `python.png`.
    *   **Explicit Mapping**: You can manually map applications in `~/.config/cheeter/mappings.tsv`.
        *   Format: `KEY` (tab or space) `PATH`
        *   Example:
            ```tsv
            exe:code    /home/user/.local/share/cheeter/sheets/vscode.pdf
            ```

## Configuration

Configuration is stored in `~/.config/cheeter/config.ini`.

```ini
[core]
# Supported modifiers: Super, Control, Alt, Shift
hotkey=Super+Slash
debug=false
```

## Backends

- **X11**: Fully supported. Uses `XGrabKey` for global hotkeys and `_NET_ACTIVE_WINDOW` for context detection.
- **Wayland**: Experimental/Mock support. Requires AT-SPI for context detection. Global hotkeys must be handled via Compositor shortcuts invoking `cheeter toggle`.

## License

MIT
