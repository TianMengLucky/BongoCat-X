//! Only advertise preferred public privacy addresses; never substitute stable IPs.
use std::net::Ipv6Addr;
fn public(ip: Ipv6Addr) -> bool {
    let s = ip.segments();
    s[0] & 0xe000 == 0x2000 && !(s[0] == 0x2001 && s[1] == 0x0db8)
}
#[cfg(windows)]
pub fn temporary() -> Vec<Ipv6Addr> {
    use windows_sys::Win32::{
        NetworkManagement::IpHelper::{
            FreeMibTable, GetUnicastIpAddressTable, MIB_UNICASTIPADDRESS_TABLE,
        },
        Networking::WinSock::{IpDadStatePreferred, IpSuffixOriginRandom, AF_INET6},
    };
    let mut table: *mut MIB_UNICASTIPADDRESS_TABLE = std::ptr::null_mut();
    // SAFETY: API owns the table until FreeMibTable; rows are read only within it.
    unsafe {
        if GetUnicastIpAddressTable(AF_INET6, &mut table) != 0 || table.is_null() {
            return Vec::new();
        }
        let rows =
            std::slice::from_raw_parts((*table).Table.as_ptr(), (*table).NumEntries as usize);
        let out = rows
            .iter()
            .filter(|r| {
                r.SuffixOrigin == IpSuffixOriginRandom
                    && r.DadState == IpDadStatePreferred
                    && r.PreferredLifetime > 0
                    && r.ValidLifetime < u32::MAX
            })
            .map(|r| Ipv6Addr::from(r.Address.Ipv6.sin6_addr.u.Byte))
            .filter(|ip| public(*ip))
            .collect();
        FreeMibTable(table.cast());
        out
    }
}
#[cfg(target_os = "linux")]
pub fn temporary() -> Vec<Ipv6Addr> {
    parse_linux(&std::fs::read_to_string("/proc/net/if_inet6").unwrap_or_default())
}
#[cfg(any(target_os = "linux", test))]
fn parse_linux(text: &str) -> Vec<Ipv6Addr> {
    text.lines()
        .filter_map(|line| {
            let parts: Vec<_> = line.split_whitespace().collect();
            if parts.len() != 6 {
                return None;
            }
            let flags = u32::from_str_radix(parts[4], 16).ok()?;
            // IFA_F_TEMPORARY, excluding tentative, DAD-failed and deprecated.
            if flags & 1 == 0 || flags & (0x08 | 0x20 | 0x40) != 0 {
                return None;
            }
            let ip = Ipv6Addr::from(u128::from_str_radix(parts[0], 16).ok()?);
            public(ip).then_some(ip)
        })
        .collect()
}
#[cfg(target_os = "macos")]
pub fn temporary() -> Vec<Ipv6Addr> {
    let Ok(interfaces) = if_addrs::get_if_addrs() else {
        return Vec::new();
    };
    let mut out = Vec::new();
    // SAFETY: ioctl uses a zeroed platform in6_ifreq and valid IPv6 socket.
    unsafe {
        let socket = libc::socket(libc::AF_INET6, libc::SOCK_DGRAM, 0);
        if socket < 0 {
            return out;
        }
        for interface in interfaces {
            let std::net::IpAddr::V6(ip) = interface.ip() else {
                continue;
            };
            if !public(ip) {
                continue;
            }
            let mut request: libc::in6_ifreq = std::mem::zeroed();
            for (to, from) in request.ifr_name.iter_mut().zip(interface.name.bytes()) {
                *to = from as libc::c_char;
            }
            request.ifr_ifru.ifru_addr.sin6_len = std::mem::size_of::<libc::sockaddr_in6>() as u8;
            request.ifr_ifru.ifru_addr.sin6_family = libc::AF_INET6 as u8;
            request.ifr_ifru.ifru_addr.sin6_addr.s6_addr = ip.octets();
            if libc::ioctl(socket, libc::SIOCGIFAFLAG_IN6, &mut request) == 0 {
                let flags = request.ifr_ifru.ifru_flags;
                if flags & libc::IN6_IFF_TEMPORARY != 0
                    && flags
                        & (libc::IN6_IFF_TENTATIVE
                            | libc::IN6_IFF_DEPRECATED
                            | libc::IN6_IFF_DUPLICATED)
                        == 0
                {
                    out.push(ip);
                }
            }
        }
        libc::close(socket);
    }
    out
}
#[cfg(not(any(windows, target_os = "linux", target_os = "macos")))]
pub fn temporary() -> Vec<Ipv6Addr> {
    Vec::new()
}
#[cfg(windows)]
pub fn stable() -> Vec<Ipv6Addr> {
    use windows_sys::Win32::{
        NetworkManagement::IpHelper::{
            FreeMibTable, GetUnicastIpAddressTable, MIB_UNICASTIPADDRESS_TABLE,
        },
        Networking::WinSock::{IpDadStatePreferred, IpSuffixOriginRandom, AF_INET6},
    };
    let mut table: *mut MIB_UNICASTIPADDRESS_TABLE = std::ptr::null_mut();
    // SAFETY: API owns the table until FreeMibTable; rows are read only within it.
    unsafe {
        if GetUnicastIpAddressTable(AF_INET6, &mut table) != 0 || table.is_null() {
            return Vec::new();
        }
        let rows =
            std::slice::from_raw_parts((*table).Table.as_ptr(), (*table).NumEntries as usize);
        let out = rows
            .iter()
            .filter(|r| {
                r.SuffixOrigin != IpSuffixOriginRandom
                    && r.DadState == IpDadStatePreferred
                    && r.PreferredLifetime > 0
            })
            .map(|r| Ipv6Addr::from(r.Address.Ipv6.sin6_addr.u.Byte))
            .filter(|ip| public(*ip))
            .collect();
        FreeMibTable(table.cast());
        out
    }
}
#[cfg(target_os = "linux")]
pub fn stable() -> Vec<Ipv6Addr> {
    parse_linux_stable(&std::fs::read_to_string("/proc/net/if_inet6").unwrap_or_default())
}
#[cfg(any(target_os = "linux", test))]
fn parse_linux_stable(text: &str) -> Vec<Ipv6Addr> {
    text.lines()
        .filter_map(|line| {
            let parts: Vec<_> = line.split_whitespace().collect();
            if parts.len() != 6 {
                return None;
            }
            let flags = u32::from_str_radix(parts[4], 16).ok()?;
            // Stable addresses: not temporary, and not tentative, DAD-failed or deprecated.
            if flags & 1 != 0 || flags & (0x08 | 0x20 | 0x40) != 0 {
                return None;
            }
            let ip = Ipv6Addr::from(u128::from_str_radix(parts[0], 16).ok()?);
            public(ip).then_some(ip)
        })
        .collect()
}
#[cfg(target_os = "macos")]
pub fn stable() -> Vec<Ipv6Addr> {
    let Ok(interfaces) = if_addrs::get_if_addrs() else {
        return Vec::new();
    };
    let mut out = Vec::new();
    // SAFETY: ioctl uses a zeroed platform in6_ifreq and valid IPv6 socket.
    unsafe {
        let socket = libc::socket(libc::AF_INET6, libc::SOCK_DGRAM, 0);
        if socket < 0 {
            return out;
        }
        for interface in interfaces {
            let std::net::IpAddr::V6(ip) = interface.ip() else {
                continue;
            };
            if !public(ip) {
                continue;
            }
            let mut request: libc::in6_ifreq = std::mem::zeroed();
            for (to, from) in request.ifr_name.iter_mut().zip(interface.name.bytes()) {
                *to = from as libc::c_char;
            }
            request.ifr_ifru.ifru_addr.sin6_len = std::mem::size_of::<libc::sockaddr_in6>() as u8;
            request.ifr_ifru.ifru_addr.sin6_family = libc::AF_INET6 as u8;
            request.ifr_ifru.ifru_addr.sin6_addr.s6_addr = ip.octets();
            if libc::ioctl(socket, libc::SIOCGIFAFLAG_IN6, &mut request) == 0 {
                let flags = request.ifr_ifru.ifru_flags;
                if flags & libc::IN6_IFF_TEMPORARY == 0
                    && flags
                        & (libc::IN6_IFF_TENTATIVE
                            | libc::IN6_IFF_DEPRECATED
                            | libc::IN6_IFF_DUPLICATED)
                        == 0
                {
                    out.push(ip);
                }
            }
        }
        libc::close(socket);
    }
    out
}
#[cfg(not(any(windows, target_os = "linux", target_os = "macos")))]
pub fn stable() -> Vec<Ipv6Addr> {
    Vec::new()
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn privacy_only() {
        let text = "24080000000000000000000000000001 02 40 00 01 eth0\n24080000000000000000000000000002 02 40 00 80 eth0\n24080000000000000000000000000003 02 40 00 21 eth0\nfe800000000000000000000000000001 02 40 20 01 eth0";
        assert_eq!(
            parse_linux(text),
            vec!["2408::1".parse::<Ipv6Addr>().unwrap()]
        );
        assert!(!public("fd00::1".parse().unwrap()));
    }
    #[test]
    fn stable_only() {
        let text = "24080000000000000000000000000001 02 40 00 00 eth0\n24080000000000000000000000000002 02 40 00 01 eth0";
        assert_eq!(
            parse_linux_stable(text),
            vec!["2408::1".parse::<Ipv6Addr>().unwrap()]
        );
    }
}
