#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"

#include <CubismFramework.hpp>
#include <Id/CubismIdManager.hpp>
#include <Model/CubismModel.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include "bongo_cat/json_dom.h"

namespace bongo_cat {

static bool curve_endpoints(BongoJsonValue *segments, float *start, float *end) {
    if (!bongo_json_is_arr(segments) || bongo_json_arr_size(segments) < 2) return false;
    BongoJsonValue *first = bongo_json_arr_get(segments, 1);
    BongoJsonValue *last = bongo_json_arr_get(segments, bongo_json_arr_size(segments) - 1);
    if (!bongo_json_is_num(first) || !bongo_json_is_num(last)) return false;
    *start = (float)bongo_json_get_num(first);
    *end = (float)bongo_json_get_num(last);
    return true;
}

static bool curve_returns_to_default(BongoJsonValue *segments, float normal) {
    float start = 0.0f, end = 0.0f;
    if (!curve_endpoints(segments, &start, &end) ||
        std::fabs(start - normal) > .0001f ||
        std::fabs(end - normal) > .0001f) return false;
    bool deviates = false;
    size_t cursor = 2, count = bongo_json_arr_size(segments);
    while (cursor < count) {
        BongoJsonValue *kind_value = bongo_json_arr_get(segments, cursor);
        if (!bongo_json_is_int(kind_value) && !bongo_json_is_uint(kind_value))
            return false;
        int kind = (int)bongo_json_get_int(kind_value);
        size_t points = kind == 1 ? 3 :
            (kind == 0 || kind == 2 || kind == 3 ? 1 : 0);
        if (!points || cursor + points * 2 >= count) return false;
        for (size_t point = 0; point < points; ++point) {
            BongoJsonValue *value = bongo_json_arr_get(segments,
                cursor + 2 + point * 2);
            if (!bongo_json_is_num(value)) return false;
            if (std::fabs((float)bongo_json_get_num(value) - normal) > .0001f)
                deviates = true;
        }
        cursor += 1 + points * 2;
    }
    return deviates;
}

static bool curve_has_state_target(const NativeModel::MotionStateCurve &curve) {
    return curve.parameter >= 0 || curve.part >= 0 || curve.model_opacity;
}

void NativeModel::load_motion_state(const std::string &key, const char *group,
    int motion_index, const std::vector<unsigned char> &bytes) {
    BongoJsonDoc *document = bongo_json_read(
        reinterpret_cast<const char *>(bytes.data()), bytes.size(), 0);
    BongoJsonValue *root = document ? bongo_json_doc_get_root(document) : nullptr;
    BongoJsonValue *curves = bongo_json_is_obj(root) ? bongo_json_obj_get(root, "Curves") : nullptr;
    MotionState state; state.group = group ? group : "";
    state.index = motion_index;
    std::vector<std::string> targets;
    size_t index, count; BongoJsonValue *curve;
    if (bongo_json_is_arr(curves)) bongo_json_arr_foreach(curves, index, count, curve) {
        const char *target = bongo_json_get_str(bongo_json_obj_get(curve, "Target"));
        const char *id = bongo_json_get_str(bongo_json_obj_get(curve, "Id"));
        if (!target || !id) continue;
        targets.push_back(std::string(target) + ":" + id);
        MotionStateCurve value{target, id};
        BongoJsonValue *segments = bongo_json_obj_get(curve, "Segments");
        if (std::strcmp(target, "Parameter") == 0) {
            auto handle = Csm::CubismFramework::GetIdManager()->GetId(id);
            value.parameter = _model->GetParameterIndex(handle);
            if (value.parameter >= 0 &&
                value.parameter < _model->GetParameterCount())
                value.normal = _model->GetParameterDefaultValue(value.parameter);
            else value.parameter = -1;
        } else if (std::strcmp(target, "PartOpacity") == 0) {
            auto handle = Csm::CubismFramework::GetIdManager()->GetId(id);
            value.part = _model->GetPartIndex(handle);
            if (value.part >= 0 && value.part < _model->GetPartCount())
                value.normal = _model->GetPartOpacity(value.part);
            else value.part = -1;
        } else if (std::strcmp(target, "Model") == 0 &&
            std::strcmp(id, "Opacity") == 0) {
            value.model_opacity = true;
            value.normal = _model->GetModelOpacity();
        }
        if (curve_has_state_target(value) &&
            curve_returns_to_default(segments, value.normal))
            state.self_contained = true;
        if (curve_endpoints(segments, &value.start, &value.end))
            state.curves.push_back(value);
    }
    if (document) bongo_json_doc_free(document);
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    for (const std::string &target : targets)
        motion_signatures_[key] += target + '\n';
    std::sort(state.curves.begin(), state.curves.end(),
        [](const MotionStateCurve &a, const MotionStateCurve &b) {
            return a.target == b.target ? a.id < b.id : a.target < b.target;
        });
    motion_states_[key] = std::move(state);
}

bool motion_toggle_pair(const NativeModel::MotionState &a,
    const NativeModel::MotionState &b, bool *first_enabled) {
    if (!first_enabled || a.group != b.group ||
        a.curves.size() != b.curves.size() || a.curves.empty()) return false;
    bool direction_known = false;
    for (size_t i = 0; i < a.curves.size(); ++i) {
        const auto &left = a.curves[i];
        const auto &right = b.curves[i];
        if (left.target != right.target || left.id != right.id) return false;
        bool same = std::fabs(left.start - right.start) <= .0001f &&
            std::fabs(left.end - right.end) <= .0001f;
        bool reversed = std::fabs(left.start - right.end) <= .0001f &&
            std::fabs(left.end - right.start) <= .0001f;
        if (!same && !reversed) return false;
        if (!reversed || same) continue;
        if (!curve_has_state_target(left) || !curve_has_state_target(right))
            return false;
        bool left_normal = std::fabs(left.end - left.normal) <= .0001f;
        bool right_normal = std::fabs(right.end - right.normal) <= .0001f;
        if (left_normal == right_normal) return false;
        bool enabled = !left_normal;
        if (direction_known && enabled != *first_enabled) return false;
        *first_enabled = enabled;
        direction_known = true;
    }
    return direction_known;
}

bool motion_enables_state(const NativeModel::MotionState &state) {
    if (state.self_contained) return false;
    for (const auto &curve : state.curves)
        if (curve_has_state_target(curve) &&
            std::fabs(curve.end - curve.normal) > .0001f) return true;
    return false;
}

static bool same_signature(const NativeModel::MotionSignatures &signatures,
    const std::string &left, const std::string &right) {
    auto a = signatures.find(left), b = signatures.find(right);
    return a != signatures.end() && b != signatures.end() &&
        !a->second.empty() && a->second == b->second;
}

static size_t pair_candidates(
    const std::map<std::string, NativeModel::MotionState> &states,
    const NativeModel::MotionSignatures &signatures, const std::string &key,
    std::string *candidate, bool *key_enabled) {
    auto source = states.find(key);
    if (source == states.end()) return 0;
    size_t count = 0;
    for (const auto &item : states) {
        if (item.first == key || !same_signature(signatures, key, item.first))
            continue;
        bool enabled = false;
        if (!motion_toggle_pair(source->second, item.second, &enabled)) continue;
        if (++count == 1) {
            if (candidate) *candidate = item.first;
            if (key_enabled) *key_enabled = enabled;
        }
    }
    return count;
}

void NativeModel::pair_motion_states() {
    for (const auto &item : motion_states_) {
        if (motion_toggle_owners_.find(item.first) != motion_toggle_owners_.end())
            continue;
        std::string candidate;
        bool item_enabled = false;
        if (pair_candidates(motion_states_, motion_signatures_, item.first,
            &candidate, &item_enabled) != 1 ||
            pair_candidates(motion_states_, motion_signatures_, candidate,
                nullptr, nullptr) != 1) continue;
        const std::string &owner = item_enabled ? item.first : candidate;
        const std::string &partner = item_enabled ? candidate : item.first;
        motion_toggle_partners_[owner] = partner;
        motion_toggle_owners_[owner] = owner;
        motion_toggle_owners_[partner] = owner;
    }
}

std::string NativeModel::motion_to_play(const std::string &key,
    bool *selected) const {
    auto owner = motion_toggle_owners_.find(key);
    const std::string &canonical = owner == motion_toggle_owners_.end() ?
        key : owner->second;
    auto partner = motion_toggle_partners_.find(canonical);
    bool active = selected_motion_keys_.find(canonical) != selected_motion_keys_.end();
    bool persistent = motion_is_persistent(canonical);
    /* One-shot actions stay checked only while their own playback is alive. */
    if (selected) *selected = persistent ? !active : true;
    if (active && persistent && partner == motion_toggle_partners_.end()) return {};
    return active && partner != motion_toggle_partners_.end() ?
        partner->second : canonical;
}

bool NativeModel::motion_is_persistent(const std::string &key) const {
    auto owner = motion_toggle_owners_.find(key);
    const std::string &canonical = owner == motion_toggle_owners_.end() ?
        key : owner->second;
    if (motion_toggle_partners_.find(canonical) != motion_toggle_partners_.end())
        return true;
    auto state = motion_states_.find(canonical);
    return state != motion_states_.end() && motion_enables_state(state->second);
}

bool NativeModel::motion_persistent(const char *group, int index) const {
    if (!group || index < 0) return false;
    return motion_is_persistent(
        std::string(group) + "_" + std::to_string(index));
}

bool NativeModel::restore_motion_defaults(const std::string &key) {
    auto state = motion_states_.find(key);
    if (_model == nullptr || state == motion_states_.end()) return false;
    bool had_run = false;
    for (const MotionRun &run : motion_runs_)
        if (run.key == key) { had_run = true; break; }
    bool restored = false;
    stop_motion_runs(key);
    for (const auto &curve : state->second.curves) {
        restored = apply_motion_curve(curve, curve.normal) || restored;
    }
    if (!restored) return had_run;
    save_parameters();
    return true;
}

bool NativeModel::apply_motion_curve(const MotionStateCurve &curve,
    float value) {
    if (!_model) return false;
    if (curve.parameter >= 0 &&
        curve.parameter < _model->GetParameterCount()) {
        const size_t index = (size_t)curve.parameter;
        parameter_baseline_values_[index] = value;
        float displayed = parameter_overrides_applied_ &&
            parameter_overrides_[index] ? parameter_override_values_[index] : value;
        _model->SetParameterValue(curve.parameter, displayed);
        return true;
    }
    if (curve.part >= 0 && curve.part < _model->GetPartCount()) {
        _model->SetPartOpacity(curve.part, value);
        return true;
    }
    if (curve.model_opacity) {
        _model->SetModelOpacity(value);
        _opacity = value;
        return true;
    }
    return false;
}

bool NativeModel::restore_motion_state(const char *group, int index) {
    if (!_model || !group || index < 0) return false;
    std::string key = std::string(group) + "_" + std::to_string(index);
    auto owner = motion_toggle_owners_.find(key);
    const std::string &canonical = owner == motion_toggle_owners_.end() ?
        key : owner->second;
    auto state = motion_states_.find(canonical);
    if (state == motion_states_.end() || !motion_is_persistent(canonical))
        return false;
    bool restored = false;
    stop_motion_runs(canonical);
    for (const auto &curve : state->second.curves)
        restored = apply_motion_curve(curve, curve.end) || restored;
    if (!restored) return false;
    select_motion(canonical, true);
    save_parameters();
    return true;
}

} // namespace bongo_cat
