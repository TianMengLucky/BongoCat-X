#ifndef BONGO_CAT_MVER_POINTER_H
#define BONGO_CAT_MVER_POINTER_H

#include "bongo_cat/common.h"

#define BONGO_CAT_MVER_ARM_POINT_COUNT 26

typedef struct BongoCatMverPoint {
    float x;
    float y;
} BongoCatMverPoint;

typedef struct BongoCatMverPointerConfig {
    float offset_x;
    float offset_y;
    float hand_offset_x;
    float hand_offset_y;
} BongoCatMverPointerConfig;

typedef struct BongoCatMverPointerGeometry {
    BongoCatMverPoint arm[BONGO_CAT_MVER_ARM_POINT_COUNT];
    float device_x;
    float device_y;
} BongoCatMverPointerGeometry;

typedef struct BongoCatMverPointerBounds {
    double left;
    double top;
    double width;
    double height;
} BongoCatMverPointerBounds;

typedef struct BongoCatMverPointerState {
    double x;
    double y;
    bool initialized;
} BongoCatMverPointerState;

#ifdef __cplusplus
extern "C" {
#endif

bool bongo_cat_mver_pointer_geometry(float x_ratio, float y_ratio,
    const BongoCatMverPointerConfig *config,
    BongoCatMverPointerGeometry *geometry);
bool bongo_cat_mver_pointer_geometry_scaled(float x_ratio, float y_ratio,
    float x_scale, float y_scale, const BongoCatMverPointerConfig *config,
    BongoCatMverPointerGeometry *geometry);
bool bongo_cat_mver_pointer_ratios(double x, double y,
    const BongoCatMverPointerBounds *bounds,
    float *x_ratio, float *y_ratio);
bool bongo_cat_mver_pointer_update(BongoCatMverPointerState *state,
    double absolute_x, double absolute_y, double relative_x,
    double relative_y, bool use_relative,
    const BongoCatMverPointerBounds *bounds, double *x, double *y);

#ifdef __cplusplus
}
#endif

#endif
