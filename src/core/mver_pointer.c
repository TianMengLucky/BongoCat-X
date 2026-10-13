#include "bongo_cat/mver_pointer.h"

#include <math.h>
#include <stddef.h>

typedef struct DoublePoint {
    double x;
    double y;
} DoublePoint;

static const double factorial[] = {
    0.001, 0.001, 0.002, 0.006, 0.024, 0.12, 0.72, 5.04,
    40.32, 362.88, 3628.8, 39916.8, 479001.6, 6227020.8,
    87178291.2, 1307674368.0, 20922789888.0, 355687428096.0,
    6402373705728.0, 121645100408832.0, 2432902008176640.0,
    51090942171709440.0
};

static DoublePoint bezier(double ratio, const DoublePoint *points,
    size_t count) {
    DoublePoint result = {0.0, 0.0};
    size_t degree = count - 1;
    for (size_t point = 0; point <= degree; ++point) {
        double coefficient = factorial[degree] /
            (factorial[point] * factorial[degree - point]);
        double weight = coefficient * pow(ratio, (double)point) *
            pow(1.0 - ratio, (double)(degree - point));
        result.x += points[point].x * weight;
        result.y += points[point].y * weight;
    }
    result.x /= 1000.0;
    result.y /= 1000.0;
    return result;
}

static double clamp_ratio(float value) {
    if (value < 0.0f) return 0.0;
    if (value > 1.0f) return 1.0;
    return value;
}

bool bongo_cat_mver_pointer_ratios(double x, double y,
    const BongoCatMverPointerBounds *bounds,
    float *x_ratio, float *y_ratio) {
    if (!bounds || !x_ratio || !y_ratio || !isfinite(x) || !isfinite(y) ||
        !isfinite(bounds->left) || !isfinite(bounds->top) ||
        !isfinite(bounds->width) || !isfinite(bounds->height) ||
        bounds->width <= 0.0 || bounds->height <= 0.0) return false;
    *x_ratio = (float)clamp_ratio((float)((x - bounds->left) / bounds->width));
    *y_ratio = (float)clamp_ratio((float)((y - bounds->top) / bounds->height));
    return true;
}

bool bongo_cat_mver_pointer_update(BongoCatMverPointerState *state,
    double absolute_x, double absolute_y, double relative_x,
    double relative_y, bool use_relative,
    const BongoCatMverPointerBounds *bounds, double *x, double *y) {
    if (!state || !bounds || !x || !y || !isfinite(absolute_x) ||
        !isfinite(absolute_y) || !isfinite(relative_x) ||
        !isfinite(relative_y) || bounds->width <= 0.0 ||
        bounds->height <= 0.0) return false;
    if (!state->initialized || !use_relative) {
        state->x = absolute_x;
        state->y = absolute_y;
        state->initialized = true;
    }
    if (use_relative) {
        state->x += relative_x;
        state->y += relative_y;
    }
    double right = bounds->left + bounds->width;
    double bottom = bounds->top + bounds->height;
    if (state->x < bounds->left) state->x = bounds->left;
    if (state->x > right) state->x = right;
    if (state->y < bounds->top) state->y = bounds->top;
    if (state->y > bottom) state->y = bottom;
    *x = state->x;
    *y = state->y;
    return true;
}

bool bongo_cat_mver_pointer_geometry(float x_ratio, float y_ratio,
    const BongoCatMverPointerConfig *config,
    BongoCatMverPointerGeometry *geometry) {
    return bongo_cat_mver_pointer_geometry_scaled(x_ratio, y_ratio, 1.0f, 1.0f, config, geometry);
}

bool bongo_cat_mver_pointer_geometry_scaled(float x_ratio, float y_ratio,
    float x_scale, float y_scale, const BongoCatMverPointerConfig *config,
    BongoCatMverPointerGeometry *geometry) {
    if (!config || !geometry || !isfinite(x_ratio) || !isfinite(y_ratio) ||
        !isfinite(x_scale) || !isfinite(y_scale) || x_scale < .25f || x_scale > 4.0f ||
        y_scale < .25f || y_scale > 4.0f) return false;
    double fx = clamp_ratio(x_ratio), fy = clamp_ratio(y_ratio);
    double x = -97.0 * fx + 44.0 * fy + 184.0;
    double y = -76.0 * fx - 40.0 * fy + 324.0;
    /* Scale the physical hand endpoint about its neutral position; shoulder
       anchors stay fixed and the arm curves are rebuilt from that endpoint. */
    x = 157.5 + (x - 157.5) * x_scale;
    y = 266.0 + (y - 266.0) * y_scale;
    const double dx = -38.0, dy = -50.0;
    const int samples = 6;
    DoublePoint path[19];
    size_t path_count = 0;
    path[path_count++] = (DoublePoint){211.0, 159.0};

    double distance = hypot(211.0 - x, 159.0 - y);
    double centre_left_x = 211.0 - 0.7237 * distance / 2.0;
    double centre_left_y = 159.0 + 0.69 * distance / 2.0;
    DoublePoint left_curve[] = {
        {211.0, 159.0}, {centre_left_x, centre_left_y}, {x, y}
    };
    for (int index = 1; index < samples; ++index)
        path[path_count++] = bezier((double)index / samples, left_curve, 3);
    path[path_count++] = (DoublePoint){x, y};

    double normal_x = y - centre_left_y;
    double normal_y = centre_left_x - x;
    double normal_length = hypot(normal_x, normal_y);
    if (normal_length <= 0.0) { normal_x = 0.0; normal_y = 1.0; normal_length = 1.0; }
    double a = x + normal_x / normal_length * 60.0;
    double b = y + normal_y / normal_length * 60.0;
    const double anchor_x = 258.0, anchor_y = 228.0;
    distance = hypot(anchor_x - a, anchor_y - b);
    double centre_right_x = anchor_x - 0.6 * distance / 2.0;
    double centre_right_y = anchor_y + 0.8 * distance / 2.0;
    const double push = 20.0;
    double tangent_x = x - centre_left_x;
    double tangent_y = y - centre_left_y;
    double tangent_length = hypot(tangent_x, tangent_y);
    if (tangent_length <= 0.0) { tangent_x = 0.0; tangent_y = 1.0; tangent_length = 1.0; }
    tangent_x *= push / tangent_length;
    tangent_y *= push / tangent_length;
    double tangent2_x = a - centre_right_x;
    double tangent2_y = b - centre_right_y;
    tangent_length = hypot(tangent2_x, tangent2_y);
    if (tangent_length <= 0.0) { tangent2_x = 0.0; tangent2_y = 1.0; tangent_length = 1.0; }
    tangent2_x *= push / tangent_length;
    tangent2_y *= push / tangent_length;
    DoublePoint middle_curve[] = {
        {x, y}, {x + tangent_x, y + tangent_y},
        {a + tangent2_x, b + tangent2_y}, {a, b}
    };
    for (int index = 1; index < samples; ++index)
        path[path_count++] = bezier((double)index / samples, middle_curve, 4);
    path[path_count++] = (DoublePoint){a, b};

    DoublePoint right_curve[] = {
        {anchor_x, anchor_y}, {centre_right_x, centre_right_y}, {a, b}
    };
    for (int index = samples - 1; index > 0; --index)
        path[path_count++] = bezier((double)index / samples, right_curve, 3);
    path[path_count++] = (DoublePoint){anchor_x, anchor_y};
    if (path_count != 19) return false;

    for (size_t index = 0; index < BONGO_CAT_MVER_ARM_POINT_COUNT; ++index) {
        DoublePoint point = index == 0 ? path[0] :
            index == BONGO_CAT_MVER_ARM_POINT_COUNT - 1 ? path[18] :
            bezier((double)index / 25.0, path, 19);
        geometry->arm[index].x = (float)(point.x + dx + config->hand_offset_x);
        geometry->arm[index].y = (float)(point.y + dy + config->hand_offset_y);
    }
    double device_x = (a + x) / 2.0 - 52.0 - 15.0;
    double device_y = (b + y) / 2.0 - 34.0 + 5.0;
    geometry->device_x = (float)(device_x + dx + config->offset_x +
        config->hand_offset_x);
    geometry->device_y = (float)(device_y + dy + config->offset_y +
        config->hand_offset_y);
    return isfinite(geometry->device_x) && isfinite(geometry->device_y);
}
