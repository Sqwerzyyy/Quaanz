# 📈 Bloomberg Terminal (C++)

> A feature-rich, Bloomberg-inspired terminal emulator running entirely in your shell — no internet required, no API keys, no subscriptions.

[![Build](https://img.shields.io/badge/build-passing-brightgreen?style=flat-square)](.)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square&logo=cplusplus)](.)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20WSL-lightgrey?style=flat-square)](.)

```
 ██████╗ ██╗      ██████╗  ██████╗ ███╗   ███╗██████╗ ███████╗██████╗  ██████╗
 ██╔══██╗██║     ██╔═══██╗██╔═══██╗████╗ ████║██╔══██╗██╔════╝██╔══██╗██╔════╝
 ██████╔╝██║     ██║   ██║██║   ██║██╔████╔██║██████╔╝█████╗  ██████╔╝██║  ███╗
 ██╔══██╗██║     ██║   ██║██║   ██║██║╚██╔╝██║██╔══██╗██╔══╝  ██╔══██╗██║   ██║
 ██████╔╝███████╗╚██████╔╝╚██████╔╝██║ ╚═╝ ██║██████╔╝███████╗██║  ██║╚██████╔╝
 ╚═════╝ ╚══════╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝╚═════╝ ╚══════╝╚═╝  ╚═╝ ╚═════╝
```

---

## Features

| Panel | Description |
|---|---|
| **Market Watchlist** | Real-time simulation of 20 assets — stocks, ETFs, crypto, FX. Sparkline charts per ticker. |
| **Portfolio Manager** | Buy/sell positions, track P&L, auto-save to CSV on exit. |
| **Price Chart** | ASCII candlestick chart for any symbol with zoom support. |
| **Options Calculator** | Black-Scholes pricing — Call/Put prices, Greeks (Δ, Γ, ν, θ) across 4 expiries. |
| **News Feed** | Simulated live market news with urgency highlighting. |

---

## One-line Install (Linux / macOS / WSL)

```bash
curl -fsSL https://raw.githubusercontent.com/Sqwerzyyy/bloomberg-terminal/main/scripts/install.sh | bash
```

Then run:
```bash
bloomberg-terminal
```

---

## Manual Build

### Prerequisites

| Tool | Version | Install |
|---|---|---|
| g++ or clang++ | ≥ 9 | `sudo apt install build-essential` |
| CMake | ≥ 3.16 | `sudo apt install cmake` |
| ncurses (dev) | any | `sudo apt install libncurses-dev` |

On **macOS**:
```bash
brew install cmake ncurses
```

### Build Steps

```bash
# Clone
git clone https://github.com/Sqwerzyyy/bloomberg-terminal.git
cd bloomberg-terminal

# Configure & build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run directly
./build/bloomberg-terminal

# Or install to ~/.local/bin
cmake --install build --prefix ~/.local
```

---

## Keyboard Controls

| Key | Action |
|---|---|
| `1` | Market Watchlist |
| `2` | Portfolio |
| `3` | Chart |
| `4` | Options Calculator |
| `5` | News Feed |
| `↑` / `↓` | Navigate list |
| `Enter` | Open chart for selected symbol |
| `b` | Buy a position |
| `s` | Sell a position |
| `a` | Set price alert |
| `r` | Reset to SPY chart |
| `q` | Quit (saves portfolio) |

---

## Architecture

```
bloomberg-terminal/
├── include/
│   └── terminal.hpp       # All data structures & class declarations
├── src/
│   ├── main.cpp           # ncurses TUI, all panel rendering
│   └── data.cpp           # MarketData, Portfolio, Black-Scholes, News
├── scripts/
│   └── install.sh         # One-line installer
├── data/                  # portfolio.csv saved here at runtime
└── CMakeLists.txt
```

**Design highlights:**
- Zero external dependencies beyond ncurses and the C++ standard library
- Market simulation uses geometric Brownian motion (random walk) with per-asset volatility profiles
- Portfolio persists as plain CSV — human-readable, zero lock-in
- Black-Scholes implementation from scratch: `d1`, `d2`, `erf`-based normal CDF

---

## Extending

Adding a real data source (e.g. Alpha Vantage, Yahoo Finance) is straightforward — just replace `MarketData::tick()` with an HTTP fetch. A minimal example using `libcurl`:

```cpp
// In MarketData::tick():
// 1. Build URL:  "https://query1.finance.yahoo.com/v8/finance/chart/AAPL?interval=1m&range=1d"
// 2. Parse JSON with nlohmann/json
// 3. Update quotes_ map as normal
```

---

## Screenshots

> Run in any terminal ≥ 100 columns wide. Recommended: iTerm2 / Windows Terminal / Kitty with a dark colorscheme.

```
┌────────────────────────────────────────── BLOOMBERG TERMINAL ──[1] MARKET──[2] PORTFOLIO──…──┐
│ SYMBOL  NAME                      PRICE      CHG    CHG%    HIGH     LOW    VOL   SPARK      │
│ AAPL    Apple Inc              185.24    +1.32  +0.72%  186.10  184.50  12.4M ▃▄▅▆▇▇▆▇     │
│ BTC     Bitcoin USD          68412.00  +842.00  +1.25%  69100   67800   8.2M ▂▃▅▆▇▇▆▅     │
│ NVDA    NVIDIA Corp            875.10   -12.40  -1.40%  892.00  871.30   6.1M ▇▆▅▄▄▃▂▁     │
└───────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## License

MIT © 2024 — free to use, fork, and modify. Pull requests welcome.

---

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/live-api`
3. Commit your changes: `git commit -m 'Add Alpha Vantage support'`
4. Push: `git push origin feature/live-api`
5. Open a Pull Request

Ideas for contributions:
- Live data via Yahoo Finance / Alpha Vantage API
- Candlestick chart with OHLC bars (not just close line)
- Portfolio import from broker CSV (Interactive Brokers, Robinhood)
- Configuration file (`~/.config/bloomberg-terminal/config.toml`)
- Unit tests with Catch2
