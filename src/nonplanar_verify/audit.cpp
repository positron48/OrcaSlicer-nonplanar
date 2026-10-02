#include "LinearRates.hpp"
#include "MaterialJson.hpp"
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
}
int main(int argc,char **argv)
{
    boost::nowide::args utf8(argc,argv);
    const bool material_mode=argc==5 && std::string(argv[1])=="--linear-material-only";
    if(!material_mode && (argc!=4 || std::string(argv[1])!="--linear-rates-only")) {
        std::cerr<<"Usage: nonplanar_rate_audit --linear-rates-only POLICY.json CANDIDATE.txt\n"
                    "       nonplanar_rate_audit --linear-material-only POLICY.json MATERIAL.json CANDIDATE.txt\n"
                    "Numerical component only; complete job and export remain blocked.\n";return 64;
    }
    nptop_verify::LinearRateResult result;nptop_verify::LinearMaterialResult material;
    try {
        std::array<double,3> initial;const auto policy=parse_policy(read_bounded(argv[2],65536),initial);
        const auto bytes=read_bounded(argv[material_mode ? 4 : 3],32*1024*1024);result=nptop_verify::verify_linear_rates(bytes,initial,policy);
        if(material_mode){
            const auto declaration=nptop_verify::parse_material_document(read_bounded(argv[3],32*1024*1024));
            material=nptop_verify::reconstruct_linear_material(result.snapshot,declaration.second,declaration.first);
        }
    } catch(const std::exception &){if(material_mode)material.reason="BOUNDED_MATERIAL_INPUT_ERROR";else result.reason="BOUNDED_RATE_INPUT_ERROR";}
    const auto bounds=[](nptop_verify::RateBounds value) {return nlohmann::json::array({value.lower,value.upper});};
    const auto component_status=material_mode ? material.status : result.status;
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
    std::cout<<report.dump(2)<<'\n';if(!std::cout)return 74;
    return component_status==nptop_verify::RateStatus::Pass ? 0 : component_status==nptop_verify::RateStatus::Fail ? 2 : 3;
}
