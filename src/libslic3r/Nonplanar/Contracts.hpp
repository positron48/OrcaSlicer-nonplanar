#pragma once

#include "../Point.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <variant>

namespace Slic3r::nptop {

// Internal schema only: there is deliberately no Verified state or exporter.
inline constexpr unsigned ir_version = 1;
enum class Frame { ModelLocal, BuildPlate, MachineCommanded, MachinePhysical, ToolLocal };

inline void require(bool condition, const char *message)
{
    if (!condition) throw std::invalid_argument(message);
}

template<Frame F> class Position {
public:
    Position(double x, double y, double z) : m_x(x), m_y(y), m_z(z)
    { require(std::isfinite(x) && std::isfinite(y) && std::isfinite(z), "nonfinite position"); }
    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }
private:
    double m_x, m_y, m_z;
};
using PhysicalPosition = Position<Frame::MachinePhysical>;

template<class Tag> class Measure {
public:
    explicit Measure(double value) : m_value(value)
    { require(std::isfinite(value) && value >= 0, "invalid nonnegative measure"); }
    double value() const { return m_value; }
private:
    double m_value;
};
using Length = Measure<struct LengthTag>;                   // mm
using WidthXY = Measure<struct WidthXYTag>;                 // mm projected onto XY
using VerticalGap = Measure<struct VerticalGapTag>;         // mm along Z
using NormalGap = Measure<struct NormalGapTag>;             // mm normal to parallel planes
using Volume = Measure<struct VolumeTag>;                   // mm3 deposited, never retraction
using FilamentLength = Measure<struct FilamentLengthTag>;   // mm of filament
using FlowCompensation = Measure<struct FlowCompensationTag>;
using Speed = Measure<struct SpeedTag>;                     // mm/s along physical XYZ
using Acceleration = Measure<struct AccelerationTag>;       // mm/s2 along physical XYZ

class NativeScale {
public:
    static NativeScale capture();
    double mm_per_unit() const { return m_scale; }
    void require_current() const;
    // Euclidean XYZ bounds, excluding import/tessellation or firmware transforms.
    double decode_error_mm() const;
    double encode_error_mm() const;
    static constexpr double max_coordinate_mm = 10000;
private:
    explicit NativeScale(double scale) : m_scale(scale) {}
    double m_scale;
};

template<Frame F> struct DecodedPosition { Position<F> position; double error_mm; };
struct EncodedPosition { Point3 position; double error_mm; };

template<Frame F> DecodedPosition<F> from_native_scaled(const Point3 &point, const NativeScale &scale)
{
    scale.require_current();
    // Checked before integer->double conversion. The domain is below 2^53.
    constexpr coord_t max_native = 10000000000LL;
    for (int i = 0; i < 3; ++i)
        require(point[i] >= -max_native && point[i] <= max_native, "native coordinate outside domain");
    const Vec3d xyz = point.cast<double>() * scale.mm_per_unit();
    require(xyz.cwiseAbs().maxCoeff() <= NativeScale::max_coordinate_mm, "physical coordinate outside domain");
    return {Position<F>(xyz.x(), xyz.y(), xyz.z()), scale.decode_error_mm()};
}

template<Frame F> EncodedPosition to_native_scaled(const Position<F> &point, const NativeScale &scale)
{
    scale.require_current();
    auto convert = [&](double v) -> coord_t {
        require(std::abs(v) <= NativeScale::max_coordinate_mm, "physical coordinate outside domain");
        return static_cast<coord_t>(std::round(v / scale.mm_per_unit()));
    };
    return {Point3(convert(point.x()), convert(point.y()), convert(point.z())), scale.encode_error_mm()};
}

// h00, h10, h01 determine an affine vertical gap over an axis-aligned XY cell.
// h11 is derived, not an independently sampled surface that could hide curvature.
Volume affine_cell_volume(Length x, WidthXY y, VerticalGap h00, VerticalGap h10, VerticalGap h01);
NormalGap normal_gap(VerticalGap gap, double gradient_x, double gradient_y);
FilamentLength filament_feed(Volume volume, Length diameter, FlowCompensation compensation);

template<class Tag> class MaterialId {
public:
    explicit MaterialId(uint64_t value) : m_value(value) { require(value != 0, "missing material identity"); }
    uint64_t value() const { return m_value; }
private:
    uint64_t m_value;
};
using NominalMaterialId = MaterialId<struct NominalTag>;
using UpperMaterialId = MaterialId<struct UpperTag>;
using LowerMaterialId = MaterialId<struct LowerTag>;
struct MaterialIds { NominalMaterialId nominal; UpperMaterialId upper; LowerMaterialId lower; };

// IDs are references, not evidence of containment/support/contact validity.
struct Deposition {
    Volume volume;
    WidthXY width;
    VerticalGap gap_min, gap_max;
    MaterialIds material;
    uint64_t support_provenance_id, contact_model_id;
};
struct Travel {};
enum class RetractionState { Ready, Retracted };
struct Retraction { FilamentLength amount; RetractionState before, after; };
using Payload = std::variant<Travel, Deposition, Retraction>;
struct MotionEvent {
    uint64_t event_id, sequence_index, source_patch_id;
    PhysicalPosition start, end;
    Speed speed_limit;
    Acceleration acceleration_limit;
    Payload payload;
    int nominal_layer_label = -1; // annotation only; never a sorting key or Z source
};
void validate_event(const MotionEvent &event);

struct NumericBudget {
    NumericBudget(double import, double chord, double distance, double conversion)
        : import_mm(import), chord_mm(chord), distance_mm(distance), conversion_mm(conversion) {}
    double import_mm, chord_mm, distance_mm, conversion_mm;
    double total_mm() const;
    void require_conversion(double bound_mm) const;
};

} // namespace Slic3r::nptop
