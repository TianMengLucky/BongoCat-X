//! Observe IME composition completion when the provider omits finalized events.
use super::element_id;
use crate::words::Batch;
use crate::words_capture::{has_preedit, histogram, Capture, MAX_TEXT};
use crate::words_text::{Tracker, CONTEXT};
use std::time::Instant;
use windows::{
    core::Interface,
    Win32::{
        Foundation::E_INVALIDARG,
        UI::Accessibility::{
            IUIAutomation3, IUIAutomationElement, IUIAutomationTextEditPattern,
            IUIAutomationTextPattern, IUIAutomationTextRange, TextPatternRangeEndpoint_End,
            TextPatternRangeEndpoint_Start, TextUnit_Character, UIA_IsReadOnlyAttributeId,
            UIA_TextEditPatternId, UIA_TextPatternId,
        },
    },
};

struct Pending {
    element: IUIAutomationElement,
    range: IUIAutomationTextRange,
}

#[derive(Default)]
pub struct Composition {
    pending: Option<Pending>,
    text: Tracker,
    pub history: Option<crate::words_undo::History>,
}

// The generated binding rejects S_OK/null, which means "no active composition" here.
unsafe fn active(
    pattern: &IUIAutomationTextEditPattern,
) -> windows::core::Result<Option<IUIAutomationTextRange>> {
    let mut range = std::ptr::null_mut();
    // SAFETY: the live interface owns this vtable; a non-null output transfers one COM reference.
    let result = (pattern.vtable().GetActiveComposition)(pattern.as_raw(), &mut range);
    let range = if range.is_null() {
        None
    } else {
        Some(IUIAutomationTextRange::from_raw(range))
    };
    result.ok()?;
    if let Some(range) = &range {
        if !has_preedit(&range.GetText((MAX_TEXT + 1) as i32)?) {
            return Ok(None);
        }
    }
    Ok(range)
}

impl Composition {
    pub unsafe fn poll(
        &mut self,
        api: &IUIAutomation3,
        keys: &[u64; 256],
        blocked: bool,
    ) -> windows::core::Result<Option<Capture>> {
        let at = Instant::now();
        self.text.input(keys, blocked, at);
        if let Some(history) = &mut self.history { history.input(keys, blocked, at); }
        let pending = self.pending.is_some();
        let result = self.observe(api);
        if result.is_err() {
            self.pending = None;
            self.reset_text();
            if !pending {
                return Ok(None);
            }
        }
        result
    }

    fn reset_text(&mut self) {
        self.text.reset();
        if let Some(history) = &mut self.history { *history = Default::default(); }
    }

    unsafe fn observe(&mut self, api: &IUIAutomation3) -> windows::core::Result<Option<Capture>> {
        let focused = api.GetFocusedElement()?;
        if focused.CurrentIsPassword()?.as_bool() || !focused.CurrentHasKeyboardFocus()?.as_bool() {
            self.pending = None;
            self.reset_text();
            return Ok(None);
        }
        if let Some(pending) = &self.pending {
            if !api.CompareElements(&focused, &pending.element)?.as_bool() {
                self.pending = None;
            }
        }
        // Some providers retain a TextEdit range after commit; plain text must work independently.
        match self.plain(&focused) {
            Ok(Some(capture)) => {
                self.pending = None;
                return Ok(Some(capture));
            }
            Err(_) => self.reset_text(),
            Ok(None) => {}
        }
        let Ok(pattern) =
            focused.GetCurrentPatternAs::<IUIAutomationTextEditPattern>(UIA_TextEditPatternId)
        else {
            self.pending = None;
            return Ok(None);
        };
        if let Some(range) = active(&pattern)? {
            self.pending = Some(Pending {
                element: focused,
                range,
            });
            return Ok(None);
        }
        let Some(pending) = self.pending.take() else {
            return Ok(None);
        };
        let selection = pattern.GetSelection()?;
        if selection.Length()? != 1 {
            return Ok(None);
        }
        let caret = selection.GetElement(0)?;
        if caret.CompareEndpoints(
            TextPatternRangeEndpoint_Start,
            &caret,
            TextPatternRangeEndpoint_End,
        )? != 0
            || pending.range.CompareEndpoints(
                TextPatternRangeEndpoint_Start,
                &caret,
                TextPatternRangeEndpoint_Start,
            )? > 0
            || pending.range.CompareEndpoints(
                TextPatternRangeEndpoint_End,
                &caret,
                TextPatternRangeEndpoint_Start,
            )? < 0
        {
            return Ok(None);
        }
        // Only shrink the observed composition to its final caret; never expand into other text.
        pending.range.MoveEndpointByRange(
            TextPatternRangeEndpoint_End,
            &caret,
            TextPatternRangeEndpoint_Start,
        )?;
        let text = pending.range.GetText((MAX_TEXT + 1) as i32)?;
        if text.len() > MAX_TEXT {
            return Err(windows::core::Error::from_hresult(E_INVALIDARG));
        }
        let counts = histogram(&text);
        if counts.is_empty() {
            return Ok(None);
        }
        self.text.reset();
        Ok(Some(Capture {
            element: element_id(&focused)?,
            polled: true,
            batch: Batch {
                date: chrono::Local::now().date_naive(),
                at: Instant::now(),
                counts,
            },
        }))
    }

    unsafe fn plain(
        &mut self,
        focused: &IUIAutomationElement,
    ) -> windows::core::Result<Option<Capture>> {
        let pattern = focused.GetCurrentPatternAs::<IUIAutomationTextPattern>(UIA_TextPatternId)?;
        let selection = pattern.GetSelection()?;
        if selection.Length()? != 1 {
            self.reset_text();
            return Ok(None);
        }
        let caret = selection.GetElement(0)?;
        // Chromium can return a structurally noncollapsed range with no selected text.
        if (caret.CompareEndpoints(
            TextPatternRangeEndpoint_Start,
            &caret,
            TextPatternRangeEndpoint_End,
        )? != 0 && !caret.GetText(1)?.is_empty())
            || bool::try_from(&caret.GetAttributeValue(UIA_IsReadOnlyAttributeId)?)?
        {
            self.reset_text();
            return Ok(None);
        }
        let bounds = pattern.DocumentRange()?;
        let inside = caret.CompareEndpoints(
            TextPatternRangeEndpoint_Start,
            &bounds,
            TextPatternRangeEndpoint_Start,
        )? >= 0
            && caret.CompareEndpoints(
                TextPatternRangeEndpoint_End,
                &bounds,
                TextPatternRangeEndpoint_End,
            )? <= 0;
        let context = if inside {
            caret.Clone()?
        } else {
            // QQ's selection endpoints can sit outside its own DocumentRange. Keep the
            // fallback inside this focused editable control, taking only its bounded tail.
            let tail = bounds.Clone()?;
            tail.MoveEndpointByRange(
                TextPatternRangeEndpoint_Start,
                &bounds,
                TextPatternRangeEndpoint_End,
            )?;
            tail
        };
        context.MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, -CONTEXT)?;
        if context.CompareEndpoints(
            TextPatternRangeEndpoint_Start,
            &bounds,
            TextPatternRangeEndpoint_Start,
        )? < 0
        {
            context.MoveEndpointByRange(
                TextPatternRangeEndpoint_Start,
                &bounds,
                TextPatternRangeEndpoint_Start,
            )?;
        }
        let text = context.GetText((MAX_TEXT + 1) as i32)?;
        if text.len() > MAX_TEXT {
            self.reset_text();
            return Ok(None);
        }
        let element = element_id(focused)?;
        let at = Instant::now();
        let text: Vec<_> = char::decode_utf16(text.iter().copied()).filter_map(Result::ok).collect();
        if let Some(history) = &mut self.history { history.observe(element.clone(), &text, at); }
        let Some(counts) = self.text.observe(element.clone(), text, at) else {
            return Ok(None);
        };
        Ok(Some(Capture {
            element,
            polled: true,
            batch: Batch {
                date: chrono::Local::now().date_naive(),
                at,
                counts,
            },
        }))
    }
}
