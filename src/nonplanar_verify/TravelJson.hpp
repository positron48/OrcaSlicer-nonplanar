#pragma once
#include "LinearMaterial.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace nptop_verify {
struct LinearTravelQuery {size_t first_record,record_count;LinearTravelScene scene;};
inline nlohmann::json travel_document(const LinearTravelQuery &q)
{
 const auto &s=q.scene;const auto box=[](const MaterialRegion &b){return nlohmann::json{{"min",b.min},{"max",b.max}};};
 nlohmann::json head=nlohmann::json::array(),obstacles=nlohmann::json::array();
 for(const auto &h:s.head)head.push_back({{"id",h.id},{"role",int(h.role)},{"min",h.local.min},{"max",h.local.max},
  {"moving",h.moving},{"all_configurations_enclosed",h.all_configurations_enclosed}});
 for(const auto &o:s.obstacles)obstacles.push_back(box(o));
 return {{"version",1},{"first_record",q.first_record},{"record_count",q.record_count},{"scene",{
  {"version",s.version},{"profile_id",s.profile_id},{"revision",s.revision},{"synthetic",s.synthetic},{"operator_confirmed_claim",s.operator_confirmed_claim},
  {"tip",{{"center",s.tip_center},{"opening_radius_mm",s.opening_radius_mm},{"outer_radius_mm",s.outer_radius_mm}}},
  {"head",head},{"obstacles",obstacles},{"nozzle_domain",box(s.nozzle_domain)},{"scene_domain",box(s.scene_domain)},
  {"obstacle_inventory_complete",s.obstacle_inventory_complete},{"unmodelled_parts_min_local_z_mm",s.unmodelled_parts_min_local_z_mm},
  {"uncertainty_mm",s.uncertainty_mm},{"clearance_mm",s.clearance_mm}}}};
}
inline LinearTravelQuery parse_travel_document(const std::string &text)
{
 using Json=nlohmann::json;std::vector<std::set<std::string>> objects;
 const auto j=Json::parse(text,[&](int depth,Json::parse_event_t event,Json &v){
  if(depth>6)throw std::runtime_error("travel nesting");if(event==Json::parse_event_t::object_start)objects.emplace_back();
  if(event==Json::parse_event_t::key && (objects.empty() || !objects.back().insert(v.get<std::string>()).second))throw std::runtime_error("duplicate travel key");
  if(event==Json::parse_event_t::object_end)objects.pop_back();return true;
 });
 const auto registry=[](const Json &v,std::initializer_list<const char*> names){std::set<std::string> expected(names.begin(),names.end()),actual;
  if(!v.is_object())throw std::runtime_error("travel object");for(auto i=v.begin();i!=v.end();++i)actual.insert(i.key());if(actual!=expected)throw std::runtime_error("travel registry");};
 const auto id=[](const Json &v){if(!v.is_number_unsigned() || !v.get<uint64_t>())throw std::runtime_error("travel id");return v.get<uint64_t>();};
 const auto number=[](const Json &v){if(!v.is_number() || !std::isfinite(v.get<double>()))throw std::runtime_error("travel finite number");return v.get<double>();};
 const auto pose=[&](const Json &v){if(!v.is_array() || v.size()!=3)throw std::runtime_error("travel axis count");return std::array<double,3>{number(v[0]),number(v[1]),number(v[2])};};
 const auto box=[&](const Json &v){registry(v,{"min","max"});return MaterialRegion{pose(v.at("min")),pose(v.at("max"))};};
 registry(j,{"version","first_record","record_count","scene"});if(id(j.at("version"))!=1)throw std::runtime_error("travel version");
 for(const char *key:{"first_record","record_count"})if(!j.at(key).is_number_unsigned() || j.at(key).get<uint64_t>()>200000)throw std::runtime_error("travel count");
 if(!j.at("record_count").get<uint64_t>())throw std::runtime_error("travel empty block");
 const auto &v=j.at("scene");registry(v,{"version","profile_id","revision","synthetic","operator_confirmed_claim","tip","head","obstacles", "nozzle_domain","scene_domain","obstacle_inventory_complete","unmodelled_parts_min_local_z_mm","uncertainty_mm","clearance_mm"});
 LinearTravelScene s;s.version=id(v.at("version"));s.profile_id=id(v.at("profile_id"));s.revision=id(v.at("revision"));
 s.synthetic=v.at("synthetic").get<bool>();s.operator_confirmed_claim=v.at("operator_confirmed_claim").get<bool>();
 s.obstacle_inventory_complete=v.at("obstacle_inventory_complete").get<bool>();s.unmodelled_parts_min_local_z_mm=number(v.at("unmodelled_parts_min_local_z_mm"));s.uncertainty_mm=number(v.at("uncertainty_mm"));
 s.nozzle_domain=box(v.at("nozzle_domain"));s.scene_domain=box(v.at("scene_domain"));
 const auto &tip=v.at("tip");registry(tip,{"center","opening_radius_mm","outer_radius_mm"});s.tip_center=pose(tip.at("center"));
 s.opening_radius_mm=number(tip.at("opening_radius_mm"));s.outer_radius_mm=number(tip.at("outer_radius_mm"));
 const auto &clearance=v.at("clearance_mm");if(!clearance.is_array() || clearance.size()!=9)throw std::runtime_error("travel clearance count");
 for(size_t i=0;i<9;++i)s.clearance_mm[i]=number(clearance[i]);
 const auto &head=v.at("head"),&obstacles=v.at("obstacles");if(!head.is_array() || head.size()>64 || !obstacles.is_array() || obstacles.size()>10000)throw std::runtime_error("travel inventory count");
 for(const auto &h:head){registry(h,{"id","role","min","max","moving","all_configurations_enclosed"});
  if(!h.at("role").is_number_unsigned() || h.at("role").get<uint64_t>()>=6)throw std::runtime_error("travel head role");
  s.head.push_back({id(h.at("id")),TravelHeadRole(h.at("role").get<unsigned>()),{pose(h.at("min")),pose(h.at("max"))},h.at("moving").get<bool>(),h.at("all_configurations_enclosed").get<bool>()});}
 for(const auto &o:obstacles)s.obstacles.push_back(box(o));
 return {j.at("first_record").get<size_t>(),j.at("record_count").get<size_t>(),std::move(s)};
}
}
