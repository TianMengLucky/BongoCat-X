//! Translate Inox2D's existing draw traversal into backend-independent GPU passes.
use glam::Mat4;
use inox2d::{
    model::Model,
    node::{
        components::{BlendMode, Blending, Mask, MaskMode, Masks},
        drawables::{CompositeComponents, TexturedMeshComponents},
        InoxNodeUuid,
    },
    render::{CompositeRenderCtx, InoxRenderer, InoxRendererExt, TexturedMeshRenderCtx},
};
use std::{
    cell::{Cell, RefCell},
    collections::HashSet,
};
#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
pub struct Vertex {
    pub position: [f32; 4],
    pub uv: [f32; 2],
}
#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
pub struct Uniform {
    pub tint: [f32; 4],
    pub screen: [f32; 4],
    pub options: [f32; 4],
}
pub struct Paint {
    pub vertices: Vec<Vertex>,
    pub uniform: Uniform,
    pub texture: Option<usize>,
    pub blend: usize,
    pub stencil: u32,
    pub mode: usize,
    pub target: usize,
}
pub enum Command {
    Clear {
        target: usize,
        color: bool,
        stencil: u32,
    },
    Paint(Paint),
}
pub struct Plan<'a> {
    model: &'a Model,
    hidden: &'a HashSet<u32>,
    matrix: Mat4,
    commands: RefCell<Vec<Command>>,
    target: Cell<usize>,
    mode: Cell<usize>,
    stencil: Cell<u32>,
    threshold: Cell<f32>,
}
impl<'a> Plan<'a> {
    pub fn collect(model: &'a Model, hidden: &'a HashSet<u32>, matrix: Mat4) -> Vec<Command> {
        let plan = Self {
            model,
            hidden,
            matrix,
            commands: RefCell::new(vec![Command::Clear {
                target: 0,
                color: true,
                stencil: 0,
            }]),
            target: Cell::new(0),
            mode: Cell::new(0),
            stencil: Cell::new(0),
            threshold: Cell::new(0.5),
        };
        plan.draw(&model.puppet);
        plan.commands.into_inner()
    }
    fn paint(&self, vertices: Vec<Vertex>, texture: Option<usize>, blending: &Blending) {
        let blend = match blending.mode {
            BlendMode::Normal => 0,
            BlendMode::Multiply => 1,
            BlendMode::ColorDodge => 2,
            BlendMode::LinearDodge => 3,
            BlendMode::Screen => 4,
            BlendMode::ClipToLower => 5,
            BlendMode::SliceFromLower => 6,
        };
        let uniform = Uniform {
            tint: blending.tint.extend(1.0).to_array(),
            screen: blending.screen_tint.extend(0.0).to_array(),
            options: [
                blending.opacity,
                self.threshold.get(),
                if self.mode.get() == 1 { 1.0 } else { 0.0 },
                0.0,
            ],
        };
        self.commands.borrow_mut().push(Command::Paint(Paint {
            vertices,
            uniform,
            texture,
            blend,
            stencil: self.stencil.get(),
            mode: self.mode.get(),
            target: self.target.get(),
        }));
    }
}
impl InoxRenderer for Plan<'_> {
    fn on_begin_masks(&self, m: &Masks) {
        self.threshold.set(m.threshold.clamp(0.0, 1.0));
        self.mode.set(1);
        self.commands.borrow_mut().push(Command::Clear {
            target: self.target.get(),
            color: false,
            stencil: if m.masks.iter().any(|v| v.mode == MaskMode::Mask) {
                0
            } else {
                1
            },
        });
    }
    fn on_begin_mask(&self, m: &Mask) {
        self.stencil
            .set(if m.mode == MaskMode::Mask { 1 } else { 0 });
    }
    fn on_begin_masked_content(&self) {
        self.mode.set(2);
        self.stencil.set(1);
    }
    fn on_end_mask(&self) {
        self.mode.set(0);
    }
    fn draw_textured_mesh_content(
        &self,
        _: bool,
        c: &TexturedMeshComponents,
        r: &TexturedMeshRenderCtx,
        id: InoxNodeUuid,
    ) {
        if self.hidden.contains(&crate::node_id(id)) {
            return;
        }
        let data = &self
            .model
            .puppet
            .render_ctx
            .as_ref()
            .unwrap()
            .vertex_buffers;
        let matrix = self.matrix * *c.transform;
        let vertices = data.indices[r.index_offset as usize..r.index_offset as usize + r.index_len]
            .iter()
            .map(|index| {
                let i = *index as usize;
                let p = matrix * (data.verts[i] + data.deforms[i]).extend(0.0).extend(1.0);
                Vertex {
                    position: [p.x, p.y, (p.z + p.w) * 0.5, p.w],
                    uv: data.uvs[i].to_array(),
                }
            })
            .collect();
        self.paint(
            vertices,
            Some(c.texture.tex_albedo.raw()),
            &c.drawable.blending,
        );
    }
    fn begin_composite_content(
        &self,
        _: bool,
        _: &CompositeComponents,
        _: &CompositeRenderCtx,
        _: InoxNodeUuid,
    ) {
        self.target.set(1);
        self.commands.borrow_mut().push(Command::Clear {
            target: 1,
            color: true,
            stencil: 0,
        });
    }
    fn finish_composite_content(
        &self,
        _: bool,
        c: &CompositeComponents,
        _: &CompositeRenderCtx,
        id: InoxNodeUuid,
    ) {
        self.target.set(0);
        if self.hidden.contains(&crate::node_id(id)) {
            return;
        }
        let vertices = [
            (-1.0, -1.0, 0.0, 1.0),
            (1.0, -1.0, 1.0, 1.0),
            (-1.0, 1.0, 0.0, 0.0),
            (-1.0, 1.0, 0.0, 0.0),
            (1.0, -1.0, 1.0, 1.0),
            (1.0, 1.0, 1.0, 0.0),
        ]
        .into_iter()
        .map(|(x, y, u, v)| Vertex {
            position: [x, y, 0.5, 1.0],
            uv: [u, v],
        })
        .collect();
        self.paint(vertices, None, &c.drawable.blending);
    }
}
