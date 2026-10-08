//! Host adapter around the upstream OpenGL renderer and native Vulkan/Metal renderers.
use crate::{gl_state::State, native_gpu::NativeGpu};
use glam::{Mat4, UVec2};
use glow::HasContext;
use inox2d::{
    model::Model,
    node::{
        components::{Mask, Masks},
        drawables::{CompositeComponents, TexturedMeshComponents},
        InoxNodeUuid,
    },
    render::{CompositeRenderCtx, InoxRenderer, InoxRendererExt, TexturedMeshRenderCtx},
};
use inox2d_opengl::OpenglRenderer;
use std::{cell::RefCell, collections::HashSet, rc::Rc};

pub enum Renderer {
    Gl {
        renderer: RefCell<OpenglRenderer>,
        gl: Rc<glow::Context>,
        hidden: HashSet<u32>,
    },
    Native(NativeGpu),
}
impl Renderer {
    pub unsafe fn new(
        backend: i32,
        model: &Model,
        hidden: HashSet<u32>,
        _quality: f32,
    ) -> Result<Self, String> {
        if backend != 0 {
            return Ok(Self::Native(NativeGpu::new(backend, model, hidden)?));
        }
        let loader = crate::host()
            .gl_proc
            .ok_or("Host provides no OpenGL loader")?;
        let context = || {
            glow::Context::from_loader_function(|name| {
                let name = std::ffi::CString::new(name).unwrap();
                loader(name.as_ptr()).cast_const()
            })
        };
        let gl = Rc::new(context());
        let _state = State::save(gl.clone());
        gl.active_texture(glow::TEXTURE0);
        gl.bind_buffer(glow::PIXEL_UNPACK_BUFFER, None);
        gl.pixel_store_i32(glow::UNPACK_ALIGNMENT, 1);
        gl.pixel_store_i32(glow::UNPACK_ROW_LENGTH, 0);
        let renderer = OpenglRenderer::new(context(), model).map_err(|e| e.to_string())?;
        Ok(Self::Gl {
            renderer: RefCell::new(renderer),
            gl,
            hidden,
        })
    }
    pub unsafe fn draw(
        &self,
        model: &Model,
        width: i32,
        height: i32,
        matrix: Mat4,
    ) -> Result<(), String> {
        if let Self::Native(renderer) = self {
            return renderer.draw(model, width, height, matrix);
        }
        let Self::Gl {
            renderer,
            gl,
            hidden,
        } = self
        else {
            unreachable!()
        };
        let state = State::save(gl.clone());
        let max = gl.get_parameter_i32(glow::MAX_TEXTURE_SIZE);
        if width <= 0
            || height <= 0
            || width > max
            || height > max
            || i64::from(width) * i64::from(height) > 4_194_304
        {
            return Err("Inox2D render surface exceeds device/memory limits".into());
        }
        gl.disable(glow::DEPTH_TEST);
        gl.disable(glow::CULL_FACE);
        gl.disable(glow::SCISSOR_TEST);
        gl.disable(glow::STENCIL_TEST);
        gl.disable(glow::FRAMEBUFFER_SRGB);
        gl.color_mask(true, true, true, true);
        gl.stencil_mask(255);
        gl.bind_buffer(glow::PIXEL_UNPACK_BUFFER, None);
        gl.pixel_store_i32(glow::UNPACK_ALIGNMENT, 1);
        gl.pixel_store_i32(glow::UNPACK_ROW_LENGTH, 0);
        let mut renderer = renderer.borrow_mut();
        renderer.bongo_begin_frame(
            UVec2::new(width as u32, height as u32),
            matrix,
            state.framebuffer(),
        );
        gl.viewport(0, 0, width, height);
        renderer.on_begin_draw(&model.puppet);
        Filter {
            renderer: &renderer,
            hidden,
        }
        .draw(&model.puppet);
        renderer.on_end_draw(&model.puppet);
        if gl.get_error() != glow::NO_ERROR {
            return Err("Inox2D OpenGL rendering failed".into());
        }
        Ok(())
    }
}
struct Filter<'a> {
    renderer: &'a OpenglRenderer,
    hidden: &'a HashSet<u32>,
}
impl InoxRenderer for Filter<'_> {
    fn on_begin_masks(&self, m: &Masks) {
        self.renderer.on_begin_masks(m);
    }
    fn on_begin_mask(&self, m: &Mask) {
        self.renderer.on_begin_mask(m);
    }
    fn on_begin_masked_content(&self) {
        self.renderer.on_begin_masked_content();
    }
    fn on_end_mask(&self) {
        self.renderer.on_end_mask();
    }
    fn draw_textured_mesh_content(
        &self,
        mask: bool,
        c: &TexturedMeshComponents,
        r: &TexturedMeshRenderCtx,
        id: InoxNodeUuid,
    ) {
        if !self.hidden.contains(&crate::node_id(id)) {
            self.renderer.draw_textured_mesh_content(mask, c, r, id);
        }
    }
    fn begin_composite_content(
        &self,
        mask: bool,
        c: &CompositeComponents,
        r: &CompositeRenderCtx,
        id: InoxNodeUuid,
    ) {
        self.renderer.begin_composite_content(mask, c, r, id);
    }
    fn finish_composite_content(
        &self,
        mask: bool,
        c: &CompositeComponents,
        r: &CompositeRenderCtx,
        id: InoxNodeUuid,
    ) {
        self.renderer.finish_composite_content(mask, c, r, id);
    }
}
