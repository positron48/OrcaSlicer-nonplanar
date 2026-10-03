# Native analysis editing transport, schema 1

The real Orca action is `--nptop-analyze request.json`. Supply an explicit
`--datadir` and ordinary Orca model/settings arguments. It cannot be combined
with another action. The current bounded analysis returns a nonzero exit even
after completing its report: mandatory UNKNOWN checks block export. Progress
appears on stderr as `NPTOP_PROGRESS <stage>`; stdout has one
`NPTOP_DIAGNOSTIC <json>` record after analysis or a bounded input refusal.
Failures in the earlier ordinary Orca loader retain that loader's diagnostics.

This JSON is an editing request, never a PASS credential or a replacement for
the native 3MF project format. The private request factory owns decoded values;
the actual job also captures the exact original JSON and ordinary source bytes.
The existing native model/settings resolver and transforms precede Print.apply.
The selected reservation/ROI are explicit inputs, not automatic full-cap coverage.

Objects require exactly the keys emitted by NativeAnalysisJson. Values are
finite JSON numbers, booleans and unsigned integers; integer IDs retain all
64 bits. Duplicate/unknown keys, unsupported versions, depth above 8, input
above 2 MiB, oversized arrays and lossy float32 vertices refuse. A missing
declaration is not filled by a default. Original vertex/face/property order and
winding remain significant. Array field orders are:

| Field | Ordered values |
|---|---|
| reservation.vertices_f32_mm | `[x,y,z]`, exactly representable float32, absolute coordinate at most 10000 mm |
| reservation.faces | Three unsigned vertex indices |
| reservation.properties | `[face type,area]`; original float64 area and type 0..3 |
| body.plate_origin_mm | `[x,y,z]` |
| body.model | `[id,outer_xy_growth,outer_z_growth,inner_xy_loss,inner_z_loss,coordinate_error]` |
| body.material_ids | `[nominal,upper,lower]` |
| body.reference_ids | `[support,contact]` |
| body.rates | `[deposit speed,travel speed,acceleration]` |
| motion.ids | `[version,profile_id,revision]` |
| motion.commanded_domain_mm, fill_region_mm | `[minimum XYZ,maximum XYZ]` |
| motion axis/drive speed/acceleration | Three XYZ/drive components |
| motion.filament | `[diameter,flow,speed,acceleration,max_retraction]` |
| motion.limits | `[max_volume_rate,max_cross_section,max_event_frequency]` |
| serializer.ids | `[profile_id,revision]` |
| serializer.digits | `[XYZ,E,feed,acceleration,dwell]`, each at most 18 |
| replay.ids | `[policy_id,revision]` |
| replay.tolerances | `[per_record_nominal,total_nominal,filament,relative_dose,absolute_dose]` |
| passes.footprint_mm | `[min_x,min_y,max_x,max_y]` |
| passes.policy | `[count,first_min,first_max,corner_error,later_vertical_min,later_vertical_max,later_normal_min,later_normal_max,total_volume_error]` |
| hatches | `[width,maximum_pitch,boundary_band,direction]`; direction 0 AlongX, 1 AlongY |
| contour | `[width,seam_corner,clockwise,maximum_outside_volume]`; corner 0..3 |

`scene` reuses the exact strict scene/clearance grammar in TravelJson.hpp,
including complete tip/head/obstacle/domain declarations. Motion origin is
0 Synthetic / 1 OperatorMeasured; model is 0 FullStop / 1 UnsupportedLookahead;
kinematics is 0 Cartesian / 1 CoreXY. Parsing an operator claim does not qualify
it. The backend retains simulation restrictions and actual policy checks.

The diagnostic carries the public immutable job, canonical report/manifest and
their hashes. Replay rows reference the actual independently parsed final bytes:
index, kind, start/end XYZ, relative E, feed, outward duration/nominal-volume
bounds and original candidate byte interval. Kinds are 0 XYZ, 1 Pressure,
2 Dwell. Raw candidate bytes and an export credential are absent. Failed work
returns no completed report or partial replay. Policy refusal names the conflicting
parameter and omits its value/code content.

The integration gate creates presets using actual Orca preset option registries.
Empty per-filament code vectors become one empty string, suppressing stock
whitespace defaults without adding code. Plate-only controls use existing CLI
options: `--filament-map-mode Manual --enable-filament-dynamic-map=0
--has-filament-switcher=0`. Boolean CLI values require `=0` in this pinned base.
The CLI uses production NativeAnalysisLimits. Its reference fixture uses those
same defaults, separately from the earlier finer packet-subdivision fixture.

Run `scripts/nonplanar/native_cli_gate.py --help` for the reproducible offline
gate. It uses fresh directories and macOS deny-network/limited-write sandboxing.
The action itself does not supply a hard process/RSS boundary for the ordinary
loader or native work. Isolated GUI execution, full CLI operation matrix, native
3MF extension, complete cap/later passes and full verification remain pending.
