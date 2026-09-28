#include "nonplanar_verify/Replay.hpp"
#include <cassert>
int main() {
    auto p = nptop_verify::replay("G90\nM83\nG1 F60\nG1 X1 Z2 E0.1\nG1 X2\n", {0,0,1});
    assert(p.size()==2 && p[1].end[2]==2 && p[1].e==0);
    for (const auto *bad : {"G91", "M82", "G1 Xnan", "G1 X1 X2", "G1 X+", "G1 X.", "G1 E1", "G1 X2 E-1"}) {
        bool rejected = false;
        try { nptop_verify::replay(std::string("G90\nM83\nG1 F60\n")+bad, {0,0,1}); }
        catch (const std::invalid_argument &) { rejected = true; }
        assert(rejected);
    }
}
