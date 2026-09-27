#include "Contracts.hpp"

namespace Slic3r::nptop {

NativeScale NativeScale::capture()
{
    require(SCALING_FACTOR == SCALING_FACTOR_INTERNAL || SCALING_FACTOR == SCALING_FACTOR_INTERNAL_LARGE_PRINTER,
            "unsupported native coordinate scale");
    return NativeScale(SCALING_FACTOR);
}

void NativeScale::require_current() const
{
    require(SCALING_FACTOR == m_scale, "native coordinate scale changed since snapshot");
}

double NativeScale::decode_error_mm() const
{
    // Integer conversion is exact in the restricted domain. Four eps * max mm
    // covers binary representation of scale and multiply/divide arithmetic.
    return std::nextafter(std::sqrt(3.) * 4 * std::numeric_limits<double>::epsilon() * max_coordinate_mm,
                          std::numeric_limits<double>::infinity());
}

double NativeScale::encode_error_mm() const
{
    return std::nextafter(std::sqrt(3.) * m_scale / 2 + decode_error_mm(),
                          std::numeric_limits<double>::infinity());
}

Volume affine_cell_volume(Length x, WidthXY y, VerticalGap h00, VerticalGap h10, VerticalGap h01)
{
    const double h11 = h10.value() + h01.value() - h00.value();
    require(x.value() > 0 && y.value() > 0 && h00.value() > 0 && h10.value() > 0 && h01.value() > 0 &&
            std::isfinite(h11) && h11 > 0, "invalid affine deposition cell");
    // The integral of an affine function on a rectangle equals area times its
    // center value. No 3D path-length or surface-normal multiplier belongs here.
    const double v = (x.value() * y.value()) * (h10.value() / 2 + h01.value() / 2);
    require(v > 0, "underflowed cell volume");
    return Volume(v);
}

NormalGap normal_gap(VerticalGap gap, double gradient_x, double gradient_y)
{
    require(std::isfinite(gradient_x) && std::isfinite(gradient_y), "nonfinite plane gradient");
    const double result = gap.value() / std::hypot(1., std::hypot(gradient_x, gradient_y));
    require(gap.value() == 0 || result > 0, "underflowed normal gap");
    return NormalGap(result);
}

FilamentLength filament_feed(Volume volume, Length diameter, FlowCompensation compensation)
{
    require(diameter.value() > 0 && compensation.value() > 0, "invalid filament or flow compensation");
    const double area = std::acos(-1.) * (diameter.value() / 2) * (diameter.value() / 2);
    require(std::isfinite(area) && area > 0, "filament area outside numeric domain");
    const double feed = volume.value() / area * compensation.value();
    require(volume.value() == 0 || feed > 0, "underflowed filament command");
    return FilamentLength(feed);
}

void validate_event(const MotionEvent &event)
{
    require(event.event_id != 0 && event.speed_limit.value() > 0 && event.acceleration_limit.value() > 0,
            "missing event identity or motion limits");
    const bool stationary = event.start.x() == event.end.x() && event.start.y() == event.end.y() &&
                            event.start.z() == event.end.z();
    if (const auto *bead = std::get_if<Deposition>(&event.payload)) {
        require(!stationary && event.source_patch_id != 0 && bead->volume.value() > 0 && bead->width.value() > 0 &&
                bead->gap_min.value() > 0 && bead->gap_max.value() >= bead->gap_min.value() &&
                bead->support_provenance_id != 0 && bead->contact_model_id != 0, "incomplete deposition event");
    } else if (const auto *retract = std::get_if<Retraction>(&event.payload)) {
        const bool transition = (retract->before == RetractionState::Ready && retract->after == RetractionState::Retracted) ||
                                (retract->before == RetractionState::Retracted && retract->after == RetractionState::Ready);
        require(stationary && transition && retract->amount.value() > 0, "invalid retraction transition");
    }
    // Chronology, state continuity, material geometry and limits need a plan
    // validator. Passing this structural check alone never permits export.
}

double NumericBudget::total_mm() const
{
    double total = 0;
    for (double v : {import_mm, chord_mm, distance_mm, conversion_mm}) {
        require(std::isfinite(v) && v >= 0, "invalid numeric budget");
        total = std::nextafter(total + v, std::numeric_limits<double>::infinity());
    }
    require(std::isfinite(total), "numeric budget overflow");
    return total;
}

void NumericBudget::require_conversion(double bound_mm) const
{
    (void) total_mm();
    require(std::isfinite(bound_mm) && bound_mm >= 0 && bound_mm <= conversion_mm, "conversion budget exceeded");
}

} // namespace Slic3r::nptop
