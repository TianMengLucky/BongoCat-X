#ifndef BONGO_CAT_CUBISM_MODEL_HPP
#define BONGO_CAT_CUBISM_MODEL_HPP

#ifdef CSM_TARGET_MAC_GL
#include "cubism_core_profile.hpp"
#endif

#include "bongo_cat/model.h"
#include "bongo_cat/image.h"
#include "cubism_mask_policy.hpp"
#include "cubism_texture_refresh_memory.hpp"

#include <Model/CubismUserModel.hpp>
#include <CubismModelSettingJson.hpp>
#include <Motion/ACubismMotion.hpp>
#include <Math/CubismMatrix44.hpp>
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
#include <Rendering/Vulkan/CubismRenderer_Vulkan.hpp>
#endif
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#include <SDL3/SDL_opengl.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace bongo_cat {

bool validate_model_setting_json(const std::vector<unsigned char> &json,
    const char *setting_file, BongoCatError *error);
class ViewerLookUpdater;
class ParameterOverrideUpdater;
struct ModelTexture;
struct TextureRefresh;
struct TextureResolution;

class NativeModel final : public Csm::CubismUserModel {
public:
    NativeModel();
    ~NativeModel() override;
    bool load(const char *directory, const char *setting_file, bool direct_textures,
        bool dynamic_texture_resolution,
        BongoCatLive2DLoadProgress progress, void *userdata,
        BongoCatError *error);
    /* Keep the internal test and tooling call shape source-compatible. */
    bool load(const char *directory, const char *setting_file, bool direct_textures,
        BongoCatLive2DLoadProgress progress, void *userdata,
        BongoCatError *error) {
        return load(directory, setting_file, direct_textures, false,
            progress, userdata, error);
    }
    bool load_textures(BongoCatError *error,
        BongoCatLive2DLoadProgress progress, void *userdata,
        int display_width = 0, int display_height = 0,
        float render_quality_percent = 100.0f);
    size_t texture_count() const { return textures_.size(); }
    double texture_storage_mib() const;
    float render_quality_percent() const { return render_quality_percent_; }
    bool try_reuse_texture_quality(float quality_percent);
    void release_render_resources();
    bool canvas_size(int *width, int *height) const;
    bool frame(BongoCatLive2DFrame *frame) const;
    bool measure_frame(BongoCatLive2DFrame *required);
    void set_frame(const BongoCatLive2DFrame &frame);
    bool viewport(int *x, int *y, int *width, int *height) const;
    bool overlay_viewport(int *x, int *y, int *width, int *height) const;
    void resize(int width, int height);
    void reshape(int width, int height);
    bool texture_refresh_pending(bool active) const;
    bool texture_refresh_due(bool active, bool allow_start) const;
    bool texture_refresh_busy() const;
    void cancel_texture_refresh_async();
    bool refresh_texture_resolution(bool active, bool allow_start);
    bool update(float delta_seconds);
    void draw();
    void set_mirror(bool mirror);
    void set_vertical_flip(bool flipped);
    void set_tight_frame(bool tight);
    void set_tight_overlay_rect(const float *rect);
    void set_render_options(const BongoCatLive2DRenderOptions &options);
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    void set_rhi_info(const BongoCatRhiDeviceInfo &info);
    const BongoCatRhiDeviceInfo &rhi_info() const;
#endif
    void set_dragging(float x, float y, bool angle_z = false);
    void prepare_viewer_audit();
    bool prepare_cover_capture();
    bool set_parameter(const char *id, float value);
    bool parameter(const char *id, float *minimum, float *maximum, float *value);
    bool start_motion(const char *group, int index);
    bool restore_motion_state(const char *group, int index);
    bool preview_motion(const char *group, int index);
    bool restore_motion_preview();
    bool commit_motion_preview(const char *group, int index);
    bool motion_selected(const char *group, int index) const;
    bool motion_persistent(const char *group, int index) const;
    bool motion_visible(const char *group, int index) const;
    bool motion_same_toggle(const char *left_group, int left_index,
        const char *right_group, int right_index) const;
    bool set_expression(int index);
    int expression() const { return expression_index_; }
    bool visual_state(BongoCatLive2DVisualState *state) const;

public:
    using MotionMap = std::map<std::string, Csm::ACubismMotion *>;
    using MotionSignatures = std::map<std::string, std::string>;
    struct MotionStateCurve {
        std::string target, id;
        float start = 0.0f, end = 0.0f;
        float normal = 0.0f;
        int parameter = -1;
        int part = -1;
        bool model_opacity = false;
    };
    struct MotionState {
        std::string group;
        int index = -1;
        std::vector<MotionStateCurve> curves;
        bool self_contained = false;
    };
private:
    friend class ParameterOverrideUpdater;
    struct MotionRun {
        Csm::CubismMotionQueueEntryHandle handle =
            Csm::InvalidMotionQueueEntryHandleValue;
        std::string key;
        bool one_shot = false;
        bool selected = false;
        bool committed = false;
    };
    struct ModelBounds {
        float min_x = 0.0f;
        float min_y = 0.0f;
        float max_x = 0.0f;
        float max_y = 0.0f;
        bool valid = false;
    };
    struct DrawableBounds { ModelBounds bounds; float area; };
    struct FrameDrawable {
        std::vector<unsigned short> vertices;
        ModelBounds bounds;
        bool dirty = true;
    };
    bool load_model(BongoCatError *error);
    void configure_builtin_accessories(const std::vector<unsigned char> &moc);
    void load_expressions();
    void load_effects();
    void load_motions(BongoCatLive2DLoadProgress progress, void *userdata);
    void start_idle_motion();
    void update_geometry();
    ModelBounds capture_visible_bounds() const;
    void prepare_expression_frame();
    void prepare_frame_bounds();
    void build_projection(Csm::CubismMatrix44 &projection,
        int width, int height);
    void apply_viewport_projection(Csm::CubismMatrix44 &projection) const;
    void update_viewport();
    void record_visible_state(Csm::CubismMatrix44 &projection) const;
    void capture_motion_preview();
    void restore_motion_preview_state();
    void load_motion_state(const std::string &key, const char *group, int index,
        const std::vector<unsigned char> &bytes);
    void pair_motion_states();
    std::string motion_to_play(const std::string &key, bool *selected) const;
    bool motion_is_persistent(const std::string &key) const;
    void record_motion_run(Csm::CubismMotionQueueEntryHandle handle,
        const std::string &key, bool selected, bool committed);
    void stop_motion_runs(const std::string &key);
    void expire_motion_runs();
    void clear_motion_runs();
    void expire_expression_fade();
    void settle_pending_expression_for_cover();
    void capture_parameter_baseline();
    void save_parameters();
    void add_parameter_override_updater();
    void apply_parameter_overrides();
    bool apply_motion_curve(const MotionStateCurve &curve, float value);
    bool restore_motion_defaults(const std::string &key);
    void select_motion(const std::string &key, bool selected);
    void release_textures();
    void schedule_texture_refresh();
    void cancel_texture_refresh();
    TextureResolution texture_refresh_bound(const ModelTexture &texture,
        int limit, float quality_percent) const;
    const BongoCatImageAlphaMask *texture_alpha(int index) const;
    void release_renderer();
    bool create_renderer(BongoCatError *error);
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    /* Cubism Vulkan renderer paths (see docs/live2d-vulkan-metal.md). */
    bool create_renderer_vulkan(BongoCatError *error);
    void bind_textures_vulkan();
    void draw_vulkan();
    BongoCatRhiDeviceInfo rhi_info_ = {};
#endif
    int prepare_mask_layout();
    bool update_mask_buffers();
    void bind_textures();
    std::vector<unsigned char> read(const std::string &path,
        size_t maximum = (size_t)-1) const;
    std::string path(const char *relative) const;

    Csm::CubismModelSettingJson *setting_ = nullptr;
    MotionMap motions_;
    MotionSignatures motion_signatures_;
    std::map<std::string, MotionState> motion_states_;
    std::map<std::string, std::string> motion_toggle_partners_;
    std::map<std::string, std::string> motion_toggle_owners_;
    std::set<std::string> selected_motion_keys_;
    MotionMap expressions_;
    std::vector<std::string> expression_names_;
    std::vector<std::shared_ptr<ModelTexture>> textures_;
    mutable std::vector<std::vector<unsigned char>> triangle_alpha_;
    mutable std::vector<DrawableBounds> bounds_scratch_;
    std::vector<FrameDrawable> frame_drawables_;
    std::vector<float> parameter_snapshot_;
    std::vector<float> part_snapshot_;
    std::vector<float> parameter_override_values_;
    std::vector<float> parameter_baseline_values_;
    std::vector<float> parameter_save_scratch_;
    std::vector<unsigned char> parameter_overrides_;
    std::vector<float> motion_preview_parameters_;
    std::vector<float> motion_preview_parts_;
    Csm::csmVector<Csm::CubismIdHandle> eye_blink_ids_;
    Csm::csmVector<Csm::CubismIdHandle> lip_sync_ids_;
    std::string directory_;
#ifdef CSM_TARGET_MAC_GL
    CoreProfileBuffers core_buffers_;
#endif
    int width_ = 612;
    int height_ = 354;
    int renderer_width_ = 0;
    int renderer_height_ = 0;
    int mask_texture_limit_ = 0;
    struct MaskBuffers { MaskSize layout; MaskSize size; };
    MaskBuffers drawable_masks_, offscreen_masks_;
    bool mask_update_failed_ = false;
    int mask_last_width_ = 0;
    int mask_last_height_ = 0;
    int viewport_x_ = 0;
    int viewport_y_ = 0;
    int viewport_width_ = 612;
    int viewport_height_ = 354;
    /* Rect the 2D overlay layers (desk art, key sprites) must draw into:
       the canvas mapped into window pixels with the SAME transform the model
       draw uses. In tight mode that is a frame_-derived sub-rect (which may
       extend past a cropped window edge), not the letterbox. */
    int overlay_x_ = 0;
    int overlay_y_ = 0;
    int overlay_width_ = 612;
    int overlay_height_ = 354;
    int expression_index_ = -1;
    bool expression_clearing_ = false;
    bool expression_frame_pending_ = false;
    BongoCatLive2DFrame frame_{};
    BongoCatLive2DFrame required_frame_{};
    float frame_fit_scale_ = 1.0f;
    bool frame_prepared_ = false;
    mutable BongoCatLive2DVisualState visual_state_{};
    mutable Csm::CubismMatrix44 visual_projection_;
    mutable bool visual_state_cached_ = false;
    bool visual_state_ready_ = false;
    bool motion_updated_ = false;
    bool suppress_eye_blink_ = false;
    bool automatic_idle_ = true;
    bool mirror_ = false;
    bool vertical_flip_ = false;
    bool tight_frame_ = false;
    /* Static 2D overlay art (desk/keyboard) bounds in canvas NDC; the
       tight envelope must cover them so the crop never slices the art. */
    bool tight_overlay_valid_ = false;
    float tight_overlay_rect_[4] = {};
    /* Frame to restore when tight mode turns off: the allocated frame at the
       moment it was enabled (base canvas plus accumulated motion overflow). */
    BongoCatLive2DFrame tight_reference_frame_ = {};
    /* Content box frozen while tight mode is on. The envelope projection
       must not follow the cropped window: the crop changes the window aspect
       ratio, and the aspect-dependent fit branch would flip the envelope and
       feed back into the crop (the window collapses or oscillates). */
    int tight_reference_content_width_ = 0;
    int tight_reference_content_height_ = 0;
    /* "Observe, converge, track" tight-frame policy. While tight mode
       (re)starts the window is held at its inherited frame for ~45 frames
       and rendered bounds are accumulated as a union — bounds are
       window-NDC, so they must be sampled from a STILL window. The union
       sets the first target and the frame animates to it; once it arrives
       the frame is considered locked and corrections switch to tracking the
       live envelope (settled geometry only), so poses outside the short
       observe window grow the crop instead of clipping, and the slack left
       by transient observe poses shrinks back away. Model switch or
       toggling tight mode starts a fresh cycle. */
    bool tight_observed_ = false;
    int tight_observe_frames_ = 0;
    float tight_observe_bounds_[4] = {};
    bool tight_locked_ = false;
    /* Geometry-settlement detector: exact frame_/required_ equality is
       unreachable when pixel rounding rejects a resize (the frame stays a
       sub-bucket value forever), which deadlocked the draw-pass self
       correction and left a stale inherited crop offset. Stable geometry
       for a few frames is settled enough. */
    int tight_geom_settle_ = 0;
    BongoCatLive2DFrame tight_geom_last_frame_ = {};
    int tight_geom_last_width_ = -1;
    int tight_geom_last_height_ = -1;
    /* Timestamp of the previous tight-frame measurement; drives the linear
       window-size animation (frame_ eases toward required_frame_ at a constant
       edge speed instead of jumping a whole bucket at once). */
    uint64_t tight_anim_last_ns_ = 0;
    /* Lazy-shrink deadline for the locked tight frame: slack is reclaimed
       only after the content has stayed smaller for the whole dwell, so the
       window stays still through ordinary pose changes. */
    uint64_t tight_shrink_due_ns_ = 0;
    static constexpr uint64_t tight_shrink_dwell_ns = 4ULL * 1000000000ULL;
    /* The window crop target starts from a frozen observe union and then
       tracks the live envelope while the geometry is settled: re-cropping
       every animation frame would resize the window continuously, but never
       re-cropping left a permanently offset window (green band on one side,
       content clipped on the other). The 1/64 dead zone and the settle gate
       keep the tracking quiet for ordinary motion. */
    BongoCatLive2DRenderOptions render_options_{};
    bool direct_textures_ = false;
    bool dynamic_texture_resolution_ = false;
    float render_quality_percent_ = 100.0f;
    int texture_limit_ = 0;
    TextureRefresh *texture_refresh_ = nullptr;
    TextureRefreshMemory texture_refresh_memory_;
    bool texture_refresh_pending_ = false;
    uint64_t texture_resize_ns_ = 0;
    size_t texture_refresh_index_ = 0;
    bool trim_offscreen_pool_ = true;
    bool parameter_overrides_applied_ = false;
    int builtin_accessory_parameter_ = -1;
    int builtin_accessory_part_ = -1;
    std::vector<std::string> idle_motion_keys_;
    std::vector<MotionRun> motion_runs_;
    std::vector<unsigned char> motion_finished_scratch_;
    ViewerLookUpdater *viewer_look_ = nullptr;
    int last_idle_motion_ = -1;
    float opacity_snapshot_ = -1.0f;
    float motion_preview_opacity_ = 1.0f;
    std::string motion_preview_key_;
    bool motion_preview_selected_ = false;
    bool motion_preview_active_ = false;
    bool motion_preview_completed_ = false;
};

bool motion_toggle_pair(const NativeModel::MotionState &left,
    const NativeModel::MotionState &right, bool *left_enabled);
bool motion_enables_state(const NativeModel::MotionState &state);
bool motion_run_clears_selection(bool one_shot, bool committed,
    bool replacement_running);

} // namespace bongo_cat

#endif
