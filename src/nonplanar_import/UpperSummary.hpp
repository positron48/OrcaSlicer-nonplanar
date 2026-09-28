#pragma once
#include <libslic3r/Nonplanar/StlWorker.hpp>
#include <nlohmann/json.hpp>

namespace Slic3r::nptop {
// One public diagnostic representation for the worker and its CLI consumer.
inline nlohmann::json upper_summary_json(const StlUpperSummary &summary)
{
    return {
        {"upward_faces",summary.upward_faces},{"selected_faces",summary.selected_faces},
        {"patches",summary.patches},{"holes",summary.holes},{"creases",summary.creases},
        {"affine_patches",summary.affine_patches},{"slope_limit",stl_worker_upper_slope_limit},
        {"area_lower_mm2",summary.area_lower_mm2},{"area_upper_mm2",summary.area_upper_mm2},
        {"selected_area_lower_mm2",summary.selected_area_lower_mm2},
        {"selected_area_upper_mm2",summary.selected_area_upper_mm2},
        {"minimum_z_mm",summary.minimum_z_mm},{"maximum_z_mm",summary.maximum_z_mm},
        {"selected_slope_upper",summary.selected_slope_upper}
    };
}
}
