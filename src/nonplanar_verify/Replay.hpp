#pragma once
// Independent bounded text replay: no planner, GCodeProcessor or writer API.
#include <array>
#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace nptop_verify {
struct Move { std::array<double, 3> end; double e, feed; };
inline std::vector<Move> replay(const std::string &bytes, std::array<double, 3> initial)
{
    auto check = [](bool ok) { if (!ok) throw std::invalid_argument("unsupported or malformed candidate"); };
    check(bytes.size() <= 2000000);
    for (double v : initial) check(std::isfinite(v) && std::abs(v) <= 10000);
    std::istringstream input(bytes);
    input.imbue(std::locale::classic());
    std::string line;
    bool absolute = false, relative_e = false;
    double feed = 0;
    std::vector<Move> moves;
    while (std::getline(input, line)) {
        check(line.size() <= 256);
        std::istringstream row(line);
        row.imbue(std::locale::classic());
        std::string command, word;
        if (!(row >> command)) continue;
        if (command == "G90" || command == "M83") {
            check(!(row >> word));
            if (command == "G90") absolute = true;
            else relative_e = true;
            continue;
        }
        check(command == "G1" && absolute && relative_e);
        bool seen[5] = {};
        auto end = initial;
        double extrusion = 0;
        while (row >> word) {
            check(word.size() > 1);
            const auto index = std::string("XYZEF").find(word[0]);
            check(index != std::string::npos && !seen[index]);
            seen[index] = true;
            // Restrict decimal grammar: no exponent, nan, hex, suffix or macro.
            size_t j = 1;
            if (word[j] == '-' || word[j] == '+') ++j;
            bool digit = false, dot = false;
            for (; j < word.size(); ++j) {
                if (word[j] == '.' && !dot) dot = true;
                else { check(word[j] >= '0' && word[j] <= '9'); digit = true; }
            }
            check(digit);
            std::istringstream number(word.substr(1));
            number.imbue(std::locale::classic());
            double value = 0;
            check(bool(number >> value) && std::isfinite(value));
            if (index < 3) { check(std::abs(value) <= 10000); end[index] = value; }
            else if (index == 3) { check(value > 0 && value <= 10000); extrusion = value; }
            else { check(value > 0 && value < 100000); feed = value; }
        }
        check(seen[0] || seen[1] || seen[2] || seen[4]);
        if (seen[0] || seen[1] || seen[2]) {
            check(feed > 0 && end != initial && moves.size() < 10000);
            moves.push_back({end, extrusion, feed});
            initial = end;
        } else check(!seen[3]);
    }
    check(absolute && relative_e && !moves.empty());
    return moves;
}
}
