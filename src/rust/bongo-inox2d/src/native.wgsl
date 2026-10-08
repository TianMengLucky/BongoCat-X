struct Uniform { tint:vec4<f32>, screen:vec4<f32>, options:vec4<f32> }
@group(0) @binding(0) var image:texture_2d<f32>;
@group(0) @binding(1) var linear:sampler;
@group(0) @binding(2) var<uniform> settings:Uniform;
struct VertexOut { @builtin(position) position:vec4<f32>, @location(0) uv:vec2<f32> }
@vertex fn vertex(@location(0) position:vec4<f32>, @location(1) uv:vec2<f32>) -> VertexOut {
    var output:VertexOut;output.position=position;output.uv=uv;return output;
}
@fragment fn fragment(input:VertexOut)->@location(0) vec4<f32> {
    let color=textureSample(image,linear,input.uv);
    if settings.options.z>0.5 && color.a<settings.options.y {discard;}
    let screened=vec3<f32>(1.0)-(vec3<f32>(1.0)-color.rgb)*
        (vec3<f32>(1.0)-settings.screen.rgb*color.a);
    return vec4<f32>(screened*settings.tint.rgb,color.a)*settings.options.x;
}
