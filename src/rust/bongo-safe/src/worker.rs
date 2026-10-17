//! Per-plugin bounded queue, synchronization and thread lifetime.
use std::{
    collections::VecDeque,
    ffi::{c_char, c_void},
    sync::{Arc, Condvar, Mutex},
    thread::{self, JoinHandle},
    time::Duration,
};
#[repr(C)]
#[derive(Clone, Copy)]
pub(super) struct Job {
    kind: u32,
    index: u32,
    notify: bool,
    play: bool,
    key: [u8; 32],
    path: [u8; 1024],
}
impl Job {
    fn command(kind: u32) -> Self {
        Self {
            kind,
            index: 0,
            notify: false,
            play: false,
            key: [0; 32],
            path: [0; 1024],
        }
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub(super) struct Error {
    code: i32,
    message: [u8; 256],
}
impl Error {
    fn empty() -> Self {
        Self {
            code: 0,
            message: [0; 256],
        }
    }
}
type Work = unsafe extern "C" fn(*mut c_void, *const Job, *mut Error) -> bool;
struct ForeignWork {
    work: Work,
    context: *mut c_void,
}
// The host transfers callback execution to exactly one thread and joins before
// unloading the plugin/freeing context. Shared C state uses its own state mutex.
unsafe impl Send for ForeignWork {}
impl ForeignWork {
    fn invoke(&self, job: &Job, error: &mut Error) -> bool {
        unsafe { (self.work)(self.context, job, error) }
    }
}
struct Queue {
    jobs: VecDeque<Job>,
    stopping: bool,
    running: bool,
    overflow: bool,
    notices: u32,
    failed: bool,
    error: Error,
}
struct Shared {
    queue: Mutex<Queue>,
    wake: Condvar,
}
pub struct Worker {
    shared: Arc<Shared>,
    thread: Option<JoinHandle<()>>,
    thread_id: thread::ThreadId,
}
fn run(work: &ForeignWork, shared: &Shared, idle: bool) {
    loop {
        let mut q = shared.queue.lock().unwrap_or_else(|e| e.into_inner());
        while q.jobs.is_empty() && !q.overflow && !q.stopping {
            if idle {
                q = shared
                    .wake
                    .wait_timeout(q, Duration::from_millis(100))
                    .unwrap_or_else(|e| e.into_inner())
                    .0;
                break;
            }
            q = shared.wake.wait(q).unwrap_or_else(|e| e.into_inner());
        }
        if q.stopping && q.jobs.is_empty() {
            break;
        }
        let overflow = q.overflow && q.jobs.is_empty();
        if overflow {
            q.overflow = false;
        }
        let job = q.jobs.pop_front().unwrap_or_else(|| Job::command(0));
        q.running = job.kind != 0 || overflow;
        drop(q);
        let mut error = Error::empty();
        if overflow {
            work.invoke(&Job::command(255), &mut error);
        }
        let ok = work.invoke(&job, &mut error);
        let mut q = shared.queue.lock().unwrap_or_else(|e| e.into_inner());
        if job.notify || (!ok && job.kind != 0) {
            q.notices = q.notices.saturating_add(1);
            if !ok {
                q.failed = true;
                q.error = error;
            }
        }
        q.running = false;
    }
}
#[no_mangle]
pub(super) unsafe extern "C" fn bongo_safe_worker_new(
    name: *const c_char,
    work: Option<Work>,
    context: *mut c_void,
    idle: bool,
    job_size: usize,
    error_size: usize,
) -> *mut Worker {
    crate::guard(std::ptr::null_mut(), || {
        if job_size != std::mem::size_of::<Job>() || error_size != std::mem::size_of::<Error>() {
            return std::ptr::null_mut();
        }
        let Some(work) = work else {
            return std::ptr::null_mut();
        };
        let Ok(name) = crate::image_rows::text(name) else {
            return std::ptr::null_mut();
        };
        let shared = Arc::new(Shared {
            queue: Mutex::new(Queue {
                // Most workers only queue a few jobs with 1 KiB paths.
                jobs: VecDeque::new(),
                stopping: false,
                running: false,
                overflow: false,
                notices: 0,
                failed: false,
                error: Error::empty(),
            }),
            wake: Condvar::new(),
        });
        let state = shared.clone();
        let foreign = ForeignWork { work, context };
        let thread = thread::Builder::new().name(name.to_owned()).spawn(move || {
            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                run(&foreign, &state, idle)
            }));
            if result.is_err() {
                let mut q = state.queue.lock().unwrap_or_else(|e| e.into_inner());
                q.jobs.clear();
                q.stopping = true;
                q.running = false;
                q.failed = true;
                q.notices = 1;
                q.error.code = 4;
                let message = b"Plugin worker panicked";
                q.error.message[..message.len()].copy_from_slice(message);
            }
            foreign.invoke(&Job::command(254), &mut Error::empty());
        });
        match thread {
            Ok(thread) => Box::into_raw(Box::new(Worker {
                shared,
                thread_id: thread.thread().id(),
                thread: Some(thread),
            })),
            Err(_) => std::ptr::null_mut(),
        }
    })
}
#[no_mangle]
pub(super) unsafe extern "C" fn bongo_safe_worker_submit(
    worker: *const Worker,
    job: *const Job,
) -> bool {
    crate::guard(false, || {
        let (Some(worker), Some(job)) = (worker.as_ref(), job.as_ref()) else {
            return false;
        };
        let Ok(mut q) = worker.shared.queue.lock() else {
            return false;
        };
        let ok = !q.stopping && q.jobs.len() < 256;
        if ok {
            q.jobs.push_back(*job);
        } else if job.kind == 4 {
            q.overflow = true;
        }
        worker.shared.wake.notify_one();
        ok
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_worker_busy(worker: *const Worker) -> bool {
    crate::guard(false, || {
        worker.as_ref().is_some_and(|w| {
            w.shared
                .queue
                .lock()
                .is_ok_and(|q| q.running || !q.jobs.is_empty() || q.overflow)
        })
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_worker_current(worker: *const Worker) -> bool {
    crate::guard(false, || {
        worker
            .as_ref()
            .is_some_and(|w| w.thread_id == thread::current().id())
    })
}
#[no_mangle]
pub(super) unsafe extern "C" fn bongo_safe_worker_poll(
    worker: *const Worker,
    failed: *mut bool,
    error: *mut Error,
) -> bool {
    crate::guard(false, || {
        let (Some(w), Some(failed), Some(error)) =
            (worker.as_ref(), failed.as_mut(), error.as_mut())
        else {
            return false;
        };
        let Ok(mut q) = w.shared.queue.lock() else {
            return false;
        };
        let ready = q.notices != 0;
        *failed = q.failed;
        *error = q.error;
        q.notices = 0;
        q.failed = false;
        ready
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_worker_free(worker: *mut Worker) {
    crate::guard((), || {
        if worker.is_null() {
            return;
        }
        let mut worker = Box::from_raw(worker);
        {
            let mut q = worker
                .shared
                .queue
                .lock()
                .unwrap_or_else(|e| e.into_inner());
            q.stopping = true;
            worker.shared.wake.notify_all();
        }
        if let Some(thread) = worker.thread.take() {
            let _ = thread.join();
        }
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn work(_: *mut c_void, _: *const Job, _: *mut Error) -> bool {
        true
    }

    #[test]
    fn idle_worker_does_not_reserve_the_maximum_queue() {
        let worker = unsafe {
            bongo_safe_worker_new(
                b"test-lazy-worker\0".as_ptr().cast(),
                Some(work),
                std::ptr::null_mut(),
                false,
                std::mem::size_of::<Job>(),
                std::mem::size_of::<Error>(),
            )
        };
        assert!(!worker.is_null());
        let capacity = unsafe { &*worker }
            .shared
            .queue
            .lock()
            .unwrap()
            .jobs
            .capacity();
        unsafe { bongo_safe_worker_free(worker) };
        assert_eq!(capacity, 0);
    }
}
