#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"

#include <Motion/CubismMotionManager.hpp>
#include <cstdlib>

namespace bongo_cat {

void NativeModel::start_idle_motion() {
    if (idle_motion_keys_.empty()) return;
    constexpr int priority = 1;
    if (!_motionManager->ReserveMotion(priority)) return;
    int next = std::rand() % (int)idle_motion_keys_.size();
    if (next == last_idle_motion_)
        next = (next + 1) % (int)idle_motion_keys_.size();
    last_idle_motion_ = next;
    const std::string &key = idle_motion_keys_[(size_t)next];
    auto found = motions_.find(key);
    if (found != motions_.end())
        _motionManager->StartMotionPriority(found->second, false, priority);
}

} // namespace bongo_cat
