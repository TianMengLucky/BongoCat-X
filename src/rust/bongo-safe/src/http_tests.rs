use super::*;
use std::io::{Read, Write};
use std::net::TcpListener;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::Instant;

fn callbacks() -> Callbacks {
    Callbacks {
        cancelled: None,
        progress: None,
        userdata: std::ptr::null_mut(),
    }
}

#[test]
fn accelerator_preserves_resources_and_only_routes_exact_github_hosts() {
    for url in [
        "https://api.github.com/repos/TianMengLucky/BongoCat-X/releases/latest",
        "https://github.com/TianMengLucky/BongoCat-X/releases/download/v2.1.2/app.zip",
        "https://avatars.githubusercontent.com/u/1?v=4&s=64",
    ] {
        assert_eq!(request_url(url, false).unwrap().as_str(), url);
        assert_eq!(
            request_url(url, true).unwrap().as_str(),
            format!("{PROXY_PREFIX}{url}")
        );
    }
    for url in [
        "https://github.com.example.net/file",
        "https://example.net/github.com/file",
        "https://gh-proxy.com/https://api.github.com/repos/a/b/releases/latest",
    ] {
        assert_eq!(request_url(url, true).unwrap().as_str(), url);
    }
    for url in [
        "http://github.com/a",
        "https://github.com@evil.net/a",
        "https://github.com:444/a",
        "https://github.com\\@evil.net/a",
        "https://github.com/a\nb",
        "file:///tmp/package",
    ] {
        assert!(request_url(url, true).is_err(), "{url}");
    }
}

fn server(response: &'static [u8], delay: Duration) -> (Url, std::thread::JoinHandle<()>) {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let url = Url::parse(&format!("http://{}/body", listener.local_addr().unwrap())).unwrap();
    let worker = std::thread::spawn(move || {
        let (mut stream, _) = listener.accept().unwrap();
        stream
            .set_read_timeout(Some(Duration::from_secs(2)))
            .unwrap();
        let mut request = [0u8; 2048];
        let _ = stream.read(&mut request);
        std::thread::sleep(delay);
        let _ = stream.write_all(response);
    });
    (url, worker)
}

fn local_client() -> BongoSafeHttpClient {
    // Transport tests bypass URL policy only for this loopback fixture.
    BongoSafeHttpClient {
        runtime: Some(Builder::new_current_thread().enable_all().build().unwrap()),
        client: Client::builder().no_proxy().build().unwrap(),
    }
}

#[test]
fn streaming_transport_handles_binary_and_rejects_chunked_overflow_and_truncation() {
    let cases: [(&[u8], u64, bool); 4] = [
        (b"HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\na\0b", 3, true),
        (b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n6\r\nabcdef\r\n0\r\n\r\n", 4, false),
        (b"HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: close\r\n\r\nabc", 6, false),
        (b"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", 4, false),
    ];
    let client = local_client();
    for (response, limit, success) in cases {
        let (url, worker) = server(response, Duration::ZERO);
        let mut output = Vec::new();
        let result = client.fetch_url(
            url,
            false,
            limit,
            Duration::from_secs(2),
            callbacks(),
            |bytes| {
                output.extend_from_slice(bytes);
                Ok(())
            },
        );
        worker.join().unwrap();
        assert_eq!(result.is_ok(), success);
        if success {
            assert_eq!(output, b"a\0b");
        } else if response.starts_with(b"HTTP/1.1 403") {
            assert_eq!(result.unwrap_err().1, "GitHub returned HTTP status 403");
            assert!(output.is_empty());
        }
    }
}

unsafe extern "C" fn cancel_after_three_polls(userdata: *mut c_void) -> bool {
    let polls = &*(userdata as *const AtomicUsize);
    polls.fetch_add(1, Ordering::Relaxed) >= 3
}

#[test]
fn cancellation_interrupts_waiting_for_response_headers() {
    let (url, worker) = server(
        b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n",
        Duration::from_millis(500),
    );
    let client = local_client();
    let polls = AtomicUsize::new(0);
    let start = Instant::now();
    let result = client.fetch_url(
        url,
        false,
        4,
        Duration::from_secs(10),
        Callbacks {
            cancelled: Some(cancel_after_three_polls),
            progress: None,
            userdata: (&polls as *const AtomicUsize).cast_mut().cast(),
        },
        |_| Ok(()),
    );
    assert_eq!(result.unwrap_err().0, CANCELLED);
    assert!(start.elapsed() < Duration::from_millis(450));
    worker.join().unwrap();
}
