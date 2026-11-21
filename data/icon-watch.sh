#!/bin/sh
# Linux Aurora Player: small background helper that keeps the app icon (dock, menu) in step with
# the system light/dark theme, also while the app is closed. It does nothing else and does not start the app.
# Dark: the dark icon is copied to ~/.local/share/icons (wins over /usr/share). Light: the copy is
# removed, so the light icon of the package is used.
P=linux-aurora-player
SRC=/usr/share/$P/icon-dark.svg
[ -f "$SRC" ] || exit 0
BASE="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor"
DST="$BASE/scalable/apps/$P.svg"

is_dark() {
    case "$(gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null)" in *dark*) return 0 ;; esac
    case "$(gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null)" in *-dark*|*-Dark*) return 0 ;; esac
    return 1
}

apply() {
    if is_dark; then
        cmp -s "$SRC" "$DST" 2>/dev/null || { mkdir -p "$(dirname "$DST")" && cp "$SRC" "$DST"; }
    else
        rm -f "$DST"
    fi
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -f -t "$BASE" 2>/dev/null
    touch "$BASE" "$BASE/.." 2>/dev/null || true  # make the shell re-read the icon theme
}

apply
gsettings monitor org.gnome.desktop.interface 2>/dev/null | while read -r line; do
    case "$line" in *color-scheme*|*gtk-theme*) apply ;; esac
done
