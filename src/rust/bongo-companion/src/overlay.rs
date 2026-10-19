use crate::{guard, Instance};
use bongo_plugin::overlay::{Api, Canvas, Visitor};
use bongo_plugin::page::{color, Rect};
use std::ffi::c_void;
unsafe extern "C" fn draw(instance: *mut c_void, canvas: *const Canvas) -> u32 {
    guard(0, || {
        let (Some(v), Some(c)) = (instance.cast::<Instance>().as_ref(), Canvas::get(canvas)) else {
            return 0;
        };
        if v.text.is_empty() {
            return 0;
        }
        c.fill(Rect::new(0.0, 0.0, 320.0, 224.0), 16.0, color::SURFACE);
        c.stroke(
            Rect::new(0.5, 0.5, 319.0, 223.0),
            16.0,
            1.0,
            color::BORDER_SUBTLE,
        );
        // Wrap by actual glyph widths, including CJK and explicit newlines.
        let mut lines = vec![String::new()];
        for ch in v.text.chars() {
            if ch == '\n' {
                lines.push(String::new());
                continue;
            }
            let line = lines.last_mut().expect("line");
            let mut next = line.clone();
            next.push(ch);
            if !line.is_empty() && c.measure(&next, 0) > 288.0 {
                lines.push(ch.to_string());
            } else {
                line.push(ch);
            }
        }
        let truncated = lines.len() > 8;
        for (i, line) in lines.iter().take(8).enumerate() {
            let mut line = line.clone();
            if i == 7 && truncated {
                line.push('…');
                while c.measure(&line, 0) > 288.0 && line.chars().count() > 1 {
                    let mut chars: Vec<_> = line.chars().collect();
                    chars.remove(chars.len() - 2);
                    line = chars.into_iter().collect();
                }
            }
            c.text(
                Rect::new(16.0, 12.0 + i as f32 * 25.0, 288.0, 25.0),
                &line,
                0,
                color::TEXT,
                0,
            );
        }
        1
    })
}
unsafe extern "C" fn pointer(instance: *mut c_void, _: f32, _: f32, down: u32) {
    guard((), || {
        if down != 0 {
            if let Some(v) = instance.cast::<Instance>().as_mut() {
                v.text.clear();
            }
        }
    })
}
unsafe extern "C" fn visit_text(
    instance: *mut c_void,
    visitor: Option<Visitor>,
    context: *mut c_void,
) -> u32 {
    guard(0, || {
        let (Some(v), Some(visitor)) = (instance.cast::<Instance>().as_ref(), visitor) else {
            return 0;
        };
        bongo_plugin::overlay::visit(visitor, context, &v.text);
        u32::from(!v.text.is_empty())
    })
}
static API: Api = Api {
    abi_version: 1,
    struct_size: std::mem::size_of::<Api>() as u32,
    width: 320,
    height: 224,
    title_key: c"pages.preference.companion.title".as_ptr(),
    title: c"AI companion (experimental)".as_ptr(),
    draw,
    pointer,
    visit_text,
};
#[no_mangle]
pub extern "C" fn bongo_cat_companion_overlay_query_v1(abi: u32) -> *const Api {
    if abi == 1 {
        &API
    } else {
        std::ptr::null()
    }
}
