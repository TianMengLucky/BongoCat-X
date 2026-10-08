use glam::{Vec2, Vec3};
use inox2d::{
    model::Model,
    node::{
        components::{Mask, Masks},
        drawables::{CompositeComponents, TexturedMeshComponents},
        InoxNodeUuid,
    },
    render::{CompositeRenderCtx, InoxRenderer, InoxRendererExt, TexturedMeshRenderCtx},
};
use std::{cell::Cell, collections::HashSet};

#[derive(Clone, Copy)]
pub struct Bounds {
    pub min: Vec2,
    pub max: Vec2,
    pub count: i32,
}
impl Bounds {
    pub fn size(self) -> Vec2 {
        (self.max - self.min).max(Vec2::ONE)
    }
    pub fn measure(model: &Model, hidden: &HashSet<u32>) -> Result<Self, String> {
        let result = Self {
            min: Vec2::splat(f32::INFINITY),
            max: Vec2::splat(f32::NEG_INFINITY),
            count: 0,
        };
        let collector = Collector {
            result: Cell::new(result),
            model,
            hidden,
        };
        collector.draw(&model.puppet);
        let result = collector.result.get();
        if !result.min.is_finite() || !result.max.is_finite() || result.count == 0 {
            return Err("Inox2D model has no visible finite mesh geometry".into());
        }
        Ok(result)
    }
}
struct Collector<'a> {
    result: Cell<Bounds>,
    model: &'a Model,
    hidden: &'a HashSet<u32>,
}
impl InoxRenderer for Collector<'_> {
    fn on_begin_masks(&self, _: &Masks) {}
    fn on_begin_mask(&self, _: &Mask) {}
    fn on_begin_masked_content(&self) {}
    fn on_end_mask(&self) {}
    fn draw_textured_mesh_content(
        &self,
        mask: bool,
        c: &TexturedMeshComponents,
        r: &TexturedMeshRenderCtx,
        id: InoxNodeUuid,
    ) {
        if mask || self.hidden.contains(&crate::node_id(id)) || c.drawable.blending.opacity <= 0.0 {
            return;
        }
        let data = &self
            .model
            .puppet
            .render_ctx
            .as_ref()
            .unwrap()
            .vertex_buffers;
        let mut bounds = self.result.get();
        for index in r.vert_offset as usize..r.vert_offset as usize + r.vert_len {
            let p = data.verts[index] + data.deforms[index];
            let point = c
                .transform
                .transform_point3(Vec3::new(p.x, p.y, 0.0))
                .truncate();
            bounds.min = bounds.min.min(point);
            bounds.max = bounds.max.max(point);
        }
        bounds.count += 1;
        self.result.set(bounds);
    }
    fn begin_composite_content(
        &self,
        _: bool,
        _: &CompositeComponents,
        _: &CompositeRenderCtx,
        _: InoxNodeUuid,
    ) {
    }
    fn finish_composite_content(
        &self,
        _: bool,
        _: &CompositeComponents,
        _: &CompositeRenderCtx,
        _: InoxNodeUuid,
    ) {
    }
}
