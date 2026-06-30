#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────
#  Quaanz Terminal — One-line installer
#  Usage:
#    curl -fsSL https://raw.githubusercontent.com/Sqwerzyyy/-Bloomberg-terminal-/main/scripts/install.sh | bash
# ─────────────────────────────────────────────────────────────────

set -euo pipefail

REPO="Sqwerzyyy/-Bloomberg-terminal-"
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
bold " ██████╗ ██╗   ██╗ █████╗  █████╗ ███╗   ██╗███████╗"
bold "██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚════██║"
bold "██║   ██║██║   ██║███████║███████║██╔██╗ ██║    ██╔╝"
bold "██║   ██║██║   ██║██╔══██║██╔══██║██║╚██╗██║   ██╔╝ "
bold "╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║  ██╔╝  "
bold " ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝╚══════╝"
bold ""
bold "              T  E  R  M  I  N  A  L"
bold ""

# ── OS detection ─────────────────────────────────────────────────
OS="$(uname -s)"
info "Detected OS: $OS"

# ── Check dependencies ───────────────────────────────────────────
need_cmd() { command -v "$1" &>/dev/null || error "Required command not found: $1  (install it first)"; }
need_cmd git
need_cmd cmake
need_cmd curl

# ── Clone & build ────────────────────────────────────────────────
info "Cloning repository…"
git clone --depth 1 "https://github.com/$REPO.git" "$BUILD_DIR/repo"

info "Configuring with CMake…"
cmake -S "$BUILD_DIR/repo" -B "$BUILD_DIR/build" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
      -Wno-dev

info "Building (this takes ~30 seconds on first run — FTXUI is fetched automatically)…"
cmake --build "$BUILD_DIR/build" --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)"

info "Installing to $INSTALL_DIR…"
mkdir -p "$INSTALL_DIR"
cp "$BUILD_DIR/build/quaanz" "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR/quaanz"

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

bold " ┌────────────────────────────────────────────┐"
bold " │   Run:   quaanz                            │"
bold " │   Keys:  1-3 tabs   ↑/↓ watchlist         │"
bold " │          A analysis   T horizon   Q quit   │"
bold " └────────────────────────────────────────────┘"
bold ""
