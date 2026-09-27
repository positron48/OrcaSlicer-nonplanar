# Gate A local source audit

Inspected revision: `8500fcdccaa10b5099ac20d252af3a7c560046f1`.
References below are repository-relative files and line numbers at that revision.
This is static evidence, not executed integration or safety qualification.

## Native path and proposed boundaries

| Stage | Actual call/site | Consequence for the new mode |
|---|---|---|
| Orchestration | `src/libslic3r/Print.cpp:2175` `Print::process` | Cached and fresh slicing branches require separate invalidation tests. |
| Slice/body | `src/libslic3r/PrintObject.cpp:453` `make_perimeters` first calls `slice` | Reserve cap before body perimeters; snapshot geometry must survive infill/shell regeneration. |
| Fill | `PrintObject.cpp:560` `prepare_infill`, `:699` `infill` | Capture actual semantic paths after native fill, not full solid CAD. |
| ZAA dispatch | `Print.cpp:2333`, `PrintObject.cpp:748` `need_z_contouring`, `:759` `contour_z` | Existing region `zaa_enabled` drives ZAA after perimeters/infill/ironing. Hybrid must bypass this path explicitly later. |
| ZAA geometry | `src/libslic3r/ContourZ.cpp:33` `contour_extrusion_path`, `:152` | XYZ are scaled integers; Z stores a relative offset `d`, then `z_contoured=true`. |
| Extrusion semantics | `src/libslic3r/ExtrusionEntity.hpp:156` | Native path carries `Polyline3`, width, height and `mm3_per_mm`; these are not the new absolute-volume IR. |
| Travel/reset | `src/libslic3r/GCode.cpp:6348` `GCode::_extrude` | Nominal-Z travel, ZAA start/reset and unretract are generated here; routing new paths through this function would introduce unplanned moves. |
| Flow | `GCode.cpp:6467–6513` | Print/filament/role multipliers affect volume and E. Passing already adjusted volume through this path would repeat corrections. |
| ZAA serialization | `GCode.cpp:7012` and `:7220` | Both branches add nominal Z and multiply E by `(height+z_diff)/height`; neither accepts physical absolute Z directly. |
| Writer | `src/libslic3r/GCodeWriter.cpp:977` `extrude_to_xyz` | Accepts explicit dE; applies plate XY offsets and modal E, and can omit unchanged Z. A06 must test those transforms and rounding from final bytes. |
| Final filters | `GCode.cpp:3658`, `:3760` | TBB pipeline includes spiral vase, pressure equalizer, cooling, fan mover and PA processing depending on settings. |
| Final rewrite | `GCode.cpp:2030` `do_export`, `:2175` `m_processor.finalize(true)` | Existing processor finalization precedes rename at `:2186`; the independent verifier must read bytes after rewrites, not pre-filter strings. |

Initial body hook candidate: snapshot-owned derived body geometry before native
slice/perimeters. This is a proposal pending A05 evidence, not a selected or
implemented partition algorithm. After body generation, a semantic adapter can
produce chronological events; `nominal_layer_label` must never drive sorting.

## Publication routes needing a shared gate

* GUI: `src/slic3r/GUI/BackgroundSlicingProcess.cpp:195` `process_fff`
  calls stock export at `:244`. Cache reuse calls
  `Print::export_gcode_from_previous_file` at `:216` (implementation in
  `Print.cpp:3757`). Both need final-byte verification.
* Local/removable: `BackgroundSlicingProcess.cpp:792` `finalize_gcode` and
  `:857` `export_gcode` copy output through distinct branches. The former can
  run external post-processing. `Plater.cpp:14857` selects removable media.
* Print-host upload: `BackgroundSlicingProcess.cpp:910` `prepare_upload`,
  `:933` post-processing and `:956` queue enqueue. Hybrid must block the route
  before network access, including 3MF upload. No upload was invoked in audit.
* CLI: `src/OrcaSlicer.cpp:6215` exports through `Print::export_gcode`;
  CLI 3MF serialization reaches `store_bbs_3mf` at `:7365`.
* Sliced 3MF: `Plater.cpp:14963` `export_gcode_3mf`, `:15027`
  `SaveStrategy::WithGcode`; `:15551` `export_3mf` reaches the archive writer.
* Archive sink: `src/libslic3r/Format/bbs_3mf.cpp:5937` selects `WithGcode`;
  `:6413` calls `_add_gcode_file_to_archive` (`:8398`). The existing
  `is_sliced_valid` test at `:8413` is not a nonplanar verification report.

This inventory identifies concrete sinks and known callers; B13/C06 still need
dynamic route tests, cloud/UI action audit and proof against every bypass.
Existing GCodeProcessor and MD5 archive metadata are not independent oracles.

## Coordinates and isolation

`src/libslic3r/libslic3r.h:43` selects `int64_t coord_t`. `SCALING_FACTOR` is
mutable (`:70`), normally 1e-6 mm and 1e-5 mm for large beds. It is reassigned
in `src/slic3r/GUI/Plater.cpp:11364`. `Point.hpp:668` truncates conversion to
integer. A03 must snapshot the actual scale, use named checked conversions,
include quantization in the error budget and reject scale changes mid-job.
Do not globally migrate Orca coordinate types.

CLI `--datadir` is registered in `PrintConfig.cpp:10960`, applied in
`OrcaSlicer.cpp:7205`. `GUI_App::init_app_config` (`GUI_App.cpp:2391`) falls
back to the user's standard directory only if it is empty. Baseline launches
must pass an explicit local directory. A separate fork bundle identity/cache
and GUI launch verification remain required; a command-line flag alone does
not close release isolation.

## Native tests/build

`BUILD_TESTS` defaults OFF and is forced OFF for cross builds
(`CMakeLists.txt:167–177`). Native arm64 is required here. The source includes
Catch2 3.11.0 in `tests/catch2`, no Git submodules are declared. Tests use
`orcaslicer_discover_tests` in `tests/CMakeLists.txt`; real selection/execution
must be recorded, never inferred from a successful configure.

The pinned macOS workflow installs automake, texinfo and libtool. The first
local dependency build stopped on missing `makeinfo`; no source or lock was
changed to fix it. Texinfo was installed and the same build resumed.

Additional cached sink: `GCode::do_export` at `GCode.cpp:2040` can return early
when psGCodeExport is already done and the file exists. B13 must gate this
return as well as fresh finalization and the UI copy/upload sinks.
