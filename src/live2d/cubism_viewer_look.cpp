#include "cubism_plugin_services.hpp"
#include "cubism_viewer_look.hpp"

#include <CubismFramework.hpp>
#include <Id/CubismIdManager.hpp>
#include <algorithm>
#include <cmath>

namespace bongo_cat {

namespace {
constexpr Csm::csmFloat32 frame_rate = 30.0f;
constexpr Csm::csmFloat32 maximum_speed = 7.2727275f / frame_rate;
constexpr Csm::csmFloat32 acceleration_time = 0.15f;
// Full-screen pointer mapping needs roughly pixel precision to avoid visible steps.
constexpr Csm::csmFloat32 epsilon = 0.001f;
constexpr Csm::csmFloat32 viewer_input_gain = 1.2f;
}

ViewerLookUpdater::ViewerLookUpdater(Csm::CubismModel &model)
    : Csm::ICubismUpdater(Csm::CubismUpdateOrder_Look) {
    add_parameter(model, "ParamAngleX", Axis::X);
    add_parameter(model, "ParamAngleY", Axis::Y);
    add_parameter(model, "ParamAngleZ", Axis::Z);
    add_parameter(model, "ParamEyeBallX", Axis::X);
    add_parameter(model, "ParamEyeBallY", Axis::Y);
    add_parameter(model, "ParamBodyAngleX", Axis::X);
}

void ViewerLookUpdater::add_parameter(Csm::CubismModel &model,
    const char *id, Axis axis) {
    if (parameter_count_ >= 6) return;
    Csm::CubismIdHandle handle =
        Csm::CubismFramework::GetIdManager()->GetId(id);
    Csm::csmInt32 index = model.GetParameterIndex(handle);
    if (index < 0 || index >= model.GetParameterCount()) return;
    parameters_[parameter_count_++] = {index, axis, 1.0f};
}

void ViewerLookUpdater::set_target(Csm::csmFloat32 x,
    Csm::csmFloat32 y, bool angle_z_enabled) {
    target_x_ = std::max(-1.0f, std::min(1.0f, x));
    target_y_ = std::max(-1.0f, std::min(1.0f, y));
    angle_z_enabled_ = angle_z_enabled;
}

void ViewerLookUpdater::update_target(Csm::csmFloat32 delta_seconds) {
    if (!initialized_) {
        initialized_ = true;
        return;
    }
    const Csm::csmFloat32 delta_weight = delta_seconds * frame_rate;
    const Csm::csmFloat32 maximum_acceleration =
        delta_weight * maximum_speed / (acceleration_time * frame_rate);
    const Csm::csmFloat32 dx = target_x_ - face_x_;
    const Csm::csmFloat32 dy = target_y_ - face_y_;
    if (std::fabs(dx) <= epsilon && std::fabs(dy) <= epsilon) return;

    const Csm::csmFloat32 distance = std::sqrt(dx * dx + dy * dy);
    const Csm::csmFloat32 desired_x = maximum_speed * dx / distance;
    const Csm::csmFloat32 desired_y = maximum_speed * dy / distance;
    Csm::csmFloat32 acceleration_x = desired_x - velocity_x_;
    Csm::csmFloat32 acceleration_y = desired_y - velocity_y_;
    const Csm::csmFloat32 acceleration = std::sqrt(
        acceleration_x * acceleration_x + acceleration_y * acceleration_y);
    if (acceleration > maximum_acceleration) {
        acceleration_x *= maximum_acceleration / acceleration;
        acceleration_y *= maximum_acceleration / acceleration;
    }
    velocity_x_ += acceleration_x;
    velocity_y_ += acceleration_y;

    const Csm::csmFloat32 stopping_speed = 0.5f * (
        std::sqrt(maximum_acceleration * maximum_acceleration +
            8.0f * maximum_acceleration * distance) - maximum_acceleration);
    const Csm::csmFloat32 speed =
        std::sqrt(velocity_x_ * velocity_x_ + velocity_y_ * velocity_y_);
    if (speed > stopping_speed) {
        velocity_x_ *= stopping_speed / speed;
        velocity_y_ *= stopping_speed / speed;
    }
    face_x_ += velocity_x_;
    face_y_ += velocity_y_;
}

Csm::csmFloat32 ViewerLookUpdater::x() const {
    return std::fabs(face_x_) < epsilon ? 0.0f : face_x_;
}

Csm::csmFloat32 ViewerLookUpdater::y() const {
    return std::fabs(face_y_) < epsilon ? 0.0f : face_y_;
}

void ViewerLookUpdater::OnLateUpdate(Csm::CubismModel *model,
    Csm::csmFloat32 delta_seconds) {
    if (!model) return;
    update_target(delta_seconds);
    const Csm::csmFloat32 input_x = x();
    const Csm::csmFloat32 input_y = y();
    for (Csm::csmInt32 i = 0; i < parameter_count_; ++i) {
        const Parameter &parameter = parameters_[i];
        if (parameter.axis == Axis::Z && !angle_z_enabled_) continue;
        const Csm::csmFloat32 current = model->GetParameterValue(parameter.index);
        Csm::csmFloat32 input = parameter.axis == Axis::X ? input_x : input_y;
        Csm::csmFloat32 gain = viewer_input_gain;
        Csm::csmFloat32 bound = current >= 0.0f
            ? model->GetParameterMaximumValue(parameter.index)
            : model->GetParameterMinimumValue(parameter.index);
        if (parameter.axis == Axis::Z) {
            input = -input_x * input_y;
            gain = 1.0f;
            bound = input >= 0.0f
                ? model->GetParameterMaximumValue(parameter.index)
                : model->GetParameterMinimumValue(parameter.index);
        }
        model->AddParameterValue(parameter.index,
            input * std::fabs(bound) * gain, parameter.weight);
    }
}

} // namespace bongo_cat
