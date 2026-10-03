#include "LinearRates.hpp"
#include "MaterialJson.hpp"
#include "TravelJson.hpp"
#include "FormingContactJson.hpp"
#include "SupportedDepositionJson.hpp"
#include <type_traits>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <boost/nowide/args.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace {
std::string read_bounded(const char *name,size_t maximum)
{
    const auto path=std::filesystem::u8path(name);
    if(!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path)>maximum)throw std::runtime_error("input limit");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("read error");
    std::string bytes;char buffer[8192];
    while(input) {
        input.read(buffer,sizeof buffer);const auto size=size_t(input.gcount());
        if(size>maximum-bytes.size())throw std::runtime_error("input limit");bytes.append(buffer,size);
    }
    if(!input.eof())throw std::runtime_error("read error");return bytes;
}
nptop_verify::LinearRatePolicy parse_policy(const std::string &text,std::array<double,3> &initial)
{
    using Json=nlohmann::json;std::set<std::string> keys;
    const auto j=Json::parse(text,[&](int depth,Json::parse_event_t event,Json &value) {
        if(event==Json::parse_event_t::object_start && depth>0)throw std::runtime_error("nested policy");
        if(event==Json::parse_event_t::key && !keys.insert(value.get<std::string>()).second)throw std::runtime_error("duplicate policy key");return true;
    });
    const std::set<std::string> allowed{"version","profile_id","revision","synthetic","operator_confirmed_claim","model","kinematics",
        "initial_position","position_min","position_max","axis_speed","axis_acceleration","drive_speed","drive_acceleration",
        "initial_acceleration","filament_diameter","flow","filament_speed","filament_acceleration","max_retraction","max_volume_rate","max_cross_section","max_event_rate"};
    if(!j.is_object() || keys!=allowed)throw std::runtime_error("policy registry");
    const auto id=[&](const char *key) {const auto &value=j.at(key);if(!value.is_number_unsigned() || value.get<uint64_t>()==0)throw std::runtime_error("policy id");return value.get<uint64_t>();};
    const auto array3=[&](const char *key) {
        const auto &value=j.at(key);if(!value.is_array() || value.size()!=3)throw std::runtime_error("axis count");
        return value.get<std::array<double,3>>();
    };
    nptop_verify::LinearRatePolicy p;p.version=id("version");p.profile_id=id("profile_id");p.revision=id("revision");
    p.synthetic=j.at("synthetic").get<bool>();p.operator_confirmed_claim=j.at("operator_confirmed_claim").get<bool>();
    const auto model=j.at("model").get<std::string>(),kinematics=j.at("kinematics").get<std::string>();
    if(model!="full_stop" && model!="firmware_lookahead")throw std::runtime_error("model");
    if(kinematics!="corexy" && kinematics!="cartesian")throw std::runtime_error("kinematics");
    p.model=model=="full_stop" ? nptop_verify::RateModel::FullStop : nptop_verify::RateModel::FirmwareLookahead;
    p.kinematics=kinematics=="corexy" ? nptop_verify::RateKinematics::CoreXY : nptop_verify::RateKinematics::Cartesian;
    initial=array3("initial_position");
    p.position_min=array3("position_min");p.position_max=array3("position_max");
    p.axis_speed=array3("axis_speed");p.axis_acceleration=array3("axis_acceleration");
    p.drive_speed=array3("drive_speed");p.drive_acceleration=array3("drive_acceleration");
    p.initial_acceleration=j.at("initial_acceleration").get<double>();p.filament_diameter=j.at("filament_diameter").get<double>();p.flow=j.at("flow").get<double>();
    p.filament_speed=j.at("filament_speed").get<double>();p.filament_acceleration=j.at("filament_acceleration").get<double>();
    p.max_retraction=j.at("max_retraction").get<double>();p.max_volume_rate=j.at("max_volume_rate").get<double>();
    p.max_cross_section=j.at("max_cross_section").get<double>();p.max_event_rate=j.at("max_event_rate").get<double>();return p;
}
struct CoverQuery {
    size_t completed;double progress;nptop_verify::MaterialRepresentation representation;nptop_verify::MaterialRegion region;
};
CoverQuery parse_cover_query(const std::string &text)
{
    using Json=nlohmann::json;std::set<std::string> keys;
    const auto j=Json::parse(text,[&](int depth,Json::parse_event_t event,Json &value) {
        if(depth>2)throw std::runtime_error("cover nesting");
        if(event==Json::parse_event_t::object_start && depth>0)throw std::runtime_error("nested cover query");
        if(event==Json::parse_event_t::key && !keys.insert(value.get<std::string>()).second)throw std::runtime_error("duplicate cover key");return true;
    });
    if(!j.is_object() || keys!=std::set<std::string>{"version","completed_records","current_progress","representation","region_min","region_max"} ||
        !j.at("version").is_number_unsigned() || j.at("version").get<uint64_t>()!=1 || !j.at("completed_records").is_number_unsigned() ||
        j.at("completed_records").get<uint64_t>()>200000 || !j.at("current_progress").is_number())
        throw std::runtime_error("cover registry");
    const auto array3=[&](const char *key) {const auto &a=j.at(key);if(!a.is_array() || a.size()!=3)throw std::runtime_error("cover axis count");return a.get<std::array<double,3>>();};
    const auto r=j.at("representation").get<std::string>();nptop_verify::MaterialRepresentation representation;
    if(r=="nominal")representation=nptop_verify::MaterialRepresentation::Nominal;else if(r=="upper")representation=nptop_verify::MaterialRepresentation::Upper;
    else if(r=="lower")representation=nptop_verify::MaterialRepresentation::Lower;else throw std::runtime_error("cover representation");
    return {j.at("completed_records").get<size_t>(),j.at("current_progress").get<double>(),representation,{array3("region_min"),array3("region_max")}};
}
template<class Result,class Verify> int audit_geometry(char **argv,bool depositing,Verify verify)
{
    using namespace nptop_verify;Result result;std::optional<LinearTravelQuery> query;
    constexpr bool forming=std::is_same_v<Result,LinearFormingContactResult>;std::optional<LinearFormingContactModel> contact;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1000);
    LinearFormingContactLimits limits;limits.cancelled=[&]{return std::chrono::steady_clock::now()>=deadline;};
    try {
        std::array<double,3> initial;const auto policy=parse_policy(read_bounded(argv[2],65536),initial);
        const auto declaration=parse_material_document(read_bounded(argv[3],32*1024*1024));query=parse_travel_document(read_bounded(argv[4],2*1024*1024));
        if constexpr(forming)contact=parse_forming_contact_document(read_bounded(argv[5],65536));
        const auto rates=verify_linear_rates(read_bounded(argv[forming ? 6 : 5],32*1024*1024),initial,policy,limits);
        result.status=rates.status;result.reason=rates.reason;result.evaluations=rates.evaluations;
        if(rates.snapshot){const auto material=reconstruct_linear_material(rates.snapshot,declaration.second,declaration.first,limits);
            result.status=material.status;result.reason=material.reason;result.evaluations=material.evaluations;
            if(material.snapshot){
                if constexpr(forming)result=verify(material.snapshot,query->first_record,query->record_count,query->scene,*contact,limits);
                else result=verify(material.snapshot,query->first_record,query->record_count,query->scene,limits);
            }
        }
        // A preceding component PASS never grants a missing geometry proof.
        if(!result.snapshot && result.status==RateStatus::Pass){result.status=RateStatus::Unknown;result.reason=forming ? "MISSING_FINAL_FORMING_CONTACT_PROOF" : depositing ? "MISSING_FINAL_DEPOSITION_PROOF" : "MISSING_FINAL_TRAVEL_PROOF";}
    }catch(const std::exception &){result.status=RateStatus::Unknown;result.snapshot.reset();result.witness.reset();result.reason=forming ? "BOUNDED_FORMING_CONTACT_INPUT_ERROR" : depositing ? "BOUNDED_DEPOSITION_INPUT_ERROR" : "BOUNDED_TRAVEL_INPUT_ERROR";}
    const char *status=result.status==RateStatus::Pass ? "PASS" : result.status==RateStatus::Fail ? "FAIL" : "UNKNOWN";
    nlohmann::json report={{"schema_version",1},{"component",forming ? "final_byte_forming_contact_geometry" : depositing ? "final_byte_deposition_geometry" : "final_byte_travel_geometry"},{"component_status",status},{"job_status","UNKNOWN"},{"export_allowed",false},
        {"scope",forming ? "complete_declared_forming_run_recent_working_face_contact_rigid_other_upper_head_static_only" : depositing ? "complete_declared_deposit_block_rigid_head_static_growing_actual_upper_only" : "complete_declared_travel_block_fixed_head_static_actual_upper_only"},{"reason",result.reason},{"work",result.evaluations},{"cells",result.cells},
        {"leaves",result.snapshot ? result.snapshot->leaves.size() : 0},{"witness",nullptr},
        {"mandatory_checks_pending",{"deposition_contact","complete_cap_routes","head_material_qualification","job_patch_integrity","firmware_transform","machine_state"}}};
    if(query)report["query"]=travel_document(*query);
    if constexpr(forming){if(contact)report["contact_model"]=forming_contact_document(*contact);
        report["forming_contact_cells"]=result.snapshot ? std::count_if(result.snapshot->leaves.begin(),result.snapshot->leaves.end(),[](const auto &leaf){return leaf.forming_contact;}) : 0;
        if(result.unresolved_cell){const auto &l=*result.unresolved_cell;report["unproved_cell"]={{"record",l.record},{"component",l.component},{"progress",{l.progress.lower,l.progress.upper}},
            {"local_min",l.local.min},{"local_max",l.local.max},{"world_min",l.world.min},{"world_max",l.world.max}};}
    }
    if(result.snapshot)report["prefix_completed_records"]=result.snapshot->prefix->completed_records;
    if(result.witness){const auto &w=*result.witness;report["witness"]={{"record",w.record},{"component",w.component},{"progress",{w.progress.lower,w.progress.upper}},
        {"point_min",w.point.min},{"point_max",w.point.max},{"material_event",w.material_event ? nlohmann::json(*w.material_event) : nlohmann::json(nullptr)},
        {"obstacle",w.obstacle ? nlohmann::json(*w.obstacle) : nlohmann::json(nullptr)}};}
    std::cout<<report.dump(2)<<'\n';if(!std::cout)return 74;return result.status==RateStatus::Pass ? 0 : result.status==RateStatus::Fail ? 2 : 3;
}
int audit_supported_deposition(char **argv)
{
    using namespace nptop_verify;LinearSupportedDepositionResult result;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1000);
    LinearSupportedDepositionLimits limits;limits.cancelled=[&]{return std::chrono::steady_clock::now()>=deadline;};
    try {
        std::array<double,3> initial;const auto policy=parse_policy(read_bounded(argv[2],65536),initial);
        const auto declaration=parse_material_document(read_bounded(argv[3],32*1024*1024));const auto query=parse_travel_document(read_bounded(argv[4],2*1024*1024));
        const auto contact=parse_forming_contact_document(read_bounded(argv[5],65536));
        const auto supported=parse_supported_deposition_document(read_bounded(argv[6],2*1024*1024));
        const auto rates=verify_linear_rates(read_bounded(argv[7],32*1024*1024),initial,policy,limits);
        result.status=rates.status;result.reason=rates.reason;result.evaluations=rates.evaluations;
        if(rates.snapshot){const auto material=reconstruct_linear_material(rates.snapshot,declaration.second,declaration.first,limits);
            result.status=material.status;result.reason=material.reason;result.evaluations=material.evaluations;
            if(material.snapshot)result=verify_linear_supported_deposition(material.snapshot,query.first_record,query.record_count,query.scene,contact,supported,limits);
        }
        if(!result.snapshot && result.status==RateStatus::Pass){result.status=RateStatus::Unknown;result.reason="MISSING_SUPPORTED_DEPOSITION_PROOF";}
    }catch(const std::exception &){result.status=RateStatus::Unknown;result.snapshot.reset();result.geometry_witness.reset();result.support_witness.reset();result.reason="BOUNDED_SUPPORTED_DEPOSITION_INPUT_ERROR";}
    const char *status=result.status==RateStatus::Pass ? "PASS" : result.status==RateStatus::Fail ? "FAIL" : "UNKNOWN";
    nlohmann::json report={{"schema_version",1},{"component","final_byte_supported_deposition"},{"component_status",status},{"job_status","UNKNOWN"},{"export_allowed",false},
        {"scope","complete_declared_forming_block_head_contact_and_pre_block_underlying_gap_lower_anchor_only"},{"reason",result.reason},{"work",result.evaluations},{"cells",result.cells},
        {"support_runs",result.snapshot ? result.snapshot->support.size() : 0},{"geometry_witness",nullptr},{"support_witness",nullptr},
        {"failed_run",result.failed_run ? nlohmann::json(*result.failed_run) : nlohmann::json(nullptr)},
        {"mandatory_checks_pending",{"measured_contact","complete_cap_fill_seams","dose_profile_qualification","full_job_provenance","firmware_transform","machine_state"}}};
    if(result.snapshot){report["geometry_cells"]=result.snapshot->geometry->cells;report["geometry_leaves"]=result.snapshot->geometry->leaves.size();
        report["underlying_completed_records"]=result.snapshot->geometry->first_record;}
    if(result.geometry_witness){const auto &w=*result.geometry_witness;report["geometry_witness"]={{"record",w.record},{"component",w.component},{"progress",{w.progress.lower,w.progress.upper}},
        {"point_min",w.point.min},{"point_max",w.point.max},{"material_event",w.material_event ? nlohmann::json(*w.material_event) : nlohmann::json(nullptr)},
        {"obstacle",w.obstacle ? nlohmann::json(*w.obstacle) : nlohmann::json(nullptr)}};}
    if(result.support_witness){const auto &w=*result.support_witness;report["support_witness"]={{"target_record",w.target_record},{"progress",{w.progress.lower,w.progress.upper}},
        {"transverse",{w.transverse.lower,w.transverse.upper}},{"region_min",w.region.min},{"region_max",w.region.max}};}
    std::cout<<report.dump(2)<<'\n';if(!std::cout)return 74;return result.status==RateStatus::Pass ? 0 : result.status==RateStatus::Fail ? 2 : 3;
}
int audit_run_support(char **argv)
{
    using namespace nptop_verify;LinearRateResult rates;LinearMaterialResult material;LinearMaterialPrefixResult prefix;JoinedMaterialResult joined;LinearRunSupportResult support;
    std::optional<LinearRunSupportQuery> query;const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1000);
    LinearRunSupportLimits limits;limits.cancelled=[&]{return std::chrono::steady_clock::now()>=deadline;};
    const auto refused=[&](const auto &stage){support.status=stage.status;support.reason=stage.reason;support.evaluations=stage.evaluations;};
    try{
        std::array<double,3> initial;const auto rate_policy=parse_policy(read_bounded(argv[2],65536),initial);
        const auto declaration=parse_material_document(read_bounded(argv[3],32*1024*1024));
        const auto join_policy=parse_joined_material_document(read_bounded(argv[4],65536));query=parse_run_support_document(read_bounded(argv[5],65536));
        rates=verify_linear_rates(read_bounded(argv[6],32*1024*1024),initial,rate_policy,limits);refused(rates);
        if(rates.snapshot){material=reconstruct_linear_material(rates.snapshot,declaration.second,declaration.first,limits);refused(material);}
        if(material.snapshot){prefix=linear_material_at(material.snapshot,query->completed_records,query->current_progress,limits);refused(prefix);}
        if(prefix.snapshot){joined=reconstruct_joined_linear_material(prefix.snapshot,join_policy,limits);refused(joined);}
        if(joined.snapshot)support=verify_linear_run_support(joined.snapshot,query->run_index,query->policy,limits);
    }catch(const std::exception &){support.status=RateStatus::Unknown;support.snapshot.reset();support.witness.reset();support.reason="BOUNDED_RUN_SUPPORT_INPUT_ERROR";}
    const char *status=support.status==RateStatus::Pass ? "PASS" : support.status==RateStatus::Fail ? "FAIL" : "UNKNOWN";
    nlohmann::json report={{"schema_version",1},{"component","final_byte_run_support"},{"component_status",status},{"job_status","UNKNOWN"},{"export_allowed",false},
        {"scope","actual_complete_run_footprint_synthetic_nominal_gap_bands_and_lower_anchor_only"},{"reason",support.reason},{"work",support.evaluations},{"cells",support.cells},
        {"support_leaves",support.snapshot ? support.snapshot->leaves.size() : 0},{"witness",nullptr},
        {"mandatory_checks_pending",{"general_support_contact","head_current_contact","complete_cap_routes","dose_profile_qualification","job_patch_integrity","firmware_transform","machine_state"}}};
    if(query)report["query"]=run_support_document(*query);
    if(joined.snapshot){report["records"]=joined.snapshot->source->source->declarations.size();report["runs"]=joined.snapshot->runs.size();}
    if(support.snapshot){report["support_completed_records"]=support.snapshot->support->source->completed_records;report["query_error_mm"]=support.snapshot->query_error_mm;
        size_t lower=0,nominal=0;for(const auto &leaf:support.snapshot->leaves){lower+=leaf.lower_anchor->leaves.size();nominal+=leaf.nominal_terminal->leaves.size();}
        report["lower_anchor_leaves"]=lower;report["nominal_terminal_leaves"]=nominal;}
    if(support.witness){const auto &w=*support.witness;report["witness"]={{"target_record",w.target_record},{"progress",{w.progress.lower,w.progress.upper}},
        {"transverse",{w.transverse.lower,w.transverse.upper}},{"region_min",w.region.min},{"region_max",w.region.max}};}
    std::cout<<report.dump(2)<<'\n';if(!std::cout)return 74;return support.status==RateStatus::Pass ? 0 : support.status==RateStatus::Fail ? 2 : 3;
}
}
int main(int argc,char **argv)
{
    boost::nowide::args utf8(argc,argv);
    if(argc==6 && std::string(argv[1])=="--linear-travel-geometry-only")return audit_geometry<nptop_verify::LinearTravelResult>(argv,false,nptop_verify::verify_linear_travel_geometry);
    if(argc==6 && std::string(argv[1])=="--linear-deposition-geometry-only")return audit_geometry<nptop_verify::LinearDepositionGeometryResult>(argv,true,nptop_verify::verify_linear_deposition_geometry);
    if(argc==7 && std::string(argv[1])=="--linear-forming-contact-geometry-only")return audit_geometry<nptop_verify::LinearFormingContactResult>(argv,true,nptop_verify::verify_linear_forming_contact_geometry);
    if(argc==8 && std::string(argv[1])=="--linear-supported-deposition-only")return audit_supported_deposition(argv);
    if(argc==7 && std::string(argv[1])=="--linear-run-support-only")return audit_run_support(argv);
    const bool nominal_run_mode=argc==7 && std::string(argv[1])=="--linear-material-nominal-run-cover-only";
    const bool joined_mode=nominal_run_mode || (argc==7 && std::string(argv[1])=="--linear-material-joined-cover-only");
    const bool cover_mode=joined_mode || (argc==6 && std::string(argv[1])=="--linear-material-cover-only");
    const bool material_mode=cover_mode || (argc==5 && std::string(argv[1])=="--linear-material-only");
    if(!material_mode && (argc!=4 || std::string(argv[1])!="--linear-rates-only")) {
        std::cerr<<"Usage: nonplanar_rate_audit --linear-rates-only POLICY.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-material-only POLICY.json MATERIAL.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-material-cover-only POLICY.json MATERIAL.json QUERY.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-material-joined-cover-only POLICY.json MATERIAL.json JOIN.json QUERY.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-material-nominal-run-cover-only POLICY.json MATERIAL.json JOIN.json QUERY.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-run-support-only POLICY.json MATERIAL.json JOIN.json SUPPORT_QUERY.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-supported-deposition-only POLICY.json MATERIAL.json GEOMETRY_QUERY.json CONTACT.json SUPPORT.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-travel-geometry-only POLICY.json MATERIAL.json TRAVEL_QUERY.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-deposition-geometry-only POLICY.json MATERIAL.json QUERY.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-forming-contact-geometry-only POLICY.json MATERIAL.json QUERY.json CONTACT.json CANDIDATE.txt\n"
                    "Numerical component only; complete job and export remain blocked.\n";return 64;
    }
    nptop_verify::LinearRateResult result;nptop_verify::LinearMaterialResult material;nptop_verify::MaterialCoverResult cover;
    nptop_verify::JoinedMaterialResult joined;nptop_verify::JoinedMaterialCoverResult joined_cover;
    std::optional<CoverQuery> query;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1000);
    const auto expired=[&]{return std::chrono::steady_clock::now()>=deadline;};
    try {
        std::array<double,3> initial;const auto policy=parse_policy(read_bounded(argv[2],65536),initial);
        const auto bytes=read_bounded(argv[joined_mode ? 6 : cover_mode ? 5 : material_mode ? 4 : 3],32*1024*1024);
        nptop_verify::LinearRateLimits rate_limits;if(cover_mode)rate_limits.cancelled=expired;
        result=nptop_verify::verify_linear_rates(bytes,initial,policy,rate_limits);
        if(material_mode){
            const auto declaration=nptop_verify::parse_material_document(read_bounded(argv[3],32*1024*1024));
            nptop_verify::LinearMaterialLimits material_limits;if(cover_mode)material_limits.cancelled=expired;
            material=nptop_verify::reconstruct_linear_material(result.snapshot,declaration.second,declaration.first,material_limits);
            if(cover_mode){
                query=parse_cover_query(read_bounded(argv[joined_mode ? 5 : 4],65536));
                std::optional<nptop_verify::JoinedMaterialPolicy> joined_policy;
                if(joined_mode){joined_policy=nptop_verify::parse_joined_material_document(read_bounded(argv[4],65536));
                    if(query->representation!=(nominal_run_mode ? nptop_verify::MaterialRepresentation::Nominal : nptop_verify::MaterialRepresentation::Lower))throw std::runtime_error("joined representation");}
                if(!material.snapshot){cover.status=material.status;cover.reason=material.reason;cover.evaluations=std::max(material.evaluations,result.evaluations);}
                else {
                    const auto prefix=nptop_verify::linear_material_at(material.snapshot,query->completed,query->progress,material_limits);
                    nptop_verify::MaterialCoverLimits cover_limits;cover_limits.cancelled=expired;
                    if(prefix.snapshot && joined_mode){
                        nptop_verify::JoinedMaterialLimits joined_limits;joined_limits.cancelled=expired;
                        joined=nptop_verify::reconstruct_joined_linear_material(prefix.snapshot,*joined_policy,joined_limits);
                        if(joined.snapshot)joined_cover=nptop_verify::cover_joined_linear_material(joined.snapshot,query->region,query->representation,joined_limits);
                        else {joined_cover.reason=joined.reason;joined_cover.evaluations=joined.evaluations;}
                    }
                    else if(prefix.snapshot)cover=nptop_verify::cover_linear_material(prefix.snapshot,query->region,query->representation,cover_limits);
                    else {cover.reason=prefix.reason;cover.evaluations=prefix.evaluations;}
                }
                if(joined_mode && !joined.snapshot){joined_cover.status=cover.status;joined_cover.reason=cover.reason.empty() ? joined.reason : cover.reason;
                    joined_cover.evaluations=std::max(joined.evaluations,cover.evaluations);}
            }
        }
    } catch(const std::exception &){if(joined_mode){joined_cover={};joined_cover.reason="BOUNDED_JOINED_MATERIAL_COVER_INPUT_ERROR";joined_cover.evaluations=std::max(material.evaluations,result.evaluations);}
        else if(cover_mode){cover.reason="BOUNDED_MATERIAL_COVER_INPUT_ERROR";cover.evaluations=std::max(material.evaluations,result.evaluations);}
        else if(material_mode)material.reason="BOUNDED_MATERIAL_INPUT_ERROR";else result.reason="BOUNDED_RATE_INPUT_ERROR";}
    const auto bounds=[](nptop_verify::RateBounds value) {return nlohmann::json::array({value.lower,value.upper});};
    const auto component_status=joined_mode ? joined_cover.status : cover_mode ? cover.status : material_mode ? material.status : result.status;
    const char *status=component_status==nptop_verify::RateStatus::Pass ? "PASS" : component_status==nptop_verify::RateStatus::Fail ? "FAIL" : "UNKNOWN";
    nlohmann::json report{{"schema_version",1},{"component","final_byte_linear_rates"},{"component_status",status},{"job_status","UNKNOWN"},
        {"scope","synthetic_identity_transform_full_stop_rates_and_ideal_mechanical_time_only"},{"export_allowed",false},
        {"reason",result.reason},{"record",result.record ? nlohmann::json(*result.record) : nlohmann::json(nullptr)},
        {"axis",result.axis ? nlohmann::json(*result.axis) : nlohmann::json(nullptr)},{"work",result.evaluations},
        {"mandatory_checks_pending",{"material","contact","support","dose_uncertainty","job_integrity","qualified_profile","machine_state"}}};
    if(result.snapshot) {
        const auto &proof=*result.snapshot;report["records"]=proof.moves.size();report["bytes"]=proof.bytes.size();
        report["command_volume_mm3"]=bounds(proof.command_volume);report["nominal_volume_mm3"]=bounds(proof.nominal_volume);
        report["ideal_mechanical_duration_s"]=bounds(proof.duration);
        report["final_pressure_debt_mm"]=bounds(proof.final_pressure_debt);
    }
    if(material_mode){
        report["component"]="final_byte_linear_material";report["scope"]="declared_synthetic_constant_flux_sections_bounds_and_dose_only";
        report["reason"]=material.reason;report["record"]=material.record ? nlohmann::json(*material.record) : nlohmann::json(nullptr);
        report["work"]=material.evaluations;report["rate_component_status"]=result.status==nptop_verify::RateStatus::Pass ? "PASS" : result.status==nptop_verify::RateStatus::Fail ? "FAIL" : "UNKNOWN";
        report["mandatory_checks_pending"]={"material_geometry","contact","support","dose_qualification","job_integrity","qualified_profile","machine_state"};
        if(material.snapshot){const auto &m=*material.snapshot;size_t deposits=0;for(const auto &b:m.beads)deposits+=bool(b);
            report["depositions"]=deposits;report["nominal_volume_mm3"]=bounds(m.nominal_volume);report["declared_delivered_volume_mm3"]=bounds(m.delivered_volume);
            report["maximum_nominal_delta_mm3"]=bounds(m.maximum_nominal_delta_mm3);report["total_nominal_delta_mm3"]=bounds(m.total_nominal_delta_mm3);
        }
    }
    if(cover_mode){
        report["component"]="final_byte_material_region_cover";report["scope"]="actual_prefix_declared_material_solids_and_region_cover_only";
        report["reason"]=cover.reason;report["work"]=cover.evaluations;report["cells"]=cover.cells;report["cover_leaves"]=cover.snapshot ? cover.snapshot->leaves.size() : 0;
        report["uncovered"]=cover.uncovered ? nlohmann::json{{"min",cover.uncovered->min},{"max",cover.uncovered->max}} : nlohmann::json(nullptr);
        if(query){const char *representations[]={"nominal","upper","lower"};report["completed_records"]=query->completed;report["current_progress"]=query->progress;report["representation"]=representations[int(query->representation)];}
        report["mandatory_checks_pending"]={"complete_material_geometry","packet_seams","actual_support_gaps","contact","dose_qualification","job_integrity","qualified_profile","machine_state"};
    }
    if(joined_mode){
        report["component"]=nominal_run_mode ? "final_byte_nominal_run_region_cover" : "final_byte_joined_lower_region_cover";
        report["scope"]=nominal_run_mode ? "actual_prefix_nominal_sections_exact_packet_cuts_only" : "actual_prefix_declared_synthetic_common_run_lower_envelope_only";
        report["reason"]=joined_cover.reason;report["work"]=joined_cover.evaluations;report["cells"]=joined_cover.cells;
        report["cover_leaves"]=joined_cover.snapshot ? joined_cover.snapshot->leaves.size() : 0;
        report["uncovered"]=joined_cover.uncovered ? nlohmann::json{{"min",joined_cover.uncovered->min},{"max",joined_cover.uncovered->max}} : nlohmann::json(nullptr);
        if(joined.snapshot){report["joined_policy"]=nptop_verify::joined_material_document(joined.snapshot->policy);report["runs"]=joined.snapshot->runs.size();}
        if(joined_cover.snapshot){nlohmann::json owners=nlohmann::json::array();for(const auto &leaf:joined_cover.snapshot->leaves){
            const auto &run=joined.snapshot->runs[leaf.run_index];owners.push_back({{"run_index",leaf.run_index},{"first_record",run.first_record},{"last_record",run.last_record},
                {"region_min",leaf.region.min},{"region_max",leaf.region.max}});}report["owners"]=std::move(owners);}
        report["mandatory_checks_pending"]={"general_material_geometry","actual_support_gaps","head_contact","complete_routes","dose_qualification","job_integrity","qualified_profile","machine_state"};
    }
    std::cout<<report.dump(2)<<'\n';if(!std::cout)return 74;
    return component_status==nptop_verify::RateStatus::Pass ? 0 : component_status==nptop_verify::RateStatus::Fail ? 2 : 3;
}
