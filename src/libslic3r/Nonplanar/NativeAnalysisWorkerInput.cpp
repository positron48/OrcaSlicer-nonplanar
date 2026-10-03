#include "NativeAnalysisWorker.hpp"
#include "NativeAnalysisView.hpp"
#include "Canonical.hpp"
#include "../Print.hpp"
#include <nlohmann/json.hpp>
#include <charconv>
#include <climits>
#include <cstring>
#include <set>

namespace Slic3r::nptop {
namespace {
using Json=nlohmann::json;
void keys(const Json &v,std::initializer_list<const char*> expected)
{require(v.is_object(),"WORKER_OBJECT");std::set<std::string> actual;for(auto it=v.begin();it!=v.end();++it)actual.insert(it.key());require(actual==std::set<std::string>(expected.begin(),expected.end()),"WORKER_KEYS");}
void count(const Json &v,size_t n){require(v.is_array() && v.size()==n,"WORKER_ARRAY");}
uint64_t natural(const Json &v){require(v.is_number_unsigned(),"WORKER_UNSIGNED");return v.get<uint64_t>();}
int integer(const Json &v)
{require(v.is_number_integer() && !v.is_boolean() && (v.is_number_unsigned() ? v.get<uint64_t>()<=uint64_t(INT_MAX) : v.get<int64_t>()>=INT_MIN && v.get<int64_t>()<=INT_MAX),"WORKER_INTEGER");return v.get<int>();}
bool flag(const Json &v){require(v.is_boolean(),"WORKER_BOOLEAN");return v.get<bool>();}
std::string unhex(const Json &v)
{
    require(v.is_string(),"WORKER_HEX");const auto s=v.get<std::string>();require(s.size()%2==0 && s.size()<=native_analysis_worker_byte_limit,"WORKER_HEX_SIZE");std::string result;result.reserve(s.size()/2);
    const auto nibble=[](char c){require((c>='0' && c<='9') || (c>='a' && c<='f'),"WORKER_HEX_DIGIT");return c<='9' ? c-'0' : c-'a'+10;};
    for(size_t i=0;i<s.size();i+=2)result.push_back(char(16*nibble(s[i])+nibble(s[i+1])));return result;
}
Json hex(const std::string &s)
{require(s.size()<=native_analysis_worker_byte_limit/2,"WORKER_FILE_BYTE_LIMIT");std::string out;out.reserve(2*s.size());for(unsigned char c:s){out.push_back("0123456789abcdef"[c>>4]);out.push_back("0123456789abcdef"[c&15]);}return out;}
double number(const Json &v)
{require(v.is_string() && v.get_ref<const std::string&>().size()==16,"WORKER_DOUBLE_BITS");const auto bytes=unhex(v);uint64_t bits=0;for(unsigned char c:bytes)bits=(bits<<8)|c;double d;std::memcpy(&d,&bits,8);return d;}
Vec2d point2(const Json &v){count(v,2);return {number(v[0]),number(v[1])};}
Vec3d point3(const Json &v){count(v,3);return {number(v[0]),number(v[1]),number(v[2])};}
Transform3d matrix(const Json &v)
{count(v,16);Transform3d t;for(int r=0;r<4;++r)for(int c=0;c<4;++c)t.matrix()(r,c)=number(v[4*r+c]);return t;}
template<class T,class Decode> std::vector<T> vector(const Json &v,Decode decode,size_t limit=200000)
{require(v.is_array() && v.size()<=limit,"WORKER_VECTOR_LIMIT");std::vector<T> result;result.reserve(v.size());for(const auto &row:v)result.push_back(decode(row));return result;}
// Canonical v1 distinguishes native scalar enums from generic dictionary enums.
// This private transport implementation retains that existing representation
// and integer meaning; actual PrintConfig consumers still create native types.
class NativeEnum final : public ConfigOptionInt {
    t_config_enum_values names;
public:
    NativeEnum(int value,const t_config_enum_values &map):ConfigOptionInt(value),names(map){}
    ConfigOptionType type() const override{return coEnum;}
    ConfigOption *clone() const override{return new NativeEnum(*this);}
    void set(const ConfigOption *other) override{require(other->type()==coEnum,"WORKER_ENUM_TYPE");value=other->getInt();}
    std::string serialize() const override{for(const auto &entry:names)if(entry.second==value)return entry.first;throw ConfigurationError("WORKER_ENUM_VALUE");}
};
struct Decoder {
    std::vector<std::unique_ptr<t_config_enum_values>> dictionaries;
    const t_config_enum_values *dictionary(const Json &v)
    {
        if(v.is_null())return nullptr;auto values=std::make_unique<t_config_enum_values>();
        require(v.is_array() && v.size()<=4096,"WORKER_ENUM_DICTIONARY");
        for(const auto &row:v){count(row,2);require(values->emplace(unhex(row[0]),integer(row[1])).second,"WORKER_ENUM_DUPLICATE");}
        const auto *ptr=values.get();dictionaries.push_back(std::move(values));return ptr;
    }
    DynamicPrintConfig config(const Json &v)
    {
        keys(v,{"options","schema"});require(natural(v.at("schema"))==1,"WORKER_CONFIG_VERSION");const auto &rows=v.at("options");require(rows.is_array() && rows.size()<=4096,"WORKER_CONFIG_COUNT");DynamicPrintConfig result;
        for(const auto &row:rows){count(row,4);const auto key=unhex(row[0]);require(!result.has(key),"WORKER_CONFIG_DUPLICATE");const auto *def=print_config_def.get(key);require(def && int(def->type)==integer(row[1]),"WORKER_CONFIG_TYPE");
            const bool nullable=flag(row[2]);std::unique_ptr<ConfigOption> opt(def->create_default_option());const auto &value=row[3];
            if(opt->nullable()!=nullable){switch(def->type){
                case coFloats:opt.reset(nullable ? static_cast<ConfigOption*>(new ConfigOptionFloatsNullable) : new ConfigOptionFloats);break;
                case coPercents:opt.reset(nullable ? static_cast<ConfigOption*>(new ConfigOptionPercentsNullable) : new ConfigOptionPercents);break;
                case coInts:opt.reset(nullable ? static_cast<ConfigOption*>(new ConfigOptionIntsNullable) : new ConfigOptionInts);break;
                case coBools:opt.reset(nullable ? static_cast<ConfigOption*>(new ConfigOptionBoolsNullable) : new ConfigOptionBools);break;
                case coFloatsOrPercents:opt.reset(nullable ? static_cast<ConfigOption*>(new ConfigOptionFloatsOrPercentsNullable) : new ConfigOptionFloatsOrPercents);break;
                case coEnums:opt.reset(nullable ? static_cast<ConfigOption*>(new ConfigOptionEnumsGenericNullable) : new ConfigOptionEnumsGeneric);break;
                default:require(false,"WORKER_NULLABLE_TYPE");}}
            const auto fp=[](const Json &v){count(v,2);return FloatOrPercent{number(v[0]),flag(v[1])};};
            switch(def->type){
                case coFloat:case coPercent:dynamic_cast<ConfigOptionSingle<double>&>(*opt).value=number(value);break;
                case coFloats:case coPercents:dynamic_cast<ConfigOptionVector<double>&>(*opt).values=vector<double>(value,number);break;
                case coInt:dynamic_cast<ConfigOptionInt&>(*opt).value=integer(value);break;
                case coInts:dynamic_cast<ConfigOptionVector<int>&>(*opt).values=vector<int>(value,integer);break;
                case coString:dynamic_cast<ConfigOptionString&>(*opt).value=unhex(value);break;
                case coStrings:dynamic_cast<ConfigOptionStrings&>(*opt).values=vector<std::string>(value,unhex);break;
                case coBool:dynamic_cast<ConfigOptionBool&>(*opt).value=flag(value);break;
                case coBools:dynamic_cast<ConfigOptionVector<unsigned char>&>(*opt).values=vector<unsigned char>(value,[](const Json &v){auto n=integer(v);require(n>=0 && n<=255,"WORKER_BYTE");return static_cast<unsigned char>(n);});break;
                case coFloatOrPercent:{const auto f=fp(value);auto &o=dynamic_cast<ConfigOptionFloatOrPercent&>(*opt);o.value=f.value;o.percent=f.percent;break;}
                case coFloatsOrPercents:dynamic_cast<ConfigOptionVector<FloatOrPercent>&>(*opt).values=vector<FloatOrPercent>(value,fp);break;
                case coPoint:dynamic_cast<ConfigOptionPoint&>(*opt).value=point2(value);break;
                case coPoint3:dynamic_cast<ConfigOptionPoint3&>(*opt).value=point3(value);break;
                case coPoints:dynamic_cast<ConfigOptionPoints&>(*opt).values=vector<Vec2d>(value,point2);break;
                case coIntsGroups:dynamic_cast<ConfigOptionIntsGroups&>(*opt).values=vector<std::vector<int>>(value,[](const Json &v){return vector<int>(v,integer);});break;
                case coPointsGroups:dynamic_cast<ConfigOptionPointsGroups&>(*opt).values=vector<std::vector<Vec2d>>(value,[](const Json &v){return vector<Vec2d>(v,point2);});break;
                case coEnum:
                    require(value.is_array() && value.size()>=2,"WORKER_ENUM_ARRAY");
                    if(value[0]=="native"){count(value,2);require(def->enum_keys_map,"WORKER_NATIVE_ENUM_MAP");opt.reset(new NativeEnum(integer(value[1]),*def->enum_keys_map));}
                    else{count(value,3);require(value[0]=="generic","WORKER_ENUM_KIND");opt.reset(new ConfigOptionEnumGeneric(dictionary(value[1]),integer(value[2])));}break;
                case coEnums:{count(value,3);require(value[0]=="generic","WORKER_ENUM_KIND");const auto *map=dictionary(value[1]);
                    if(nullable)dynamic_cast<ConfigOptionEnumsGenericNullable&>(*opt).keys_map=map;else dynamic_cast<ConfigOptionEnumsGeneric&>(*opt).keys_map=map;
                    dynamic_cast<ConfigOptionVector<int>&>(*opt).values=vector<int>(value[2],integer);break;}
                default:require(false,"WORKER_CONFIG_UNSUPPORTED");
            }
            result.set_key_value(key,opt.release());
        }
        require(ResolvedConfigSnapshot(result).canonical_json()==v.dump(),"WORKER_CONFIG_EXACT_RECONSTRUCTION");return result;
    }
    void model(const Json &source,const Json &meshes,Model &model)
    {
        keys(source,{"config","materials","model_plate_index","objects","plate_actions","schema"});require(natural(source.at("schema"))==1,"WORKER_SOURCE_VERSION");
        const auto &actions=source.at("plate_actions");require(actions.is_array() && actions.size()<=256,"WORKER_PLATE_ACTION_COUNT");
        for(const auto &a:actions){count(a,3);require(a[2]==Json::array(),"WORKER_PLATE_ACTIONS_REFUSED");CustomGCode::Info info;info.mode=CustomGCode::Mode(integer(a[1]));require(model.plates_custom_gcodes.emplace(integer(a[0]),std::move(info)).second,"WORKER_PLATE_ACTION_DUPLICATE");}
        model.curr_plate_index=integer(source.at("model_plate_index"));const auto &materials=source.at("materials");require(materials.is_array() && materials.size()<=256,"WORKER_MATERIAL_COUNT");
        for(const auto &row:materials){count(row,3);auto *m=model.add_material(unhex(row[0]));m->config.apply(config(row[1]));require(row[2].is_array() && row[2].size()<=256,"WORKER_ATTRIBUTES");
            for(const auto &a:row[2]){count(a,2);require(m->attributes.emplace(unhex(a[0]),unhex(a[1])).second,"WORKER_ATTRIBUTE_DUPLICATE");}}
        const auto &objects=source.at("objects");require(objects.is_array() && objects.size()<=256 && meshes.is_array() && meshes.size()==objects.size(),"WORKER_OBJECT_COUNT");
        for(size_t oi=0;oi<objects.size();++oi){const auto &row=objects[oi];keys(row,{"config","instances","layer_height_profile","layer_ranges","origin_translation","printable","volumes"});auto *o=model.add_object();
            o->config.apply(config(row.at("config")));o->origin_translation=point3(row.at("origin_translation"));o->printable=flag(row.at("printable"));o->layer_height_profile.set(vector<double>(row.at("layer_height_profile"),number));
            const auto &ranges=row.at("layer_ranges");require(ranges.is_array() && ranges.size()<=256,"WORKER_RANGES");for(const auto &range:ranges){count(range,3);ModelConfig c;c.apply(config(range[2]));require(o->layer_config_ranges.emplace(std::make_pair(number(range[0]),number(range[1])),std::move(c)).second,"WORKER_RANGE_DUPLICATE");}
            const auto &instances=row.at("instances");require(instances.is_array() && instances.size()<=256,"WORKER_INSTANCES");for(const auto &i:instances){count(i,5);auto *instance=o->add_instance();instance->set_transformation(Geometry::Transformation(matrix(i[0])));
                instance->printable=flag(i[1]);instance->auto_drop=flag(i[2]);auto state=integer(i[3]);require(state>=0 && state<ModelInstanceNum_BedStates,"WORKER_INSTANCE_STATE");instance->print_volume_state=ModelInstanceEPrintVolumeState(state);instance->arrange_order=integer(i[4]);}
            const auto &volumes=row.at("volumes");require(volumes.is_array() && volumes.size()<=256 && meshes[oi].is_array() && meshes[oi].size()==volumes.size(),"WORKER_VOLUMES");
            for(size_t vi=0;vi<volumes.size();++vi){const auto &v=volumes[vi];keys(v,{"annotations","builtin","config","from_inches","from_meters","material_id","mesh_sha256","source_file","source_object_index","source_offset","source_transform","source_volume_index","transform","type"});
                const auto &payload=meshes[oi][vi];keys(payload,{"mesh","annotations"});auto mesh=parse_native_mesh_document(payload.at("mesh"),200000);require(native_mesh_fingerprint(mesh)==unhex(v.at("mesh_sha256")),"WORKER_MESH_BINDING");auto type=integer(v.at("type"));require(type>=0 && type<=int(ModelVolumeType::SUPPORT_ENFORCER),"WORKER_VOLUME_TYPE");auto *volume=o->add_volume(TriangleMesh(std::move(mesh)),ModelVolumeType(type),false);
                volume->config.apply(config(v.at("config")));volume->set_transformation(matrix(v.at("transform")));require(unhex(v.at("material_id")).empty(),"WORKER_REFERENCED_MATERIAL_REFUSED");auto &s=volume->source;
                s.input_file=unhex(v.at("source_file"));s.object_idx=integer(v.at("source_object_index"));s.volume_idx=integer(v.at("source_volume_index"));s.mesh_offset=point3(v.at("source_offset"));s.transform.set_matrix(matrix(v.at("source_transform")));
                s.is_converted_from_inches=flag(v.at("from_inches"));s.is_converted_from_meters=flag(v.at("from_meters"));s.is_from_builtin_objects=flag(v.at("builtin"));
                // Source identity records annotation hashes. Actual data is
                // transferred separately and checked by recapturing the source.
                const auto &annotations=payload.at("annotations");count(annotations,4);size_t index=0;
                for(auto *target:{&volume->supported_facets,&volume->seam_facets,&volume->mmu_segmentation_facets,&volume->fuzzy_skin_facets}){
                    const auto &a=annotations[index++];count(a,3);TriangleSelector::TriangleSplittingData data;
                    require(a[0].is_array() && a[0].size()<=200000 && a[1].is_array() && a[1].size()<=1024*1024 && a[2].is_array() && a[2].size()<=1024,"WORKER_ANNOTATION_COUNT");
                    for(const auto &row:a[0]){count(row,2);data.triangles_to_split.emplace_back(integer(row[0]),integer(row[1]));}
                    data.bitstream=vector<bool>(a[1],flag,1024*1024);data.used_states=vector<bool>(a[2],flag);target->set_data(std::move(data));
                }
            }
        }
    }
};
Json meshes(const NativeInputSnapshot &source)
{
    Json result=Json::array();
    for(const auto &o:source.objects){Json volumes=Json::array();for(const auto &v:o.volumes){require(v.material_id.empty(),"WORKER_REFERENCED_MATERIAL_REFUSED");Json annotations=Json::array();
        for(const auto &data:v.annotations){Json rows=Json::array();for(const auto &r:data.triangles_to_split)rows.push_back({r.triangle_idx,r.bitstream_start_idx});annotations.push_back({rows,data.bitstream,data.used_states});}
        volumes.push_back({{"mesh",native_mesh_document(v.mesh)},{"annotations",std::move(annotations)}});}result.push_back(std::move(volumes));}
    return result;
}
Json document(unsigned version,const char *host,const Json &ticket,const NativeInputSnapshot &source,
    const JobIdentityView &identity,const NativeAnalysisRequestSnapshot &request,Json files,const Vec3d &origin,const std::string &software)
{
    Json result={{"schema",version},{"software_sha256",software},{"input_sha256",identity.fingerprint},
        {"source",Json::parse(identity.canonical_json)},{"meshes",meshes(source)},{"request",Json::parse(native_analysis_document(request))},
        {"request_sha256",request.sha256},{"files",std::move(files)},{"plate_origin",{origin.x(),origin.y(),origin.z()}}};
    result[host]=ticket;return result;
}
}
bool NativeAnalysisWorkerInput::is_current() const
{return task ? task->is_current() : view_task && view_task->is_current();}
std::shared_ptr<const NativeAnalysisWorkerInput> capture_native_analysis_worker_input(
    std::shared_ptr<const GuardedJobTask> task,std::shared_ptr<const NativeAnalysisRequestSnapshot> request)
{
    require(task && task->is_current() && task->phase==GuardedJobPhase::Analyzing && request,"WORKER_CURRENT_INPUT_REQUIRED");const auto &job=*task->snapshot;
    require(job.native_inputs && job.software,"WORKER_TYPED_COMPILED_JOB_REQUIRED");bool bound=false;Json files=Json::array();
    const auto actual=capture_native_job_inputs(request->values.inputs,[&]{require(task->is_current(),"WORKER_STALE_HOST_TASK");});
    require(actual->resources.size()==job.native_inputs->resources.size(),"WORKER_TYPED_INPUT_BINDING");
    for(size_t i=0;i<actual->resources.size();++i)require(actual->resources[i].bytes==job.native_inputs->resources[i].bytes,"WORKER_TYPED_INPUT_BINDING");
    size_t file_bytes=0;for(const auto &r:job.resources)if(r.kind==JobResourceKind::SourceFile && r.name!="native-analysis-request-v1"){
        require(r.bytes.size()<=native_analysis_worker_byte_limit/2-file_bytes,"WORKER_FILE_BYTE_LIMIT");file_bytes+=r.bytes.size();}
    for(const auto &r:job.resources)if(r.kind==JobResourceKind::SourceFile){if(r.name=="native-analysis-request-v1"){require(r.bytes==request->canonical_json,"WORKER_REQUEST_BINDING");bound=true;}else files.push_back({r.name,hex(r.bytes)});}
    require(bound,"WORKER_REQUEST_NOT_BOUND");const Json ticket={job.job_id,job.input_revision,task->attempt,job.fingerprint};
    auto bytes=document(1,"host_job",ticket,*job.input,job.input_identity,*request,std::move(files),job.settings->plate_origin_mm,job.software->sha256).dump();
    require(bytes.size()<=native_analysis_worker_byte_limit && task->is_current(),"WORKER_INPUT_LIMIT_OR_STALE");
    return std::shared_ptr<const NativeAnalysisWorkerInput>(new NativeAnalysisWorkerInput(std::move(task),{},1,job.job_id,"host_job",ticket.dump(),job.software->sha256,std::move(bytes),job.input_identity.fingerprint,request->sha256));
}
std::shared_ptr<const NativeAnalysisWorkerInput> capture_native_analysis_worker_input(
    std::shared_ptr<const NativeAnalysisViewTask> task,const std::vector<JobResource> &source_files)
{
    require(task && task->is_current(),"WORKER_CURRENT_VIEW_REQUIRED");const auto &view=*task->input;
    require(source_files.size()<=247,"WORKER_FILE_COUNT");size_t total=0;bool editing=false;std::set<std::string> names;Json files=Json::array();
    for(const auto &file:source_files){require(file.kind==JobResourceKind::SourceFile && !file.name.empty() && file.name.size()<=4096 &&
        file.name!="native-analysis-request-v1" && names.insert(file.name).second,"WORKER_SOURCE_FILE_REGISTRY");
        if(file.name=="native-analysis-json-v1"){require(file.bytes==view.editing_bytes,"WORKER_EDITING_BYTES_BINDING");editing=true;}
        require(!file.bytes.empty() && file.bytes.size()<=native_analysis_worker_byte_limit/2-total,"WORKER_FILE_BYTE_LIMIT");total+=file.bytes.size();files.push_back({file.name,hex(file.bytes)});}
    require(editing,"WORKER_EDITING_BYTES_REQUIRED");
    for(const auto &o:view.source->objects)for(const auto &v:o.volumes)require(v.source_file.empty() || names.count(v.source_file),"WORKER_MISSING_SOURCE_FILE");
    const auto identity=guarded_source_identity(*view.source);const auto software=compiled_build_inputs();
    const Json ticket={task->revision,task->attempt,view.identity_sha256};
    auto bytes=document(2,"host_view",ticket,*view.source,identity,*view.request,std::move(files),view.plate_origin,software->sha256).dump();
    require(bytes.size()<=native_analysis_worker_byte_limit && task->is_current(),"WORKER_INPUT_LIMIT_OR_STALE");
    return std::shared_ptr<const NativeAnalysisWorkerInput>(new NativeAnalysisWorkerInput({},std::move(task),2,ticket[0].get<uint64_t>(),"host_view",ticket.dump(),software->sha256,std::move(bytes),identity.fingerprint,view.request->sha256));
}
std::string execute_native_analysis_worker(const std::string &bytes,const std::function<void(NativeAnalysisStage)> &progress)
{
    require(bytes.size()<=native_analysis_worker_byte_limit,"WORKER_INPUT_BYTE_LIMIT");std::vector<std::set<std::string>> objects;
    const auto j=Json::parse(bytes,[&](int depth,Json::parse_event_t event,Json &v){require(depth<=16,"WORKER_INPUT_DEPTH");if(event==Json::parse_event_t::object_start)objects.emplace_back();if(event==Json::parse_event_t::key)require(objects.back().insert(v.get<std::string>()).second,"WORKER_DUPLICATE_KEY");if(event==Json::parse_event_t::object_end)objects.pop_back();return true;});
    const auto version=natural(j.at("schema"));require(version==1 || version==2,"WORKER_PROTOCOL_VERSION");const char *host=version==1 ? "host_job" : "host_view";
    keys(j,{"schema",host,"software_sha256","input_sha256","source","meshes","request","request_sha256","files","plate_origin"});require(j.at("software_sha256")==compiled_build_inputs()->sha256,"WORKER_SOFTWARE_BINDING");count(j.at(host),version==1 ? 4 : 3);
    require(natural(j.at(host)[0]) && natural(j.at(host)[1]),"WORKER_HOST_IDENTITY");
    Decoder decoder;Model model;auto config=decoder.config(j.at("source").at("config"));decoder.model(j.at("source"),j.at("meshes"),model);
    const auto source=capture_native_input(model,config);require(source && source->canonical_json==j.at("source").dump() && source->fingerprint==j.at("input_sha256"),"WORKER_EXACT_SOURCE_RECONSTRUCTION");
    const auto request=parse_native_analysis_document(j.at("request").dump());require(request->sha256==j.at("request_sha256"),"WORKER_REQUEST_HASH");std::vector<JobResource> files;
    require(j.at("files").is_array() && j.at("files").size()<=247,"WORKER_FILE_COUNT");for(const auto &f:j.at("files")){count(f,2);require(f[0].is_string(),"WORKER_FILE_NAME");files.push_back({JobResourceKind::SourceFile,f[0].get<std::string>(),unhex(f[1])});}
    const auto &origin=j.at("plate_origin");count(origin,3);Vec3d position;for(int axis=0;axis<3;++axis){require(origin[axis].is_number() && std::isfinite(origin[axis].get<double>()),"WORKER_PLATE_ORIGIN");position[axis]=origin[axis].get<double>();}
    NativeAnalysisResult result;NativeAnalysisLimits limits;limits.progress=progress;
    if(version==2){
        if(progress)progress(NativeAnalysisStage::Capture);
        limits.progress=[&](NativeAnalysisStage stage){if(stage!=NativeAnalysisStage::Capture && progress)progress(stage);};
    }
    if(version==2)if(const auto conflict=input_policy_conflict(model,config))result.reason="VIEW_POLICY_REFUSED:"+conflict->key;
    if(result.reason.empty()){
        Print print;print.set_plate_index(model.curr_plate_index);print.set_plate_origin(position);print.apply(model,config);
        if(version==2){
            const auto settings=capture_print_config(print);require(bool(settings),"WORKER_PRINT_CONFIG_REFUSED");
            const auto policy=resolve_policy(settings->resolved_print_config,settings->object_count,settings->instance_count);
            if(!policy.conflicts.empty())result.reason="VIEW_CONFIG_POLICY_REFUSED:"+policy.conflicts.front().key;
            for(const auto &region:settings->regions)if(result.reason.empty() && !region.policy.conflicts.empty())result.reason="VIEW_REGION_POLICY_REFUSED:"+region.policy.conflicts.front().key;
        }
        if(result.reason.empty())result=run_native_analysis(print,natural(j.at(host)[0]),request,files,limits);
    }
    Json response={{"schema",version},{"input_payload_sha256",sha256_bytes(bytes)},
        {"input_sha256",source->fingerprint},{"request_sha256",request->sha256},{"software_sha256",compiled_build_inputs()->sha256},
        {"diagnostic",Json::parse(native_analysis_diagnostic(result))},{"peak_rss_bytes","00000000000000000000"}};
    response[host]=j.at(host);return response.dump();
}
}
