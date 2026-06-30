# Quaanz Terminal

```
 ██████╗ ██╗   ██╗ █████╗  █████╗ ███╗   ██╗███████╗
██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚════██║
██║   ██║██║   ██║███████║███████║██╔██╗ ██║    ██╔╝
██║   ██║██║   ██║██╔══██║██╔══██║██║╚██╗██║   ██╔╝ 
╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║  ██╔╝  
 ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝╚══════╝
              T  E  R  M  I  N  A  L
```

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg) ![FTXUI](https://img.shields.io/badge/UI-FTXUI-orange.svg) ![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)

A Bloomberg-inspired financial terminal that runs entirely inside your command line. It pulls live market data from Yahoo Finance, displays candlestick charts, and lets you chat with an AI assistant about any asset. Built in C++ with no web browser, no Electron, no subscriptions — just a fast terminal application.

> Quaanz is an independent open-source project, inspired by professional financial terminals, and is not affiliated with or endorsed by Bloomberg L.P. or any financial data provider.

![Demo](docs/demo.gif)

---

## Features

- Live prices for 55 assets across 5 sectors: Technology, Finance, Crypto, Commodities, and RWA (real-world assets), fetched directly from Yahoo Finance
- Candlestick chart for any selected asset with dynamic resizing
- Three selectable candle color schemes (chosen at first launch, saved automatically)
- AI chat tab: ask questions about any market, sector, or specific ticker — supports Claude (paid, by Anthropic) and any locally running Ollama model (free)
- Model switcher dropdown inside the app — switch between Claude and Ollama without restarting
- Live news feed with urgency highlighting
- Monte Carlo risk simulation with adjustable volatility and time horizon, probability histogram that fills the full terminal height
- Sector performance overview

---

## Requirements

No external libraries need to be installed manually. The terminal UI library (FTXUI) is downloaded and compiled automatically by CMake the first time you build.

You do need:

**macOS**

- Xcode Command Line Tools (provides `g++`, `git`, and `make`)
- CMake
- `curl` (pre-installed on macOS)

**Linux (Ubuntu or Debian)**

- `build-essential` (provides `g++` and `make`)
- `cmake`
- `git`
- `curl` (used at runtime to fetch market data)

**Windows**

Quaanz runs on Windows through WSL2 (Windows Subsystem for Linux). WSL2 lets you run a full Linux environment inside Windows without a virtual machine. Once WSL is set up, every Linux step below applies exactly.

---

## Installation and build

### macOS

**Step 1 — Install Xcode Command Line Tools**

This installs the C++ compiler and `git`. If you already have Xcode or have done this before, skip it.

```bash
xcode-select --install
```

A dialog will appear asking you to install the tools. Click Install and wait for it to finish.

**Step 2 — Install CMake**

CMake is the build system that compiles the project. If you do not have Homebrew, install it from [brew.sh](https://brew.sh) first.

```bash
brew install cmake
```

**Step 3 — Clone the repository**

This downloads the source code to your computer.

```bash
git clone https://github.com/Sqwerzyyy/-Bloomberg-terminal-.git
cd -Bloomberg-terminal-
```

**Step 4 — Configure the build**

CMake reads the project description and prepares everything for compilation. On first run it will automatically download the FTXUI library — this takes about 10 to 20 seconds depending on your connection.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

**Step 5 — Compile**

This compiles all source files in parallel. First-time compilation takes about 1 to 3 minutes because FTXUI also needs to be compiled.

```bash
cmake --build build --parallel
```

**Step 6 — Run**

```bash
./build/quaanz
```

---

### Linux (Ubuntu / Debian)

**Step 1 — Install dependencies**

```bash
sudo apt update
sudo apt install -y build-essential cmake git curl
```

`build-essential` installs `g++` and `make`. `cmake` is the build system. `curl` is used at runtime to fetch live market data.

**Step 2 — Clone the repository**

```bash
git clone https://github.com/Sqwerzyyy/-Bloomberg-terminal-.git
cd -Bloomberg-terminal-
```

**Step 3 — Configure the build**

CMake will automatically download FTXUI the first time this runs.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

**Step 4 — Compile**

```bash
cmake --build build --parallel
```

**Step 5 — Run**

```bash
./build/quaanz
```

---

### Windows (via WSL2)

WSL2 is a feature built into Windows 10 and 11 that runs a real Linux environment side by side with Windows. You install Linux inside it, and then work in a Linux terminal. No dual boot, no virtual machine to manage.

**Step 1 — Install WSL2 with Ubuntu**

Open PowerShell or Windows Terminal as Administrator and run:

```powershell
wsl --install
```

This installs WSL2 and Ubuntu automatically. Restart your computer when prompted, then open the Ubuntu app from the Start menu and create a username and password.

**Step 2 — From here, follow the Linux steps above**

Once inside the Ubuntu terminal, all subsequent steps are identical to the Linux instructions. Run them in the Ubuntu window, not in PowerShell.

---

## Setting up the AI chat

The AI chat tab works without any setup — it will just tell you that no AI backend is available until you configure one. You have two options.

**What is an environment variable?**
An environment variable is a named value that your terminal makes available to programs when they start. In this case, the app reads a variable called `ANTHROPIC_API_KEY` to know your API key, so you do not have to type it inside the app itself.

---

### Option A: Ollama (free, runs on your computer)

Ollama is a program that downloads and runs AI language models locally on your machine. No internet connection is needed after the model is downloaded, and there is no cost.

**Install Ollama**

On macOS:
```bash
brew install ollama
```

On Linux or WSL: download the installer from [ollama.com](https://ollama.com) and follow the instructions there.

**Download a model**

This downloads the llama3.2 model (about 2 GB). You only need to do this once.

```bash
ollama pull llama3.2
```

**Start the Ollama server**

Ollama needs to be running in the background while you use Quaanz. Open a separate terminal window and run:

```bash
ollama serve
```

Leave that window open. Now start Quaanz in another terminal window — it will detect Ollama automatically and show it in the model dropdown on the AI Terminal tab.

---

### Option B: Claude by Anthropic (paid)

Claude is Anthropic's AI model. Using it requires an API key, which you get from [console.anthropic.com](https://console.anthropic.com). This is a paid API with no free tier — you are charged per request based on the number of tokens processed. For casual use the cost is very low, but it is not zero.

**Set the API key**

Add this line to your `~/.bashrc` or `~/.zshrc` file (replace the placeholder with your actual key):

```bash
export ANTHROPIC_API_KEY=sk-ant-your-key-here
```

Then reload your shell:

```bash
source ~/.bashrc
```

Or you can set it just for the current session without saving:

```bash
export ANTHROPIC_API_KEY=sk-ant-your-key-here
```

Start Quaanz and Claude will appear in the model dropdown.

---

### Switching models inside the app

In the AI Terminal tab (press `2` to open it), there is a dropdown in the top bar showing the active model. Click on it with the mouse to open it and select a different model. Changes take effect immediately.

---

## Keyboard controls

| Key | Where it works | Action |
|---|---|---|
| `1` | Anywhere | Switch to Markets tab |
| `2` | Anywhere | Switch to AI Terminal tab |
| `3` | Anywhere | Switch to Risk tab |
| `←` / `→` | Anywhere | Switch to previous / next tab |
| `Q` | Anywhere | Quit |
| `↑` / `↓` | Markets tab | Move selection up / down in the watchlist |
| `Tab` | Markets tab | Cycle sector filter forward (All → Technology → Finance → …) |
| `Shift+Tab` | Markets tab | Cycle sector filter backward |
| `A` | Markets or Risk tab | Run a quick AI market analysis |
| `+` | Markets or Risk tab | Increase Monte Carlo volatility (sigma) by 5% |
| `-` | Markets or Risk tab | Decrease Monte Carlo volatility (sigma) by 5% |
| `T` | Risk tab | Cycle simulation horizon: 7 → 30 → 90 → 180 days → back to 7 |
| `PgUp` / `PgDn` | AI Terminal tab | Scroll the chat history |
| `F1` / `F2` | AI Terminal tab | Scroll the news feed up / down |
| `Enter` | AI Terminal tab | Send the typed message to the AI |

---

## Tech stack

- **C++20** — the programming language the app is written in
- **FTXUI** — a C++ library for building interactive terminal user interfaces, similar to what web developers use React for but for the command line; fetched and compiled automatically by CMake
- **CMake** — the build system that manages compilation and dependency fetching
- **Yahoo Finance** — the source of live market data, accessed via standard HTTP requests using `curl`
- **Anthropic API** — the cloud API used to call Claude when `ANTHROPIC_API_KEY` is set
- **Ollama** — a locally running program that serves open-source AI models; the app talks to it over a local HTTP connection

---

## Project structure

```
-Bloomberg-terminal-/
├── src/
│   ├── main.cpp           All three tab panels, splash screen, keyboard handling, color scheme selection
│   ├── HighResChart.cpp   Candlestick chart and Monte Carlo histogram — both are custom rendering nodes
│   └── data.cpp           Yahoo Finance fetching, market data storage, AI chat logic, news feed
├── include/
│   ├── terminal.hpp       Data structures shared across all files: Quote, Candle, MarketData, AIAnalyst, etc.
│   ├── HighResChart.hpp   Interface for the chart rendering functions
│   └── json_mini.hpp      A minimal JSON parser with no external dependencies, used to read Yahoo Finance responses
├── scripts/
│   └── install.sh         One-line installer script for Linux and macOS
└── CMakeLists.txt         Build configuration; declares FTXUI as an automatically fetched dependency
```

---

## Troubleshooting

**The AI tab shows "Ollama error. Is `ollama serve` running?"**

The app detected Ollama but cannot reach it. Make sure `ollama serve` is running in a separate terminal window. If you closed it, open a new terminal and run `ollama serve` again, then the app will reconnect on the next request.

**CMake says it cannot find a C++ compiler**

On macOS, run `xcode-select --install` and complete the installation. On Linux, run `sudo apt install build-essential`. Then re-run the `cmake -B build` command.

**A candlestick chart shows "no data"**

The app failed to fetch data from Yahoo Finance for that ticker. Check your internet connection. Some assets may temporarily be unavailable from Yahoo Finance. Navigate to a different asset and try again.

**The build fails with "cmake: command not found"**

CMake is not installed. On macOS: `brew install cmake`. On Linux: `sudo apt install cmake`.

**Colors or characters look wrong in the terminal**

Quaanz uses Unicode block characters and requires a terminal with full Unicode support. On macOS, use Terminal.app or iTerm2. On Linux, most modern terminals work. On Windows, use Windows Terminal (not the old CMD or PowerShell windows) with the WSL Ubuntu profile.

---

## License

MIT. See [LICENSE](LICENSE) for the full text.
