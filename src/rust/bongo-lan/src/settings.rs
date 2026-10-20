use crate::{guard, Instance};
use bongo_plugin::{
    page::{self, Host, Page, Services},
    ui::Ui,
};
use std::ffi::c_void;
pub const ANY_ADDRESS: &str = "[::]:39027";
pub fn local_addresses() -> String {
    let mut addresses = bongo_safe::lan::addresses::temporary();
    // Fall back to stable addresses when the machine has no preferred privacy address:
    // an empty list leaves the host with nothing to share and no usable listen address.
    if addresses.is_empty() {
        addresses = bongo_safe::lan::addresses::stable();
    }
    addresses.sort();
    addresses.dedup();
    addresses
        .iter()
        .map(|ip| format!("[{ip}]:39027"))
        .collect::<Vec<_>>()
        .join("\n")
}
fn tr(p: &Page, key: &str, fallback: &str) -> String {
    p.tr(&format!("pages.preference.lan.{key}"), fallback)
}
fn field(p: &Page, id: &str, title: &str, value: &mut [u8], ascii: bool) {
    // label supplies its own row; inserting an extra row doubles the spacing.
    p.label(title);
    p.row(1, 36.);
    p.edit(id, value, ascii);
}
fn copy(p: &Page, value: &str) {
    let ok = p.copy_text(value);
    p.notice(&tr(p, if ok { "copied" } else { "copyFailed" }, ""), !ok);
}
fn connection(p: &Page, v: &mut Instance, busy: bool) {
    let hosting = v.host != 0;
    let addresses: Vec<_> = if hosting {
        v.addresses.lines().map(str::to_owned).collect()
    } else {
        Vec::new()
    };
    p.section(&tr(p, if hosting { "createRoom" } else { "joinRoom" }, ""));
    p.wrapped(
        &tr(p, if hosting { "createHint" } else { "joinHint" }, ""),
        40.,
    );
    if hosting {
        p.label(&tr(p, "addresses", "Public temporary IPv6 addresses"));
        if addresses.is_empty() {
            p.wrapped(&tr(p, "noAddress", ""), 36.);
        }
        for (i, address) in addresses.iter().enumerate() {
            p.row(1, 32.);
            if p.button(
                &format!("lan-copy-ip-{i}"),
                &format!("{address}  ·  {}", tr(p, "copy", "Copy")),
                4,
            ) {
                copy(p, address);
            }
        }
    }
    if busy {
        p.disabled(true);
    }
    field(
        p,
        if hosting {
            "lan-listen-address"
        } else {
            "lan-peer-address"
        },
        &tr(
            p,
            if hosting {
                "listenAddress"
            } else {
                "peerAddress"
            },
            "",
        ),
        &mut v.address,
        true,
    );
    field(
        p,
        if hosting {
            "lan-create-code"
        } else {
            "lan-join-code"
        },
        &tr(p, "code", "Room code (8–64 characters)"),
        &mut v.code,
        true,
    );
    let labels = if hosting {
        vec![
            tr(p, "startRoom", ""),
            tr(p, "copyAddress", ""),
            tr(p, "refresh", ""),
        ]
    } else {
        vec![tr(p, "joinRoom", "")]
    };
    p.button_row(&labels);
    let start = p.button(
        "lan-connect",
        &tr(p, if hosting { "startRoom" } else { "joinRoom" }, ""),
        0,
    );
    if busy {
        p.disabled(false);
    }
    if hosting {
        if p.button(
            "lan-copy-listen",
            &tr(p, "copyAddress", "Copy connection address"),
            4,
        ) {
            copy(p, page::text(&v.address));
        }
        if busy {
            p.disabled(true);
        }
        if p.button("lan-refresh", &tr(p, "refresh", "Refresh addresses"), 4) {
            v.addresses = local_addresses();
        }
        if busy {
            p.disabled(false);
        }
    }
    if start && !busy && !v.start() {
        p.notice(&tr(p, "invalid", ""), true);
    }
}
pub unsafe extern "C" fn draw(
    p: *mut c_void,
    ui: *mut Ui,
    host: *const Host,
    services: *const Services,
) -> u32 {
    guard(0, || {
        let (Some(v), Some(page)) = (p.cast::<Instance>().as_mut(), Page::new(ui, host, services))
        else {
            return 0;
        };
        page.intro(&tr(&page, "description", ""));
        let busy = v.worker.is_some();
        let status = v.shared.lock().map(|s| s.status).unwrap_or(3);
        if busy {
            page.disabled(true);
        }
        page.row(1, 44.);
        let mode = page.segments(
            "lan-room-mode",
            &[
                tr(&page, "createRoom", "Create room"),
                tr(&page, "joinRoom", "Join room"),
            ],
            u32::from(v.host == 0),
        );
        if !busy && mode != u32::from(v.host == 0) {
            std::mem::swap(&mut v.address, &mut v.other_address);
            std::mem::swap(&mut v.code, &mut v.other_code);
            v.host = u32::from(mode == 0);
            page.unfocus();
        }
        if busy {
            page.disabled(false);
        }
        if busy || status == 3 {
            let key = match status {
                1 => "waiting",
                2 => "connected",
                3 => "failed",
                _ => "idle",
            };
            page.wrapped(&tr(&page, key, ""), 44.);
            if busy {
                page.row(1, 36.);
                if page.button(
                    "lan-disconnect",
                    &tr(&page, if status == 2 { "disconnect" } else { "cancel" }, ""),
                    1,
                ) {
                    v.stop();
                }
            }
        }
        connection(&page, v, busy);
        let ids: Vec<_> = v.frozen.iter().map(|peer| peer.id).collect();
        if ids.is_empty() {
            field(
                &page,
                "lan-friend",
                &tr(&page, "friend", ""),
                &mut v.friend,
                false,
            );
            clean(&mut v.friend);
        } else {
            page.section(&format!(
                "{} ({}/8)",
                tr(&page, "members", "Room members"),
                ids.len() + 1
            ));
            for id in ids {
                let name = v.names.entry(id).or_insert(v.friend);
                field(
                    &page,
                    &format!("lan-member-name-{id}"),
                    &format!("{} {id}", tr(&page, "memberName", "Friend display name")),
                    name,
                    false,
                );
                clean(name);
            }
        }
        if page.tree(
            "lan-security",
            &tr(&page, "security", "Connection privacy"),
            &mut v.details,
        ) {
            page.wrapped(&tr(&page, "privacy", ""), 112.);
            page.tree_end();
        }
        1
    })
}

fn clean(value: &mut [u8; 128]) {
    let text: String = page::text(value)
        .chars()
        .filter(|c| !c.is_control())
        .take(32)
        .collect();
    *value = bongo_plugin::text::buffer(&text);
}
