#pragma once
#include "LinearMaterial.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace nptop_verify {
inline nlohmann::json forming_contact_document(const LinearFormingContactModel &m)
{
 return {{"version",m.version},{"model_id",m.model_id},{"revision",m.revision},{"profile_id",m.profile_id},{"profile_revision",m.profile_revision},
  {"material_model_id",m.material_model_id},{"synthetic",m.synthetic},{"operator_confirmed_claim",m.operator_confirmed_claim},
  {"working_radius_mm",m.working_radius_mm},{"wake_length_mm",m.wake_length_mm},{"max_top_above_tip_mm",m.max_top_above_tip_mm},
  {"gap_min_mm",m.gap_min_mm},{"gap_max_mm",m.gap_max_mm},{"width_min_mm",m.width_min_mm},{"width_max_mm",m.width_max_mm},{"max_path_gradient",m.max_path_gradient}};
}
inline LinearFormingContactModel parse_forming_contact_document(const std::string &text)
{
 using Json=nlohmann::json;std::set<std::string> keys;
 const auto j=Json::parse(text,[&](int depth,Json::parse_event_t event,Json &value){
  if(depth>1)throw std::runtime_error("contact nesting");
  if(event==Json::parse_event_t::key && !keys.insert(value.get<std::string>()).second)throw std::runtime_error("duplicate contact key");return true;
 });
 const std::set<std::string> expected{"version","model_id","revision","profile_id","profile_revision","material_model_id","synthetic","operator_confirmed_claim",
  "working_radius_mm","wake_length_mm","max_top_above_tip_mm","gap_min_mm","gap_max_mm","width_min_mm","width_max_mm","max_path_gradient"};
 if(!j.is_object() || keys!=expected)throw std::runtime_error("contact registry");
 const auto id=[&](const char *key){const auto &v=j.at(key);if(!v.is_number_unsigned() || !v.get<uint64_t>())throw std::runtime_error("contact id");return v.get<uint64_t>();};
 const auto number=[&](const char *key){const auto &v=j.at(key);if(!v.is_number() || !std::isfinite(v.get<double>()))throw std::runtime_error("contact finite number");return v.get<double>();};
 LinearFormingContactModel m;m.version=id("version");m.model_id=id("model_id");m.revision=id("revision");m.profile_id=id("profile_id");m.profile_revision=id("profile_revision");m.material_model_id=id("material_model_id");
 if(m.version!=1)throw std::runtime_error("contact version");m.synthetic=j.at("synthetic").get<bool>();m.operator_confirmed_claim=j.at("operator_confirmed_claim").get<bool>();
 m.working_radius_mm=number("working_radius_mm");m.wake_length_mm=number("wake_length_mm");m.max_top_above_tip_mm=number("max_top_above_tip_mm");
 m.gap_min_mm=number("gap_min_mm");m.gap_max_mm=number("gap_max_mm");m.width_min_mm=number("width_min_mm");m.width_max_mm=number("width_max_mm");m.max_path_gradient=number("max_path_gradient");return m;
}
}
