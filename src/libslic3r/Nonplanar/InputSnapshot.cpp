#include "InputSnapshot.hpp"
#include "Canonical.hpp"
#include "StlImport.hpp"
#include "../Model.hpp"
#include <cstring>
#include <limits>

namespace Slic3r::nptop {
namespace {
constexpr size_t max_faces=200000, max_vertices=600000, max_graph_entries=256;
struct CaptureBudget {
    size_t faces=0, vertices=0, entries=0, annotation_bits=0, annotation_entries=0, config_bytes=0;
    void add(size_t &total, size_t count, size_t limit) {
        if (count>limit-total) throw std::length_error("Native input aggregate capture limit");
        total+=count;
    }
    void mesh(const indexed_triangle_set &mesh) {
        add(faces,mesh.indices.size(),max_faces); add(vertices,mesh.vertices.size(),max_vertices);
        if (mesh.properties.size()>mesh.indices.size()) throw ConfigurationError("Native mesh property count exceeds face count");
    }
    void entry(size_t count) { add(entries,count,max_graph_entries); }
    void config(const ResolvedConfigSnapshot &value) { add(config_bytes,value.canonical_json().size(),4*1024*1024); }
};
void matrix(detail::CanonicalConfigWriter &w, const Transform3d &transform) {
    w.append("[");
    for (int row=0; row<4; ++row) for (int column=0; column<4; ++column) {
        if (row || column) w.append(",");
        w.value(transform.matrix()(row,column));
    }
    w.append("]");
}
std::string annotation_identity(const TriangleSelector::TriangleSplittingData &data) {
    detail::CanonicalConfigWriter w;
    w.append("["); bool first=true;
    for (const auto &entry : data.triangles_to_split) {
        if (!first) w.append(","); first=false;
        w.append("["); w.value(entry.triangle_idx); w.append(","); w.value(entry.bitstream_start_idx); w.append("]");
    }
    w.append("],"); w.value(data.bitstream); w.append(","); w.value(data.used_states);
    return sha256_bytes(w.take());
}
void volume(detail::CanonicalConfigWriter &w, const InputVolume &v) {
    w.append("{\"annotations\":["); bool first=true;
    for (const auto &data : v.annotations) {
        if (!first) w.append(","); first=false; w.value(annotation_identity(data));
    }
    w.append("],\"builtin\":"); w.value(v.builtin);
    w.append(",\"config\":"); w.append(v.config.canonical_json());
    w.append(",\"from_inches\":"); w.value(v.from_inches);
    w.append(",\"from_meters\":"); w.value(v.from_meters);
    w.append(",\"material_id\":"); w.value(v.material_id);
    w.append(",\"mesh_sha256\":"); w.value(v.mesh_sha256);
    w.append(",\"source_file\":"); w.value(v.source_file);
    w.append(",\"source_object_index\":"); w.value(v.source_object_index);
    w.append(",\"source_offset\":"); w.value(v.source_offset);
    w.append(",\"source_transform\":"); matrix(w,v.source_transform);
    w.append(",\"source_volume_index\":"); w.value(v.source_volume_index);
    w.append(",\"transform\":"); matrix(w,v.transform);
    w.append(",\"type\":"); w.value(v.type); w.append("}");
}
void object(detail::CanonicalConfigWriter &w, const InputObject &o) {
    w.append("{\"config\":"); w.append(o.config.canonical_json());
    w.append(",\"instances\":["); bool first=true;
    for (const auto &i : o.instances) {
        if (!first) w.append(","); first=false;
        w.append("["); matrix(w,i.transform); w.append(","); w.value(i.printable);
        w.append(","); w.value(i.auto_drop); w.append(","); w.value(i.print_volume_state);
        w.append(","); w.value(i.arrange_order); w.append("]");
    }
    w.append("],\"layer_height_profile\":"); w.value(o.layer_height_profile);
    w.append(",\"layer_ranges\":["); first=true;
    for (const auto &range : o.layer_ranges) {
        if (!first) w.append(","); first=false;
        w.append("["); w.value(range.lower_z); w.append(","); w.value(range.upper_z);
        w.append(","); w.append(range.config.canonical_json()); w.append("]");
    }
    w.append("],\"origin_translation\":"); w.value(o.origin_translation);
    w.append(",\"printable\":"); w.value(o.printable);
    w.append(",\"volumes\":["); first=true;
    for (const auto &v : o.volumes) { if (!first) w.append(","); first=false; volume(w,v); }
    w.append("]}");
}
}

std::string native_mesh_fingerprint(const indexed_triangle_set &mesh)
{
    CaptureBudget budget; budget.mesh(mesh);
    // Bound above before allocating. Canonical binary framing is big-endian,
    // independent of host endianness, structure padding and native caches.
    std::string bytes("nptop-native-mesh-v1\0",21);
    const auto integer=[&](uint64_t value, int size) {
        for (int shift=8*(size-1); shift>=0; shift-=8) bytes.push_back(char((value>>shift)&255));
    };
    const auto binary32=[&](float value) {
        static_assert(sizeof(float)==sizeof(uint32_t) && std::numeric_limits<float>::is_iec559);
        uint32_t bits; std::memcpy(&bits,&value,sizeof bits); integer(bits,4);
    };
    integer(mesh.vertices.size(),8);
    for (const auto &v : mesh.vertices) for (int axis=0; axis<3; ++axis) binary32(v[axis]);
    integer(mesh.indices.size(),8);
    for (const auto &face : mesh.indices) for (int axis=0; axis<3; ++axis) integer(uint32_t(face[axis]),4);
    integer(mesh.properties.size(),8);
    for (const auto &property : mesh.properties) {
        integer(uint32_t(property.type),4);
        uint64_t bits; static_assert(sizeof(double)==sizeof(bits) && std::numeric_limits<double>::is_iec559);
        std::memcpy(&bits,&property.area,sizeof bits); integer(bits,8);
    }
    return sha256_bytes(bytes);
}

std::shared_ptr<const NativeInputSnapshot> capture_native_input(const Model &model, const ConfigBase &source)
{
    if (!requests_guarded_mode(model,source)) return {};
    CaptureBudget budget; budget.entry(model.objects.size()); budget.entry(model.materials.size());
    const ResolvedConfigSnapshot config(source); budget.config(config);
    detail::CanonicalConfigWriter w; w.append("{\"config\":"); w.append(config.canonical_json());
    w.append(",\"materials\":["); bool first=true;
    std::vector<InputMaterial> materials;
    for (const auto &[id, material] : model.materials) {
        if (!material) throw ConfigurationError("Null native input material");
        budget.entry(material->attributes.size());
        const ResolvedConfigSnapshot settings(material->config.get()); budget.config(settings);
        materials.push_back({id,settings,material->attributes});
        if (!first) w.append(","); first=false;
        w.append("["); w.value(id); w.append(","); w.append(materials.back().config.canonical_json()); w.append(",[");
        bool first_attribute=true;
        for (const auto &[key,value] : material->attributes) {
            if (!first_attribute) w.append(","); first_attribute=false;
            w.append("["); w.value(key); w.append(","); w.value(value); w.append("]");
        }
        w.append("]]");
    }
    w.append("],\"model_plate_index\":"); w.value(model.curr_plate_index);
    w.append(",\"objects\":["); first=true;
    std::vector<InputObject> objects;
    for (const auto *o : model.objects) {
        if (!o) throw ConfigurationError("Null native input object");
        // These controls are not represented by this FFF input contract. A
        // rejected capture must never leave a usable partial source snapshot.
        if (!o->sla_support_points.empty() || !o->sla_drain_holes.empty() ||
            !o->brim_points.empty() || !o->cut_connectors.empty())
            throw ConfigurationError("Unsupported native input object controls");
        budget.entry(o->volumes.size()); budget.entry(o->instances.size());
        budget.entry(o->layer_config_ranges.size());
        std::vector<InputVolume> volumes;
        for (const auto *v : o->volumes) {
            if (!v) throw ConfigurationError("Null native input volume");
            budget.mesh(v->mesh().its);
            const std::array<const FacetsAnnotation *,4> annotations{&v->supported_facets,&v->seam_facets,&v->mmu_segmentation_facets,&v->fuzzy_skin_facets};
            for (const auto *a : annotations) {
                budget.add(budget.annotation_bits,a->get_data().bitstream.size(),1024*1024);
                budget.add(budget.annotation_entries,a->get_data().triangles_to_split.size(),max_faces);
                if (a->get_data().triangles_to_split.size()>max_faces || a->get_data().used_states.size()>1024)
                    throw std::length_error("Native input annotation capture limit");
            }
            const ResolvedConfigSnapshot settings(v->config.get()); budget.config(settings);
            volumes.push_back({settings,v->mesh().its,v->get_matrix(),v->source.transform.get_matrix(),
                v->source.mesh_offset,native_mesh_fingerprint(v->mesh().its),v->source.input_file,v->material_id(),int(v->type()),
                v->source.object_idx,v->source.volume_idx,v->source.is_converted_from_inches,v->source.is_converted_from_meters,
                v->source.is_from_builtin_objects,{annotations[0]->get_data(),annotations[1]->get_data(),annotations[2]->get_data(),annotations[3]->get_data()}});
        }
        std::vector<InputInstance> instances;
        for (const auto *i : o->instances) {
            if (!i) throw ConfigurationError("Null native input instance");
            instances.push_back({i->get_matrix(),i->printable,i->auto_drop,int(i->print_volume_state),i->arrange_order});
        }
        std::vector<InputLayerRange> ranges;
        for (const auto &[range,settings] : o->layer_config_ranges) {
            const ResolvedConfigSnapshot captured(settings.get()); budget.config(captured);
            ranges.push_back({range.first,range.second,captured});
        }
        if (o->layer_height_profile.get().size()>4096) throw std::length_error("Native input layer profile capture limit");
        const ResolvedConfigSnapshot settings(o->config.get()); budget.config(settings);
        objects.push_back({settings,std::move(volumes),std::move(instances),std::move(ranges),
            o->layer_height_profile.get(),o->origin_translation,o->printable});
        if (!first) w.append(","); first=false; object(w,objects.back());
    }
    w.append("],\"plate_actions\":["); first=true;
    budget.entry(model.plates_custom_gcodes.size());
    for (const auto &[plate,actions] : model.plates_custom_gcodes) {
        budget.entry(actions.gcodes.size());
        if (!first) w.append(","); first=false;
        w.append("["); w.value(plate); w.append(","); w.value(int(actions.mode)); w.append(",["); bool first_action=true;
        for (const auto &a : actions.gcodes) {
            if (!first_action) w.append(","); first_action=false;
            w.append("["); w.value(a.print_z); w.append(","); w.value(int(a.type)); w.append(","); w.value(a.extruder);
            w.append(","); w.value(a.color); w.append(","); w.value(a.extra); w.append("]");
        }
        w.append("]]");
    }
    w.append("],\"schema\":1}"); auto canonical=w.take(); const auto identity=sha256_bytes(canonical);
    return std::make_shared<const NativeInputSnapshot>(NativeInputSnapshot{config,std::move(materials),std::move(objects),
        model.curr_plate_index,std::move(canonical),identity});
}
}
