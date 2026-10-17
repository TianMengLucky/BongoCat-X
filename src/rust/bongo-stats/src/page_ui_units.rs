use super::State;
use bongo_plugin::page::{action, read, text, Page};
#[repr(C)]
#[derive(Clone, Copy)]
pub(super) struct Units {
    pub count: [u8; 65],
    pub movement: [u8; 65],
    pub scroll: [u8; 65],
    pub movement_scale: f64,
    pub scroll_scale: f64,
}
impl Default for Units {
    fn default() -> Self {
        Self {
            count: [0; 65],
            movement: [0; 65],
            scroll: [0; 65],
            movement_scale: 1.0,
            scroll_scale: 1.0,
        }
    }
}
pub(super) fn draw(state: &mut State, p: &Page<'_>) {
    if !p.tree(
        "statistics-units",
        &p.tr("native.statisticsUnitSettings", "Display units"),
        &mut state.units_open,
    ) {
        return;
    }
    p.intro(&p.tr("native.statisticsUnitHint", "Display = raw value x multiplier. Labels: up to 16 characters; empty uses the language default. Only this page changes; history, CSV and the floating panel keep raw units."));
    let draft = &mut state.draft;
    for (id, key, fallback, label) in [
        (
            "count-unit",
            "native.statisticsCountUnit",
            "Key and mouse count unit",
            &mut draft.count,
        ),
        (
            "movement-unit",
            "native.statisticsMovementUnit",
            "Pointer travel unit",
            &mut draft.movement,
        ),
        (
            "scroll-unit",
            "native.statisticsScrollUnit",
            "Scroll unit",
            &mut draft.scroll,
        ),
    ] {
        p.label(&p.tr(key, fallback));
        p.row(1, 32.0);
        p.edit(id, label, false);
    }
    p.number(
        "statistics-movement-scale",
        &p.tr(
            "native.statisticsMovementScale",
            "Pointer travel multiplier",
        ),
        &mut draft.movement_scale,
        (0.000000001, 1_000_000_000.0),
    );
    p.number(
        "statistics-scroll-scale",
        &p.tr("native.statisticsScrollScale", "Scroll multiplier"),
        &mut draft.scroll_scale,
        (0.000000001, 1_000_000_000.0),
    );
    p.glyphs(
        &[
            text(&draft.count),
            text(&draft.movement),
            text(&draft.scroll),
        ],
        &mut state.unit_glyph_hash,
    );
    p.row(2, 36.0);
    let apply = p.button(
        "apply-units",
        &p.tr("native.statisticsApplyUnits", "Apply units"),
        0,
    );
    let reset = p.button(
        "default-units",
        &p.tr("native.statisticsDefaultUnits", "Use default units"),
        0,
    );
    if reset {
        *draft = Units::default();
    }
    if apply || reset {
        if unsafe { p.send(action::SET_UNITS, 0, draft) } {
            state.units = unsafe { p.read(read::UNITS, 0) };
            if let Some(units) = state.units {
                *draft = units;
            }
            p.invalidate();
            p.unfocus();
        } else {
            p.notice(&p.tr("native.statisticsUnitSaveFailed", "Unable to save units. Use up to 16 characters per label and a multiplier between 0.000000001 and 1000000000."), true);
        }
    }
    p.tree_end();
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn unit_record_matches_host_abi() {
        assert_eq!(std::mem::size_of::<Units>(), 216);
    }
}
