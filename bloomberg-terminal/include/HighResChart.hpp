#pragma once
#include <string>
#include <vector>
#include <deque>
#include "terminal.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

struct BrailleCanvas {
    int cols, rows;
    std::vector<std::vector<uint8_t>> dots;

    BrailleCanvas(int c, int r);
    void clear();
    void set(int px, int py);
    std::string cell(int col, int row) const;
};

struct ChartData {
    std::vector<double> prices;
    std::vector<double> volumes;
    std::string symbol;
    double current_price = 0;
    double change_pct    = 0;
    double high          = 0;
    double low           = 0;
    double open          = 0;
    double volume        = 0;
    double market_cap    = 0;
    double week52_high   = 0;
    double week52_low    = 0;
};

ChartData build_chart_data(const std::string& sym,
                            const std::deque<Candle>& hist,
                            const Quote* q);

std::vector<std::string> render_braille_chart(const ChartData& data,
                                               int width, int height);

ftxui::Element chart_element(ChartData data, ftxui::Color line_color);

std::string render_sparkline(const std::deque<Candle>& hist, int width);