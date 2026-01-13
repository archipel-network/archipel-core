//! A module containing the bundle transfer protocol sender logic

use heapless::{BinaryHeap, Deque, binary_heap::Max};

use crate::{
    TransferIdentifier, TransferWindow,
    message::{Message, WriteToError},
    serializer::{MessageIter, PduSize},
};

#[derive(PartialEq, Eq, PartialOrd, Ord)]
#[repr(u8)]
enum Priority {
    Low = 0x00,
    Normal = 0x01,
    High = 0x10,
}

struct BundlePriorityPair<'a>(MessageIter<'a>, Priority);

impl PartialEq for BundlePriorityPair<'_> {
    fn eq(&self, other: &Self) -> bool {
        self.1 == other.1
    }
}

impl Eq for BundlePriorityPair<'_> {}

impl PartialOrd for BundlePriorityPair<'_> {
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        self.1.partial_cmp(&other.1)
    }
}

impl Ord for BundlePriorityPair<'_> {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.1.cmp(&other.1)
    }
}

/// The minimum size in bytes of a tranfer for a complete or segmented bundle
///
/// Header size + transfer number size + segment index size
pub const MIN_BUNDLE_TRANSFER_SIZE: usize = 4 + 4 + 4;

/// A bundle transfer protocol sender
pub struct Sender<'a> {
    window: TransferWindow,
    pdu: PduSize,
    bundles: BinaryHeap<BundlePriorityPair<'a>, Max, 4096>,
    low_priority_bundles: Deque<MessageIter<'a>, 4096>,
    medium_priority_bundles: Deque<MessageIter<'a>, 4096>,
    high_priority_bundles: Deque<MessageIter<'a>, 4096>,
}

impl<'a> Sender<'a> {
    /// Creates a new sender with a sliding transfer window
    pub const fn new(window: TransferWindow, pdu: PduSize) -> Self {
        Self {
            window,
            pdu,
            bundles: BinaryHeap::new(),
            low_priority_bundles: Deque::new(),
            medium_priority_bundles: Deque::new(),
            high_priority_bundles: Deque::new(),
        }
    }

    fn cancel_outdated_transfers(&mut self) {
        todo!()
    }

    fn continue_processing(&mut self) {
        todo!()
    }

    pub fn queue_bundle<'b: 'a>(
        &mut self,
        bundle_buf: &'b [u8],
        prioriry: Priority,
        repeat: usize,
    ) {
        let id = if let Some(id) = self.window.greatest_transfer_identifier {
            id.next()
        } else {
            TransferIdentifier(0)
        };

        let message_iter = MessageIter::new(bundle_buf, id, repeat);

        match prioriry {
            Priority::Low => self.low_priority_bundles.push_back(message_iter),
            Priority::Normal => self.medium_priority_bundles.push_back(message_iter),
            Priority::High => self.high_priority_bundles.push_back(message_iter),
        };

        //TODO: Slide window and remove outdated transfers

        // if self.window.is_new(segment.transfer_identifier) {
        //     self.window.slide_to(segment.transfer_identifier);
        //     self.cancel_outdated_transfers();
        // } else if self.window.is_in(segment.transfer_identifier) {
        //     self.continue_processinmessageg();
        // }
    }

    pub fn poll(&mut self, buf: &mut [u8]) -> Result<usize, WriteToError> {
        let mut bytes_written = 0;
        while bytes_written < buf.len() {
            let remaining = buf.len() - bytes_written;
            if remaining > MIN_BUNDLE_TRANSFER_SIZE {
                if let Some(mut message_iter) = self.high_priority_bundles.pop_front() {
                    if let Some(message) = message_iter.next(
                        PduSize::new(buf.len() - bytes_written)
                            .expect("No remaining space in buffer"),
                    ) {
                        bytes_written += message.write_to_buf(&mut buf[bytes_written..])?;
                        match message {
                            Message::Bundle { content: _ } => (),
                            Message::TransferEnd {
                                metadata: _,
                                segment: _,
                            } => self
                                .high_priority_bundles
                                .push_front(message_iter)
                                .expect("Can't push message iter"),
                            Message::TransferSegment {
                                metadata: _,
                                segment,
                            } => {
                                if segment.index > 0 {
                                    self.high_priority_bundles
                                        .push_front(message_iter)
                                        .expect("Can't push message iter")
                                }
                            }
                            Message::IndefinitePadding(_)
                            | Message::DefinitePadding(_)
                            | Message::TransferCancel(_) => unreachable!(),
                        }
                    }
                } else if let Some(mut message_iter) = self.medium_priority_bundles.pop_front() {
                    if let Some(message) = message_iter.next(
                        PduSize::new(buf.len() - bytes_written)
                            .expect("No remaining space in buffer"),
                    ) {
                        bytes_written += message.write_to_buf(&mut buf[bytes_written..])?;
                        match message {
                            Message::Bundle { content: _ } => (),
                            Message::TransferEnd {
                                metadata: _,
                                segment: _,
                            } => self
                                .high_priority_bundles
                                .push_front(message_iter)
                                .expect("Can't push message iter"),
                            Message::TransferSegment {
                                metadata: _,
                                segment,
                            } => {
                                if segment.index > 0 {
                                    self.high_priority_bundles
                                        .push_front(message_iter)
                                        .expect("Can't push message iter")
                                }
                            }
                            Message::IndefinitePadding(_)
                            | Message::DefinitePadding(_)
                            | Message::TransferCancel(_) => unreachable!(),
                        }
                    }
                } else if let Some(mut message_iter) = self.low_priority_bundles.pop_front() {
                    if let Some(message) = message_iter.next(
                        PduSize::new(buf.len() - bytes_written)
                            .expect("No remaining space in buffer"),
                    ) {
                        bytes_written += message.write_to_buf(&mut buf[bytes_written..])?;
                        match message {
                            Message::Bundle { content: _ } => (),
                            Message::TransferEnd {
                                metadata: _,
                                segment: _,
                            } => self
                                .high_priority_bundles
                                .push_front(message_iter)
                                .expect("Can't push message iter"),
                            Message::TransferSegment {
                                metadata: _,
                                segment,
                            } => {
                                if segment.index > 0 {
                                    self.high_priority_bundles
                                        .push_front(message_iter)
                                        .expect("Can't push message iter")
                                }
                            }
                            Message::IndefinitePadding(_)
                            | Message::DefinitePadding(_)
                            | Message::TransferCancel(_) => unreachable!(),
                        }
                    }
                } else {
                    return Ok(0);
                }
            } else {
                if remaining >= 4 {
                    bytes_written += Message::DefinitePadding(remaining - 4).write_to_buf(buf)?;
                } else {
                    bytes_written += Message::IndefinitePadding(remaining - 1).write_to_buf(buf)?;
                }
            }
        }

        Ok(bytes_written)
    }
}
