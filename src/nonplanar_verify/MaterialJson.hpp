#pragma once
#include "LinearMaterial.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace nptop_verify {
inline nlohmann::json joined_material_document(const JoinedMaterialPolicy &p)
{
 return {{"version",p.version},{"policy_id",p.policy_id},{"revision",p.revision},{"synthetic",p.synthetic},
  {"operator_confirmed_claim",p.operator_confirmed_claim},{"model","common_run_envelope"}};
}
inline JoinedMaterialPolicy parse_joined_material_document(const std::string &text)
{
 using Json=nlohmann::json;std::set<std::string> keys;
 const auto j=Json::parse(text,[&](int depth,Json::parse_event_t event,Json &value){
  if(depth>1)throw std::runtime_error("joined policy nesting");
  if(event==Json::parse_event_t::key && !keys.insert(value.get<std::string>()).second)throw std::runtime_error("duplicate joined key");return true;
 });
 if(!j.is_object() || keys!=std::set<std::string>{"version","policy_id","revision","synthetic","operator_confirmed_claim","model"})throw std::runtime_error("joined registry");
 const auto id=[&](const char *key){const auto &v=j.at(key);if(!v.is_number_unsigned() || !v.get<uint64_t>())throw std::runtime_error("joined id");return v.get<uint64_t>();};
 if(j.at("model").get<std::string>()!="common_run_envelope")throw std::runtime_error("joined model");
 JoinedMaterialPolicy p;p.version=id("version");p.policy_id=id("policy_id");p.revision=id("revision");
 p.synthetic=j.at("synthetic").get<bool>();p.operator_confirmed_claim=j.at("operator_confirmed_claim").get<bool>();return p;
}
inline nlohmann::json material_document(const LinearMaterialSnapshot &m)
{
 const auto &p=m.policy;nlohmann::json policy={{"version",p.version},{"model_id",p.model_id},{"policy_id",p.policy_id},{"revision",p.revision},{"source_revision",p.source_revision},{"source_fingerprint",p.source_fingerprint},{"synthetic",p.synthetic},{"operator_confirmed_claim",p.operator_confirmed_claim},
 {"outer_xy_growth_mm",p.outer_xy_growth_mm},{"outer_z_growth_mm",p.outer_z_growth_mm},{"inner_xy_loss_mm",p.inner_xy_loss_mm},{"inner_z_loss_mm",p.inner_z_loss_mm},{"numerical_coordinate_error_mm",p.numerical_coordinate_error_mm},
 {"max_coordinate_delta_mm",p.max_coordinate_delta_mm},{"max_nominal_delta_mm3",p.max_nominal_delta_mm3},{"max_total_nominal_delta_mm3",p.max_total_nominal_delta_mm3},{"max_filament_delta_mm",p.max_filament_delta_mm},{"relative_dose_error",p.relative_dose_error},{"absolute_dose_error_mm3",p.absolute_dose_error_mm3}};
 nlohmann::json rows=nlohmann::json::array();const char *kinds[]={"deposit","travel","retraction","restore","dwell"};
 for(const auto &r:m.declarations){nlohmann::json section=nullptr;
  if(r.section)section={{"kind",r.section->kind==MaterialSectionKind::Rectangle ? "rectangle" : "rounded_rectangle"},{"gap_begin_mm",r.section->gap_begin_mm},{"gap_end_mm",r.section->gap_end_mm}};
  rows.push_back({{"event_id",r.event_id},{"sequence_index",r.sequence_index},{"kind",kinds[int(r.kind)]},{"start",r.start},{"end",r.end},{"expected_nominal_volume_mm3",r.expected_nominal_volume_mm3},{"expected_filament_mm",r.expected_filament_mm},{"section",section}});
 }
 return {{"version",1},{"policy",policy},{"events",rows}};
}
inline std::pair<LinearMaterialPolicy,std::vector<MaterialDeclaration>> parse_material_document(const std::string &text)
{
 using Json=nlohmann::json;std::vector<std::set<std::string>> objects;
 const auto j=Json::parse(text,[&](int depth,Json::parse_event_t event,Json &v){
  if(depth>5)throw std::runtime_error("material nesting");
  if(event==Json::parse_event_t::object_start)objects.emplace_back();
  if(event==Json::parse_event_t::key && (objects.empty() || !objects.back().insert(v.get<std::string>()).second))throw std::runtime_error("duplicate material key");
  if(event==Json::parse_event_t::object_end)objects.pop_back();return true;
 });
 const auto registry=[](const Json &v,std::initializer_list<const char*> names){std::set<std::string> wanted(names.begin(),names.end()),actual;
  if(!v.is_object())throw std::runtime_error("material object");for(auto i=v.begin();i!=v.end();++i)actual.insert(i.key());if(actual!=wanted)throw std::runtime_error("material registry");};
 const auto id=[](const Json &v){if(!v.is_number_unsigned() || !v.get<uint64_t>())throw std::runtime_error("material id");return v.get<uint64_t>();};
 const auto pose=[](const Json &v){if(!v.is_array() || v.size()!=3)throw std::runtime_error("material axis count");return v.get<std::array<double,3>>();};
 registry(j,{"version","policy","events"});if(id(j.at("version"))!=1)throw std::runtime_error("material version");
 const auto &v=j.at("policy");registry(v,{"version","model_id","policy_id","revision","source_revision","source_fingerprint","synthetic","operator_confirmed_claim","outer_xy_growth_mm","outer_z_growth_mm","inner_xy_loss_mm","inner_z_loss_mm","numerical_coordinate_error_mm","max_coordinate_delta_mm","max_nominal_delta_mm3","max_total_nominal_delta_mm3","max_filament_delta_mm","relative_dose_error","absolute_dose_error_mm3"});
 LinearMaterialPolicy p;p.version=id(v.at("version"));p.model_id=id(v.at("model_id"));p.policy_id=id(v.at("policy_id"));p.revision=id(v.at("revision"));p.source_revision=id(v.at("source_revision"));p.source_fingerprint=v.at("source_fingerprint").get<std::string>();
 p.synthetic=v.at("synthetic").get<bool>();p.operator_confirmed_claim=v.at("operator_confirmed_claim").get<bool>();
 p.outer_xy_growth_mm=v.at("outer_xy_growth_mm").get<double>();p.outer_z_growth_mm=v.at("outer_z_growth_mm").get<double>();p.inner_xy_loss_mm=v.at("inner_xy_loss_mm").get<double>();p.inner_z_loss_mm=v.at("inner_z_loss_mm").get<double>();p.numerical_coordinate_error_mm=v.at("numerical_coordinate_error_mm").get<double>();
 p.max_coordinate_delta_mm=v.at("max_coordinate_delta_mm").get<double>();p.max_nominal_delta_mm3=v.at("max_nominal_delta_mm3").get<double>();p.max_total_nominal_delta_mm3=v.at("max_total_nominal_delta_mm3").get<double>();p.max_filament_delta_mm=v.at("max_filament_delta_mm").get<double>();p.relative_dose_error=v.at("relative_dose_error").get<double>();p.absolute_dose_error_mm3=v.at("absolute_dose_error_mm3").get<double>();
 const auto &events=j.at("events");if(!events.is_array() || events.empty() || events.size()>200000)throw std::runtime_error("material count");
 std::vector<MaterialDeclaration> rows;rows.reserve(events.size());
 for(const auto &r:events){registry(r,{"event_id","sequence_index","kind","start","end","expected_nominal_volume_mm3","expected_filament_mm","section"});
  const auto k=r.at("kind").get<std::string>();MaterialEventKind kind;
  if(k=="deposit")kind=MaterialEventKind::Deposit;else if(k=="travel")kind=MaterialEventKind::Travel;else if(k=="retraction")kind=MaterialEventKind::Retraction;else if(k=="restore")kind=MaterialEventKind::Restore;else if(k=="dwell")kind=MaterialEventKind::Dwell;else throw std::runtime_error("material kind");
  if(!r.at("sequence_index").is_number_unsigned())throw std::runtime_error("material sequence");
  MaterialDeclaration row{id(r.at("event_id")),r.at("sequence_index").get<size_t>(),kind,pose(r.at("start")),pose(r.at("end")),r.at("expected_nominal_volume_mm3").get<double>(),r.at("expected_filament_mm").get<double>(),{}};
  if(!r.at("section").is_null()){const auto &s=r.at("section");registry(s,{"kind","gap_begin_mm","gap_end_mm"});const auto section=s.at("kind").get<std::string>();
   if(section!="rectangle" && section!="rounded_rectangle")throw std::runtime_error("material section kind");
   row.section=MaterialSection{section=="rectangle" ? MaterialSectionKind::Rectangle : MaterialSectionKind::RoundedRectangle,s.at("gap_begin_mm").get<double>(),s.at("gap_end_mm").get<double>()};}
  rows.push_back(std::move(row));
 }
 return {std::move(p),std::move(rows)};
}
}
