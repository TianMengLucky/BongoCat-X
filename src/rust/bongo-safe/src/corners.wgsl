struct Params {
    rect: vec4<f32>,
    surface: vec4<f32>,
}
var<push_constant> params: Params;
@vertex fn vertex(@builtin(vertex_index) index: u32) -> @builtin(position) vec4<f32> {
    var vertices = array<vec2<f32>, 6>(vec2(0.0, 0.0), vec2(1.0, 0.0),
        vec2(0.0, 1.0), vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));
    let corner = index / 6u;
    let side = vec2<f32>(f32(corner & 1u), f32((corner >> 1u) & 1u));
    let span = min(vec2<f32>(params.surface.x + 0.5), params.rect.zw * 0.5);
    let pixel = params.rect.xy + side * (params.rect.zw - span) + vertices[index % 6u] * span;
    return vec4<f32>(pixel / params.surface.yz * 2.0 - 1.0, 0.0, 1.0);
}
@fragment fn fragment(@builtin(position) position: vec4<f32>) -> @location(0) vec4<f32> {
    let half_size = params.rect.zw * 0.5;
    let q = abs(position.xy - params.rect.xy - half_size) - (half_size - params.surface.x);
    let d = length(max(q, vec2<f32>(0.0))) + min(max(q.x, q.y), 0.0) - params.surface.x;
    return vec4<f32>(clamp(0.5 - d, 0.0, 1.0));
}
