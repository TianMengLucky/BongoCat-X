//! Noise owns key exchange, authenticated encryption and directional nonces.
use super::{archive, network::stopped};
use sha2::{Digest, Sha256};
use std::{
    io::{self, Read, Write},
    net::TcpStream,
    sync::atomic::AtomicBool,
    time::{Duration, Instant},
};
pub const MAX_PLAINTEXT: usize = 60 * 1024;
const MAX_FRAME: usize = 65535;
const PROTOCOL: &[u8] = b"BongoCat LAN encrypted room v3";
pub struct Channel {
    stream: TcpStream,
    transport: Option<snow::TransportState>,
    host: bool,
}
impl Channel {
    pub fn new(stream: TcpStream, host: bool) -> Self {
        Self {
            stream,
            transport: None,
            host,
        }
    }
    pub fn authenticate(&mut self, code: &str, cancel: &AtomicBool) -> io::Result<()> {
        if self.transport.is_some() || !(8..=64).contains(&code.len()) {
            return Err(archive::invalid());
        }
        let key: [u8; 32] = Sha256::digest([PROTOCOL, code.as_bytes()].concat()).into();
        let params = "Noise_NNpsk0_25519_ChaChaPoly_SHA256"
            .parse()
            .map_err(|_| archive::invalid())?;
        let builder = snow::Builder::new(params).psk(0, &key).prologue(PROTOCOL);
        let mut handshake = if self.host {
            builder.build_responder()
        } else {
            builder.build_initiator()
        }
        .map_err(|_| archive::invalid())?;
        let mut output = vec![0; MAX_FRAME];
        while !handshake.is_handshake_finished() {
            stopped(cancel)?;
            if handshake.is_my_turn() {
                let n = handshake
                    .write_message(PROTOCOL, &mut output)
                    .map_err(|_| archive::invalid())?;
                raw_send(&mut self.stream, &output[..n], cancel)?;
            } else {
                let data = raw_receive(&mut self.stream, MAX_FRAME, cancel)?;
                let n = handshake
                    .read_message(&data, &mut output)
                    .map_err(|_| archive::invalid())?;
                if &output[..n] != PROTOCOL {
                    return Err(archive::invalid());
                }
            }
        }
        self.transport = Some(
            handshake
                .into_transport_mode()
                .map_err(|_| archive::invalid())?,
        );
        // Encrypted confirmation proves both sides completed the handshake before model IO.
        send(self, PROTOCOL, cancel)?;
        if receive(self, PROTOCOL.len(), cancel)? != PROTOCOL {
            return Err(archive::invalid());
        }
        Ok(())
    }
}
fn transfer(s: &mut TcpStream, data: &mut [u8], send: bool, cancel: &AtomicBool) -> io::Result<()> {
    let deadline = Instant::now() + Duration::from_secs(30);
    let mut offset = 0;
    while offset < data.len() {
        stopped(cancel)?;
        if Instant::now() > deadline {
            return Err(io::ErrorKind::TimedOut.into());
        }
        let result = if send {
            s.write(&data[offset..])
        } else {
            s.read(&mut data[offset..])
        };
        match result {
            Ok(0) => return Err(io::ErrorKind::UnexpectedEof.into()),
            Ok(n) => offset += n,
            Err(e)
                if matches!(
                    e.kind(),
                    io::ErrorKind::WouldBlock
                        | io::ErrorKind::TimedOut
                        | io::ErrorKind::Interrupted
                ) => {}
            Err(e) => return Err(e),
        }
    }
    Ok(())
}

fn raw_send(s: &mut TcpStream, data: &[u8], cancel: &AtomicBool) -> io::Result<()> {
    if data.len() > MAX_FRAME {
        return Err(archive::invalid());
    }
    transfer(s, &mut (data.len() as u32).to_be_bytes(), true, cancel)?;
    transfer(s, &mut data.to_vec(), true, cancel)
}
fn raw_receive(s: &mut TcpStream, limit: usize, cancel: &AtomicBool) -> io::Result<Vec<u8>> {
    let mut size = [0; 4];
    transfer(s, &mut size, false, cancel)?;
    let size = u32::from_be_bytes(size) as usize;
    if size > limit || size > MAX_FRAME {
        return Err(archive::invalid());
    }
    let mut data = vec![0; size];
    transfer(s, &mut data, false, cancel)?;
    Ok(data)
}
pub fn send(s: &mut Channel, data: &[u8], cancel: &AtomicBool) -> io::Result<()> {
    if data.len() > MAX_PLAINTEXT {
        return Err(archive::invalid());
    }
    let transport = s.transport.as_mut().ok_or_else(archive::invalid)?;
    let mut encrypted = vec![0; data.len() + 16];
    let n = transport
        .write_message(data, &mut encrypted)
        .map_err(|_| archive::invalid())?;
    raw_send(&mut s.stream, &encrypted[..n], cancel)
}
pub fn receive(s: &mut Channel, limit: usize, cancel: &AtomicBool) -> io::Result<Vec<u8>> {
    let transport = s.transport.as_mut().ok_or_else(archive::invalid)?;
    let data = raw_receive(&mut s.stream, limit.min(MAX_PLAINTEXT) + 16, cancel)?;
    let mut plain = vec![0; data.len()];
    let n = transport
        .read_message(&data, &mut plain)
        .map_err(|_| archive::invalid())?;
    if n > limit {
        return Err(archive::invalid());
    }
    plain.truncate(n);
    Ok(plain)
}
#[cfg(test)]
mod tests {
    use super::*;
    fn transports() -> (snow::TransportState, snow::TransportState) {
        let key = [7; 32];
        let params: snow::params::NoiseParams =
            "Noise_NNpsk0_25519_ChaChaPoly_SHA256".parse().unwrap();
        let mut a = snow::Builder::new(params.clone())
            .psk(0, &key)
            .build_initiator()
            .unwrap();
        let mut b = snow::Builder::new(params)
            .psk(0, &key)
            .build_responder()
            .unwrap();
        let mut wire = [0; 256];
        let mut plain = [0; 256];
        let n = a.write_message(&[], &mut wire).unwrap();
        b.read_message(&wire[..n], &mut plain).unwrap();
        let n = b.write_message(&[], &mut wire).unwrap();
        a.read_message(&wire[..n], &mut plain).unwrap();
        (
            a.into_transport_mode().unwrap(),
            b.into_transport_mode().unwrap(),
        )
    }

    #[test]
    fn channel_never_sends_plaintext() {
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let stream = TcpStream::connect(listener.local_addr().unwrap()).unwrap();
        let (mut peer, _) = listener.accept().unwrap();
        let mut channel = Channel::new(stream, false);
        let cancel = AtomicBool::new(false);
        assert!(send(&mut channel, b"secret model", &cancel).is_err());
        let (a, mut b) = transports();
        channel.transport = Some(a);
        send(&mut channel, b"secret model", &cancel).unwrap();
        let data = raw_receive(&mut peer, MAX_FRAME, &cancel).unwrap();
        assert!(!data.windows(12).any(|s| s == b"secret model"));
        let mut output = [0; 64];
        let n = b.read_message(&data, &mut output).unwrap();
        assert_eq!(&output[..n], b"secret model");
        let n = b.write_message(b"pose", &mut output).unwrap();
        output[0] ^= 1;
        raw_send(&mut peer, &output[..n], &cancel).unwrap();
        assert!(receive(&mut channel, 64, &cancel).is_err());
    }
    #[test]
    fn encrypted_frames_reject_tamper_and_replay() {
        let (mut a, mut b) = transports();
        let secret = b"model resource and pose";
        let mut wire = [0; 256];
        let mut plain = [0; 256];
        let n = a.write_message(secret, &mut wire).unwrap();
        assert!(!wire[..n].windows(secret.len()).any(|p| p == secret));
        let mut bad = wire;
        bad[0] ^= 1;
        assert!(b.read_message(&bad[..n], &mut plain).is_err());
        let n_plain = b.read_message(&wire[..n], &mut plain).unwrap();
        assert_eq!(&plain[..n_plain], secret);
        assert!(b.read_message(&wire[..n], &mut plain).is_err());
    }
}
