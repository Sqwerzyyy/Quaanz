#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────
#  Bloomberg Terminal — One-line installer
#  Usage:
#    curl -fsSL https://raw.githubusercontent.com/YOUR_USERNAME/bloomberg-terminal/main/scripts/install.sh | bash
# ─────────────────────────────────────────────────────────────────

set -euo pipefail

REPO="Sqwerzyyy/bloomberg-terminal"
INSTALL_DIR="$HOME/.local/bin"
BUILD_DIR=$(mktemp -d)

BOLD='\033[1m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

info()  { echo -e "${GREEN}▶${NC} $*"; }
warn()  { echo -e "${YELLOW}⚠${NC} $*"; }
error() { echo -e "${RED}✗${NC} $*" >&2; exit 1; }
bold()  { echo -e "${BOLD}$*${NC}"; }

trap 'rm -rf "$BUILD_DIR"' EXIT

bold ""
bold " ██████╗ ██╗      ██████╗  ██████╗ ███╗   ███╗██████╗ ███████╗██████╗  ██████╗"
bold " ██╔══██╗██║     ██╔═══██╗██╔═══██╗████╗ ████║██╔══██╗██╔════╝██╔══██╗██╔════╝"
bold " ██████╔╝██║     ██║   ██║██║   ██║██╔████╔██║██████╔╝█████╗  ██████╔╝██║  ███╗"
bold " ██╔══██╗██║     ██║   ██║██║   ██║██║╚██╔╝██║██╔══██╗██╔══╝  ██╔══██╗██║   ██║"
bold " ██████╔╝███████╗╚██████╔╝╚██████╔╝██║ ╚═╝ ██║██████╔╝███████╗██║  ██║╚██████╔╝"
bold " ╚═════╝ ╚══════╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝╚═════╝ ╚══════╝╚═╝  ╚═╝ ╚═════╝"
bold ""
bold "         T  E  R  M  I  N  A  L       —      C++ edition"
bold ""

# ── OS detection ─────────────────────────────────────────────────
OS="$(uname -s)"
info "Detected OS: $OS"

# ── Check dependencies ───────────────────────────────────────────
need_cmd() { command -v "$1" &>/dev/null || error "Required command not found: $1  (install it first)"; }
need_cmd git
need_cmd cmake

# ── Install ncurses if missing ───────────────────────────────────
if ! pkg-config --exists ncurses 2>/dev/null && ! ldconfig -p 2>/dev/null | grep -q libncurses; then
    warn "ncurses not found — attempting to install…"
    case "$OS" in
        Linux)
            if command -v apt-get &>/dev/null; then
                sudo apt-get update -qq && sudo apt-get install -y libncurses-dev
            elif command -v dnf &>/dev/null; then
                sudo dnf install -y ncurses-devel
            elif command -v pacman &>/dev/null; then
                sudo pacman -Sy --noconfirm ncurses
            else
                error "Cannot install ncurses automatically. Please install ncurses development headers manually."
            fi
            ;;
        Darwin)
            if command -v brew &>/dev/null; then
                brew install ncurses
            else
                error "Homebrew not found. Install it from https://brew.sh/ then re-run this script."
            fi
            ;;
        *) error "Unsupported OS: $OS" ;;
    esac
fi

# ── Clone & build ────────────────────────────────────────────────
info "Cloning repository…"
git clone --depth 1 "https://github.com/$REPO.git" "$BUILD_DIR/repo"

info "Configuring with CMake…"
cmake -S "$BUILD_DIR/repo" -B "$BUILD_DIR/build" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
      -Wno-dev

info "Building (this takes ~10 seconds)…"
cmake --build "$BUILD_DIR/build" --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)"

info "Installing to $INSTALL_DIR…"
mkdir -p "$INSTALL_DIR"
cp "$BUILD_DIR/build/bloomberg-terminal" "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR/bloomberg-terminal"

# ── PATH check ───────────────────────────────────────────────────
bold ""
if echo "$PATH" | grep -q "$INSTALL_DIR"; then
    info "Installation complete!"
else
    warn "$INSTALL_DIR is not in your PATH."
    info "Add this line to your ~/.bashrc or ~/.zshrc:"
    echo ""
    echo '    export PATH="$HOME/.local/bin:$PATH"'
    echo ""
    info "Then restart your terminal or run:"
    echo ""
    echo '    source ~/.bashrc'
    echo ""
fi

bold " ┌──────────────────────────────────────┐"
bold " │   Run:  bloomberg-terminal           │"
bold " │   Keys: 1-5 panels, b/s buy/sell     │"
bold " │         a alerts, q quit             │"
bold " └──────────────────────────────────────┘"
bold ""
