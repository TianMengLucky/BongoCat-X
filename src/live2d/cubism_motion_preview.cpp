#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"

#include <Motion/CubismMotionManager.hpp>

namespace bongo_cat {

void NativeModel::capture_motion_preview() {
    if (!_model || motion_preview_active_) return;
    const int parameter_count = _model->GetParameterCount();
    const bool overrides_were_applied = parameter_overrides_applied_;
    std::vector<float> current_parameters((size_t)parameter_count);
    for (int i = 0; i < parameter_count; ++i)
        current_parameters[(size_t)i] = _model->GetParameterValue(i);
    _model->LoadParameters();
    parameter_overrides_applied_ = false;
    capture_parameter_baseline();
    motion_preview_parameters_.resize((size_t)parameter_count);
    for (int i = 0; i < parameter_count; ++i) {
        motion_preview_parameters_[(size_t)i] = _model->GetParameterValue(i);
        _model->SetParameterValue(i, current_parameters[(size_t)i]);
    }
    if (overrides_were_applied) apply_parameter_overrides();
    const int part_count = _model->GetPartCount();
    motion_preview_parts_.resize((size_t)part_count);
    for (int i = 0; i < part_count; ++i)
        motion_preview_parts_[(size_t)i] = _model->GetPartOpacity(i);
    motion_preview_opacity_ = _model->GetModelOpacity();
    motion_preview_active_ = true;
}

void NativeModel::restore_motion_preview_state() {
    if (!_model || !motion_preview_active_) return;
    _motionManager->StopAllMotions();
    clear_motion_runs();
    parameter_overrides_applied_ = false;
    const int parameter_count = _model->GetParameterCount();
    for (int i = 0; i < parameter_count &&
        (size_t)i < motion_preview_parameters_.size(); ++i)
        _model->SetParameterValue(i, motion_preview_parameters_[(size_t)i]);
    save_parameters();
    const int part_count = _model->GetPartCount();
    for (int i = 0; i < part_count &&
        (size_t)i < motion_preview_parts_.size(); ++i)
        _model->SetPartOpacity(i, motion_preview_parts_[(size_t)i]);
    _model->SetModelOpacity(motion_preview_opacity_);
    _opacity = motion_preview_opacity_;
    apply_parameter_overrides();
}

bool NativeModel::preview_motion(const char *group, int index) {
    if (!_model || !group || index < 0) return false;
    if (motion_preview_active_) restore_motion_preview_state();
    else capture_motion_preview();
    std::string key = std::string(group) + "_" + std::to_string(index);
    auto owner = motion_toggle_owners_.find(key);
    motion_preview_key_ = owner == motion_toggle_owners_.end() ?
        key : owner->second;
    bool active = selected_motion_keys_.find(motion_preview_key_) !=
        selected_motion_keys_.end();
    std::string playback = motion_to_play(key, &motion_preview_selected_);
    /* A running one-shot is previewed as a cancellation, while a committed
       click keeps the normal replay behavior in start_motion(). */
    if (active && !motion_is_persistent(motion_preview_key_)) {
        motion_preview_selected_ = false;
        playback.clear();
    }
    motion_preview_completed_ = false;
    if (playback.empty())
        return restore_motion_defaults(motion_preview_key_);
    auto found = motions_.find(playback);
    if (found == motions_.end()) {
        restore_motion_preview();
        return false;
    }
    constexpr int priority = 2;
    Csm::CubismMotionQueueEntryHandle handle =
        _motionManager->StartMotionPriority(found->second, false, priority);
    if (handle == Csm::InvalidMotionQueueEntryHandleValue) {
        restore_motion_preview();
        return false;
    }
    record_motion_run(handle, motion_preview_key_,
        motion_preview_selected_, false);
    return true;
}

bool NativeModel::commit_motion_preview(const char *group, int index) {
    if (!motion_preview_active_ || !group || index < 0) return false;
    std::string key = std::string(group) + "_" + std::to_string(index);
    auto owner = motion_toggle_owners_.find(key);
    const std::string &canonical = owner == motion_toggle_owners_.end() ?
        key : owner->second;
    if (canonical != motion_preview_key_) return false;
    select_motion(canonical, motion_preview_selected_);
    for (auto &run : motion_runs_)
        if (!run.committed && run.key == canonical) run.committed = true;
    if (motion_preview_completed_ && !motion_is_persistent(canonical))
        select_motion(canonical, false);
    motion_preview_parameters_.clear();
    motion_preview_parts_.clear();
    motion_preview_key_.clear();
    motion_preview_active_ = false;
    motion_preview_completed_ = false;
    return true;
}

bool NativeModel::restore_motion_preview() {
    if (!motion_preview_active_) return false;
    restore_motion_preview_state();
    motion_preview_parameters_.clear();
    motion_preview_parts_.clear();
    motion_preview_key_.clear();
    motion_preview_active_ = false;
    motion_preview_completed_ = false;
    return true;
}

} // namespace bongo_cat
