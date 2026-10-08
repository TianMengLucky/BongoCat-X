#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"

#include <Motion/CubismExpressionMotionManager.hpp>
#include <Motion/CubismMotionQueueEntry.hpp>
#include <cmath>

namespace bongo_cat {

bool NativeModel::set_expression(int index) {
    if (index == -1) {
        expression_index_ = -1;
        expression_frame_pending_ = false;
        auto *entries = _expressionManager->GetCubismMotionQueueEntries();
        expression_clearing_ = entries && entries->GetSize() > 0;
        if (!expression_clearing_) {
            _expressionManager->StopAllMotions();
            return true;
        }
        for (Csm::csmUint32 i = 0; i < entries->GetSize(); ++i) {
            Csm::CubismMotionQueueEntry *entry = entries->At(i);
            if (!entry) continue;
            Csm::ACubismMotion *motion = entry->GetCubismMotion();
            if (motion) {
                float fade_out = motion->GetFadeOutTime();
                entry->SetFadeout(fade_out < 0.0f ? 0.0f : fade_out);
            }
        }
        return true;
    }
    if (index < 0 || (size_t)index >= expression_names_.size()) return false;
    auto found = expressions_.find(expression_names_[(size_t)index]);
    if (found == expressions_.end()) return false;
    Csm::CubismMotionQueueEntryHandle handle = _expressionManager->StartMotion(
        found->second, false);
    if (handle == Csm::InvalidMotionQueueEntryHandleValue) return false;
    expression_clearing_ = false;
    expression_index_ = index;
    expression_frame_pending_ = true;
    return true;
}

void NativeModel::settle_pending_expression_for_cover() {
    if (!expression_frame_pending_ || !_model || expression_index_ < 0 ||
        (size_t)expression_index_ >= expression_names_.size()) return;
    auto found = expressions_.find(expression_names_[(size_t)expression_index_]);
    if (found == expressions_.end()) {
        expression_frame_pending_ = false;
        return;
    }
    float fade_seconds = found->second->GetFadeInTime();
    if (!std::isfinite(fade_seconds) || fade_seconds < 0.0f)
        fade_seconds = 0.0f;
    /* Only the expression clock is advanced. The model baseline already
       contains restored persistent motions, while idle effects stay put. */
    _model->LoadParameters();
    _expressionManager->UpdateMotion(_model, 0.0f);
    if (fade_seconds > 0.0f)
        _expressionManager->UpdateMotion(_model, fade_seconds);
    expression_frame_pending_ = false;
}

void NativeModel::expire_expression_fade() {
    if (!expression_clearing_) return;
    auto *entries = _expressionManager->GetCubismMotionQueueEntries();
    if (!entries || entries->GetSize() == 0) {
        expression_clearing_ = false;
        return;
    }
    for (Csm::csmUint32 i = 0; i < entries->GetSize(); ++i) {
        Csm::CubismMotionQueueEntry *entry = entries->At(i);
        if (entry && (entry->GetEndTime() < 0.0f ||
            entry->GetStateTime() < entry->GetEndTime())) return;
    }
    _expressionManager->StopAllMotions();
    expression_clearing_ = false;
}

} // namespace bongo_cat
