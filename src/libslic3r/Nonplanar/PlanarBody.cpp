#include "PlanarBody.hpp"
#include "InputSnapshot.hpp"
#include "Canonical.hpp"
#include "Interval.hpp"
#include "StlImport.hpp"
#include "../Model.hpp"
#include "../Print.hpp"
#include "../Layer.hpp"
#include "../Flow.hpp"
#include <mutex>

namespace Slic3r::nptop {
namespace {
DynamicPrintConfig clone_config(const ResolvedConfigSnapshot &source)
{
    DynamicPrintConfig result;
    for (const auto &key : source.keys()) result.set_key_value(key,source.option(key)->clone());
    return result;
}
struct Rejection : std::runtime_error { using std::runtime_error::runtime_error; };
void reject(const char *reason) { throw Rejection(reason); }
}

std::string PlanarBodySnapshot::canonical_json() const
{
    detail::CanonicalConfigWriter w;
    const auto number=[&](auto v) { w.append(std::to_string(v)); };
    const auto range=[&](ScalarBounds v) { w.append("["); w.value(v.lower); w.append(","); w.value(v.upper); w.append("]"); };
    const auto position=[&](auto v) { w.value(Vec3d(v.x(),v.y(),v.z())); };
    w.append("{\"engine_non_bbl\":true,\"executed_full\":"); w.value(executed_full_config.fingerprint());
    w.append(",\"executed_object\":"); w.value(executed_object_config.fingerprint());
    w.append(",\"executed_print\":"); w.value(executed_print_config.fingerprint());
    w.append(",\"guarded_settings\":"); w.value(guarded_settings->fingerprint());
    w.append(",\"native_scale\":"); w.value(native_scale.mm_per_unit());
    w.append(",\"origin_error\":"); w.value(origin_error_upper_mm);
    w.append(",\"partition\":"); w.value(partition->fingerprint());
    w.append(",\"regions\":["); bool first_region=true;
    for (const auto &region : regions) {
        if (!first_region) w.append(","); first_region=false;
        const auto &g=*region.geometry;
        w.append("["); number(region.native_region_id); w.append(","); w.value(region.config.fingerprint());
        w.append(","); number(g.revision); w.append(","); number(g.native_layer_id);
        w.append(","); w.value(g.native_layer_height_mm); w.append(","); w.value(g.native_print_z_mm);
        w.append(","); position(g.object_origin); w.append(","); w.value(g.native_scale.mm_per_unit());
        w.append(",["); bool first=true;
        for (const auto &entity : g.entities) {
            if (!first) w.append(","); first=false;
            w.append("["); number(entity.parent_id); w.append(","); number(int(entity.kind));
            w.append(","); w.value(entity.can_reverse); w.append(","); w.value(entity.can_sort);
            w.append(","); number(entity.native_inset_index); w.append(",");
            if (entity.loop_role) number(int(*entity.loop_role)); else w.append("null");
            w.append("]");
        }
        w.append("],["); first=true;
        for (const auto &path : g.paths) {
            if (!first) w.append(","); first=false;
            w.append("["); number(path.entity_id); w.append(","); number(int(path.role));
            for (double v : {path.width_mm,path.height_mm,path.mm3_per_mm,path.coordinate_error_upper_mm}) { w.append(","); w.value(v); }
            w.append(","); range(path.length_mm); w.append(","); range(path.native_volume_mm3);
            w.append(",["); bool first_point=true;
            for (const auto &point : path.native_points) {
                if (!first_point) w.append(","); first_point=false;
                w.append("["); number(point.x()); w.append(","); number(point.y()); w.append(","); number(point.z()); w.append("]");
            }
            w.append("],["); first_point=true;
            for (const auto &point : path.points) { if (!first_point) w.append(","); first_point=false; position(point); }
            w.append("]]");
        }
        w.append("],"); range(g.native_volume_mm3); w.append("]");
    }
    w.append("],\"revision\":"); number(revision); w.append(",\"schema\":1,\"volume\":"); range(native_volume_mm3); w.append("}");
    return w.take();
}
std::string PlanarBodySnapshot::fingerprint() const { return sha256_bytes(canonical_json()); }

PlanarBodyResult generate_planar_body(const VolumePartitionResult &requested, const PlanarBodyLimits &requested_limits)
{
    const auto input=requested; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    std::mutex callback_mutex;
    const auto stop=[&] {
        std::lock_guard<std::mutex> lock(callback_mutex);
        if (limits.paths.cancelled && limits.paths.cancelled()) reject("CANCELLED");
        if (limits.paths.is_current && !limits.paths.is_current(input.snapshot->revision)) reject("STALE_REVISION");
        if (std::chrono::steady_clock::now()-started>=limits.paths.timeout) reject("BODY_DEADLINE");
    };
    try {
        detail::require_interval_environment();
        if (!limits.max_layers || limits.max_layers>1000 || !limits.max_regions || limits.max_regions>1000 ||
            limits.paths.max_entities<2 || limits.paths.max_entities>20000 || limits.paths.max_points<2 ||
            limits.paths.max_points>200000 || !limits.paths.max_depth || limits.paths.max_depth>32 ||
            limits.paths.timeout.count()<=0 || limits.paths.timeout>std::chrono::seconds(30)) reject("INVALID_BODY_LIMITS");
        if (input.status!=VolumePartitionStatus::Partitioned || !input.snapshot || !input.snapshot->revision ||
            !input.snapshot->placement || !input.snapshot->placement->native_input || !input.snapshot->body)
            reject("MISSING_BODY_PARTITION");
        const auto partition=input.snapshot;
        const auto source=partition->placement->native_input;
        if (source->objects.size()!=1 || source->objects.front().volumes.size()!=1 ||
            source->objects.front().instances.size()!=1) reject("BODY_REQUIRES_SINGLE_SOURCE");
        const auto &object_source=source->objects.front(); const auto &volume_source=object_source.volumes.front();
        if (!object_source.layer_ranges.empty() || !object_source.layer_height_profile.empty()) reject("UNSUPPORTED_BODY_LAYER_PROFILE");
        for (const auto &annotation : volume_source.annotations)
            if (!annotation.bitstream.empty() || !annotation.triangles_to_split.empty() ||
                std::any_of(annotation.used_states.begin(),annotation.used_states.end(),[](bool v){ return v; }))
                reject("UNSUPPORTED_BODY_PAINTING");
        if (partition->body->bounding_box().min.z()!=0) reject("BODY_REQUIRES_EXACT_BED_CONTACT");
        stop();
        Model model; auto *object=model.add_object();
        auto *volume=object->add_volume(TriangleMesh(*partition->body),ModelVolumeType::MODEL_PART,false);
        object->add_instance();
        object->config.apply(clone_config(object_source.config)); volume->config.apply(clone_config(volume_source.config));
        for (const auto &material_source : source->materials) {
            auto *material=model.add_material(material_source.id); material->attributes=material_source.attributes;
            material->config.apply(clone_config(material_source.config));
        }
        volume->set_material_id(volume_source.material_id);
        auto config=clone_config(source->config);
        Print print; print.is_BBL_printer()=false;
        print.apply(model,config); stop();
        const auto guarded=capture_print_config(print);
        if (!guarded || !guarded->input_conflict.empty() || guarded->model_conflict ||
            guarded->object_count!=1 || guarded->instance_count!=1 || guarded->regions.empty())
            throw Rejection(guarded ? guarded->block_reason() : "MISSING_GUARDED_BODY_SETTINGS");
        for (const auto &region : guarded->regions)
            if (!region.policy.passes_config_preflight()) throw Rejection(guarded->block_reason());
        guarded->fingerprint(); // Reject an oversized settings identity before native processing.
        // Only this private derived model enters the ordinary native planar
        // engine. The owning source/partition and public guarded export stay gated.
        config.set_key_value("nptop_mode",new ConfigOptionString("off"));
        for (auto *native_config : {&object->config,&volume->config})
            if (native_config->has("nptop_mode")) native_config->set_key_value("nptop_mode",new ConfigOptionString("off"));
        print.apply(model,config); stop();
        const auto validation=print.validate();
        if (!validation.string.empty()) throw Rejection(validation.string);
        print.set_status_callback([&](const PrintBase::SlicingStatus &) { stop(); });
        print.process(); stop(); detail::require_interval_environment();
        if (print.objects().size()!=1 || print.objects().front()->instances().size()!=1) reject("BODY_NATIVE_OBJECT_MISMATCH");
        const auto &native=*print.objects().front();
        if (!native.support_layers().empty()) reject("UNSUPPORTED_BODY_SUPPORT");
        if (!print.skirt().entities.empty() || !native.object_skirt().entities.empty()) reject("UNSUPPORTED_BODY_HELPER_EXTRUSIONS");
        for (const auto &[id,brim] : print.get_brimMap())
            if (!brim.entities.empty()) reject("UNSUPPORTED_BODY_HELPER_EXTRUSIONS");
        for (const auto &group : print.skirt_brim_groups()) {
            if (!group.skirt.entities.empty()) reject("UNSUPPORTED_BODY_HELPER_EXTRUSIONS");
            for (const auto &brim : group.brims)
                if (!brim.brim.entities.empty()) reject("UNSUPPORTED_BODY_HELPER_EXTRUSIONS");
        }
        if (native.layers().empty() || native.layers().size()>limits.max_layers) reject("BODY_LAYER_LIMIT");
        const auto scale=NativeScale::capture(); const auto shift=native.instances().front().shift;
        const auto origin=from_native_scaled<Frame::BuildPlate>(Point3(shift.x(),shift.y(),coord_t(0)),scale);
        auto resolved=print.full_print_config(); resolved.apply(print.config());
        std::map<int,ResolvedConfigSnapshot> region_configs;
        for (const PrintRegion &region : native.all_regions()) {
            auto config=resolved; config.apply(native.config()); config.apply(region.config());
            if (!region_configs.emplace(region.print_region_id(),ResolvedConfigSnapshot(config)).second)
                reject("BODY_NATIVE_REGION_MISMATCH");
        }
        std::vector<PlanarBodyRegion> regions; size_t entities=0, points=0;
        detail::Interval total(0);
        for (const auto *layer : native.layers()) for (const auto *region : layer->regions()) {
            stop();
            if (regions.size()>=limits.max_regions) reject("BODY_REGION_LIMIT");
            if (limits.paths.max_entities-entities<2 || limits.paths.max_points-points<2) reject("BODY_PATH_LIMIT");
            auto remaining=limits.paths;
            remaining.max_entities-=entities; remaining.max_points-=points;
            remaining.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
            remaining.cancelled=[&] { stop(); return false; }; remaining.is_current={};
            const auto captured=capture_planar_region(*region,origin.position,partition->revision,remaining);
            if (!captured.snapshot) throw Rejection(captured.reason);
            entities+=captured.snapshot->entities.size();
            for (const auto &path : captured.snapshot->paths) points+=path.native_points.size();
            const auto config=region_configs.find(region->region().print_region_id());
            if (config==region_configs.end()) reject("BODY_NATIVE_REGION_MISMATCH");
            regions.push_back({config->first,config->second,captured.snapshot});
            total=total+detail::Interval(captured.snapshot->native_volume_mm3.lower,captured.snapshot->native_volume_mm3.upper);
        }
        auto snapshot=std::make_shared<const PlanarBodySnapshot>(PlanarBodySnapshot{
            partition->revision,partition,guarded,ResolvedConfigSnapshot(print.full_print_config()),ResolvedConfigSnapshot(resolved),
            ResolvedConfigSnapshot(native.config()),scale,origin.error_mm,std::move(regions),{total.lo,total.hi}});
        snapshot->fingerprint(); // Bound the complete canonical encoding before publication.
        stop(); detail::require_interval_environment();
        return {"NOMINAL_NATIVE_BODY_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) { return {e.what(),{}}; }
    catch (const std::exception &e) { return {"BODY_NATIVE_OR_CAPTURE_FAILURE: "+std::string(e.what()),{}}; }
}

std::string BodyMaterialSnapshot::canonical_context() const
{
    detail::CanonicalConfigWriter w;
    w.append("{\"body\":"); w.value(body_fingerprint);
    w.append(",\"material\":"); w.value(material_fingerprint);
    w.append(",\"plate_origin\":"); w.value(Vec3d(plate_origin.x(),plate_origin.y(),plate_origin.z()));
    w.append(",\"reference_count\":"); w.append(std::to_string(references.size()));
    w.append(",\"schema\":1}"); return w.take();
}
std::string BodyMaterialSnapshot::canonical_reference(size_t index) const
{
    const auto &r=references.at(index); detail::CanonicalConfigWriter w;
    w.append("["); w.append(std::to_string(r.record_index)); w.append(","); w.append(std::to_string(r.region_index));
    w.append(","); w.append(std::to_string(r.path_index)); w.append(","); w.append(std::to_string(r.segment_index));
    w.append(","); w.value(r.width_reconciliation_upper_mm); w.append(","); w.value(r.volume_reconciliation_upper_mm3);
    w.append("]"); return w.take();
}
namespace {
std::string body_material_hash(const BodyMaterialSnapshot &snapshot, const std::function<void()> &poll)
{
    std::string hash=sha256_bytes(std::string("nptop-body-material-v1\0",23)+snapshot.canonical_context());
    for (size_t i=0; i<snapshot.references.size(); ++i) {
        if (i%128==0) poll();
        hash=sha256_bytes(std::string("nptop-body-bead-v1\0",19)+hash+snapshot.canonical_reference(i));
    }
    return hash;
}
}
std::string BodyMaterialSnapshot::fingerprint() const { return body_material_hash(*this,[]{}); }

BodyMaterialResult reconstruct_planar_body_material(const PlanarBodyResult &requested,
    const BodyMaterialParameters &requested_parameters, const MaterialLimits &requested_limits)
{
    const auto body=requested.snapshot; const auto parameters=requested_parameters; const auto limits=requested_limits;
    const auto started=std::chrono::steady_clock::now();
    using detail::Interval;
    const auto absolute_upper=[](Interval value) { return std::max(std::abs(value.lo),std::abs(value.hi)); };
    const auto stop=[&] {
        if (limits.cancelled && limits.cancelled()) reject("CANCELLED");
        if (limits.is_current && !limits.is_current(body->revision)) reject("STALE_REVISION");
        if (std::chrono::steady_clock::now()-started>=limits.timeout) reject("BODY_MATERIAL_DEADLINE");
        detail::require_interval_environment();
    };
    try {
        detail::require_interval_environment();
        if (!body || !body->revision || !body->partition || !body->guarded_settings || body->regions.empty() || body->regions.size()>1000)
            reject("MISSING_MATERIAL_BODY");
        if (!limits.max_records || limits.max_records>200000 || limits.timeout.count()<=0 || limits.timeout>std::chrono::seconds(30) ||
            !parameters.support_reference_id || !parameters.contact_reference_id || !parameters.model.model_id ||
            parameters.deposition_speed.value()<=0 || parameters.travel_speed.value()<=0 || parameters.acceleration.value()<=0)
            reject("INVALID_BODY_MATERIAL_CONTEXT");
        for (double value : {body->origin_error_upper_mm,body->partition->total_error_upper_mm})
            if (!std::isfinite(value) || value<0 || value>.05) reject("BODY_MATERIAL_NUMERIC_BUDGET");
        const auto *nozzles=body->executed_full_config.option<ConfigOptionFloats>("nozzle_diameter");
        if (!nozzles || nozzles->values.size()!=1 || !std::isfinite(nozzles->values.front()) || nozzles->values.front()<=0)
            reject("MISSING_BODY_NOZZLE_REFERENCE");
        stop(); const auto body_id=body->fingerprint(); stop();
        body->native_scale.require_current();
        std::vector<MaterialRecord> records; std::vector<BodyBeadReference> references;
        std::optional<PhysicalPosition> previous; size_t path_id=0, point_count=0; double conversion_error=0;
        const auto add=[&](PhysicalPosition a, PhysicalPosition b, Payload payload, std::optional<BeadSection> section,
                           uint64_t patch, int label, Speed speed) {
            if (records.size()>=limits.max_records) reject("BODY_MATERIAL_RECORD_LIMIT");
            const auto index=records.size();
            records.push_back({{index+1,index,patch,a,b,speed,parameters.acceleration,std::move(payload),label},section});
        };
        const auto translate=[&](Position<Frame::BuildPlate> p) {
            const std::array<double,3> v{p.x()+parameters.plate_origin.x(),p.y()+parameters.plate_origin.y(),p.z()+parameters.plate_origin.z()};
            const std::array<Interval,3> exact{Interval(p.x())+Interval(parameters.plate_origin.x()),
                Interval(p.y())+Interval(parameters.plate_origin.y()),Interval(p.z())+Interval(parameters.plate_origin.z())};
            Interval error(0);
            for (size_t axis=0; axis<3; ++axis) {
                if (!std::isfinite(v[axis]) || std::abs(v[axis])>NativeScale::max_coordinate_mm) reject("BODY_MATERIAL_COORDINATE_DOMAIN");
                error=error+Interval(absolute_upper(exact[axis]-Interval(v[axis])));
            }
            return std::pair<PhysicalPosition,double>{{v[0],v[1],v[2]},error.hi};
        };
        for (size_t r=0; r<body->regions.size(); ++r) {
            stop(); const auto &region=body->regions[r].geometry;
            if (!region || region->revision!=body->revision || region->native_layer_id>size_t(std::numeric_limits<int>::max()))
                reject("BODY_MATERIAL_REGION_BINDING");
            region->native_scale.require_current();
            for (size_t p=0; p<region->paths.size(); ++p) {
                stop(); const auto &path=region->paths[p]; ++path_id;
                if (path.points.size()<2 || path.points.size()!=path.native_points.size() || path.points.size()>200000-point_count)
                    reject("BODY_MATERIAL_POINT_LIMIT");
                point_count+=path.points.size();
                if (path.role!=erPerimeter && path.role!=erExternalPerimeter && path.role!=erInternalInfill &&
                    path.role!=erSolidInfill && path.role!=erTopSolidInfill && path.role!=erBottomSurface)
                    reject("UNSUPPORTED_BODY_MATERIAL_ROLE");
                if (!std::isfinite(path.width_mm) || !std::isfinite(path.height_mm) || !std::isfinite(path.mm3_per_mm) ||
                    path.height_mm<=0 || path.width_mm<=path.height_mm || path.width_mm>NativeScale::max_coordinate_mm ||
                    path.mm3_per_mm!=Flow(float(path.width_mm),float(path.height_mm),float(nozzles->values.front())).mm3_per_mm())
                    reject("UNSUPPORTED_NATIVE_MATERIAL_FLOW");
                if (!std::isfinite(path.coordinate_error_upper_mm) || path.coordinate_error_upper_mm<0)
                    reject("BODY_MATERIAL_NUMERIC_BUDGET");
                for (size_t segment=0; segment+1<path.points.size(); ++segment) {
                    if (segment%128==0) stop();
                    // Validate before signed native subtraction, including for
                    // diagnostic snapshots constructed outside the factory.
                    for (size_t i : {segment,segment+1}) {
                        const auto native=from_native_scaled<Frame::ModelLocal>(path.native_points[i],body->native_scale).position;
                        const auto &point=path.points[i];
                        if (point.x()!=native.x()+region->object_origin.x() || point.y()!=native.y()+region->object_origin.y() ||
                            point.z()!=region->native_print_z_mm+region->object_origin.z()) reject("BODY_MATERIAL_NATIVE_POINT_BINDING");
                    }
                    const auto a=translate(path.points[segment]), b=translate(path.points[segment+1]);
                    if (a.first.z()!=b.first.z() || path.native_points[segment].z()!=0 || path.native_points[segment+1].z()!=0)
                        reject("UNSUPPORTED_BODY_MATERIAL_SLOPE");
                    const auto dx=Interval(b.first.x())-Interval(a.first.x()), dy=Interval(b.first.y())-Interval(a.first.y());
                    const auto length=detail::root(detail::square(dx)+detail::square(dy));
                    if (length.lo<=0) reject("BODY_MATERIAL_ZERO_SEGMENT");
                    const double amount=((length.lo+length.hi)/2)*path.mm3_per_mm;
                    const auto area=Interval(amount)/length, height=Interval(path.height_mm);
                    const auto width=area/height+(Interval(1)-Interval(3.141592653589793,3.1415926535897936)/Interval(4))*height;
                    const double nominal_width=(width.lo+width.hi)/2;
                    const double width_error=absolute_upper(width-Interval(path.width_mm));
                    const auto native_dx=Interval(double(path.native_points[segment+1].x()-path.native_points[segment].x()))*Interval(body->native_scale.mm_per_unit());
                    const auto native_dy=Interval(double(path.native_points[segment+1].y()-path.native_points[segment].y()))*Interval(body->native_scale.mm_per_unit());
                    const auto native_volume=detail::root(detail::square(native_dx)+detail::square(native_dy))*Interval(path.mm3_per_mm);
                    const double volume_error=absolute_upper(native_volume-Interval(amount));
                    const auto error=Interval(path.coordinate_error_upper_mm)+Interval(std::max(a.second,b.second))+Interval(width_error)/Interval(2);
                    conversion_error=std::max(conversion_error,error.hi);
                    if (previous && (previous->x()!=a.first.x() || previous->y()!=a.first.y() || previous->z()!=a.first.z()))
                        add(*previous,a.first,Travel{},{},0,int(region->native_layer_id),parameters.travel_speed);
                    references.push_back({records.size(),r,p,segment,width_error,volume_error});
                    add(a.first,b.first,Deposition{Volume(amount),WidthXY(nominal_width),VerticalGap(path.height_mm),VerticalGap(path.height_mm),
                        parameters.material,parameters.support_reference_id,parameters.contact_reference_id},
                        BeadSection{BeadSectionKind::RoundedRectangle,path.height_mm,path.height_mm,{width.lo,width.hi}},
                        path_id,int(region->native_layer_id),parameters.deposition_speed);
                    previous=b.first;
                }
            }
        }
        auto model=parameters.model;
        const auto total=Interval(model.numerical_coordinate_error.value())+Interval(body->partition->total_error_upper_mm)+
            Interval(body->origin_error_upper_mm)+Interval(conversion_error);
        if (total.hi>.05) reject("BODY_MATERIAL_NUMERIC_BUDGET");
        model.numerical_coordinate_error=Length(total.hi);
        auto remaining=limits;
        remaining.timeout-=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        remaining.cancelled=[&] { stop(); return false; }; remaining.is_current={};
        const auto captured=capture_material_sequence(records,model,body->revision,body_id,remaining);
        if (!captured.snapshot) throw Rejection(captured.reason);
        stop(); const auto material_id=captured.snapshot->fingerprint(); stop();
        auto snapshot=std::make_shared<const BodyMaterialSnapshot>(BodyMaterialSnapshot{body,captured.snapshot,body_id,material_id,
            parameters.plate_origin,std::move(references)});
        body_material_hash(*snapshot,stop); stop();
        return {"DECLARED_NATIVE_BODY_MATERIAL_ONLY",std::move(snapshot)};
    } catch (const Rejection &e) { return {e.what(),{}}; }
    catch (const std::exception &e) { return {"BODY_MATERIAL_CAPTURE_OR_NUMERIC_FAILURE: "+std::string(e.what()),{}}; }
}
}
