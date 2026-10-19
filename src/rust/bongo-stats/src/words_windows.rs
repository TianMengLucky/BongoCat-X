//! UIA owns the callback on COM threads; only histograms leave the callback.
use crate::words::Batch;
use crate::words_capture::{histogram, Capture, Filter, MAX_TEXT};
#[path = "words_composition.rs"]
mod composition;
use std::sync::{
    atomic::{AtomicBool, AtomicU32, Ordering},
    mpsc, Arc,
};
use std::time::Instant;
use windows::{
    core::{implement, Ref, BSTR},
    Win32::{
        Foundation::E_INVALIDARG,
        System::{
            Com::{
                CoCreateInstance, CoInitializeEx, CoUninitialize, CLSCTX_INPROC_SERVER,
                COINIT_MULTITHREADED, SAFEARRAY,
            },
            Ole::{
                SafeArrayDestroy, SafeArrayGetDim, SafeArrayGetElement, SafeArrayGetLBound,
                SafeArrayGetUBound, SafeArrayGetVartype,
            },
            Variant::{VT_BSTR, VT_I4},
        },
        UI::Accessibility::{
            CUIAutomation8, IUIAutomation3, IUIAutomationCacheRequest, IUIAutomationElement,
            IUIAutomationTextEditTextChangedEventHandler,
            IUIAutomationTextEditTextChangedEventHandler_Impl, TextEditChangeType,
            TextEditChangeType_CompositionFinalized, TreeScope_Subtree,
        },
        UI::Input::KeyboardAndMouse::{
            GetAsyncKeyState, VK_BACK, VK_CONTROL, VK_LWIN, VK_MENU, VK_RWIN,
        },
    },
};

#[implement(IUIAutomationTextEditTextChangedEventHandler)]
struct Handler {
    sender: mpsc::SyncSender<Capture>,
    enabled: Arc<AtomicBool>,
    status: Arc<AtomicU32>,
    _lease: Arc<()>,
}
impl IUIAutomationTextEditTextChangedEventHandler_Impl for Handler_Impl {
    fn HandleTextEditTextChangedEvent(
        &self,
        sender: Ref<IUIAutomationElement>,
        kind: TextEditChangeType,
        strings: *const SAFEARRAY,
    ) -> windows::core::Result<()> {
        crate::guarded((), || {
            if !self.enabled.load(Ordering::Acquire)
                || kind != TextEditChangeType_CompositionFinalized
            {
                return;
            }
            let result = unsafe { self.collect(sender, strings) };
            if result.is_err() {
                self.status.store(3, Ordering::Relaxed);
            }
        });
        Ok(())
    }
}
impl Handler_Impl {
    unsafe fn collect(
        &self,
        sender: Ref<IUIAutomationElement>,
        strings: *const SAFEARRAY,
    ) -> windows::core::Result<()> {
        let Some(sender) = sender.as_ref() else {
            return Ok(());
        };
        // Reject password and unfocused controls before touching event text. No document reads.
        if sender.CurrentIsPassword()?.as_bool()
            || !sender.CurrentHasKeyboardFocus()?.as_bool()
            || strings.is_null()
        {
            return Ok(());
        }
        if SafeArrayGetDim(strings) != 1 || SafeArrayGetVartype(strings)? != VT_BSTR {
            return Err(windows::core::Error::from_hresult(windows::core::HRESULT(
                0x80070057u32 as i32,
            )));
        }
        let low = SafeArrayGetLBound(strings, 1)?;
        let high = SafeArrayGetUBound(strings, 1)?;
        if high < low {
            return Ok(());
        }
        if i64::from(high) - i64::from(low) >= 64 {
            self.status.store(3, Ordering::Relaxed);
            return Ok(());
        }
        let mut batch = Batch {
            date: chrono::Local::now().date_naive(),
            at: Instant::now(),
            counts: Default::default(),
        };
        for index in low..=high {
            let mut text = BSTR::new();
            SafeArrayGetElement(strings, &index, (&mut text as *mut BSTR).cast())?;
            if text.len() > MAX_TEXT {
                self.status.store(3, Ordering::Relaxed);
                continue;
            }
            for (c, count) in histogram(&text) {
                *batch.counts.entry(c).or_default() += count;
            }
        }
        if !batch.counts.is_empty()
            && self.enabled.load(Ordering::Acquire)
            && self
                .sender
                .try_send(Capture {
                    element: element_id(sender).unwrap_or_default(),
                    polled: false,
                    batch,
                })
                .is_err()
        {
            self.status.store(3, Ordering::Relaxed);
        }
        Ok(())
    }
}

unsafe fn element_id(element: &IUIAutomationElement) -> windows::core::Result<Vec<i32>> {
    struct OwnedArray(*mut SAFEARRAY);
    impl Drop for OwnedArray {
        fn drop(&mut self) {
            // SAFETY: GetRuntimeId transfers ownership; event arrays are never wrapped here.
            if !self.0.is_null() {
                unsafe {
                    let _ = SafeArrayDestroy(self.0);
                }
            }
        }
    }
    let array = OwnedArray(element.GetRuntimeId()?);
    if array.0.is_null() || SafeArrayGetDim(array.0) != 1 || SafeArrayGetVartype(array.0)? != VT_I4
    {
        return Err(windows::core::Error::from_hresult(E_INVALIDARG));
    }
    let low = SafeArrayGetLBound(array.0, 1)?;
    let high = SafeArrayGetUBound(array.0, 1)?;
    if high < low || i64::from(high) - i64::from(low) >= 64 {
        return Err(windows::core::Error::from_hresult(E_INVALIDARG));
    }
    let mut id = Vec::new();
    for index in low..=high {
        let mut value = 0i32;
        SafeArrayGetElement(array.0, &index, (&mut value as *mut i32).cast())?;
        id.push(value);
    }
    Ok(id)
}

pub struct Collector {
    api: Option<IUIAutomation3>,
    root: Option<IUIAutomationElement>,
    handler: Option<IUIAutomationTextEditTextChangedEventHandler>,
    enabled: Arc<AtomicBool>,
    lease: Arc<()>,
    pub status: Arc<AtomicU32>,
    sender: mpsc::SyncSender<Capture>,
    receiver: mpsc::Receiver<Capture>,
    composition: composition::Composition,
    filter: Filter,
}
impl Collector {
    pub fn new() -> windows::core::Result<Self> {
        unsafe {
            CoInitializeEx(None, COINIT_MULTITHREADED).ok()?;
            let result = Self::register();
            if result.is_err() {
                CoUninitialize();
            }
            result
        }
    }
    unsafe fn register() -> windows::core::Result<Self> {
        let api: IUIAutomation3 = CoCreateInstance(&CUIAutomation8, None, CLSCTX_INPROC_SERVER)?;
        let root = api.GetRootElement()?;
        let (sender, receiver) = mpsc::sync_channel(128);
        let enabled = Arc::new(AtomicBool::new(true));
        let status = Arc::new(AtomicU32::new(1));
        let lease = Arc::new(());
        let handler: IUIAutomationTextEditTextChangedEventHandler = Handler {
            sender: sender.clone(),
            enabled: enabled.clone(),
            status: status.clone(),
            _lease: lease.clone(),
        }
        .into();
        api.AddTextEditTextChangedEventHandler(
            &root,
            TreeScope_Subtree,
            TextEditChangeType_CompositionFinalized,
            None::<&IUIAutomationCacheRequest>,
            &handler,
        )?;
        Ok(Self {
            api: Some(api),
            root: Some(root),
            handler: Some(handler),
            enabled,
            lease,
            status,
            sender,
            receiver,
            composition: composition::Composition::default(),
            filter: Filter::default(),
        })
    }
}
impl Collector {
    pub fn set_backspace(&mut self, enabled: bool) {
        if enabled != self.composition.history.is_some() {
            self.composition.history = enabled.then(Default::default);
        }
    }
    pub fn reset_tracking(&mut self) {
        self.composition = Default::default();
        self.filter = Default::default();
        while self.receiver.try_recv().is_ok() {}
    }
    pub fn poll(&mut self, keys: &[u64; 256], mouse: bool) {
        let Some(api) = &self.api else {
            return;
        };
        let blocked = mouse
            || [VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN]
                .iter()
                .any(|key| unsafe { GetAsyncKeyState(key.0 as i32) } < 0);
        let mut keys = *keys;
        if unsafe { GetAsyncKeyState(VK_BACK.0 as i32) } < 0 {
            keys[8] = keys[8].max(1);
        }
        // SAFETY: the collector and its ranges stay on the worker's initialized MTA.
        match unsafe { self.composition.poll(api, &keys, blocked) } {
            Ok(Some(capture)) => {
                if !capture.batch.counts.is_empty() && self.sender.try_send(capture).is_err() {
                    self.status.store(3, Ordering::Relaxed);
                }
            }
            Err(_) => {
                self.status.store(3, Ordering::Relaxed);
            }
            Ok(None) => {}
        }
    }

    pub fn next_batch(&mut self) -> Option<(Batch, bool)> {
        if let Some(history) = &mut self.composition.history {
            if let Some(batch) = history.removed.pop_front() {
                return Some((batch, true));
            }
        }
        while let Ok(capture) = self.receiver.try_recv() {
            let element = capture.element.clone();
            if let Some(batch) = self.filter.accept(capture) {
                if let Some(history) = &mut self.composition.history {
                    history.committed(&element, &batch);
                }
                return Some((batch, false));
            }
        }
        None
    }

    pub fn stop(&mut self) {
        self.enabled.store(false, Ordering::Release);
        self.composition = composition::Composition::default();
        if let Some(handler) = self.handler.take() {
            unsafe {
                let _ = self
                    .api
                    .as_ref()
                    .unwrap()
                    .RemoveTextEditTextChangedEventHandler(self.root.as_ref().unwrap(), &handler);
            }
            drop(handler);
        }
        drop(self.root.take());
        drop(self.api.take());
        // UIA can retain callbacks after removal. Never unload their code while COM owns one.
        while Arc::strong_count(&self.lease) != 1 {
            std::thread::sleep(std::time::Duration::from_millis(10));
        }
    }
}
impl Drop for Collector {
    fn drop(&mut self) {
        self.stop();
        unsafe {
            CoUninitialize();
        }
    }
}
