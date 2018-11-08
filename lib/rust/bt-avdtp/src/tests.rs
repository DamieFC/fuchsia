// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![deny(warnings)]

use fuchsia_zircon::{self as zx, Status};
use futures::executor::block_on;
use futures::{future, StreamExt, TryStreamExt};
use std::result;

use crate::*;

// Helper functions

fn setup_peer() -> (Peer, zx::Socket) {
    let (remote, signaling) = zx::Socket::create(zx::SocketOpts::DATAGRAM).unwrap();

    let peer = Peer::new(signaling);
    assert!(peer.is_ok());
    (peer.unwrap(), remote)
}

fn setup_stream_test() -> (RequestStream, Peer, zx::Socket, fasync::Executor) {
    let exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let stream = peer.take_request_stream();
    (stream, peer, remote, exec)
}

fn next_request(stream: &mut RequestStream, exec: &mut fasync::Executor) -> Request {
    let mut fut = stream.next();
    let complete = exec.run_until_stalled(&mut fut);

    match complete {
        Poll::Ready(Some(Ok(r))) => r,
        _ => panic!("should have a request"),
    }
}

fn recv_remote(remote: &zx::Socket) -> result::Result<Vec<u8>, zx::Status> {
    let waiting = remote.outstanding_read_bytes();
    assert!(waiting.is_ok());
    let mut response: Vec<u8> = vec![0; waiting.unwrap()];
    let response_read = remote.read(response.as_mut_slice())?;
    assert_eq!(response.len(), response_read);
    Ok(response)
}

fn expect_remote_recv(expected: &[u8], remote: &zx::Socket) {
    let r = recv_remote(&remote);
    assert!(r.is_ok());
    let response = r.unwrap();
    if expected.len() != response.len() {
        panic!(
            "received wrong length\nexpected: {:?}\nreceived: {:?}",
            expected, response
        );
    }
    assert_eq!(expected, &response[0..expected.len()]);
}

fn stream_request_response(
    exec: &mut fasync::Executor, stream: &mut RequestStream, remote: &zx::Socket, cmd: &[u8],
    expect: &[u8],
) {
    assert!(remote.write(cmd).is_ok());

    let mut fut = stream.next();
    let complete = exec.run_until_stalled(&mut fut);

    // We shouldn't have received anything on the stream.
    assert!(complete.is_pending());

    // The peer should have responded with:
    // Response Reject message with the same TxLabel
    // with the MediaTransport code and BAD_MEDIA_TRANSPORT_FORMAT (0x23)
    expect_remote_recv(expect, &remote);
}

#[test]
fn closes_socket_when_dropped() {
    let mut _exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer_sock, signaling) = zx::Socket::create(zx::SocketOpts::DATAGRAM).unwrap();

    {
        let peer = Peer::new(signaling);
        assert!(peer.is_ok());
        let mut _stream = peer.unwrap().take_request_stream();
    }

    // Writing to the sock from the other end should fail.
    let write_res = peer_sock.write(&[0; 1]);
    assert!(write_res.is_err());
    assert_eq!(Status::PEER_CLOSED, write_res.err().unwrap());
}

#[test]
#[should_panic] // TODO: can't use catch_unwind here because of PeerInner?
fn can_only_take_stream_once() {
    let mut _exec = fasync::Executor::new().expect("failed to create an executor");
    let (_, signaling) = zx::Socket::create(zx::SocketOpts::DATAGRAM).unwrap();

    let p = Peer::new(signaling);
    assert!(p.is_ok());
    let peer = p.unwrap();
    let mut _stream = peer.take_request_stream();
    peer.take_request_stream();
}

// Generic Request tests

const CMD_DISCOVER: &'static [u8] = &[
    0x40, // TxLabel (4), Single Packet (0b00), Command (0b00)
    0x01, // RFA (0b00), Discover (0x01)
];

#[test]
fn closed_peer_ends_request_stream() {
    let (stream, _, _, _) = setup_stream_test();
    let collected = block_on(stream.collect::<Vec<Result<Request>>>());
    assert_eq!(0, collected.len());
}

#[test]
fn command_not_supported_response() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    // TxLabel 4, DelayReport, which is not implemented
    assert!(remote.write(&[0x40, 0x0D, 0x40, 0x00, 0x00]).is_ok());

    let mut fut = stream.next();
    let complete = exec.run_until_stalled(&mut fut);

    // We shouldn't have received anything on the future
    assert!(complete.is_pending());

    // The peer should have responded with a Response Reject message with the
    // same TxLabel with NOT_SUPPORTED_COMMAND
    expect_remote_recv(&[0x43, 0x0D, 0x19], &remote);
}

#[test]
fn requests_are_queued_if_they_arrive_early() {
    let (stream, _, remote, mut exec) = setup_stream_test();
    assert!(remote.write(&CMD_DISCOVER).is_ok());
    let mut collected = Vec::<Request>::new();

    let mut fut = stream.try_for_each(|r| {
        collected.push(r);
        future::ready(Ok(()))
    });

    let _complete = exec.run_until_stalled(&mut fut);

    assert_eq!(1, collected.len());
}

#[test]
fn responds_with_same_tx_id() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    assert!(remote.write(&CMD_DISCOVER).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::Discover { responder } => {
            let s = StreamInformation::new(
                0x0A, // SEID from CMD_DISCOVER
                false,
                MediaType::Video,
                EndpointType::Source,
            )
            .unwrap();
            responder.send(&[s])
        }
        _ => panic!("should have received a Discover"),
    };

    assert!(respond_res.is_ok());

    let response: &[u8] = &[
        // txlabel (same as CMD_DISCOVER, 4), Single (0b00), Response Accept (0b10)
        0x4 << 4 | 0x0 << 2 | 0x2,
        0x01,                 // Discover
        0x0A << 2 | 0x0 << 1, // SEID (0A), Not In Use (0b0)
        0x01 << 4 | 0x0 << 3, // Video (0x01), Source (0x00)
    ];

    expect_remote_recv(response, &remote);
}

#[test]
fn invalid_signal_id_responds_error() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    // This is TxLabel 4, Signal Id 0b110000, which is invalid.
    assert!(remote.write(&[0x40, 0x30]).is_ok());

    let mut fut = stream.next();
    let complete = exec.run_until_stalled(&mut fut);

    // We shouldn't have received anything on the future
    assert!(complete.is_pending());

    // The peer should have responded with a General Reject message with the
    // same TxLabel, and the same (invalid) signal identifier.
    expect_remote_recv(&[0x41, 0x30], &remote);
}

#[test]
fn exhaust_request_ids() {
    let mut exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let mut response_futures = Vec::new();
    // There are only 16 labels, so fill up the "outgoing requests pending" buffer
    for _ in 0..16 {
        let mut response_fut = Box::pinned(peer.discover());
        assert!(exec.run_until_stalled(&mut response_fut).is_pending());
        response_futures.push(response_fut);
    }

    let mut should_fail_fut = Box::pinned(peer.discover());
    let res = exec.run_until_stalled(&mut should_fail_fut);
    assert!(res.is_ready());

    if let Poll::Ready(x) = res {
        assert!(x.is_err());
    }

    // Finish some of them.
    for _ in 0..4 {
        let received = recv_remote(&remote).unwrap();
        // Last half of header must be Single (0b00) and Command (0b00)
        assert_eq!(0x00, received[0] & 0xF);
        assert_eq!(0x01, received[1]); // 0x01 = Discover

        let txlabel_raw = received[0] & 0xF0;
        let response: &[u8] = &[
            txlabel_raw | 0x0 << 2 | 0x2, // txlabel (same), Single (0b00), Response Accept (0b10)
            0x01,                         // Discover
            0x3E << 2 | 0x0 << 1,         // SEID (3E), Not In Use (0b0)
            0x00 << 4 | 0x1 << 3,         // Audio (0x00), Sink (0x01)
        ];
        assert!(remote.write(response).is_ok());
    }

    for idx in 0..4 {
        assert!(exec
            .run_until_stalled(&mut response_futures[idx])
            .is_ready());
    }

    // We should be able to make new requests now.
    let mut another_fut = Box::pinned(peer.discover());
    assert!(exec.run_until_stalled(&mut another_fut).is_pending());
}

#[test]
fn command_timeout() {
    let mut exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let mut response_fut = Box::pinned(peer.discover());
    assert!(exec.run_until_stalled(&mut response_fut).is_pending());

    let received = recv_remote(&remote).unwrap();
    // Last half of header must be Single (0b00) and Command (0b00)
    assert_eq!(0x00, received[0] & 0xF);
    assert_eq!(0x01, received[1]); // 0x01 = Discover

    // We should be able to timeout the discover.
    let complete = exec.run_until_stalled(&mut response_fut);
    assert!(complete.is_pending());

    exec.wake_next_timer();
    assert_eq!(
        Poll::Ready(Err(Error::Timeout)),
        exec.run_until_stalled(&mut response_fut)
    );

    // We should be able to make a new request now.
    let mut another_fut = Box::pinned(peer.discover());
    assert!(exec.run_until_stalled(&mut another_fut).is_pending());

    // Responding to the first command after it timed out doesn't complete the new command
    let txlabel_raw = received[0] & 0xF0;
    let response: &[u8] = &[
        txlabel_raw | 0x0 << 2 | 0x2, // txlabel (same), Single (0b00), Response Accept (0b10)
        0x01,                         // Discover
        0x3E << 2 | 0x0 << 1,         // SEID (3E), Not In Use (0b0)
        0x00 << 4 | 0x1 << 3,         // Audio (0x00), Sink (0x01)
    ];

    assert!(remote.write(response).is_ok());

    assert!(exec.run_until_stalled(&mut another_fut).is_pending());
}

// Discovery tests

#[test]
fn discover_invalid_length() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    // TxLabel 4, Discover, which doesn't have a payload, but we're including one.
    assert!(remote.write(&[0x40, 0x01, 0x40]).is_ok());

    let mut fut = stream.next();
    let complete = exec.run_until_stalled(&mut fut);

    // We shouldn't have received anything on the future
    assert!(complete.is_pending());

    // The peer should have responded with a Response Reject message with the
    // same TxLabel with BAD_LENGTH
    expect_remote_recv(&[0x43, 0x01, 0x11], &remote);
}

#[test]
fn discover_event_responder_send_works() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    assert!(remote.write(&CMD_DISCOVER).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::Discover { responder } => {
            let s = StreamInformation::new(0x0A, false, MediaType::Video, EndpointType::Source);
            assert!(s.is_ok());
            responder.send(&[s.unwrap()])
        }
        _ => panic!("should have received a Discover"),
    };

    assert!(respond_res.is_ok());

    expect_remote_recv(&[0x42, 0x01, 0x0A << 2, 0x01 << 4], &remote);
}

#[test]
fn discover_event_responder_reject_works() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    assert!(remote.write(&CMD_DISCOVER).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::Discover { responder } => responder.reject(ErrorCode::BadState),
        _ => panic!("should have received a Discover"),
    };

    assert!(respond_res.is_ok());

    let discover_rsp = &[
        0x43, // TxLabel (4),  RemoteRejected (3)
        0x01, // Discover
        0x31, // BAD_STATE
    ];
    expect_remote_recv(discover_rsp, &remote);
}

#[test]
fn discover_command_works() {
    let mut exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let mut response_fut = Box::pinned(peer.discover());
    assert!(exec.run_until_stalled(&mut response_fut).is_pending());

    let received = recv_remote(&remote).unwrap();
    // Last half of header must be Single (0b00) and Command (0b00)
    assert_eq!(0x00, received[0] & 0xF);
    assert_eq!(0x01, received[1]); // 0x01 = Discover

    let txlabel_raw = received[0] & 0xF0;

    let response: &[u8] = &[
        txlabel_raw << 4 | 0x0 << 2 | 0x2, // txlabel (same), Single (0b00), Response Accept (0b10)
        0x01,                              // Discover
        0x3E << 2 | 0x0 << 1,              // SEID (3E), Not In Use (0b0)
        0x00 << 4 | 0x1 << 3,              // Audio (0x00), Sink (0x01)
        0x01 << 2 | 0x1 << 1,              // SEID (1), In Use (0b1)
        0x01 << 4 | 0x0 << 3,              // Video (0x01), Source (0x01)
        0x17 << 2 | 0x0 << 1,              // SEID (17), Not In Use (0b0)
        0x02 << 4 | 0x1 << 3,              // Multimedia (0x02), Sink (0x01)
    ];
    assert!(remote.write(response).is_ok());

    let complete = exec.run_until_stalled(&mut response_fut);

    let endpoints = match complete {
        Poll::Ready(Ok(response)) => response,
        x => panic!("Should have a ready Ok response: {:?}", x),
    };

    assert_eq!(3, endpoints.len());
    let e1 = StreamInformation::new(0x3E, false, MediaType::Audio, EndpointType::Sink).unwrap();
    assert_eq!(e1, endpoints[0]);
    let e2 = StreamInformation::new(0x01, true, MediaType::Video, EndpointType::Source).unwrap();
    assert_eq!(e2, endpoints[1]);
    let e3 =
        StreamInformation::new(0x17, false, MediaType::Multimedia, EndpointType::Sink).unwrap();
    assert_eq!(e3, endpoints[2]);
}

#[test]
fn discover_command_rejected() {
    let mut exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let mut response_fut = Box::pinned(peer.discover());
    assert!(exec.run_until_stalled(&mut response_fut).is_pending());

    let received = recv_remote(&remote).unwrap();
    // Last half of header must be Single (0b00) and Command (0b00)
    assert_eq!(0x00, received[0] & 0xF);
    assert_eq!(0x01, received[1]); // 0x01 = Discover

    let txlabel_raw = received[0] & 0xF0;

    let response: &[u8] = &[
        txlabel_raw | 0x0 << 2 | 0x3, // txlabel (same), Single (0b00), Response Reject (0b11)
        0x01,                         // Discover
        0x31,                         // BAD_STATE
    ];

    assert!(remote.write(response).is_ok());

    let complete = exec.run_until_stalled(&mut response_fut);

    let error = match complete {
        Poll::Ready(Err(response)) => response,
        x => panic!("Should have a ready Error response: {:?}", x),
    };

    assert_eq!(Error::RemoteRejected(0x31), error);
}

// Get(All)Capabilities tests

const CMD_GET_CAPABILITIES: &'static [u8] = &[
    0x40, // TxLabel (4), Single Packet (0b00), Command (0b00)
    0x02, // RFA (0b00), Get Capabilities (0x02)
    0x30, // SEID 12 (0b001100)
];

#[test]
fn get_capabilities_invalid_length() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    // TxLabel 4, GetCapabilities, which needs a SEID.
    assert!(remote.write(&[0x40, 0x02]).is_ok());

    let mut fut = stream.next();
    let complete = exec.run_until_stalled(&mut fut);

    // We shouldn't have received any request.
    assert!(complete.is_pending());

    // The peer should have responded with a Response Reject message with the
    // same TxLabel with BAD_LENGTH
    expect_remote_recv(&[0x43, 0x02, 0x11], &remote);

    // TxLabel 4, GetCapabilities, with more than we expected.
    assert!(remote.write(&[0x40, 0x02, 0x01, 0x10]).is_ok());

    let mut fut = stream.next();
    let complete = exec.run_until_stalled(&mut fut);

    // We shouldn't have received any request.
    assert!(complete.is_pending());

    // The peer should have responded with a Response Reject message with the
    // same TxLabel with BAD_LENGTH
    expect_remote_recv(&[0x43, 0x02, 0x11], &remote);
}

#[test]
fn get_capabilities_event_responder_send_works() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    assert!(remote.write(&CMD_GET_CAPABILITIES).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::GetCapabilities {
            responder,
            stream_id,
        } => {
            assert_eq!(StreamEndpointId::try_from(12).unwrap(), stream_id);
            let capabilities = &[
                ServiceCapability::MediaTransport,
                ServiceCapability::Reporting,
                ServiceCapability::Recovery {
                    recovery_type: 0x01,
                    max_recovery_window_size: 10,
                    max_number_media_packets: 255,
                },
                ServiceCapability::MediaCodec {
                    media_type: MediaType::Audio,
                    codec_type: MediaCodecType::new(0x40),
                    codec_extra: vec![0xDE, 0xAD, 0xBE, 0xEF],
                },
                ServiceCapability::ContentProtection {
                    protection_type: ContentProtectionType::DigitalTransmissionContentProtection,
                    extra: vec![0xF0, 0x0D],
                },
                ServiceCapability::DelayReporting,
            ];
            responder.send(capabilities)
        }
        _ => panic!("should have received a GetCapabilities"),
    };

    assert!(respond_res.is_ok());

    // Should receive the encoded version of the MediaTransport
    #[rustfmt::skip]
    let get_capabilities_rsp = &[
        0x42, // TxLabel (4) + ResponseAccept (0x02)
        0x02, // GetCapabilities Signal
        // MediaTransport (Length of Service Capability = 0)
        0x01, 0x00,
        // Reporting (LOSC = 0)
        0x02, 0x00,
        // Recovery (LOSC = 3), Type (0x01), Window size (10), Number Media Packets (255)
        0x03, 0x03, 0x01, 0x0A, 0xFF,
        // Media Codec (LOSC = 2 + 4), Media Type Audio (0x00), Codec type (0x40), Codec speciic
        // as above
        0x07, 0x06, 0x00, 0x40, 0xDE, 0xAD, 0xBE, 0xEF,
        // Content Protection (LOSC = 2 + 2), DTCP (0x01, 0x00), CP Specific as above
        0x04, 0x04, 0x01, 0x00, 0xF0, 0x0D,
        /* NOTE: DelayReporting should NOT be included here
         * (GetCapabilities response cannot return DelayReporting) */
    ];
    expect_remote_recv(get_capabilities_rsp, &remote);
}

#[test]
fn get_capabilities_responder_reject_works() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    assert!(remote.write(&CMD_GET_CAPABILITIES).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::GetCapabilities {
            responder,
            stream_id,
        } => {
            assert_eq!(StreamEndpointId::try_from(12).unwrap(), stream_id);
            responder.reject(ErrorCode::BadAcpSeid)
        }
        _ => panic!("should have received a GetAllCapabilities"),
    };

    assert!(respond_res.is_ok());

    let get_capabilities_rsp = &[
        0x43, // TxLabel (4),  RemoteRejected (3)
        0x02, // Get Capabilities
        0x12, // BAD_ACP_SEID
    ];
    expect_remote_recv(get_capabilities_rsp, &remote);
}

#[test]
fn get_capabilities_command_works() {
    let mut exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let seid = &StreamEndpointId::try_from(1).unwrap();
    let mut response_fut = Box::pinned(peer.get_capabilities(&seid));
    assert!(exec.run_until_stalled(&mut response_fut).is_pending());

    let received = recv_remote(&remote).unwrap();
    // Last half of header must be Single (0b00) and Command (0b00)
    assert_eq!(0x00, received[0] & 0xF);
    assert_eq!(0x02, received[1]); // 0x02 = GetCapabilities
    assert_eq!(0x01 << 2, received[2]); // SEID (0x01) , RFA

    let txlabel_raw = received[0] & 0xF0;

    #[rustfmt::skip]
    let response: &[u8] = &[
        txlabel_raw << 4 | 0x0 << 2 | 0x2, // txlabel (same), Single (0b00), Response Accept (0b10)
        0x02,                              // Get Capabilities
        // MediaTransport (Length of Service Capability = 0)
        0x01, 0x00,
        // Recovery (LOSC = 3), Type (0x01), Window size (2), Number Media Packets (5)
        0x03, 0x03, 0x01, 0x02, 0x05,
        // Media Codec (LOSC = 2 + 2), Video (0x1), Codec type (0x20), Codec speific (0xB0DE)
        0x07, 0x04, 0x10, 0x20, 0xB0, 0xDE,
        // Content Protection (LOSC = 2 + 1), SCMS (0x02, 0x00), CP Specific (0x7A)
        0x04, 0x03, 0x02, 0x00, 0x7A,
    ];
    assert!(remote.write(response).is_ok());

    let complete = exec.run_until_stalled(&mut response_fut);

    let capabilities = match complete {
        Poll::Ready(Ok(response)) => response,
        x => panic!("Should have a ready Ok response: {:?}", x),
    };

    assert_eq!(4, capabilities.len());
    let c1 = ServiceCapability::MediaTransport;
    assert_eq!(c1, capabilities[0]);
    let c2 = ServiceCapability::Recovery {
        recovery_type: 1,
        max_recovery_window_size: 2,
        max_number_media_packets: 5,
    };
    assert_eq!(c2, capabilities[1]);
    let c3 = ServiceCapability::MediaCodec {
        media_type: MediaType::Video,
        codec_type: MediaCodecType::new(0x20),
        codec_extra: vec![0xb0, 0xde],
    };
    assert_eq!(c3, capabilities[2]);
    let c4 = ServiceCapability::ContentProtection {
        protection_type: ContentProtectionType::SerialCopyManagementSystem,
        extra: vec![0x7a],
    };
    assert_eq!(c4, capabilities[3]);
}

#[test]
fn get_capabilities_reject_command() {
    let mut exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let seid = &StreamEndpointId::try_from(1).unwrap();
    let mut response_fut = Box::pinned(peer.get_capabilities(&seid));
    assert!(exec.run_until_stalled(&mut response_fut).is_pending());

    let received = recv_remote(&remote).unwrap();
    // Last half of header must be Single (0b00) and Command (0b00)
    assert_eq!(0x00, received[0] & 0xF);
    assert_eq!(0x02, received[1]); // 0x02 = GetCapabilities
    assert_eq!(0x01 << 2, received[2]); // SEID (0x01), RFA

    let txlabel_raw = received[0] & 0xF0;

    let response: &[u8] = &[
        txlabel_raw | 0x0 << 2 | 0x3, // txlabel (same), Single (0b00), Response Reject (0b11)
        0x02,                         // Get Capabiliities
        0x12,                         // BAD_ACP_SEID
    ];

    assert!(remote.write(response).is_ok());

    let complete = exec.run_until_stalled(&mut response_fut);

    let error = match complete {
        Poll::Ready(Err(response)) => response,
        x => panic!("Should have a ready Error response: {:?}", x),
    };

    assert_eq!(Error::RemoteRejected(0x12), error);
}

//  Get All Capabilities

const CMD_GET_ALL_CAPABILITIES: &'static [u8] = &[
    0x40, // TxLabel (4), Single Packet (0b00), Command (0b00)
    0x0C, // RFA (0b00), Get All Capabilities (0x0C)
    0x30, // SEID 12 (0b001100)
];

#[test]
fn get_all_capabilities_invalid_length() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    // TxLabel 4, GetAllCapabilities, which needs a SEID.
    assert!(remote.write(&[0x40, 0x0C]).is_ok());

    let mut fut = stream.next();
    let complete = exec.run_until_stalled(&mut fut);

    // We shouldn't have received any request.
    assert!(complete.is_pending());

    // The peer should have responded with a Response Reject message with the
    // same TxLabel with BAD_LENGTH
    expect_remote_recv(&[0x43, 0x0C, 0x11], &remote);

    // TxLabel 4, GetCapabilities, with more than we expected.
    assert!(remote.write(&[0x40, 0x0C, 0x01, 0x10]).is_ok());

    let mut fut = stream.next();
    let complete = exec.run_until_stalled(&mut fut);

    // We shouldn't have received any request.
    assert!(complete.is_pending());

    // The peer should have responded with a Response Reject message with the
    // same TxLabel with BAD_LENGTH
    expect_remote_recv(&[0x43, 0x0C, 0x11], &remote);
}

#[test]
fn get_all_capabilities_event_responder_send_works() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    assert!(remote.write(&CMD_GET_ALL_CAPABILITIES).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::GetAllCapabilities {
            responder,
            stream_id,
        } => {
            assert_eq!(StreamEndpointId::try_from(12).unwrap(), stream_id);
            let capabilities = &[
                ServiceCapability::MediaTransport,
                ServiceCapability::Reporting,
                ServiceCapability::Recovery {
                    recovery_type: 0x01,
                    max_recovery_window_size: 10,
                    max_number_media_packets: 255,
                },
                ServiceCapability::MediaCodec {
                    media_type: MediaType::Audio,
                    codec_type: MediaCodecType::new(0x40),
                    codec_extra: vec![0xDE, 0xAD, 0xBE, 0xEF],
                },
                ServiceCapability::ContentProtection {
                    protection_type: ContentProtectionType::DigitalTransmissionContentProtection,
                    extra: vec![0xF0, 0x0D],
                },
                ServiceCapability::DelayReporting,
            ];
            responder.send(capabilities)
        }
        _ => panic!("should have received a GetAllCapabilities"),
    };

    assert!(respond_res.is_ok());

    // Should receive the encoded version of the MediaTransport
    #[rustfmt::skip]
    let get_capabilities_rsp = &[
        0x42, // TxLabel (4) + ResponseAccept (0x02)
        0x0C, // GetAllCapabilities Signal
        // MediaTransport (Length of Service Capability = 0)
        0x01, 0x00,
        // Reporting (LOSC = 0)
        0x02, 0x00,
        // Recovery (LOSC = 3), Type (0x01), Window size (10), Number Media Packets (255)
        0x03, 0x03, 0x01, 0x0A, 0xFF,
        // Media Codec (LOSC = 2 + 4), Media Type Audio (0x00), Codec type (0x40), Codec speciic
        // as above
        0x07, 0x06, 0x00, 0x40, 0xDE, 0xAD, 0xBE, 0xEF,
        // Content Protection (LOSC = 2 + 2), DTCP (0x01, 0x00), CP Specific as above
        0x04, 0x04, 0x01, 0x00, 0xF0, 0x0D,
        // DelayReporting (LOSC = 0)
        0x08, 0x00,
    ];
    expect_remote_recv(get_capabilities_rsp, &remote);
}

#[test]
fn get_all_capabilities_responder_reject_works() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    assert!(remote.write(&CMD_GET_ALL_CAPABILITIES).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::GetAllCapabilities {
            responder,
            stream_id,
        } => {
            assert_eq!(StreamEndpointId::try_from(12).unwrap(), stream_id);
            responder.reject(ErrorCode::BadAcpSeid)
        }
        _ => panic!("should have received a GetAllCapabilities"),
    };

    assert!(respond_res.is_ok());

    let get_all_capabilities_rsp = &[
        0x43, // TxLabel (4),  RemoteRejected (3)
        0x0C, // Get All Capabilities
        0x12, // BAD_ACP_SEID
    ];
    expect_remote_recv(get_all_capabilities_rsp, &remote);
}

#[test]
fn get_all_capabilities_command_works() {
    let mut exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let seid = StreamEndpointId::try_from(1).unwrap();
    let mut response_fut = Box::pinned(peer.get_all_capabilities(&seid));
    assert!(exec.run_until_stalled(&mut response_fut).is_pending());

    let received = recv_remote(&remote).unwrap();
    // Last half of header must be Single (0b00) and Command (0b00)
    assert_eq!(0x00, received[0] & 0xF);
    assert_eq!(0x0C, received[1]); // 0x0C = GetAllCapabilities
    assert_eq!(0x01 << 2, received[2]); // SEID (0x01) , RFA

    let txlabel_raw = received[0] & 0xF0;

    #[rustfmt::skip]
    let response: &[u8] = &[
        txlabel_raw << 4 | 0x0 << 2 | 0x2, // txlabel (same), Single (0b00), Response Accept (0b10)
        0x0C,                              // Get All Capabilities
        // MediaTransport (Length of Service Capability = 0)
        0x01, 0x00,
        // Reporting (LOSC = 0)
        0x02, 0x00,
        // Media Codec (LOSC = 2 + 2), Video (0x1), Codec type (0x20), Codec speific (0xB0DE)
        0x07, 0x04, 0x10, 0x20, 0xB0, 0xDE,
        // Content Protection (LOSC = 2 + 1), SCMS (0x02, 0x00), CP Specific (0x7A)
        0x04, 0x03, 0x02, 0x00, 0x7A,
        // Delay Reporting
        0x08, 0x00,
    ];
    assert!(remote.write(response).is_ok());

    let complete = exec.run_until_stalled(&mut response_fut);

    let capabilities = match complete {
        Poll::Ready(Ok(response)) => response,
        x => panic!("Should have a ready Ok response: {:?}", x),
    };

    assert_eq!(5, capabilities.len());
    let c1 = ServiceCapability::MediaTransport;
    assert_eq!(c1, capabilities[0]);
    let c2 = ServiceCapability::Reporting;
    assert_eq!(c2, capabilities[1]);
    let c3 = ServiceCapability::MediaCodec {
        media_type: MediaType::Video,
        codec_type: MediaCodecType::new(0x20),
        codec_extra: vec![0xb0, 0xde],
    };
    assert_eq!(c3, capabilities[2]);
    let c4 = ServiceCapability::ContentProtection {
        protection_type: ContentProtectionType::SerialCopyManagementSystem,
        extra: vec![0x7a],
    };
    assert_eq!(c4, capabilities[3]);
    let c5 = ServiceCapability::DelayReporting;
    assert_eq!(c5, capabilities[4]);
}

#[test]
fn get_all_capabilities_reject_command() {
    let mut exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let seid = StreamEndpointId::try_from(1).unwrap();
    let mut response_fut = Box::pinned(peer.get_all_capabilities(&seid));
    assert!(exec.run_until_stalled(&mut response_fut).is_pending());

    let received = recv_remote(&remote).unwrap();
    // Last half of header must be Single (0b00) and Command (0b00)
    assert_eq!(0x00, received[0] & 0xF);
    assert_eq!(0x0C, received[1]); // 0x02 = GetAllCapabilities
    assert_eq!(0x01 << 2, received[2]); // SEID (0x01), RFA

    let txlabel_raw = received[0] & 0xF0;

    let response: &[u8] = &[
        txlabel_raw | 0x0 << 2 | 0x3, // txlabel (same), Single (0b00), Response Reject (0b11)
        0x0C,                         // Get Capabiliities
        0x12,                         // BAD_ACP_SEID
    ];

    assert!(remote.write(response).is_ok());

    let complete = exec.run_until_stalled(&mut response_fut);

    let error = match complete {
        Poll::Ready(Err(response)) => response,
        x => panic!("Should have a ready Error response: {:?}", x),
    };

    assert_eq!(Error::RemoteRejected(0x12), error);
}

macro_rules! incoming_cmd_length_fail_test {
    ($test_name:ident, $signal_value:expr, $length:expr) => {
        #[test]
        fn $test_name() {
            let (mut stream, _, remote, mut exec) = setup_stream_test();

            // TxLabel 4, signal value, no params
            let mut incoming_cmd = vec![0x40, $signal_value];

            // Add $length bytes
            incoming_cmd.extend_from_slice(&[0; $length]);

            // Send the command
            assert!(remote.write(&incoming_cmd).is_ok());

            let mut fut = stream.next();
            let complete = exec.run_until_stalled(&mut fut);

            // We shouldn't have received any request.
            assert!(complete.is_pending());

            // The peer should have responded with a Response Reject message with the
            // same TxLabel with BAD_LENGTH
            expect_remote_recv(&[0x43, $signal_value, 0x11], &remote);
        }
    };
}

macro_rules! seid_command_test {
    ($test_name:ident, $signal_value:expr, $peer_function:ident) => {
        #[test]
        fn $test_name() {
            let mut exec = fasync::Executor::new().expect("failed to create an executor");
            let (peer, remote) = setup_peer();
            let seid = StreamEndpointId::try_from(1).unwrap();
            let mut response_fut = Box::pinned(peer.$peer_function(&seid));
            assert!(exec.run_until_stalled(&mut response_fut).is_pending());

            let received = recv_remote(&remote).unwrap();
            // Last half of header must be Single (0b00) and Command (0b00)
            assert_eq!(0x00, received[0] & 0xF);
            assert_eq!($signal_value, received[1]); // $signal_value = $request_variant
            assert_eq!(0x01 << 2, received[2]); // SEID (0x01) , RFA

            let txlabel_raw = received[0] & 0xF0;

            let response: &[u8] = &[
                // txlabel (same), Single (0b00), Response Accept (0b10)
                txlabel_raw << 4 | 0x0 << 2 | 0x2,
                $signal_value, // $request_variant
            ];
            assert!(remote.write(response).is_ok());

            let complete = exec.run_until_stalled(&mut response_fut);

            assert_eq!(Poll::Ready(Ok(())), complete);
        }
    };
}

macro_rules! seids_command_test {
    ($test_name:ident, $signal_value:expr, $peer_function:ident) => {
        #[test]
        fn $test_name() {
            let mut exec = fasync::Executor::new().expect("failed to create an executor");
            let (peer, remote) = setup_peer();
            let seid1 = StreamEndpointId::try_from(1).unwrap();
            let seid2 = StreamEndpointId::try_from(16).unwrap();
            let seids = [seid1, seid2];
            let mut response_fut = Box::pinned(peer.$peer_function(&seids));
            assert!(exec.run_until_stalled(&mut response_fut).is_pending());

            let received = recv_remote(&remote).unwrap();
            // Last half of header must be Single (0b00) and Command (0b00)
            assert_eq!(0x00, received[0] & 0xF);
            assert_eq!($signal_value, received[1]); // $signal_value = $request_variant
            assert_eq!(0x01 << 2, received[2]); // SEID (0x01) , RFA
            assert_eq!(0x10 << 2, received[3]); // SEID (0x10) , RFA

            let txlabel_raw = received[0] & 0xF0;

            let response: &[u8] = &[
                // txlabel (same), Single (0b00), Response Accept (0b10)
                txlabel_raw << 4 | 0x0 << 2 | 0x2,
                $signal_value, // Signal value
            ];
            assert!(remote.write(response).is_ok());

            let complete = exec.run_until_stalled(&mut response_fut);

            assert_eq!(Poll::Ready(Ok(())), complete);
        }
    };
}

macro_rules! seid_command_reject_test {
    ($test_name:ident, $signal_value: expr, $peer_function:ident) => {
        #[test]
        fn $test_name() {
            let mut exec = fasync::Executor::new().expect("failed to create an executor");
            let (peer, remote) = setup_peer();
            let seid = StreamEndpointId::try_from(1).unwrap();
            let mut response_fut = Box::pinned(peer.$peer_function(&seid));
            assert!(exec.run_until_stalled(&mut response_fut).is_pending());

            let received = recv_remote(&remote).unwrap();
            // Last half of header must be Single (0b00) and Command (0b00)
            assert_eq!(0x00, received[0] & 0xF);
            assert_eq!($signal_value, received[1]); // 0x02 = GetAllCapabilities
            assert_eq!(0x01 << 2, received[2]); // SEID (0x01), RFA

            let txlabel_raw = received[0] & 0xF0;

            let response: &[u8] = &[
                // txlabel (same), Single (0b00), Response Reject (0b11)
                txlabel_raw | 0x0 << 2 | 0x3,
                $signal_value, // $request_variant
                0x12,          // BAD_ACP_SEID
            ];

            assert!(remote.write(response).is_ok());

            let complete = exec.run_until_stalled(&mut response_fut);

            let error = match complete {
                Poll::Ready(Err(response)) => response,
                x => panic!("Should have a ready Error response: {:?}", x),
            };

            assert_eq!(Error::RemoteRejected(0x12), error);
        }
    };
}

macro_rules! seids_command_reject_test {
    ($test_name:ident, $signal_value: expr, $peer_function:ident) => {
        #[test]
        fn $test_name() {
            let mut exec = fasync::Executor::new().expect("failed to create an executor");
            let (peer, remote) = setup_peer();
            let seid1 = StreamEndpointId::try_from(1).unwrap();
            let seid2 = StreamEndpointId::try_from(16).unwrap();
            let seids = [seid1, seid2];
            let mut response_fut = Box::pinned(peer.$peer_function(&seids));
            assert!(exec.run_until_stalled(&mut response_fut).is_pending());

            let received = recv_remote(&remote).unwrap();
            // Last half of header must be Single (0b00) and Command (0b00)
            assert_eq!(0x00, received[0] & 0xF);
            assert_eq!($signal_value, received[1]); // 0x02 = GetAllCapabilities
            assert_eq!(0x01 << 2, received[2]); // SEID (0x01), RFA
            assert_eq!(0x10 << 2, received[3]); // SEID (0x10), RFA

            let txlabel_raw = received[0] & 0xF0;

            let response: &[u8] = &[
                // txlabel (same), Single (0b00), Response Reject (0b11)
                txlabel_raw | 0x0 << 2 | 0x3,
                $signal_value, // $request_variant
                0x10 << 2,     // SEID 16, RFA
                0x12,          // BAD_ACP_SEID
            ];

            assert!(remote.write(response).is_ok());

            let complete = exec.run_until_stalled(&mut response_fut);

            let error = match complete {
                Poll::Ready(Err(response)) => response,
                x => panic!("Should have a ready Error response: {:?}", x),
            };

            assert_eq!(Error::RemoteStreamRejected(0x10, 0x12), error);
        }
    };
}
macro_rules! seid_event_responder_send_test {
    ($test_name:ident, $variant:ident, $signal_value:expr, $command_var:ident) => {
        #[test]
        fn $test_name() {
            let (mut stream, _, remote, mut exec) = setup_stream_test();

            assert!(remote.write(&$command_var).is_ok());

            let respond_res = match next_request(&mut stream, &mut exec) {
                Request::$variant {
                    responder,
                    stream_id,
                } => {
                    assert_eq!(StreamEndpointId::try_from(12).unwrap(), stream_id);
                    responder.send()
                }
                _ => panic!("received the wrong request"),
            };

            assert!(respond_res.is_ok());

            // Should receive the response.
            let ok_rsp = &[
                0x42,          // TxLabel (4) + ResponseAccept (0x02)
                $signal_value, // $variant Signal
            ];
            expect_remote_recv(ok_rsp, &remote);
        }
    };
}

macro_rules! seids_event_responder_send_test {
    ($test_name:ident, $variant:ident, $signal_value:expr, $command_var:ident) => {
        #[test]
        fn $test_name() {
            let (mut stream, _, remote, mut exec) = setup_stream_test();

            assert!(remote.write(&$command_var).is_ok());

            let respond_res = match next_request(&mut stream, &mut exec) {
                Request::$variant {
                    responder,
                    stream_ids,
                } => {
                    assert_eq!(
                        &StreamEndpointId::try_from(12).unwrap(),
                        stream_ids.get(0).unwrap()
                    );
                    responder.send()
                }
                _ => panic!("received the wrong request"),
            };

            assert!(respond_res.is_ok());

            // Should receive the response.
            let ok_rsp = &[
                0x42,          // TxLabel (4) + ResponseAccept (0x02)
                $signal_value, // $variant Signal
            ];
            expect_remote_recv(ok_rsp, &remote);
        }
    };
}

macro_rules! seid_event_responder_reject_test {
    ($test_name:ident, $variant:ident, $signal_value:expr, $command_var:ident) => {
        #[test]
        fn $test_name() {
            let (mut stream, _, remote, mut exec) = setup_stream_test();

            assert!(remote.write(&$command_var).is_ok());

            let respond_res = match next_request(&mut stream, &mut exec) {
                Request::$variant {
                    responder,
                    stream_id,
                } => {
                    assert_eq!(StreamEndpointId::try_from(12).unwrap(), stream_id);
                    responder.reject(ErrorCode::BadAcpSeid)
                }
                _ => panic!("received the wrong request"),
            };

            assert!(respond_res.is_ok());

            let rejected_rsp = &[
                0x43,          // TxLabel (4),  RemoteRejected (3)
                $signal_value, // $variant
                0x12,          // BAD_ACP_SEID
            ];
            expect_remote_recv(rejected_rsp, &remote);
        }
    };
}

macro_rules! stream_event_responder_reject_test {
    ($test_name:ident, $variant:ident, $signal_value:expr, $command_var:ident) => {
        #[test]
        fn $test_name() {
            let (mut stream, _, remote, mut exec) = setup_stream_test();

            assert!(remote.write(&$command_var).is_ok());

            let respond_res = match next_request(&mut stream, &mut exec) {
                Request::$variant {
                    responder,
                    stream_ids,
                } => {
                    let stream_id = stream_ids.get(0).unwrap();
                    assert_eq!(&StreamEndpointId::try_from(12).unwrap(), stream_id);
                    responder.reject(stream_id, ErrorCode::BadAcpSeid)
                }
                _ => panic!("received the wrong request"),
            };

            assert!(respond_res.is_ok());

            let rejected_rsp = &[
                0x43,          // TxLabel (4),  RemoteRejected (3)
                $signal_value, // $variant
                12 << 2,       // Stream ID (12), RFA
                0x12,          // BAD_ACP_SEID
            ];
            expect_remote_recv(rejected_rsp, &remote);
        }
    };
}

// Open

const CMD_OPEN: &'static [u8] = &[
    0x40, // TxLabel (4), Single Packet (0b00), Command (0b00)
    0x06, // RFA (0b00), Open (0x06)
    0x30, // SEID 12 (0b001100)
];

const CMD_OPEN_VALUE: &'static u8 = &0x06;

incoming_cmd_length_fail_test!(open_length_too_short, *CMD_OPEN_VALUE, 0);
incoming_cmd_length_fail_test!(open_length_too_long, *CMD_OPEN_VALUE, 2);
seid_command_test!(open_command_works, *CMD_OPEN_VALUE, open);
seid_command_reject_test!(open_command_reject_works, *CMD_OPEN_VALUE, open);
seid_event_responder_send_test!(open_responder_send, Open, *CMD_OPEN_VALUE, CMD_OPEN);
seid_event_responder_reject_test!(open_responder_reject, Open, *CMD_OPEN_VALUE, CMD_OPEN);

// Start

const CMD_START: &'static [u8] = &[
    0x40, // TxLabel (4), Single Packet (0b00), Command (0b00)
    0x07, // RFA (0b00), Start (0x07)
    0x30, // SEID 12 (0b001100)
];

const CMD_START_VALUE: &'static u8 = &0x07;

incoming_cmd_length_fail_test!(start_length, *CMD_START_VALUE, 0);
seids_command_test!(start_command_works, *CMD_START_VALUE, start);
seids_command_reject_test!(start_command_reject_works, *CMD_START_VALUE, start);
seids_event_responder_send_test!(start_responder_send, Start, *CMD_START_VALUE, CMD_START);
stream_event_responder_reject_test!(start_responder_reject, Start, *CMD_START_VALUE, CMD_START);

// Close

const CMD_CLOSE: &'static [u8] = &[
    0x40, // TxLabel (4), Single Packet (0b00), Command (0b00)
    0x08, // RFA (0b00), Close (0x08)
    0x30, // SEID 12 (0b001100)
];

const CMD_CLOSE_VALUE: &'static u8 = &0x08;

incoming_cmd_length_fail_test!(close_length_too_short, *CMD_CLOSE_VALUE, 0);
incoming_cmd_length_fail_test!(close_length_too_long, *CMD_CLOSE_VALUE, 2);
seid_command_test!(close_command_works, *CMD_CLOSE_VALUE, close);
seid_command_reject_test!(close_command_reject_works, *CMD_CLOSE_VALUE, close);
seid_event_responder_send_test!(close_responder_send, Close, *CMD_CLOSE_VALUE, CMD_CLOSE);
seid_event_responder_reject_test!(close_responder_reject, Close, *CMD_CLOSE_VALUE, CMD_CLOSE);

// Suspend

const CMD_SUSPEND: &'static [u8] = &[
    0x40, // TxLabel (4), Single Packet (0b00), Command (0b00)
    0x09, // RFA (0b00), Suspend (0x09)
    0x30, // SEID 12 (0b001100)
];

const CMD_SUSPEND_VALUE: &'static u8 = &0x09;

incoming_cmd_length_fail_test!(suspend_length, *CMD_SUSPEND_VALUE, 0);
seids_command_test!(suspend_command_works, *CMD_SUSPEND_VALUE, suspend);
seids_command_reject_test!(suspend_command_reject_works, *CMD_SUSPEND_VALUE, suspend);
seids_event_responder_send_test!(
    suspend_responder_send,
    Suspend,
    *CMD_SUSPEND_VALUE,
    CMD_SUSPEND
);
stream_event_responder_reject_test!(
    suspend_responder_reject,
    Suspend,
    *CMD_SUSPEND_VALUE,
    CMD_SUSPEND
);

// Abort

const CMD_ABORT: &'static [u8] = &[
    0x40, // TxLabel (4), Single Packet (0b00), Command (0b00)
    0x0A, // RFA (0b00), Suspend (0x0A)
    0x30, // SEID 12 (0b001100)
];

const CMD_ABORT_VALUE: &'static u8 = &0x0A;

incoming_cmd_length_fail_test!(abort_length_too_short, *CMD_ABORT_VALUE, 0);
incoming_cmd_length_fail_test!(abort_length_too_long, *CMD_ABORT_VALUE, 2);
seid_command_test!(abort_command_works, *CMD_ABORT_VALUE, abort);
seid_command_reject_test!(abort_command_reject_works, *CMD_ABORT_VALUE, abort);
seid_event_responder_send_test!(abort_responder_send, Abort, *CMD_ABORT_VALUE, CMD_ABORT);
seid_event_responder_reject_test!(abort_responder_reject, Abort, *CMD_ABORT_VALUE, CMD_ABORT);

/// Abort requests with an invalid SEID are allowed to be dropped.
/// We timeout after some amount of time.
#[test]
fn abort_sent_no_response() {
    let mut exec = fasync::Executor::new().expect("failed to create an executor");
    let (peer, remote) = setup_peer();
    let seid = StreamEndpointId::try_from(1).unwrap();
    let mut response_fut = Box::pinned(peer.abort(&seid));
    assert!(exec.run_until_stalled(&mut response_fut).is_pending());

    let received = recv_remote(&remote).unwrap();
    // Last half of header must be Single (0b00) and Command (0b00)
    assert_eq!(0x00, received[0] & 0xF);
    assert_eq!(0x0A, received[1]); // $signal_value = $request_variant
    assert_eq!(0x01 << 2, received[2]); // SEID (0x01) , RFA

    let complete = exec.run_until_stalled(&mut response_fut);
    assert!(complete.is_pending());

    exec.wake_next_timer();
    assert_eq!(
        Poll::Ready(Err(Error::Timeout)),
        exec.run_until_stalled(&mut response_fut)
    );
}

// Set Configuration

fn expect_config_recv_cap_okay(
    cmd: &[u8], local_seid: StreamEndpointId, remote_seid: StreamEndpointId,
    capability: ServiceCapability,
) {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    let txlabel_mask = cmd[0] & 0xF0;

    assert!(remote.write(cmd).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::SetConfiguration {
            responder,
            local_stream_id,
            remote_stream_id,
            capabilities,
        } => {
            assert_eq!(local_seid, local_stream_id);
            assert_eq!(remote_seid, remote_stream_id);
            assert_eq!(capability, capabilities[0]);
            responder.send()
        }
        _ => panic!("should have received a SetConfiguration"),
    };

    assert!(respond_res.is_ok());

    // Expected response: Same TxLabel & ResponseAccept (0x2) , Signal.
    expect_remote_recv(&[txlabel_mask | 0x02, 0x03], &remote);
}

// Set config must be at least 2 SEIDs and one configuration long.
incoming_cmd_length_fail_test!(set_config_length_too_short, 0x03, 2);

#[test]
fn set_configration_invalid_media_transport_format() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    // TxLabel (4), ResponseReject, Signal, Relevant Cap (Media Transport),
    // BadMediaTransportFormat
    let rsp = &[0x43, 0x03, 0x01, 0x23];

    let cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x01, 0x01, 0x01, // Media Transport, Length 1 (too long), dummy
    ];
    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
}

// These also test that the MediaTransport is decoded and received correctly.
#[test]
fn set_config_event_responder_send_works() {
    let set_config_cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x01, 0x00, // Media Transport, Length 0
    ];

    expect_config_recv_cap_okay(
        set_config_cmd,
        StreamEndpointId(1),
        StreamEndpointId(2),
        ServiceCapability::MediaTransport,
    );
}

#[test]
fn set_config_event_responder_reject_works() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    let set_config_cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x01, 0x00, // Media Transport, Length 0
    ];

    assert!(remote.write(set_config_cmd).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::SetConfiguration {
            responder,
            local_stream_id,
            remote_stream_id,
            capabilities,
        } => {
            assert_eq!(StreamEndpointId(1), local_stream_id);
            assert_eq!(StreamEndpointId(2), remote_stream_id);
            assert_eq!(ServiceCapability::MediaTransport, capabilities[0]);
            responder.reject(Some(&capabilities[0]), ErrorCode::UnsupportedConfiguration)
        }
        _ => panic!("should have received a SetConfiguration"),
    };

    assert!(respond_res.is_ok());

    // Expected response: Same TxLabel (4) & ResponseReject, Signal,
    // Relevant capabilitiy, Error Code (Unsupported Configure)
    expect_remote_recv(&[0x43, 0x03, 0x01, 0x29], &remote);
}

// Set Config: Reporting

#[test]
fn set_config_bad_reporting_format() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    // TxLabel (4), ResponseReject, Signal, Relevant Cap (Reporting), BadPayloadFormat
    let rsp = &[0x43, 0x03, 0x02, 0x18];

    let cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x02, 0x01, 0x01, // Reporting, Length 1 (too long), dummy
    ];
    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
}

#[test]
fn set_config_reporting_ok() {
    let set_config_cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x02, 0x00, // Reporting, Length 0
    ];

    expect_config_recv_cap_okay(
        set_config_cmd,
        StreamEndpointId(1),
        StreamEndpointId(2),
        ServiceCapability::Reporting,
    );
}

// Set Config: Content Protection

#[test]
fn set_config_bad_content_protection_format() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    // TxLabel (4), ResponseReject, Signal, Relevant Cap (CP), BadCpFormat
    let rsp = &[0x43, 0x03, 0x04, 0x27];

    let cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x04, 0x01, 0x01, // Content Protection (4), Length 1 (too short), dummy
    ];
    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);

    let cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x04, 0x02, // Content Protection (4), Length 2
        0xF0, 0x0F, // CP Type 0x0FF0 (invalid)
    ];
    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
}

#[test]
fn set_config_content_protection_ok() {
    let set_config_cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x04, 0x02, // Content Protection (4), Length 2
        0x01, 0x00, // LSB, MSB of DTCP
    ];

    expect_config_recv_cap_okay(
        set_config_cmd,
        StreamEndpointId(1),
        StreamEndpointId(2),
        ServiceCapability::ContentProtection {
            protection_type: ContentProtectionType::DigitalTransmissionContentProtection,
            extra: vec![],
        },
    );
}

// Set Config: Recovery

#[rustfmt::skip]
fn build_recovery_cmd(recovery_type: u8, mrws: u8, mnmp: u8) -> [u8; 9] {
    [
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x03, 0x03,             // Recovery (3), length 3
        recovery_type,          // Recovery Type
        mrws,                   // Maximum Recovery Window Size
        mnmp,                   // Maximum Number of Media Packets
    ]
}

#[test]
fn set_config_bad_recovery_type() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    // TxLabel (4), Signal, Relevant Cap (Recovery), BadRecoveryType
    let rsp = &[0x43, 0x03, 0x03, 0x22];

    // Forbidden Recovery Type
    let cmd = &build_recovery_cmd(0x00, 0x01, 0x01);

    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);

    // RFD recovery type
    let cmd = &build_recovery_cmd(0x02, 0x01, 0x01);

    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
}

#[test]
fn set_config_bad_recovery_format() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    let cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x03, 0x01, 0x01, // Recovery (3), length 1, 0x01
    ];
    // TxLabel 4, ResponseReject, Relevant Cap (Recovery), BadRecoveryFormat
    let rsp = &[0x43, 0x03, 0x03, 0x25];

    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);

    // Bad Maximum Recovery Window Size (zero is not allowed)
    let cmd = &build_recovery_cmd(0x01, 0x00, 0x01);
    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
    // Bad Maximum Recovery Window Size (too large)
    let cmd = &build_recovery_cmd(0x01, 0x20, 0x01);
    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
    // Bad Maximum Number of Media Packets (zero is not allowed)
    let cmd = &build_recovery_cmd(0x01, 0x01, 0x00);
    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
    // Bad Maximum Number of Media Packets (too large)
    let cmd = &build_recovery_cmd(0x01, 0x01, 0x21);
    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
}

#[test]
fn set_config_recovery_ok() {
    let set_config_cmd = &build_recovery_cmd(0x01, 0x01, 0x01);
    expect_config_recv_cap_okay(
        set_config_cmd,
        StreamEndpointId(1),
        StreamEndpointId(2),
        ServiceCapability::Recovery {
            recovery_type: 0x01,
            max_recovery_window_size: 0x01,
            max_number_media_packets: 0x01,
        },
    );
}

// Media Codec

#[test]
fn set_config_bad_media_codec_format() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    let cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x07, 0x01, 0x01, // Media Codec (7), length 1, dummy byte
    ];
    // TxLabel 4, ResponseReject, Relevant Cap (Media Codec), BadPayloadFormat
    let rsp = &[0x43, 0x03, 0x07, 0x18];

    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);

    let cmd = &[
        0x40, 0x03, 0x04, 0x08, // TxLabel 4, Signal, ACP (1) and INT (2) SEID
        0x07, 0x02, // Media Codec (7), length 2
        0x40, // Unknown Media Type, RFA
        0x01, // Media Codec Type
    ];
    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
}

#[test]
fn set_config_media_type_ok() {
    // Sample set config command sent from a phone
    let set_config_cmd = &[
        0x40, 0x03, 0x18, 0x04, // TxLabel 4, Signal, ACP (6) and INT (1) SEID
        0x07, 0x06, // Media Codec (7), length 6
        0x00, // Media Type: Audio
        0x00, // Media Codec Type: SBC (0x00)
        0x21, // Sampling Freq: 44.1khz (0x20), Channel Mode Joint Stereo (0x01)
        0x15, // Block length 16 (0x10), 8 Subbands (0x04), Loudness Allocation (0x01)
        0x02, // Minimum bitpool: 2
        0x35, // Maximum bitpool: 53 (0x35)
    ];
    expect_config_recv_cap_okay(
        set_config_cmd,
        StreamEndpointId(6),
        StreamEndpointId(1),
        ServiceCapability::MediaCodec {
            media_type: MediaType::Audio,
            codec_type: MediaCodecType::new(0),
            codec_extra: vec![0x21, 0x15, 0x02, 0x35],
        },
    );
}

// Reconfigure

// Set config must be at least 1 SEID and one configuration long.
incoming_cmd_length_fail_test!(reconfigure_length_too_short, 0x05, 1);

#[test]
fn reconfigure_invalid_capabilities() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    let cmd = &[
        0x40, 0x05, 0x04, // TxLabel 4, Signal, ACP (1) SEID
        0x03, 0x03, // Recovery (3), length 3,
        0x01, 0x01, 0x01, // RFC2733, MRWS, MNMP
    ];
    // TxLabel (4), ResponseRreject, Relevant cap (Recovery), Invalid Capabilities
    let rsp = &[0x43, 0x05, 0x03, 0x1A];

    stream_request_response(&mut exec, &mut stream, &remote, cmd, rsp);
}

#[test]
fn reconfig_event_responder_send_works() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    let reconfigure_cmd = &[
        0x40, 0x05, 0x04, // TxLabel 4, Signal, ACP (1) SEID
        0x07, 0x02, // Media Codec, Length 2
        0x00, // Media Type: Audio
        0x00, // Media Codec Type: SBC
    ];

    assert!(remote.write(reconfigure_cmd).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::Reconfigure {
            responder,
            local_stream_id,
            capabilities,
        } => {
            assert_eq!(StreamEndpointId(1), local_stream_id);
            assert_eq!(
                ServiceCapability::MediaCodec {
                    media_type: MediaType::Audio,
                    codec_type: MediaCodecType::new(0),
                    codec_extra: vec![],
                },
                capabilities[0]
            );
            responder.send()
        }
        _ => panic!("should have received a Reconfigure"),
    };

    assert!(respond_res.is_ok());

    // Expected response: Same TxLabel (4) & ResponseAccept, Signal,
    expect_remote_recv(&[0x42, 0x05], &remote);
}

#[test]
fn reconfig_event_responder_reject_works() {
    let (mut stream, _, remote, mut exec) = setup_stream_test();

    let reconfigure_cmd = &[
        0x40, 0x05, 0x04, // TxLabel 4, Signal, ACP (1) SEID
        0x07, 0x02, // Media Codec, Length 2
        0x00, // Media Type: Audio
        0x00, // Media Codec Type: SBC
    ];

    assert!(remote.write(reconfigure_cmd).is_ok());

    let respond_res = match next_request(&mut stream, &mut exec) {
        Request::Reconfigure {
            responder,
            local_stream_id,
            capabilities,
        } => {
            assert_eq!(StreamEndpointId(1), local_stream_id);
            assert_eq!(
                ServiceCapability::MediaCodec {
                    media_type: MediaType::Audio,
                    codec_type: MediaCodecType::new(0),
                    codec_extra: vec![],
                },
                capabilities[0]
            );
            responder.reject(Some(&capabilities[0]), ErrorCode::UnsupportedConfiguration)
        }
        _ => panic!("should have received a Reconfigure"),
    };

    assert!(respond_res.is_ok());

    // Expected response: Same TxLabel (4) & ResponseReject, Signal,
    // Relevant capability, Error Code (Unsupported Config)
    expect_remote_recv(&[0x43, 0x05, 0x07, 0x29], &remote);
}
