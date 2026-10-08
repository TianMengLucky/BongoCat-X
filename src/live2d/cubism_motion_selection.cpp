#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"

namespace bongo_cat {

void NativeModel::select_motion(const std::string &key, bool selected) {
    auto owner = motion_toggle_owners_.find(key);
    const std::string &canonical = owner == motion_toggle_owners_.end() ?
        key : owner->second;
    if (!selected) { selected_motion_keys_.erase(canonical); return; }
    const std::string &signature = motion_signatures_[canonical];
    for (auto item = selected_motion_keys_.begin();
        item != selected_motion_keys_.end();) {
        auto existing = motion_signatures_.find(*item);
        if (!signature.empty() && existing != motion_signatures_.end() &&
            existing->second == signature)
            item = selected_motion_keys_.erase(item);
        else ++item;
    }
    selected_motion_keys_.insert(canonical);
}

} // namespace bongo_cat
