#include "HighResChart.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/box.hpp>

using namespace ftxui;

static const uint8_t BRAILLE_MAP[4][2] = {
    {0x01, 0x08},
    {0x02, 0x10},
    {0x04, 0x20},
    {0x40, 0x80}
};

BrailleCanvas::BrailleCanvas(int c, int r) : cols(c), rows(r) {
    dots.assign(r * 4, std::vector<uint8_t>(c * 2, 0));
}

void BrailleCanvas::clear() {
    for (auto& row : dots)
        for (auto& d : row) d = 0;
}

void BrailleCanvas::set(int px, int py) {
    if (px < 0 || py < 0 || px >= cols*2 || py >= rows*4) return;
    dots[py][px] = 1;
}

std::string BrailleCanvas::cell(int col, int row) const {
    uint8_t mask = 0;
    for (int dr = 0; dr < 4; ++dr)
        for (int dc = 0; dc < 2; ++dc)
            if ((row*4+dr) < (int)dots.size() &&
                (col*2+dc) < (int)dots[row*4+dr].size() &&
                dots[row*4+dr][col*2+dc])
                mask |= BRAILLE_MAP[dr][dc];

    if (mask == 0) return " ";

    uint32_t cp = 0x2800 + mask;
    std::string s;
    if (cp < 0x800) {
        s += (char)(0xC0 | (cp>>6));
        s += (char)(0x80 | (cp&0x3F));
    } else {
        s += (char)(0xE0 | (cp>>12));
        s += (char)(0x80 | ((cp>>6)&0x3F));
        s += (char)(0x80 | (cp&0x3F));
    }
    return s;
}

ChartData build_chart_data(const std::string& sym,
                            const std::deque<Candle>& hist,
                            const Quote* q)
{
    ChartData cd;
    cd.symbol = sym;
    if (q) {
        cd.current_price = q->price;
        cd.change_pct    = q->change_pct;
        cd.high          = q->high;
        cd.low           = q->low;
        cd.open          = q->open;
        cd.volume        = (double)q->volume;
        cd.market_cap    = q->market_cap;
        cd.week52_high   = q->week52_high;
        cd.week52_low    = q->week52_low;
    }
    for (const auto& c : hist) {
        cd.prices.push_back(c.close);
        cd.volumes.push_back((double)c.volume);
    }
    return cd;
}

static void fill_canvas(const ChartData& data, BrailleCanvas& canvas,
                        int width, int height, double& mn_out, double& mx_out)
{
    int pw = width  * 2;
    int ph = height * 4;

    size_t n = std::min((size_t)pw, data.prices.size());
    size_t start = data.prices.size() - n;

    double mn = data.prices[start];
    double mx = data.prices[start];
    for (size_t i = start; i < data.prices.size(); ++i) {
        mn = std::min(mn, data.prices[i]);
        mx = std::max(mx, data.prices[i]);
    }
    if (mx == mn) { mx += mx * 0.01 + 1e-9; mn -= mn * 0.01 + 1e-9; }
    double range = mx - mn;
    mn_out = mn; mx_out = mx;

    int prev_py = -1;
    for (size_t i = 0; i < n; ++i) {
        double p = data.prices[start + i];
        int px = (int)(i * (pw - 1) / std::max((size_t)1, n - 1));
        int py = ph - 1 - (int)((p - mn) / range * (ph - 1));
        py = std::max(0, std::min(ph - 1, py));
        canvas.set(px, py);
        if (prev_py >= 0 && px > 0) {
            int y1 = std::min(py, prev_py);
            int y2 = std::max(py, prev_py);
            for (int y = y1; y <= y2; ++y) canvas.set(px, y);
        }
        prev_py = py;
    }
}

namespace {
class ChartNode : public Node {
public:
    ChartNode(ChartData data, Color color)
        : data_(std::move(data)), color_(color) {}

    void ComputeRequirement() override {
        requirement_.min_x = 24;
        requirement_.min_y = 6;
        requirement_.flex_grow_x = 1;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_x = 1;
        requirement_.flex_shrink_y = 1;
    }

    void Render(Screen& screen) override {
        int bx0 = box_.x_min, bx1 = box_.x_max;
        int by0 = box_.y_min, by1 = box_.y_max;
        int total_w = bx1 - bx0 + 1;
        int total_h = by1 - by0 + 1;
        if (total_w < 4 || total_h < 2) return;

        int label_w = 9;
        int width  = total_w - label_w;
        int height = total_h - 1;
        if (width < 4 || height < 1) { width = std::max(1, total_w); height = std::max(1, total_h); label_w = 0; }

        if (data_.prices.empty()) {
            std::string msg = "no data";
            int mxp = bx0 + std::max(0, (total_w - (int)msg.size()) / 2);
            int myp = by0 + total_h / 2;
            for (size_t i = 0; i < msg.size() && mxp + (int)i <= bx1; ++i) {
                Pixel& px = screen.PixelAt(mxp + (int)i, myp);
                px.character = std::string(1, msg[i]);
                px.foreground_color = Color::GrayDark;
            }
            return;
        }

        BrailleCanvas canvas(width, height);
        double mn = 0, mx = 0;
        fill_canvas(data_, canvas, width, height, mn, mx);
        double range = mx - mn;

        for (int row = 0; row < height; ++row) {
            int sy = by0 + row;
            if (sy > by1) break;
            if (label_w > 0) {
                double price_at_row = (height > 1)
                    ? mx - (double)row / (height - 1) * range
                    : mx;
                std::ostringstream lbl;
                lbl << std::fixed << std::setprecision(2)
                    << std::setw(label_w - 1) << price_at_row;
                std::string ls = lbl.str();
                if ((int)ls.size() > label_w - 1)
                    ls = ls.substr(0, label_w - 1);
                for (int c = 0; c < (int)ls.size() && bx0 + c <= bx1; ++c) {
                    Pixel& px = screen.PixelAt(bx0 + c, sy);
                    px.character = std::string(1, ls[c]);
                    px.foreground_color = Color::GrayDark;
                }
                int sepx = bx0 + label_w - 1;
                if (sepx <= bx1) {
                    Pixel& px = screen.PixelAt(sepx, sy);
                    px.character = "\u2502";
                    px.foreground_color = Color::GrayDark;
                }
            }
            for (int col = 0; col < width; ++col) {
                int sx = bx0 + label_w + col;
                if (sx > bx1) break;
                std::string g = canvas.cell(col, row);
                if (g == " ") continue;
                Pixel& px = screen.PixelAt(sx, sy);
                px.character = g;
                px.foreground_color = color_;
            }
        }

        int axis_y = by0 + height;
        if (axis_y <= by1) {
            if (label_w > 0) {
                int corner = bx0 + label_w - 1;
                if (corner <= bx1) {
                    Pixel& px = screen.PixelAt(corner, axis_y);
                    px.character = "\u2514";
                    px.foreground_color = Color::GrayDark;
                }
            }
            for (int col = 0; col < width; ++col) {
                int sx = bx0 + label_w + col;
                if (sx > bx1) break;
                Pixel& px = screen.PixelAt(sx, axis_y);
                px.character = "\u2500";
                px.foreground_color = Color::GrayDark;
            }
        }
    }

private:
    ChartData data_;
    Color     color_;
};
}

Element chart_element(ChartData data, Color line_color) {
    return std::make_shared<ChartNode>(std::move(data), line_color);
}

std::vector<std::string> render_braille_chart(const ChartData& data,
                                               int width, int height)
{
    std::vector<std::string> lines;
    if (data.prices.empty() || width < 4 || height < 3) {
        for (int i = 0; i < height; ++i) lines.push_back("");
        return lines;
    }

    BrailleCanvas canvas(width, height);
    double mn = 0, mx = 0;
    fill_canvas(data, canvas, width, height, mn, mx);
    double range = mx - mn;

    int label_w = 9;
    for (int row = 0; row < height; ++row) {
        std::string line;
        double price_at_row = mx - (double)row / (height - 1) * range;
        std::ostringstream lbl;
        lbl << std::fixed << std::setprecision(2) << std::setw(label_w - 1) << price_at_row << "\u2502";
        line += lbl.str();
        for (int col = 0; col < width; ++col)
            line += canvas.cell(col, row);
        lines.push_back(line);
    }

    std::string xaxis(label_w, ' ');
    xaxis += "\u2514";
    for (int i = 1; i < width; ++i) xaxis += "\u2500";
    lines.push_back(xaxis);

    return lines;
}

std::string render_sparkline(const std::deque<Candle>& hist, int width) {
    if (hist.empty() || width < 2) return std::string(width, ' ');
    static const char* SPARKS[] = {"▁","▂","▃","▄","▅","▆","▇","█"};
    size_t n = std::min((size_t)width, hist.size());
    double mn = 1e18, mx = -1e18;
    for (size_t i = hist.size()-n; i < hist.size(); ++i) {
        mn = std::min(mn, hist[i].low);
        mx = std::max(mx, hist[i].high);
    }
    if (mx == mn) mx = mn + 0.01;
    std::string s;
    for (size_t i = hist.size()-n; i < hist.size(); ++i) {
        int bar = (int)((hist[i].close - mn) / (mx - mn) * 7.0 + 0.5);
        bar = std::max(0, std::min(7, bar));
        s += SPARKS[bar];
    }
    return s;
}