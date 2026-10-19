use crate::{SoundKey, SoundScene, SOUND_KEYS};
use bongo_plugin::page::{action, color, read, text, Page, PageState, Rect};

#[derive(Default)]
struct State {
    selected: usize,
    glyph_hash: u64,
}
fn tr(p: &Page<'_>, key: &str, fallback: &str) -> String {
    p.tr(&format!("pages.preference.sounds.{key}"), fallback)
}
fn act(p: &Page<'_>, op: u32, index: usize) {
    if !p.act(op, index as u32) {
        p.notice(&tr(p, "failed", "Unable to update sound effects"), true);
    }
}
// Move navigation and numpad keys below the main block in narrow windows.
fn key_position(key: &SoundKey, copy: u32, compact: bool) -> Option<(f32, f32, u32)> {
    let (mut x, width, mut row) = if copy == 0 {
        (key.x, key.width, key.row)
    } else if text(&key.id) == "Return" {
        (22.0, 1.0, 4)
    } else if text(&key.id) == "Meta" {
        (11.75, 1.25, key.row)
    } else {
        return None;
    };
    if compact && x >= 15.5 {
        x -= 15.5;
        row += 6;
    }
    Some((x, width, row))
}
impl PageState for State {
    fn draw(&mut self, p: &Page<'_>) {
        let status = p.status();
        let Some(scene) = (unsafe { p.read::<SoundScene>(read::SNAPSHOT, 0) }) else {
            p.wrapped(
                &if status.busy != 0 {
                    tr(p, "busy", "Worker is processing audio")
                } else {
                    tr(p, "failed", "Unable to update sound effects")
                },
                45.0,
            );
            return;
        };
        let count = (scene.count as usize)
            .min(scene.bindings.len())
            .min(SOUND_KEYS.len());
        if count == 0 {
            return;
        }
        p.intro(&tr(p, "help", "Click a key to preview its sound and choose an audio file. Hover to see the filename. Physical key presses play the assigned sound; bindings save automatically."));
        let names: Vec<_> = scene.bindings[..count]
            .iter()
            .map(|b| text(&b.name))
            .collect();
        p.glyphs(&names, &mut self.glyph_hash);
        self.selected = self.selected.min(count - 1);
        let compact = p.width() < 760.0;
        for row in 0..if compact { 12 } else { 6 } {
            p.row(1, 42.0);
            let Some((area, enabled)) = p.region() else {
                continue;
            };
            let columns = if !compact {
                23.0
            } else if row < 6 {
                15.0
            } else {
                7.5
            };
            let unit = area.w / columns;
            for (i, key) in SOUND_KEYS[..count].iter().enumerate() {
                for copy in 0..2 {
                    let Some((x, width, key_row)) = key_position(key, copy, compact) else {
                        continue;
                    };
                    if key_row != row {
                        continue;
                    }
                    let r = Rect::new(
                        area.x + x * unit + 1.0,
                        area.y + 1.0,
                        width * unit - 2.0,
                        38.0,
                    );
                    let bound = scene.bindings[i].asset[0] != 0;
                    let hover = p.input(r, 0);
                    p.fill(
                        r,
                        5.0,
                        if hover || bound {
                            color::SELECTION
                        } else {
                            color::FIELD
                        },
                    );
                    p.stroke(
                        r,
                        5.0,
                        if self.selected == i { 2.0 } else { 1.0 },
                        if self.selected == i {
                            color::ACCENT
                        } else {
                            color::BORDER_SUBTLE
                        },
                    );
                    p.paint_text(
                        r.inset(1.0),
                        text(&key.label),
                        1,
                        if bound { color::ACCENT } else { color::TEXT },
                        1,
                    );
                    if bound {
                        p.circle(
                            Rect::new(r.x + r.w - 5.0, r.y + r.h - 5.0, 3.0, 3.0),
                            color::ACCENT,
                        );
                    }
                    if hover {
                        p.tooltip(&format!(
                            "{} · {}",
                            text(&key.label),
                            if bound {
                                names[i].to_owned()
                            } else {
                                tr(p, "unassigned", "No sound assigned")
                            }
                        ));
                        if enabled && status.busy == 0 && status.blocked == 0 && p.input(r, 1) {
                            self.selected = i;
                            if bound {
                                act(p, action::PREVIEW_SOUND, i);
                            }
                            act(p, action::IMPORT, i);
                            p.invalidate();
                        }
                    }
                }
            }
        }
        let key = &SOUND_KEYS[self.selected];
        p.label(text(&key.id));
        p.label(&if names[self.selected].is_empty() {
            tr(p, "unassigned", "No sound assigned")
        } else {
            names[self.selected].to_owned()
        });
        let actions = [
            ("choose", "Choose audio", action::IMPORT),
            ("preview", "Preview", action::PREVIEW_SOUND),
            ("clear", "Clear binding", action::CLEAR_SOUND),
        ];
        let labels: Vec<_> = actions
            .iter()
            .map(|(id, title, _)| tr(p, id, title))
            .collect();
        p.button_row(&labels);
        for ((id, _, op), label) in actions.into_iter().zip(labels.iter()) {
            let blocked = status.busy != 0
                || status.blocked != 0
                || (op != action::IMPORT && scene.bindings[self.selected].asset[0] == 0);
            if blocked {
                p.disabled(true);
            }
            if p.button(
                id,
                label,
                if op == action::CLEAR_SOUND {
                    1
                } else if op == action::PREVIEW_SOUND {
                    4
                } else {
                    0
                },
            ) {
                act(p, op, self.selected);
            }
            if blocked {
                p.disabled(false);
            }
        }
        p.wrapped(&tr(p, "formats", "WAV, MP3, Ogg Vorbis and FLAC, up to 64 MB. Sound runs independently of the selected model. Key availability depends on global input permission and keyboard layout."), 58.0);
    }
}
bongo_plugin::export_page!(bongo_cat_sounds_page_ui_query_v1, State);
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn keyboard_layout_keeps_all_bindings_inside_each_width() {
        for compact in [false, true] {
            for key in &SOUND_KEYS {
                for copy in 0..2 {
                    if let Some((x, width, row)) = key_position(key, copy, compact) {
                        let columns = if !compact {
                            23.0
                        } else if row < 6 {
                            15.0
                        } else {
                            7.5
                        };
                        assert!(x >= 0.0 && x + width <= columns);
                        assert!(row < if compact { 12 } else { 6 });
                        if copy == 0 {
                            assert_eq!(row % 6, key.row);
                        }
                    } else {
                        assert_eq!(copy, 1);
                    }
                }
            }
        }
    }
}
