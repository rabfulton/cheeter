# Cheeter

Cheeter is a context-aware cheatsheet overlay daemon for Linux. It runs in the background and, when triggered (default: `ctrl + alt + c`), detects the currently active application and displays a relevant PDF cheatsheet in an overlay.

## Features

- **Context-Aware**: Automatically detects the active window (via X11 or AT-SPI) to show the correct sheets.
- **Overlay UI**: Minimalist, borderless window that appears in the center of the screen.
- **Fast**: Written in C using GLib and GTK+.
- **Configurable**: Simple text-based configuration and mapping files.
- **PDF Support**: Renders cheatsheets directly using Poppler.

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

1.  **Start the Daemon**: Ensure `cheeterd` is running (manually or via systemd).
2.  **Add Cheatsheets**: Place your PDF cheatsheets in `~/.local/share/cheeter/sheets/`.
    *   Example: `cp ~/Downloads/emacs-refcard.pdf ~/.local/share/cheeter/sheets/emacs.pdf`
3.  **Mappings**: Cheeter tries to resolve sheets automatically (e.g., if the app is `emacs`, it looks for `emacs.pdf`). You can explicitly map applications in `~/.config/cheeter/mappings.tsv`.
    *   Format: `KEY` (tab or space) `PATH`
    *   The `KEY` is usually `exe:<binary_name>` or `class:<wm_class>`.
    *   Example content for `mappings.tsv`:
        ```tsv
        exe:code    /home/user/.local/share/cheeter/sheets/vscode.pdf
        exe:firefox /home/user/.local/share/cheeter/sheets/browser-shortcuts.pdf
        ```
4.  **Trigger**: Press the hotkey (default `Super+/` on X11) to toggle the overlay.
    *   You can also toggle via CLI: `cheeter toggle`.

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
