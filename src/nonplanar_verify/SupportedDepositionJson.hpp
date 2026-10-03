#pragma once
#include "MaterialJson.hpp"
namespace nptop_verify {
inline nlohmann::json supported_deposition_document(const LinearSupportedDepositionPolicy &p)
{
 nlohmann::json runs=nlohmann::json::array();for(const auto &r:p.runs)
  runs.push_back({{"first_record",r.first_record},{"last_record",r.last_record},{"policy",run_support_document({0,0,0,r.policy}).at("policy")}});
 return {{"version",p.version},{"policy_id",p.policy_id},{"revision",p.revision},{"join",joined_material_document(p.join)},{"runs",runs}};
}
inline LinearSupportedDepositionPolicy parse_supported_deposition_document(const std::string &text)
{
 using Json=nlohmann::json;std::vector<std::set<std::string>> objects;
 const auto j=Json::parse(text,[&](int depth,Json::parse_event_t event,Json &value){
  if(depth>4)throw std::runtime_error("supported deposition nesting");
  if(event==Json::parse_event_t::object_start)objects.emplace_back();
  if(event==Json::parse_event_t::key && (objects.empty() || !objects.back().insert(value.get<std::string>()).second))throw std::runtime_error("duplicate supported deposition key");
  if(event==Json::parse_event_t::object_end)objects.pop_back();return true;
 });
 const auto registry=[](const Json &v,std::initializer_list<const char*> names){std::set<std::string> expected(names.begin(),names.end()),actual;
  if(!v.is_object())throw std::runtime_error("supported deposition object");for(auto i=v.begin();i!=v.end();++i)actual.insert(i.key());
  if(actual!=expected)throw std::runtime_error("supported deposition registry");};
 const auto id=[](const Json &v){if(!v.is_number_unsigned() || !v.get<uint64_t>())throw std::runtime_error("supported deposition id");return v.get<uint64_t>();};
 const auto record=[](const Json &v){if(!v.is_number_unsigned() || v.get<uint64_t>()>=200000)throw std::runtime_error("supported deposition record");return v.get<size_t>();};
 registry(j,{"version","policy_id","revision","join","runs"});LinearSupportedDepositionPolicy p;
 p.version=id(j.at("version"));if(p.version!=1)throw std::runtime_error("supported deposition version");p.policy_id=id(j.at("policy_id"));p.revision=id(j.at("revision"));
 p.join=parse_joined_material_document(j.at("join").dump());const auto &runs=j.at("runs");
 if(!runs.is_array() || runs.empty() || runs.size()>200000)throw std::runtime_error("supported deposition runs");
 for(const auto &r:runs){registry(r,{"first_record","last_record","policy"});const auto first=record(r.at("first_record")),last=record(r.at("last_record"));
  if(last<first)throw std::runtime_error("supported deposition reversed records");
  const auto &v=r.at("policy");for(const char *key:{"cross_slope","vertical_min","vertical_max","normal_min","normal_max"})
   if(!v.at(key).is_number() || !std::isfinite(v.at(key).get<double>()))throw std::runtime_error("supported deposition finite number");
  const Json query={{"version",1},{"completed_records",0},{"current_progress",0},{"run_index",0},{"policy",v}};
  p.runs.push_back({first,last,parse_run_support_document(query.dump()).policy});
 }
 return p;
}
}
