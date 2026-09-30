#pragma once
#include "../Config.hpp"
#include <cstring>
#include <limits>
#include <string_view>

// Internal exact encoder shared by settings and source-input identities.
namespace Slic3r::nptop::detail {
class CanonicalConfigWriter {
public:
    void append(std::string_view value) {
        if (value.size()>4*1024*1024-bytes.size()) throw std::length_error("Canonical config byte limit");
        bytes.append(value);
    }
    void value(bool v) { append(v ? "true" : "false"); }
    void value(int v) { append(std::to_string(v)); }
    void value(unsigned char v) { value(int(v)); }
    void value(double v) {
        static_assert(sizeof(double)==sizeof(uint64_t) && std::numeric_limits<double>::is_iec559);
        uint64_t bits; std::memcpy(&bits,&v,sizeof bits);
        char encoded[16];
        for (int i=0; i<16; ++i) encoded[i]=hex[(bits>>(60-4*i))&15];
        append("\""); append(std::string_view(encoded,sizeof encoded)); append("\"");
    }
    void value(const std::string &v) {
        append("\"");
        for (unsigned char c : v) { const char pair[]{hex[c>>4],hex[c&15]}; append(std::string_view(pair,2)); }
        append("\"");
    }
    void value(const Vec2d &v) { append("["); value(v.x()); append(","); value(v.y()); append("]"); }
    void value(const Vec3d &v) { append("["); value(v.x()); append(","); value(v.y()); append(","); value(v.z()); append("]"); }
    void value(const FloatOrPercent &v) { append("["); value(v.value); append(","); value(v.percent); append("]"); }
    template<class T, class Allocator> void value(const std::vector<T,Allocator> &values) {
        append("["); bool first=true;
        for (const auto &v : values) { if (!first) append(","); first=false; value(v); }
        append("]");
    }
    void dictionary(const t_config_enum_values *names) {
        if (!names) { append("null"); return; }
        append("["); bool first=true;
        for (const auto &[name,number] : *names) {
            if (!first) append(","); first=false;
            append("["); value(name); append(","); value(number); append("]");
        }
        append("]");
    }
    template<class T> const T &native(const ConfigOption &option) {
        const auto *typed=dynamic_cast<const T *>(&option);
        if (!typed) throw ConfigurationError("Unsupported native canonical config representation");
        return *typed;
    }
    void option(const ConfigOption &option) {
        switch (option.type()) {
        case coFloat: case coPercent: value(native<ConfigOptionSingle<double>>(option).value); break;
        case coFloats: case coPercents: value(native<ConfigOptionVector<double>>(option).values); break;
        case coInt: value(native<ConfigOptionInt>(option).value); break;
        case coInts: value(native<ConfigOptionVector<int>>(option).values); break;
        case coString: value(native<ConfigOptionString>(option).value); break;
        case coStrings: value(native<ConfigOptionStrings>(option).values); break;
        case coBool: value(native<ConfigOptionBool>(option).value); break;
        case coBools: value(native<ConfigOptionVector<unsigned char>>(option).values); break;
        case coFloatOrPercent: {
            const auto &v=native<ConfigOptionFloatOrPercent>(option);
            value(FloatOrPercent{v.value,v.percent}); break;
        }
        case coFloatsOrPercents: value(native<ConfigOptionVector<FloatOrPercent>>(option).values); break;
        case coPoint: value(native<ConfigOptionPoint>(option).value); break;
        case coPoints: value(native<ConfigOptionPoints>(option).values); break;
        case coPoint3: value(native<ConfigOptionPoint3>(option).value); break;
        case coPointsGroups: value(native<ConfigOptionPointsGroups>(option).values); break;
        case coIntsGroups: value(native<ConfigOptionIntsGroups>(option).values); break;
        case coEnum:
            if (const auto *v=dynamic_cast<const ConfigOptionEnumGeneric *>(&option)) {
                append("[\"generic\","); dictionary(v->keys_map); append(","); value(v->value); append("]");
            } else { append("[\"native\","); value(option.getInt()); append("]"); }
            break;
        case coEnums: {
            const auto *plain=dynamic_cast<const ConfigOptionEnumsGeneric *>(&option);
            const auto *nullable=dynamic_cast<const ConfigOptionEnumsGenericNullable *>(&option);
            if (!plain && !nullable) throw ConfigurationError("Unsupported native canonical enum vector");
            append("[\"generic\","); dictionary(plain ? plain->keys_map : nullable->keys_map); append(",");
            value(native<ConfigOptionVector<int>>(option).values); append("]"); break;
        }
        default: throw ConfigurationError("Unsupported native canonical config type");
        }
    }
    std::string take() { return std::move(bytes); }
private:
    static constexpr char hex[]="0123456789abcdef";
    std::string bytes;
};
}
