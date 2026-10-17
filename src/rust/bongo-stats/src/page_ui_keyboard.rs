use super::{tr, State};
use bongo_plugin::page::{color, read, Page, Rect};
// Virtual key IDs are stable across platform adapters; modifiers keep separate counters.
fn key(p: &Page<'_>, r: Rect, label: &str, count: u64, maximum: u64) {
    let base = p.color(color::FIELD);
    let accent = p.color(color::ACCENT);
    let amount = if count == 0 {
        0.0
    } else {
        0.18 + 0.70 * (count as f64 / maximum as f64).sqrt() as f32
    };
    let mut rgba = 0xff000000;
    for shift in [0, 8, 16] {
        let a = ((base >> shift) & 255) as f32;
        let b = ((accent >> shift) & 255) as f32;
        rgba |= ((a + (b - a) * amount) as u32) << shift;
    }
    p.fill_rgba(r, 5.0, rgba);
    p.stroke(r, 5.0, 1.0, color::BORDER_SUBTLE);
    let luminance = (rgba & 255) * 299 + ((rgba >> 8) & 255) * 587 + ((rgba >> 16) & 255) * 114;
    p.text_rgba(
        r,
        label,
        1,
        if luminance > 145000 {
            0xff202020
        } else {
            0xfffafafa
        },
        1,
    );
    if p.input(r, 0) {
        p.tooltip(&format!("{label}: {count}"));
    }
}
pub(super) fn draw(state: &State, p: &Page<'_>) {
    p.section(&tr(p, "heatmap", "Keyboard heatmap"));
    let Some(counts) = (unsafe { p.read::<[u64; 256]>(read::HEATMAP, state.period) }) else {
        p.wrapped(
            &tr(
                p,
                "heatmapUnavailable",
                "Update the Statistics plugin to show the keyboard heatmap.",
            ),
            44.0,
        );
        return;
    };
    p.wrapped(&tr(p, "heatmapHint", "Darker keys mean more presses. Hover for counts. Only unordered per-key totals are stored; older history has no per-key counts."), 52.0);
    let full = p.width() >= 760.0;
    let columns = if full { 23.5 } else { 15.0 };
    let unit = ((p.width() - 24.0) / columns).clamp(12.0, 46.0);
    p.row(1, unit * if full { 6.8 } else { 13.8 } + 24.0);
    let Some((card, _)) = p.region() else {
        return;
    };
    p.fill(card, 14.0, color::SURFACE);
    p.stroke(card, 14.0, 1.0, color::BORDER_SUBTLE);
    let maximum = counts.iter().copied().max().unwrap_or(1).max(1);
    let origin = Rect::new(card.x + 12.0, card.y + 12.0, unit, unit);
    let draw = |x: f32, y: f32, width: f32, height: f32, code: usize, label: &str| {
        key(
            p,
            Rect::new(
                origin.x + x * unit,
                origin.y + y * unit,
                width * unit - 4.0,
                height * unit - 4.0,
            ),
            label,
            counts[code],
            maximum,
        );
    };
    draw(0.0, 0.0, 1.0, 1.0, 27, "Esc");
    for i in 0..12 {
        draw(
            2.0 + i as f32 + (i / 4) as f32 * 0.5,
            0.0,
            1.0,
            1.0,
            112 + i,
            &format!("F{}", i + 1),
        );
    }
    let rows: &[&[(usize, &str, f32)]] = &[
        &[
            (192, "`", 1.0),
            (49, "1", 1.0),
            (50, "2", 1.0),
            (51, "3", 1.0),
            (52, "4", 1.0),
            (53, "5", 1.0),
            (54, "6", 1.0),
            (55, "7", 1.0),
            (56, "8", 1.0),
            (57, "9", 1.0),
            (48, "0", 1.0),
            (189, "-", 1.0),
            (187, "=", 1.0),
            (8, "Back", 2.0),
        ],
        &[
            (9, "Tab", 1.5),
            (81, "Q", 1.0),
            (87, "W", 1.0),
            (69, "E", 1.0),
            (82, "R", 1.0),
            (84, "T", 1.0),
            (89, "Y", 1.0),
            (85, "U", 1.0),
            (73, "I", 1.0),
            (79, "O", 1.0),
            (80, "P", 1.0),
            (219, "[", 1.0),
            (221, "]", 1.0),
            (220, "\\", 1.5),
        ],
        &[
            (20, "Caps", 1.75),
            (65, "A", 1.0),
            (83, "S", 1.0),
            (68, "D", 1.0),
            (70, "F", 1.0),
            (71, "G", 1.0),
            (72, "H", 1.0),
            (74, "J", 1.0),
            (75, "K", 1.0),
            (76, "L", 1.0),
            (186, ";", 1.0),
            (222, "'", 1.0),
            (13, "Enter", 2.25),
        ],
        &[
            (160, "Shift", 2.25),
            (90, "Z", 1.0),
            (88, "X", 1.0),
            (67, "C", 1.0),
            (86, "V", 1.0),
            (66, "B", 1.0),
            (78, "N", 1.0),
            (77, "M", 1.0),
            (188, ",", 1.0),
            (190, ".", 1.0),
            (191, "/", 1.0),
            (161, "Shift", 2.75),
        ],
        &[
            (162, "Ctrl", 1.5),
            (91, "Meta", 1.25),
            (164, "Alt", 1.25),
            (32, "Space", 6.0),
            (165, "Alt", 1.25),
            (93, "Menu", 1.25),
            (163, "Ctrl", 1.5),
        ],
    ];
    for (row, keys) in rows.iter().enumerate() {
        let mut x = 0.0;
        for &(code, label, width) in *keys {
            draw(x, 1.6 + row as f32, width, 1.0, code, label);
            x += width;
        }
    }
    {
        let extra_y = if full { 0.0 } else { 7.0 };
        let nav_x = if full { 15.5 } else { 0.0 };
        let pad_x = if full { 19.5 } else { 4.0 };
        for (x, y, code, label) in [
            (0., 0., 44, "Prt"),
            (1., 0., 145, "Scr"),
            (2., 0., 19, "Pause"),
            (0., 1.6, 45, "Ins"),
            (1., 1.6, 36, "Home"),
            (2., 1.6, 33, "PgUp"),
            (0., 2.6, 46, "Del"),
            (1., 2.6, 35, "End"),
            (2., 2.6, 34, "PgDn"),
            (1., 4.6, 38, "Up"),
            (0., 5.6, 37, "Left"),
            (1., 5.6, 40, "Down"),
            (2., 5.6, 39, "Right"),
        ] {
            draw(nav_x + x, extra_y + y, 1.0, 1.0, code, label);
        }
        for (x, y, w, h, code, label) in [
            (0., 1.6, 1., 1., 144, "Num"),
            (1., 1.6, 1., 1., 111, "/"),
            (2., 1.6, 1., 1., 106, "*"),
            (3., 1.6, 1., 1., 109, "-"),
            (0., 2.6, 1., 1., 103, "7"),
            (1., 2.6, 1., 1., 104, "8"),
            (2., 2.6, 1., 1., 105, "9"),
            (3., 2.6, 1., 2., 107, "+"),
            (0., 3.6, 1., 1., 100, "4"),
            (1., 3.6, 1., 1., 101, "5"),
            (2., 3.6, 1., 1., 102, "6"),
            (0., 4.6, 1., 1., 97, "1"),
            (1., 4.6, 1., 1., 98, "2"),
            (2., 4.6, 1., 1., 99, "3"),
            (3., 4.6, 1., 2., 13, "Ent"),
            (0., 5.6, 2., 1., 96, "0"),
            (2., 5.6, 1., 1., 110, "."),
        ] {
            draw(pad_x + x, extra_y + y, w, h, code, label);
        }
    }
}
