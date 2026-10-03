#pragma once
#include "libslic3r/PrintConfig.hpp"
#include <functional>

namespace Slic3r::GUI {
class Plater;
struct NativeAnalysisHostState {
    DynamicPrintConfig config;
    int plate;
    Vec3d origin;
};
void show_native_analysis_dialog(Plater &, std::function<NativeAnalysisHostState()> capture);
}
