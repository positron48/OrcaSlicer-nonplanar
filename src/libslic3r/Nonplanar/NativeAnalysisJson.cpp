#include "NativeAnalysisJson.hpp"
#include <nonplanar_verify/TravelJson.hpp>
#include <limits>
#include <set>

namespace Slic3r::nptop {
namespace {
using Json=nlohmann::json;
template<class Position> std::array<double,3> xyz(Position p)
{require(std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z()),"NATIVE_ANALYSIS_JSON_FINITE");return {p.x(),p.y(),p.z()};}
template<class Box> Json region(const Box &b){return Json::array({xyz(b.min),xyz(b.max)});}
void registry(const Json &v,std::initializer_list<const char*> keys)
{
    require(v.is_object(),"NATIVE_ANALYSIS_JSON_OBJECT");std::set<std::string> actual,expected(keys.begin(),keys.end());
    for(auto it=v.begin();it!=v.end();++it)actual.insert(it.key());require(actual==expected,"NATIVE_ANALYSIS_JSON_REGISTRY");
}
void array(const Json &v,size_t count){require(v.is_array() && v.size()==count,"NATIVE_ANALYSIS_JSON_ARRAY");}
double number(const Json &v){require(v.is_number() && std::isfinite(v.get<double>()),"NATIVE_ANALYSIS_JSON_FINITE");return v.get<double>();}
uint64_t integer(const Json &v,uint64_t maximum=std::numeric_limits<uint64_t>::max())
{require(v.is_number_unsigned() && v.get<uint64_t>()<=maximum,"NATIVE_ANALYSIS_JSON_INTEGER");return v.get<uint64_t>();}
std::array<double,3> point(const Json &v)
{array(v,3);return {number(v[0]),number(v[1]),number(v[2])};}
PhysicalPosition physical(const Json &v){const auto p=point(v);return {p[0],p[1],p[2]};}
SceneBox scene_box(const Json &v){array(v,2);return {physical(v[0]),physical(v[1])};}
bool flag(const Json &v){require(v.is_boolean(),"NATIVE_ANALYSIS_JSON_BOOLEAN");return v.get<bool>();}
Json scene_document(const SimulationScene &s,const ClearancePolicy &p)
{
    require(s.origin==ProfileOrigin::Synthetic || s.origin==ProfileOrigin::OperatorMeasured,"NATIVE_ANALYSIS_JSON_ORIGIN");
    nptop_verify::LinearTravelScene v;v.version=s.version;v.profile_id=s.profile_id;v.revision=s.revision;
    v.synthetic=s.origin==ProfileOrigin::Synthetic;v.operator_confirmed_claim=s.operator_confirmed_claim;
    v.tip_center=xyz(s.tip.center);v.opening_radius_mm=s.tip.opening_radius.value();v.outer_radius_mm=s.tip.outer_radius.value();
    for(const auto &h:s.head){require(unsigned(h.part)<6,"NATIVE_ANALYSIS_JSON_HEAD_ROLE");
        v.head.push_back({h.id,nptop_verify::TravelHeadRole(h.part),{xyz(h.outer.min),xyz(h.outer.max)},h.moving,h.all_configurations_enclosed});}
    for(const auto &o:s.obstacles)v.obstacles.push_back({xyz(o.min),xyz(o.max)});
    v.nozzle_domain={xyz(s.nozzle_domain.min),xyz(s.nozzle_domain.max)};v.scene_domain={xyz(s.scene_domain.min),xyz(s.scene_domain.max)};
    v.obstacle_inventory_complete=s.obstacle_inventory_complete;v.unmodelled_parts_min_local_z_mm=s.unmodelled_parts_min_local_z.value();v.uncertainty_mm=s.uncertainty.value();
    v.clearance_mm={p.required.value(),p.numeric.import_mm,p.numeric.chord_mm,p.numeric.distance_mm,p.numeric.conversion_mm,
        p.tool_measurement.value(),p.positioning.value(),p.material.value(),p.scene_geometry.value()};
    return nptop_verify::travel_document({0,1,v}).at("scene");
}
}
nlohmann::json native_mesh_document(const indexed_triangle_set &mesh)
{
    Json vertices=Json::array(),faces=Json::array(),properties=Json::array();
    for(const auto &v:mesh.vertices)vertices.push_back(xyz(v));
    for(const auto &f:mesh.indices){for(int i=0;i<3;++i)require(f[i]>=0 && size_t(f[i])<vertices.size(),"NATIVE_ANALYSIS_JSON_FACE_INDEX");
        faces.push_back({uint64_t(f.x()),uint64_t(f.y()),uint64_t(f.z())});}
    for(const auto &p:mesh.properties){require(p.type>=eNormal && p.type<eMaxNumFaceTypes && std::isfinite(p.area),"NATIVE_ANALYSIS_JSON_FACE_PROPERTY");properties.push_back({unsigned(p.type),p.area});}
    return {{"vertices_f32_mm",vertices},{"faces",faces},{"properties",properties}};
}
indexed_triangle_set parse_native_mesh_document(const nlohmann::json &mesh,size_t max_faces)
{
    registry(mesh,{"vertices_f32_mm","faces","properties"});const auto &vertices=mesh.at("vertices_f32_mm"),&faces=mesh.at("faces"),&properties=mesh.at("properties");
    require(max_faces<=200000 && vertices.is_array() && vertices.size()<=3*max_faces && faces.is_array() && faces.size()<=max_faces && properties.is_array() && properties.size()<=faces.size(),"NATIVE_ANALYSIS_JSON_MESH_SIZE");indexed_triangle_set its;
    for(const auto &row:vertices){const auto v=point(row);for(double x:v)require(std::abs(x)<=10000 && double(float(x))==x,"NATIVE_ANALYSIS_JSON_EXACT_FLOAT32");its.vertices.emplace_back(float(v[0]),float(v[1]),float(v[2]));}
    for(const auto &row:faces){array(row,3);std::array<int,3> f{};for(size_t i=0;i<3;++i){auto n=integer(row[i]);require(n<vertices.size(),"NATIVE_ANALYSIS_JSON_FACE_INDEX");f[i]=int(n);}its.indices.emplace_back(f[0],f[1],f[2]);}
    for(const auto &row:properties){array(row,2);its.properties.push_back({EnumFaceTypes(integer(row[0],eMaxNumFaceTypes-1)),number(row[1])});}
    return its;
}
std::string native_analysis_document(const NativeAnalysisRequestSnapshot &input)
{
    const auto &r=input.values;
    const auto &b=r.inputs.body;const auto &m=r.inputs.motion;const auto &s=r.inputs.serializer;const auto &p=r.passes.policy;const auto &replay=r.inputs.replay;
    require(unsigned(m.origin)<=1 && unsigned(m.model)<=1 && unsigned(m.kinematics)<=1,"NATIVE_ANALYSIS_JSON_MOTION_ENUM");
    Json value={{"schema",1},{"millimeters_declared",r.millimeters_declared},{"reservation",native_mesh_document(r.reservation.its)},
        {"body",{{"plate_origin_mm",xyz(b.plate_origin)},{"model",{b.model.model_id,b.model.outer_xy_growth.value(),b.model.outer_z_growth.value(),b.model.inner_xy_loss.value(),b.model.inner_z_loss.value(),b.model.numerical_coordinate_error.value()}},
            {"material_ids",{b.material.nominal.value(),b.material.upper.value(),b.material.lower.value()}},{"reference_ids",{b.support_reference_id,b.contact_reference_id}},
            {"rates",{b.deposition_speed.value(),b.travel_speed.value(),b.acceleration.value()}}}},
        {"scene",scene_document(r.inputs.scene,r.inputs.clearance)},
        {"motion",{{"ids",{m.version,m.profile_id,m.revision}},{"origin",unsigned(m.origin)},{"operator_confirmed_claim",m.operator_confirmed_claim},
            {"model",unsigned(m.model)},{"kinematics",unsigned(m.kinematics)},{"commanded_domain_mm",region(m.commanded_domain)},
            {"axis_speed_mm_s",m.axis_speed_mm_s},{"axis_acceleration_mm_s2",m.axis_acceleration_mm_s2},{"drive_speed_mm_s",m.drive_speed_mm_s},{"drive_acceleration_mm_s2",m.drive_acceleration_mm_s2},
            {"filament",{m.filament_diameter.value(),m.flow.value(),m.filament_speed.value(),m.filament_acceleration.value(),m.max_retraction.value()}},
            {"limits",{m.max_volume_mm3_s,m.max_extrude_cross_section_mm2,m.max_events_per_second}}}},
        {"serializer",{{"ids",{s.profile_id,s.revision}},{"initial_acceleration_mm_s2",s.initial_acceleration.value()},
            {"digits",{s.xyz_digits,s.e_digits,s.feed_digits,s.acceleration_digits,s.dwell_digits}}}},
        {"replay",{{"ids",{replay.policy_id,replay.revision}},{"tolerances",{replay.max_nominal_delta_mm3,replay.max_total_nominal_delta_mm3,replay.max_filament_delta_mm,replay.relative_dose_error,replay.absolute_dose_error_mm3}}}},
        {"passes",{{"footprint_mm",{r.passes.footprint.min_x,r.passes.footprint.min_y,r.passes.footprint.max_x,r.passes.footprint.max_y}},
            {"patch_index",r.passes.patch},{"support_plane_z_mm",r.passes.support_plane_z_mm},
            {"policy",{p.passes,p.first_gap.minimum.value(),p.first_gap.maximum.value(),p.first_gap.corner_height_error.value(),p.later_vertical_minimum.value(),p.later_vertical_maximum.value(),p.later_normal_minimum.value(),p.later_normal_maximum.value(),p.total_volume_error.value()}}}},
        {"hatches",{r.hatches.width.value(),r.hatches.maximum_pitch.value(),r.hatches.boundary_band.value(),unsigned(r.hatches.first_direction)}},
        {"contour",{r.contour.width.value(),r.contour.seam_corner,r.contour.clockwise,r.contour.maximum_outside_target.value()}},{"fill_region_mm",region(r.fill_region)}};
    if(!r.later_paths.empty()){
        value["schema"]=2;value["later_paths"]=Json::array();
        for(const auto &p:r.later_paths)value["later_paths"].push_back({p.pass_index,p.footprint.min_x,p.footprint.min_y,
            p.footprint.max_x,p.footprint.max_y,p.support_plane_z_mm});
    }
    auto text=value.dump();require(text.size()<=2*1024*1024,"NATIVE_ANALYSIS_JSON_BYTE_LIMIT");return text;
}
std::shared_ptr<const NativeAnalysisRequestSnapshot> parse_native_analysis_document(const std::string &text)
{
    require(text.size()<=2*1024*1024,"NATIVE_ANALYSIS_JSON_BYTE_LIMIT");std::vector<std::set<std::string>> objects;
    const auto j=Json::parse(text,[&](int depth,Json::parse_event_t event,Json &v){
        require(depth<=8,"NATIVE_ANALYSIS_JSON_DEPTH");if(event==Json::parse_event_t::object_start)objects.emplace_back();
        if(event==Json::parse_event_t::key)require(!objects.empty() && objects.back().insert(v.get<std::string>()).second,"NATIVE_ANALYSIS_JSON_DUPLICATE_KEY");
        if(event==Json::parse_event_t::object_end)objects.pop_back();return true;
    });
    const auto version=integer(j.at("schema"));require(version==1 || version==2,"NATIVE_ANALYSIS_JSON_VERSION");
    if(version==1)registry(j,{"schema","millimeters_declared","reservation","body","scene","motion","serializer","replay","passes","hatches","contour","fill_region_mm"});
    else registry(j,{"schema","millimeters_declared","reservation","body","scene","motion","serializer","replay","passes","hatches","contour","fill_region_mm","later_paths"});
    std::vector<NextCapPathRequest> later;
    if(version==2){const auto &paths=j.at("later_paths");require(paths.is_array() && !paths.empty() && paths.size()<=4096,"NATIVE_ANALYSIS_JSON_LATER_SIZE");
        for(const auto &p:paths){array(p,6);later.push_back({size_t(integer(p[0],15)),
            {number(p[1]),number(p[2]),number(p[3]),number(p[4])},number(p[5])});}
    }
    auto its=parse_native_mesh_document(j.at("reservation"));
    const auto &b=j.at("body");registry(b,{"plate_origin_mm","model","material_ids","reference_ids","rates"});
    const auto &model=b.at("model"),&ids=b.at("material_ids"),&references=b.at("reference_ids"),&rates=b.at("rates");array(model,6);array(ids,3);array(references,2);array(rates,3);
    BodyMaterialParameters body{physical(b.at("plate_origin_mm")),{integer(model[0]),Length(number(model[1])),Length(number(model[2])),Length(number(model[3])),Length(number(model[4])),Length(number(model[5]))},
        {NominalMaterialId(integer(ids[0])),UpperMaterialId(integer(ids[1])),LowerMaterialId(integer(ids[2]))},Speed(number(rates[0])),Speed(number(rates[1])),Acceleration(number(rates[2])),integer(references[0]),integer(references[1])};
    // Reuse the independent strict scene/clearance grammar; these sentinels
    // only adapt its query wrapper and are never request movement selections.
    const auto v=nptop_verify::parse_travel_document(Json{{"version",1},{"first_record",0},{"record_count",1},{"scene",j.at("scene")}}.dump()).scene;
    require(v.version<=std::numeric_limits<unsigned>::max(),"NATIVE_ANALYSIS_JSON_SCENE_VERSION");
    const auto pos=[](const std::array<double,3> &p){return PhysicalPosition(p[0],p[1],p[2]);};
    const auto box=[&](const nptop_verify::MaterialRegion &r){return SceneBox{pos(r.min),pos(r.max)};};
    SimulationScene scene{unsigned(v.version),v.profile_id,v.revision,v.synthetic ? ProfileOrigin::Synthetic : ProfileOrigin::OperatorMeasured,v.operator_confirmed_claim,
        {{v.tip_center[0],v.tip_center[1],v.tip_center[2]},Length(v.opening_radius_mm),Length(v.outer_radius_mm)}, {},box(v.nozzle_domain),box(v.scene_domain),{},v.obstacle_inventory_complete,Length(v.unmodelled_parts_min_local_z_mm),Length(v.uncertainty_mm)};
    for(const auto &h:v.head)scene.head.push_back({h.id,HeadPart(h.role),{{h.local.min[0],h.local.min[1],h.local.min[2]},{h.local.max[0],h.local.max[1],h.local.max[2]}},h.moving,h.all_configurations_enclosed});
    for(const auto &o:v.obstacles)scene.obstacles.push_back(box(o));const auto &c=v.clearance_mm;
    ClearancePolicy clearance{Length(c[0]),NumericBudget(c[1],c[2],c[3],c[4]),Length(c[5]),Length(c[6]),Length(c[7]),Length(c[8])};
    const auto &m=j.at("motion");registry(m,{"ids","origin","operator_confirmed_claim","model","kinematics","commanded_domain_mm","axis_speed_mm_s","axis_acceleration_mm_s2","drive_speed_mm_s","drive_acceleration_mm_s2","filament","limits"});
    const auto &mi=m.at("ids"),&f=m.at("filament"),&l=m.at("limits");array(mi,3);array(f,5);array(l,3);
    LinearMotionPolicy motion{integer(mi[0]),integer(mi[1]),integer(mi[2]),ProfileOrigin(integer(m.at("origin"),1)),flag(m.at("operator_confirmed_claim")),
        LinearPlannerModel(integer(m.at("model"),1)),LinearKinematics(integer(m.at("kinematics"),1)),scene_box(m.at("commanded_domain_mm")),
        point(m.at("axis_speed_mm_s")),point(m.at("axis_acceleration_mm_s2")),point(m.at("drive_speed_mm_s")),point(m.at("drive_acceleration_mm_s2")),
        Length(number(f[0])),FlowCompensation(number(f[1])),Speed(number(f[2])),Acceleration(number(f[3])),Length(number(f[4])),number(l[0]),number(l[1]),number(l[2])};
    const auto &s=j.at("serializer");registry(s,{"ids","initial_acceleration_mm_s2","digits"});const auto &si=s.at("ids"),&d=s.at("digits");array(si,2);array(d,5);
    LinearCandidatePolicy serializer{integer(si[0]),integer(si[1]),Acceleration(number(s.at("initial_acceleration_mm_s2"))),unsigned(integer(d[0],18)),unsigned(integer(d[1],18)),unsigned(integer(d[2],18)),unsigned(integer(d[3],18)),unsigned(integer(d[4],18))};
    const auto &re=j.at("replay");registry(re,{"ids","tolerances"});const auto &ri=re.at("ids"),&t=re.at("tolerances");array(ri,2);array(t,5);
    LinearMaterialOptions replay{integer(ri[0]),integer(ri[1]),number(t[0]),number(t[1]),number(t[2]),number(t[3]),number(t[4])};
    const auto &p=j.at("passes");registry(p,{"footprint_mm","patch_index","support_plane_z_mm","policy"});const auto &fp=p.at("footprint_mm"),&pp=p.at("policy");array(fp,4);array(pp,9);
    NativeAffinePassRequest passes{{number(fp[0]),number(fp[1]),number(fp[2]),number(fp[3])},size_t(integer(p.at("patch_index"),5000)),number(p.at("support_plane_z_mm")),
        {size_t(integer(pp[0],200000)),{VerticalGap(number(pp[1])),VerticalGap(number(pp[2])),Length(number(pp[3]))},VerticalGap(number(pp[4])),VerticalGap(number(pp[5])),NormalGap(number(pp[6])),NormalGap(number(pp[7])),Volume(number(pp[8]))}};
    const auto &h=j.at("hatches"),&co=j.at("contour");array(h,4);array(co,4);
    return capture_native_analysis_request({{body,std::move(scene),clearance,motion,serializer,replay},flag(j.at("millimeters_declared")),TriangleMesh(std::move(its)),passes,
        {WidthXY(number(h[0])),Length(number(h[1])),Length(number(h[2])),HatchDirection(integer(h[3],1))},
        {WidthXY(number(co[0])),size_t(integer(co[1],3)),flag(co[2]),Volume(number(co[3]))},scene_box(j.at("fill_region_mm")),std::move(later)});
}
const char *native_analysis_stage_name(NativeAnalysisStage stage)
{
    static constexpr const char *names[]={"capture","body","hatches","cap","material","motion","serialize","lineage","replay","admission"};
    return unsigned(stage)<10 ? names[unsigned(stage)] : "unknown";
}
std::string native_analysis_diagnostic(const NativeAnalysisResult &r)
{
    Json result={{"schema",1},{"stage",native_analysis_stage_name(r.stage)},{"reason",r.reason},{"completed",bool(r.snapshot)},{"export_allowed",false},{"job",nullptr},{"report",nullptr},{"replay",Json::array()}};
    if(r.job)result["job"]={{"id",r.job->job_id},{"revision",r.job->input_revision},{"fingerprint",r.job->fingerprint},{"canonical",r.job->canonical_json}};
    if(r.snapshot){const auto &s=*r.snapshot;const auto &report=*s.report;const auto &candidate=*s.plan->candidate;
        result["request_sha256"]=s.request->sha256;result["report"]=Json::parse(report.canonical_json);result["report_sha256"]=report.sha256;
        result["manifest"]=Json::parse(report.binding->manifest_json);result["manifest_sha256"]=report.binding->manifest_sha256;
        result["replay_evaluations"]=s.replay_evaluations;
        if(report.rates)for(size_t i=0;i<report.rates->moves.size();++i){const auto &move=report.rates->moves[i];const auto &step=report.rates->steps[i];
            result["replay"].push_back({{"index",i},{"kind",unsigned(move.kind)},{"start_mm",move.start},{"end_mm",move.end},{"e_mm",move.e},{"feed_mm_min",move.feed},
                {"duration_s",{step.duration.lower,step.duration.upper}},{"nominal_volume_mm3",{step.nominal_volume.lower,step.nominal_volume.upper}},
                {"candidate_byte_range",{candidate.events[i].begin,candidate.events[i].end}}});
        }
    }
    return result.dump();
}
}
