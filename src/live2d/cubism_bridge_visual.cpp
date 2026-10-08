#include "cubism_plugin_services.hpp"
#include "cubism_runtime.hpp"

extern "C" bool bongo_cat_model_runtime_visual_state(
    const BongoCatModelRuntime *runtime, BongoCatModelRuntimeVisualState *state) {
    return runtime && runtime->model && runtime->model->visual_state(state);
}
