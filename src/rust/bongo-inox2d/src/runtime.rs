use crate::{abi::Frame, bounds::Bounds, parameters::Parameters, renderer::Renderer};
use glam::{Mat4, Vec3};
use inox2d::model::Model;
use serde_json::Value;
use std::{collections::HashSet, path::Path};

pub struct Loaded {
    pub renderer: Renderer,
    pub model: Model,
    pub parameters: Parameters,
    pub base: Bounds,
    pub current: Bounds,
    pub hidden: HashSet<u32>,
}
pub struct Runtime {
    pub loaded: Option<Loaded>,
    pub width: i32,
    pub height: i32,
    pub backend: i32,
    pub large_allocation: bool,
    pub parallel_recording: bool,
    pub mirror: bool,
    pub flipped: bool,
    pub tight: bool,
    pub frame: Frame,
}
fn hidden_nodes(node: &Value, inherited: bool, result: &mut HashSet<u32>) {
    let hidden = inherited || node["enabled"] == false;
    if hidden {
        result.insert(node["uuid"].as_u64().unwrap() as u32);
    }
    if let Some(children) = node["children"].as_array() {
        for child in children {
            hidden_nodes(child, hidden, result);
        }
    }
}
impl Runtime {
    pub fn new() -> Self {
        Self {
            loaded: None,
            width: 612,
            height: 354,
            backend: 0,
            large_allocation: false,
            parallel_recording: false,
            mirror: false,
            flipped: false,
            tight: false,
            frame: Frame::default(),
        }
    }
    pub unsafe fn load(
        &mut self,
        directory: &Path,
        setting: &str,
        quality: f32,
    ) -> Result<(), String> {
        let file = directory.join(setting);
        let bytes = bongo_safe::inochi::read_file(&file)?;
        let (model, json) = bongo_safe::inochi::parse(&bytes)?;
        let mut hidden = HashSet::new();
        hidden_nodes(&json["nodes"], false, &mut hidden);
        let base = Bounds::measure(&model, &hidden)?;
        let parameters = Parameters::new(&json, directory)?;
        let renderer = Renderer::new(
            self.backend,
            &model,
            hidden.clone(),
            quality,
            self.large_allocation,
            self.parallel_recording,
        )?;
        self.loaded = Some(Loaded {
            renderer,
            model,
            parameters,
            base,
            current: base,
            hidden,
        });
        self.frame = Frame::default();
        Ok(())
    }
    pub fn update(&mut self, dt: f32) -> bool {
        let Some(loaded) = &mut self.loaded else {
            return false;
        };
        loaded.model.puppet.begin_frame();
        for (name, param) in &loaded.parameters.values {
            if loaded
                .model
                .puppet
                .param_ctx
                .as_mut()
                .unwrap()
                .set(name, param.value)
                .is_err()
            {
                return false;
            }
        }
        loaded.model.puppet.end_frame(if dt.is_finite() {
            dt.clamp(0.0, 0.1)
        } else {
            0.0
        });
        if let Ok(bounds) = Bounds::measure(&loaded.model, &loaded.hidden) {
            loaded.current = bounds;
        }
        true
    }
    pub fn required_frame(&self) -> Frame {
        let Some(l) = &self.loaded else {
            return Frame::default();
        };
        let size = l.base.size();
        let margin = |v: f32| if self.tight { v.max(-0.49) } else { v.max(0.0) };
        Frame {
            left: margin((l.base.min.x - l.current.min.x) / size.x),
            top: margin((l.base.min.y - l.current.min.y) / size.y),
            right: margin((l.current.max.x - l.base.max.x) / size.x),
            bottom: margin((l.current.max.y - l.base.max.y) / size.y),
        }
    }
    pub fn viewport(&self) -> [i32; 4] {
        let w = (self.width as f32 / (1.0 + self.frame.left + self.frame.right)) as i32;
        let h = (self.height as f32 / (1.0 + self.frame.top + self.frame.bottom)) as i32;
        [
            (w as f32 * self.frame.left) as i32,
            (h as f32 * self.frame.bottom) as i32,
            w,
            h,
        ]
    }
    pub unsafe fn draw(&self) -> Result<(), String> {
        let l = self.loaded.as_ref().ok_or("No Inox2D model is loaded")?;
        let size = l.base.size();
        let center = (l.base.min + l.base.max) * 0.5;
        let matrix = Mat4::orthographic_rh_gl(
            l.base.min.x - size.x * self.frame.left,
            l.base.max.x + size.x * self.frame.right,
            l.base.max.y + size.y * self.frame.bottom,
            l.base.min.y - size.y * self.frame.top,
            -10000.0,
            10000.0,
        ) * Mat4::from_translation(center.extend(0.0))
            * Mat4::from_scale(Vec3::new(
                if self.mirror { -1.0 } else { 1.0 },
                if self.flipped { -1.0 } else { 1.0 },
                1.0,
            ))
            * Mat4::from_translation(-center.extend(0.0));
        l.renderer.draw(&l.model, self.width, self.height, matrix)
    }
}
