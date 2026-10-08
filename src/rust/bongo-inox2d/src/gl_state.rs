use glow::HasContext;
use std::{num::NonZeroU32, rc::Rc};

/// OpenGL belongs to the SDL host. Restore bindings and fixed function state
/// on both normal returns and unwinding, including interrupted plugin loads.
pub struct State {
    gl: Rc<glow::Context>,
    integers: Vec<(u32, i32)>,
    enabled: Vec<(u32, bool)>,
    viewport: [i32; 4],
    color: [i32; 4],
    clear: [f32; 4],
    textures: [i32; 3],
    active_texture: i32,
}
impl State {
    pub unsafe fn save(gl: Rc<glow::Context>) -> Self {
        let keys = [
            glow::CURRENT_PROGRAM,
            glow::VERTEX_ARRAY_BINDING,
            glow::ARRAY_BUFFER_BINDING,
            glow::DRAW_FRAMEBUFFER_BINDING,
            glow::READ_FRAMEBUFFER_BINDING,
            glow::BLEND_SRC_RGB,
            glow::BLEND_DST_RGB,
            glow::BLEND_SRC_ALPHA,
            glow::BLEND_DST_ALPHA,
            glow::BLEND_EQUATION_RGB,
            glow::BLEND_EQUATION_ALPHA,
            glow::STENCIL_FUNC,
            glow::STENCIL_REF,
            glow::STENCIL_VALUE_MASK,
            glow::STENCIL_WRITEMASK,
            glow::STENCIL_FAIL,
            glow::STENCIL_PASS_DEPTH_FAIL,
            glow::STENCIL_PASS_DEPTH_PASS,
            glow::STENCIL_CLEAR_VALUE,
            glow::UNPACK_ALIGNMENT,
            glow::PIXEL_UNPACK_BUFFER_BINDING,
            glow::UNPACK_ROW_LENGTH,
            glow::STENCIL_BACK_FUNC,
            glow::STENCIL_BACK_REF,
            glow::STENCIL_BACK_VALUE_MASK,
            glow::STENCIL_BACK_WRITEMASK,
            glow::STENCIL_BACK_FAIL,
            glow::STENCIL_BACK_PASS_DEPTH_FAIL,
            glow::STENCIL_BACK_PASS_DEPTH_PASS,
        ];
        let integers = keys
            .into_iter()
            .map(|key| (key, gl.get_parameter_i32(key)))
            .collect();
        let enabled = [
            glow::BLEND,
            glow::DEPTH_TEST,
            glow::CULL_FACE,
            glow::SCISSOR_TEST,
            glow::STENCIL_TEST,
            glow::FRAMEBUFFER_SRGB,
        ]
        .into_iter()
        .map(|key| (key, gl.is_enabled(key)))
        .collect();
        let mut viewport = [0; 4];
        let mut color = [0; 4];
        let mut clear = [0.0; 4];
        gl.get_parameter_i32_slice(glow::VIEWPORT, &mut viewport);
        gl.get_parameter_i32_slice(glow::COLOR_WRITEMASK, &mut color);
        gl.get_parameter_f32_slice(glow::COLOR_CLEAR_VALUE, &mut clear);
        let active_texture = gl.get_parameter_i32(glow::ACTIVE_TEXTURE);
        let textures = std::array::from_fn(|i| {
            gl.active_texture(glow::TEXTURE0 + i as u32);
            gl.get_parameter_i32(glow::TEXTURE_BINDING_2D)
        });
        Self {
            gl,
            integers,
            enabled,
            viewport,
            color,
            clear,
            textures,
            active_texture,
        }
    }
    pub fn framebuffer(&self) -> Option<glow::NativeFramebuffer> {
        NonZeroU32::new(self.get(glow::DRAW_FRAMEBUFFER_BINDING) as u32)
            .map(glow::NativeFramebuffer)
    }
    fn get(&self, key: u32) -> i32 {
        self.integers.iter().find(|v| v.0 == key).unwrap().1
    }
}
impl Drop for State {
    fn drop(&mut self) {
        unsafe {
            let gl = &self.gl;
            let number = |key| NonZeroU32::new(self.get(key) as u32);
            gl.use_program(number(glow::CURRENT_PROGRAM).map(glow::NativeProgram));
            gl.bind_vertex_array(number(glow::VERTEX_ARRAY_BINDING).map(glow::NativeVertexArray));
            gl.bind_buffer(
                glow::ARRAY_BUFFER,
                number(glow::ARRAY_BUFFER_BINDING).map(glow::NativeBuffer),
            );
            gl.bind_framebuffer(glow::DRAW_FRAMEBUFFER, self.framebuffer());
            gl.bind_framebuffer(
                glow::READ_FRAMEBUFFER,
                number(glow::READ_FRAMEBUFFER_BINDING).map(glow::NativeFramebuffer),
            );
            for (i, texture) in self.textures.iter().enumerate() {
                gl.active_texture(glow::TEXTURE0 + i as u32);
                gl.bind_texture(
                    glow::TEXTURE_2D,
                    NonZeroU32::new(*texture as u32).map(glow::NativeTexture),
                );
            }
            gl.active_texture(self.active_texture as u32);
            gl.blend_func_separate(
                self.get(glow::BLEND_SRC_RGB) as u32,
                self.get(glow::BLEND_DST_RGB) as u32,
                self.get(glow::BLEND_SRC_ALPHA) as u32,
                self.get(glow::BLEND_DST_ALPHA) as u32,
            );
            gl.blend_equation_separate(
                self.get(glow::BLEND_EQUATION_RGB) as u32,
                self.get(glow::BLEND_EQUATION_ALPHA) as u32,
            );
            gl.stencil_func(
                self.get(glow::STENCIL_FUNC) as u32,
                self.get(glow::STENCIL_REF),
                self.get(glow::STENCIL_VALUE_MASK) as u32,
            );
            gl.stencil_mask(self.get(glow::STENCIL_WRITEMASK) as u32);
            gl.stencil_op(
                self.get(glow::STENCIL_FAIL) as u32,
                self.get(glow::STENCIL_PASS_DEPTH_FAIL) as u32,
                self.get(glow::STENCIL_PASS_DEPTH_PASS) as u32,
            );
            gl.stencil_func_separate(
                glow::BACK,
                self.get(glow::STENCIL_BACK_FUNC) as u32,
                self.get(glow::STENCIL_BACK_REF),
                self.get(glow::STENCIL_BACK_VALUE_MASK) as u32,
            );
            gl.stencil_mask_separate(glow::BACK, self.get(glow::STENCIL_BACK_WRITEMASK) as u32);
            gl.stencil_op_separate(
                glow::BACK,
                self.get(glow::STENCIL_BACK_FAIL) as u32,
                self.get(glow::STENCIL_BACK_PASS_DEPTH_FAIL) as u32,
                self.get(glow::STENCIL_BACK_PASS_DEPTH_PASS) as u32,
            );
            gl.bind_buffer(
                glow::PIXEL_UNPACK_BUFFER,
                number(glow::PIXEL_UNPACK_BUFFER_BINDING).map(glow::NativeBuffer),
            );
            gl.pixel_store_i32(glow::UNPACK_ROW_LENGTH, self.get(glow::UNPACK_ROW_LENGTH));
            gl.clear_stencil(self.get(glow::STENCIL_CLEAR_VALUE));
            gl.pixel_store_i32(glow::UNPACK_ALIGNMENT, self.get(glow::UNPACK_ALIGNMENT));
            gl.viewport(
                self.viewport[0],
                self.viewport[1],
                self.viewport[2],
                self.viewport[3],
            );
            gl.color_mask(
                self.color[0] != 0,
                self.color[1] != 0,
                self.color[2] != 0,
                self.color[3] != 0,
            );
            gl.clear_color(self.clear[0], self.clear[1], self.clear[2], self.clear[3]);
            for &(key, enabled) in &self.enabled {
                if enabled {
                    gl.enable(key);
                } else {
                    gl.disable(key);
                }
            }
        }
    }
}
