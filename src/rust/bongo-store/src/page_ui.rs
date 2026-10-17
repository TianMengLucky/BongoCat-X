use crate::Entry;
use bongo_plugin::page::{action, color, read, text, Page, PageState, Rect, Status};

struct State {
    repository: [u8; 512],
    initialized: bool,
    glyph_hash: u64,
}
impl Default for State {
    fn default() -> Self {
        Self {
            repository: [0; 512],
            initialized: false,
            glyph_hash: 0,
        }
    }
}
fn tr(p: &Page<'_>, key: &str, fallback: &str) -> String {
    p.tr(&format!("pages.preference.store.{key}"), fallback)
}
fn failure(p: &Page<'_>) {
    p.notice(&tr(p, "failed", "Unable to update the store"), true);
}
fn act(p: &Page<'_>, op: u32, index: u32) {
    if !p.act(op, index) {
        failure(p);
    }
}
fn link(p: &Page<'_>, r: Rect, title: &str, url: &str, enabled: bool) {
    let linked = !title.is_empty() && url.starts_with("https://");
    let width = p.measure(title, if linked { 2 } else { 1 }).min(r.w);
    let hit = Rect::new(r.x + (r.w - width) / 2.0, r.y, width, r.h);
    let hover = enabled && linked && p.input(hit, 0);
    p.paint_text(
        r,
        title,
        if linked { 2 } else { 1 },
        if !linked {
            color::MUTED
        } else if hover {
            color::LINK_HOVER
        } else {
            color::LINK
        },
        1,
    );
    if hover {
        p.input(hit, 3);
        p.line(
            Rect::new(hit.x, hit.y + hit.h - 2.0, hit.w, 0.0),
            1.0,
            color::LINK_HOVER,
        );
        if p.input(hit, 1) && !p.url(url) {
            failure(p);
        }
    }
}
fn card(
    p: &Page<'_>,
    index: u32,
    entry: &Entry,
    status: &Status,
    preview_height: f32,
    buy_row: bool,
) {
    let Some((r, enabled)) = p.region() else {
        return;
    };
    p.fill(r, 12.0, color::SURFACE);
    p.stroke(r, 12.0, 1.0, color::BORDER_SUBTLE);
    let preview = Rect::new(r.x + 12.0, r.y + 12.0, r.w - 24.0, preview_height);
    p.fill(preview, 8.0, color::FIELD);
    let path = unsafe { p.read::<[u8; 1024]>(read::IMAGE, index) };
    if !path
        .as_ref()
        .is_some_and(|path| p.image(text(path), preview))
    {
        p.paint_text(
            preview,
            &tr(p, "preview", "Model preview"),
            1,
            color::MUTED,
            1,
        );
    } else {
        p.stroke(
            Rect::new(
                preview.x - 4.0,
                preview.y - 4.0,
                preview.w + 8.0,
                preview.h + 8.0,
            ),
            12.0,
            8.0,
            color::SURFACE,
        );
    }
    p.stroke(preview, 8.0, 1.0, color::BORDER_SUBTLE);
    let title = Rect::new(preview.x, preview.y + preview.h + 10.0, preview.w, 22.0);
    p.paint_text(title, text(&entry.name), 2, color::TEXT, 1);
    let author = Rect::new(title.x, title.y + 26.0, title.w, 22.0);
    link(
        p,
        author,
        text(&entry.author),
        text(&entry.author_url),
        enabled,
    );
    if entry.paid != 0 && text(&entry.purchase_url).starts_with("https://") {
        link(
            p,
            Rect::new(author.x, author.y + 30.0, author.w, 22.0),
            &tr(p, "buyFull", "Buy full version"),
            text(&entry.purchase_url),
            enabled,
        );
    }
    let busy = status.operation == 1 || status.operation == 2;
    let current = status.operation_index == index;
    let installed = unsafe { p.read::<u32>(read::INSTALLED, index).unwrap_or(0) } != 0;
    let action_title = if busy && current {
        if status.operation == 2 {
            tr(p, "installing", "Installing...")
        } else {
            tr(p, "downloading", "Downloading...")
        }
    } else if installed {
        tr(p, "uninstall", "Uninstall")
    } else {
        tr(p, "download", "Download")
    };
    let button = Rect::new(
        author.x,
        author.y + 30.0 + if buy_row { 28.0 } else { 0.0 },
        author.w,
        34.0,
    );
    let hover = enabled && p.input(button, 0);
    if busy {
        p.fill(button, 7.0, color::HOVER);
    } else {
        p.fill_rgba(button, 7.0, if hover { 0xff4b911d } else { 0xff56a523 });
    }
    if busy {
        p.paint_text(button, &action_title, 1, color::MUTED, 1);
    } else {
        p.text_rgba(button, &action_title, 1, 0xffffffff, 1);
    }
    if hover && !busy {
        p.input(button, 3);
    }
    if busy && current && status.total != 0 {
        let fraction = (status.received as f64 / status.total as f64).min(1.0) as f32;
        p.fill(
            Rect::new(
                button.x,
                button.y + button.h - 3.0,
                button.w * fraction,
                3.0,
            ),
            1.0,
            color::ACCENT,
        );
    }
    if !busy && enabled && p.input(button, 1) {
        act(
            p,
            if installed {
                action::UNINSTALL
            } else {
                action::DOWNLOAD
            },
            index,
        );
    }
    if current && (status.operation == 4 || status.operation == 5) {
        p.paint_text(
            Rect::new(button.x, button.y + button.h + 8.0, button.w, 22.0),
            &if status.operation == 5 {
                tr(p, "hashMismatch", "SHA-256 mismatch; download rejected")
            } else {
                tr(p, "failed", "Unable to update the store")
            },
            1,
            color::DANGER,
            1,
        );
    }
}
impl State {
    fn cards(&mut self, p: &Page<'_>, status: &Status) {
        let count = status.count.min(512);
        let mut names = Vec::with_capacity(count as usize * 2);
        let mut buy_row = false;
        for i in 0..count {
            if let Some(entry) = unsafe { p.read::<Entry>(read::ENTRY, i) } {
                buy_row |= entry.paid != 0;
                names.push(text(&entry.name).to_owned());
                names.push(text(&entry.author).to_owned());
            }
        }
        let texts: Vec<_> = names.iter().map(String::as_str).collect();
        p.glyphs(&texts, &mut self.glyph_hash);
        let gap = p.spacing();
        let columns = ((p.width() + gap) / (260.0 + gap)).floor().max(1.0) as u32;
        let card_width = (p.width() - gap * (columns - 1) as f32) / columns as f32;
        let preview_height = ((card_width - 24.0) / (16.0 / 9.0)).max(1.0);
        let error_height = if status.operation == 4 || status.operation == 5 {
            32.0
        } else {
            0.0
        };
        p.row(
            columns,
            preview_height + 124.0 + if buy_row { 28.0 } else { 0.0 } + error_height,
        );
        p.images_begin();
        for i in 0..count {
            if let Some(entry) = unsafe { p.read::<Entry>(read::ENTRY, i) } {
                card(p, i, &entry, status, preview_height, buy_row);
            }
        }
        p.images_end();
    }
}
impl PageState for State {
    fn draw(&mut self, p: &Page<'_>) {
        let mut status = p.status();
        if status.status == 0 {
            act(p, action::REFRESH_STORE, 0);
            status = p.status();
        }
        p.intro(&tr(p, "help", "Browse community models and author links. Download community models after SHA-256 verification, or uninstall them here."));
        if !self.initialized {
            if let Some(repository) = unsafe { p.read(read::REPOSITORY, 0) } {
                self.repository = repository;
                self.initialized = true;
            }
        }
        p.wrapped(
            &tr(
                p,
                "repositoryUrl",
                "GitHub repository URL (main branch; empty restores default)",
            ),
            44.0,
        );
        p.row(1, 32.0);
        p.edit("store-repository", &mut self.repository, true);
        p.row(1, 32.0);
        let disabled = status.busy != 0;
        if disabled {
            p.disabled(true);
        }
        if p.button(
            "apply-repository",
            &tr(p, "applyRepository", "Apply repository"),
            0,
        ) {
            if !unsafe { p.send(action::SET_REPOSITORY, 0, &self.repository) } {
                p.notice(
                    &tr(
                        p,
                        "invalidRepository",
                        "Enter a GitHub repository URL: https://github.com/owner/repository",
                    ),
                    true,
                );
            } else {
                self.initialized = false;
                act(p, action::REFRESH_STORE, 0);
                status = p.status();
            }
        }
        if disabled {
            p.disabled(false);
        }
        let refresh = tr(p, "refresh", "Refresh");
        let repository = tr(p, "repository", "Community repository");
        let needed = (p.measure(&refresh, 0) + 28.0).max(p.measure(&repository, 2) + 28.0);
        p.row(
            if needed * 2.0 + p.spacing() <= p.width() {
                2
            } else {
                1
            },
            36.0,
        );
        if status.status == 1 {
            if let Some((r, _)) = p.region() {
                p.paint_text(
                    r,
                    &tr(p, "loading", "Loading catalog..."),
                    0,
                    color::TEXT,
                    0,
                );
            }
        } else if p.button("refresh", &tr(p, "refresh", "Refresh"), 0) {
            act(p, action::REFRESH_STORE, 0);
            status = p.status();
        }
        if p.button(
            "repository",
            &tr(p, "repository", "Community repository"),
            2,
        ) {
            if let Some(repository) = unsafe { p.read::<[u8; 512]>(read::REPOSITORY, 0) } {
                if !p.url(text(&repository)) {
                    failure(p);
                }
            }
        }
        if status.status == 3 || status.status == 4 || (status.status == 2 && status.count == 0) {
            p.wrapped(&match status.status {
                3 => tr(p, "unpublished", "The community catalog has not been published yet."),
                4 => tr(p, "networkError", "Unable to load the catalog. Check your connection or catalog format and retry."),
                _ => tr(p, "empty", "No models are listed yet.") }, 60.0);
        }
        if status.status != 1 {
            self.cards(p, &status);
        }
    }
}
bongo_plugin::export_page!(bongo_cat_store_page_ui_query_v1, State);
