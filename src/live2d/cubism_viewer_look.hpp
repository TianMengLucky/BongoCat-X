#ifndef BONGO_CAT_CUBISM_VIEWER_LOOK_HPP
#define BONGO_CAT_CUBISM_VIEWER_LOOK_HPP

#include <Motion/ICubismUpdater.hpp>

namespace bongo_cat {

class ViewerLookUpdater final : public Csm::ICubismUpdater {
public:
    explicit ViewerLookUpdater(Csm::CubismModel &model);
    void set_target(Csm::csmFloat32 x, Csm::csmFloat32 y,
        bool angle_z_enabled);
    void OnLateUpdate(Csm::CubismModel *model,
        Csm::csmFloat32 delta_seconds) override;

private:
    enum class Axis { X, Y, Z };
    struct Parameter {
        Csm::csmInt32 index;
        Axis axis = Axis::X;
        Csm::csmFloat32 weight;
    };

    void add_parameter(Csm::CubismModel &model, const char *id, Axis axis);
    void update_target(Csm::csmFloat32 delta_seconds);
    Csm::csmFloat32 x() const;
    Csm::csmFloat32 y() const;

    Parameter parameters_[6]{};
    Csm::csmInt32 parameter_count_ = 0;
    Csm::csmFloat32 target_x_ = 0.0f;
    Csm::csmFloat32 target_y_ = 0.0f;
    Csm::csmFloat32 face_x_ = 0.0f;
    Csm::csmFloat32 face_y_ = 0.0f;
    Csm::csmFloat32 velocity_x_ = 0.0f;
    Csm::csmFloat32 velocity_y_ = 0.0f;
    bool angle_z_enabled_ = false;
    bool initialized_ = false;
};

} // namespace bongo_cat

#endif
