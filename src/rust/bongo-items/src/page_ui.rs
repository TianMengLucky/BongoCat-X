use crate::{Item, ItemScene, CAPACITY};
use bongo_plugin::page::{action, color, read, text, Page, PageState, Rect};

#[derive(Default)]
struct State {
    selected: usize,
    count: usize,
    glyph_hash: u64,
}
fn tr(p: &Page<'_>, key: &str, fallback: &str) -> String {
    p.tr(&format!("pages.preference.items.{key}"), fallback)
}
fn act(p: &Page<'_>, op: u32, index: usize) -> bool {
    let ok = p.act(op, index as u32);
    if !ok {
        p.notice(&tr(p, "failed", "Unable to update items"), true);
    }
    ok
}
fn item_action(p: &Page<'_>, r: Rect, duplicate: bool, enabled: bool) -> bool {
    let hover = p.input(r, 0);
    let role = if !enabled {
        color::MUTED
    } else if duplicate {
        color::ACCENT
    } else {
        color::DANGER
    };
    if hover && enabled {
        p.fill(
            r,
            8.0,
            if duplicate {
                color::SELECTION
            } else {
                color::DANGER_BACKGROUND
            },
        );
    }
    let g = Rect::new(r.x + 7.0, r.y + 7.0, 18.0, 18.0);
    if duplicate {
        p.stroke(Rect::new(g.x + 5.0, g.y, 12.0, 12.0), 2.0, 1.5, role);
        p.fill(Rect::new(g.x, g.y + 5.0, 12.0, 12.0), 2.0, color::SURFACE);
        p.stroke(Rect::new(g.x, g.y + 5.0, 12.0, 12.0), 2.0, 1.5, role);
    } else {
        p.line(Rect::new(g.x + 1.0, g.y + 3.0, 16.0, 0.0), 1.5, role);
        p.stroke(Rect::new(g.x + 4.0, g.y + 4.0, 10.0, 12.0), 2.0, 1.5, role);
        p.line(Rect::new(g.x + 6.0, g.y, 6.0, 0.0), 1.5, role);
    }
    if hover {
        p.tooltip(&if duplicate {
            tr(p, "duplicate", "Duplicate")
        } else {
            tr(p, "remove", "Remove from scene")
        });
        if enabled {
            p.input(r, 3);
        }
    }
    enabled && p.input(r, 1)
}
impl State {
    fn list(&mut self, p: &Page<'_>, scene: &ItemScene, busy: bool) -> bool {
        for (i, item) in scene.items[..self.count].iter().enumerate() {
            p.row(1, 40.0);
            let Some((r, enabled)) = p.region() else {
                continue;
            };
            let selected = self.selected == i;
            p.fill(
                r,
                10.0,
                if selected {
                    color::SELECTION
                } else if p.input(r, 0) {
                    color::HOVER
                } else {
                    color::SURFACE
                },
            );
            p.stroke(
                r,
                10.0,
                1.0,
                if selected {
                    color::BORDER
                } else {
                    color::BORDER_SUBTLE
                },
            );
            let remove = Rect::new(r.x + r.w - 38.0, r.y + 4.0, 32.0, 32.0);
            let duplicate = Rect::new(remove.x - 38.0, remove.y, 32.0, 32.0);
            let title = Rect::new(r.x + 12.0, r.y, (r.w - 98.0).max(1.0), r.h);
            p.paint_text(title, text(&item.name), 0, color::TEXT, 0);
            if item_action(
                p,
                duplicate,
                true,
                enabled && !busy && self.count < CAPACITY,
            ) {
                if act(p, action::DUPLICATE_ITEM, i) {
                    self.selected = self.count;
                    return true;
                }
            }
            if item_action(p, remove, false, enabled && !busy) && act(p, action::REMOVE_ITEM, i) {
                self.selected = self.selected.min(self.count.saturating_sub(2));
                return true;
            }
            if enabled && p.input(title, 1) {
                self.selected = i;
                p.invalidate();
            }
            if p.input(title, 0) {
                if p.measure(text(&item.name), 0) > title.w {
                    p.tooltip(text(&item.name));
                }
                if enabled {
                    p.input(title, 3);
                }
            }
        }
        false
    }
    fn edit(&self, p: &Page<'_>, item: &mut Item) {
        p.section(&tr(p, "edit", "Selected item"));
        if let Some(path) = unsafe { p.read::<[u8; 1024]>(read::IMAGE, self.selected as u32) } {
            p.images_begin();
            p.row(1, 96.0);
            if let Some((r, _)) = p.region() {
                p.image(text(&path), r);
            }
            p.images_end();
        }
        p.toggle(
            "item-visible",
            &tr(p, "visible", "Visible"),
            &mut item.visible,
        );
        p.toggle(
            "item-locked",
            &tr(p, "locked", "Lock placement"),
            &mut item.locked,
        );
        p.wrapped(
            &tr(p, "lockHint", "Prevent accidental placement changes."),
            36.0,
        );
        p.toggle(
            "item-follow",
            &tr(p, "follow", "Follow model viewport"),
            &mut item.follow_model,
        );
        p.wrapped(&tr(p, "followHint", "Follow the model canvas, mirror and vertical flip. This does not attach to an animated mesh."), 48.0);
        p.row(1, 150.0);
        if let Some((r, enabled)) = p.region() {
            p.fill(r, 12.0, color::HOVER);
            p.stroke(r, 12.0, 1.0, color::BORDER);
            p.line(
                Rect::new(r.x + r.w / 2.0, r.y, 0.0, r.h),
                1.0,
                color::BORDER_SUBTLE,
            );
            p.line(
                Rect::new(r.x, r.y + r.h / 2.0, r.w, 0.0),
                1.0,
                color::BORDER_SUBTLE,
            );
            if enabled && item.locked == 0 && p.input(r, 2) && r.w > 0.0 && r.h > 0.0 {
                let (x, y) = p.pointer();
                item.x = ((x - r.x) / r.w * 100.0).clamp(0.0, 100.0);
                item.y = ((y - r.y) / r.h * 100.0).clamp(0.0, 100.0);
                p.invalidate();
            }
            p.circle(
                Rect::new(
                    r.x + r.w * item.x / 100.0 - 7.0,
                    r.y + r.h * item.y / 100.0 - 7.0,
                    14.0,
                    14.0,
                ),
                color::ACCENT,
            );
        }
        if item.locked != 0 {
            p.disabled(true);
        }
        p.slider(
            "item-x",
            &tr(p, "x", "Horizontal position (%)"),
            "",
            &mut item.x,
            (0.0, 100.0),
            50.0,
        );
        p.slider(
            "item-y",
            &tr(p, "y", "Vertical position (%)"),
            "",
            &mut item.y,
            (0.0, 100.0),
            50.0,
        );
        p.slider(
            "item-scale",
            &tr(p, "scale", "Size (%)"),
            &tr(
                p,
                "scaleHint",
                "Percentage of the shorter viewport edge, preserving proportions.",
            ),
            &mut item.scale,
            (1.0, 200.0),
            25.0,
        );
        p.slider(
            "item-rotation",
            &tr(p, "rotation", "Rotation"),
            "",
            &mut item.rotation,
            (-180.0, 180.0),
            0.0,
        );
        if item.locked != 0 {
            p.disabled(false);
        }
        p.slider(
            "item-opacity",
            &tr(p, "opacity", "Opacity (%)"),
            "",
            &mut item.opacity,
            (0.0, 100.0),
            100.0,
        );
        let mut layer = item.layer as f32;
        p.slider("item-layer", &tr(p, "layer", "Layer"), &tr(p, "layerHint", "Negative layers are behind the model; zero and positive layers are in front. Higher layers draw last."), &mut layer, (-30.0, 30.0), 0.0);
        item.layer = layer.round() as i32;
    }
}
impl PageState for State {
    fn draw(&mut self, p: &Page<'_>) {
        let Some(scene) = (unsafe { p.read::<ItemScene>(read::SNAPSHOT, 0) }) else {
            p.wrapped(&tr(p, "failed", "Unable to update items"), 45.0);
            return;
        };
        let count = (scene.count as usize).min(CAPACITY);
        if count > self.count {
            self.selected = count - 1;
        }
        self.count = count;
        p.intro(&tr(p, "help", "Import images and arrange props around your pet. Drag the positioning pad or use the controls. Up to 16 items; GIF uses its first frame."));
        let actions = [
            ("add", "Add item", action::IMPORT),
            ("save", "Save scene", action::SAVE),
            ("folder", "Open items folder", action::OPEN_FOLDER),
        ];
        let labels: Vec<_> = actions
            .iter()
            .map(|(id, title, _)| tr(p, id, title))
            .collect();
        p.button_row(&labels);
        let busy = p.status().busy != 0 || p.status().blocked != 0;
        if busy {
            p.disabled(true);
        }
        for ((id, _, op), label) in actions.into_iter().zip(labels.iter()) {
            if p.button(id, label, if op == action::OPEN_FOLDER { 4 } else { 0 }) {
                act(p, op, 0);
            }
        }
        if busy {
            p.disabled(false);
        }
        if count == 0 {
            p.wrapped(
                &tr(
                    p,
                    "empty",
                    "No items yet. Add an image or drop it onto this page.",
                ),
                70.0,
            );
            return;
        }
        self.selected = self.selected.min(count - 1);
        p.section(&tr(p, "list", "Scene items"));
        let names: Vec<_> = scene.items[..count].iter().map(|i| text(&i.name)).collect();
        p.glyphs(&names, &mut self.glyph_hash);
        let busy = p.status().busy != 0;
        if self.list(p, &scene, busy) {
            return;
        }
        if busy {
            p.disabled(true);
        }
        let mut item = scene.items[self.selected];
        self.edit(p, &mut item);
        if busy {
            p.disabled(false);
        }
        if item != scene.items[self.selected]
            && !unsafe { p.send(action::UPDATE_ITEM, self.selected as u32, &item) }
        {
            p.notice(&tr(p, "failed", "Unable to update items"), true);
        }
    }
}
bongo_plugin::export_page!(bongo_cat_items_page_ui_query_v1, State);
