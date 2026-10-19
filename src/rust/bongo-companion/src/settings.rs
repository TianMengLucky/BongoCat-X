use crate::{guard, Instance};
use bongo_plugin::page::{Host, Page, Services};
use bongo_plugin::ui::Ui;
use std::ffi::c_void;
fn tr(page: &Page, key: &str, fallback: &str) -> String {
    page.tr(&format!("pages.preference.companion.{key}"), fallback)
}
fn field(page: &Page, key: &str, title: &str, value: &mut [u8], ascii: bool) {
    page.label(&tr(page, key, title));
    page.row(1, 34.0);
    page.edit(key, value, ascii);
}
pub unsafe extern "C" fn draw(
    instance: *mut c_void,
    ui: *mut Ui,
    host: *const Host,
    services: *const Services,
) -> u32 {
    guard(0, || {
        let (Some(instance), Some(page)) = (
            instance.cast::<Instance>().as_mut(),
            Page::new(ui, host, services),
        ) else {
            return 0;
        };
        page.intro(&tr(&page, "description", "Experimental AI companion. Disabled by default. Only event types are sent; no typed text or application data."));
        let c = &mut instance.draft;
        page.toggle(
            "companion-enabled",
            &tr(&page, "enabled", "Enable AI companion"),
            &mut c.enabled,
        );
        field(
            &page,
            "endpoint",
            "Chat Completions HTTPS endpoint",
            &mut c.endpoint,
            true,
        );
        field(&page, "model", "Model", &mut c.model, true);
        field(
            &page,
            "keyEnv",
            "API key environment variable",
            &mut c.key_env,
            true,
        );
        page.wrapped(&tr(&page, "keyHint", "Set DEEPSEEK_API_KEY before starting BongoCat. The key is never saved to settings. Requests may incur API charges."), 52.0);
        field(
            &page,
            "prompt",
            "Pet personality / system prompt",
            &mut c.prompt,
            false,
        );
        page.section(&tr(&page, "triggers", "Triggers"));
        for (index, key, name) in [
            (0, "startup", "Startup"),
            (1, "idle", "Idle"),
            (2, "keyboard", "Keyboard activity"),
            (3, "mouse", "Mouse activity"),
            (4, "behavior", "Pet behavior"),
        ] {
            page.toggle(key, &tr(&page, key, name), &mut c.triggers[index]);
        }
        page.number(
            "companion-cooldown",
            &tr(&page, "cooldown", "Request cooldown (seconds)"),
            &mut c.cooldown,
            (10.0, 3600.0),
        );
        page.number(
            "companion-idle",
            &tr(&page, "idleTime", "Idle threshold (seconds)"),
            &mut c.idle,
            (30.0, 86400.0),
        );
        page.number(
            "companion-duration",
            &tr(&page, "duration", "Speech bubble duration (seconds)"),
            &mut c.duration,
            (3.0, 120.0),
        );
        page.row(1, 34.0);
        if page.button("companion-save", &tr(&page, "save", "Save"), 0) {
            let ok = instance.save();
            page.notice(
                &tr(
                    &page,
                    if ok { "saved" } else { "failed" },
                    if ok {
                        "Saved"
                    } else {
                        "Invalid settings or save failed"
                    },
                ),
                !ok,
            );
        }
        page.row(1, 34.0);
        let blocked = instance.config.enabled == 0 || instance.worker.is_some();
        if blocked {
            page.disabled(true);
        }
        if page.button(
            "companion-test",
            &tr(&page, "test", "Test connection (uses API)"),
            4,
        ) {
            instance.trigger(5, instance.now);
        }
        if blocked {
            page.disabled(false);
        }
        let key = if instance.worker.is_some() {
            "busy"
        } else if instance.status == 5 {
            "missingKey"
        } else if instance.status != 0 {
            "requestFailed"
        } else {
            "ready"
        };
        page.label(&tr(&page, key, "Ready"));
        page.wrapped(
            &tr(
                &page,
                "tts",
                "TTS extension interface is reserved; no speech provider is bundled.",
            ),
            52.0,
        );
        if instance.worker.is_some() {
            page.invalidate();
        }
        1
    })
}
