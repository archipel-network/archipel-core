//! A module containing the bundle transfer protocol sender logic

use heapless::Deque;

use crate::{
    TransferIdentifier, TransferWindow, TransferWindowError,
    message::{Message, WriteToError},
    serializer::{MessageIter, PduSize},
};

/// The priority of a transfer
#[derive(PartialEq, Eq, PartialOrd, Ord)]
#[repr(u8)]
pub enum Priority {
    /// Low priority
    Low = 0x00,
    /// Medium priority
    Normal = 0x01,
    /// High priority
    High = 0x10,
}

/// The minimum size in bytes of a tranfer for a complete or segmented bundle
///
/// Header size + transfer number size + segment index size
pub const MIN_BUNDLE_TRANSFER_SIZE: usize = 4 + 4 + 4;

/// A bundle transfer protocol sender
///
/// const
pub struct Sender<'a, const W: usize> {
    window: TransferWindow,
    low_priority_bundles: Deque<MessageIter<'a>, W>,
    medium_priority_bundles: Deque<MessageIter<'a>, W>,
    high_priority_bundles: Deque<MessageIter<'a>, W>,
}

impl<'a, const W: usize> Sender<'a, W> {
    /// Creates a new sender
    ///
    /// Returns an error if the window size is less than 4 or greater than
    pub const fn new() -> Result<Self, TransferWindowError> {
        match TransferWindow::new(W as u32) {
            Ok(window) => Ok(Self {
                window,
                low_priority_bundles: Deque::new(),
                medium_priority_bundles: Deque::new(),
                high_priority_bundles: Deque::new(),
            }),
            Err(err) => Err(err),
        }
    }

    /// Queues a bundle to transfer
    ///
    /// # Parameters
    /// - `priority`: The [Priority] of the transfer
    /// - `repeat`: How many times the messages constituting the transfer are repeated
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

        self.window.slide_to(id);

        self.low_priority_bundles
            .retain(|message_iter| self.window.is_in(message_iter.transfer_id()));
        self.medium_priority_bundles
            .retain(|message_iter| self.window.is_in(message_iter.transfer_id()));
        self.high_priority_bundles
            .retain(|message_iter| self.window.is_in(message_iter.transfer_id()));

        let message_iter: MessageIter<'_> = MessageIter::new(bundle_buf, id, repeat);

        match prioriry {
            Priority::Low => self.low_priority_bundles.push_back(message_iter),
            Priority::Normal => self.medium_priority_bundles.push_back(message_iter),
            Priority::High => self.high_priority_bundles.push_back(message_iter),
        }
        .expect("No more room in bundles queue");
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
                            Message::Bundle { content: _ } => self
                                .high_priority_bundles
                                .push_front(message_iter)
                                .expect("Can't push message iter"),
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
                            Message::Bundle { content: _ } => self
                                .medium_priority_bundles
                                .push_front(message_iter)
                                .expect("Can't push message iter"),
                            Message::TransferEnd {
                                metadata: _,
                                segment: _,
                            } => self
                                .medium_priority_bundles
                                .push_front(message_iter)
                                .expect("Can't push message iter"),
                            Message::TransferSegment {
                                metadata: _,
                                segment,
                            } => {
                                if segment.index > 0 {
                                    self.medium_priority_bundles
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
                            Message::Bundle { content: _ } => self
                                .low_priority_bundles
                                .push_front(message_iter)
                                .expect("Can't push message iter"),
                            Message::TransferEnd {
                                metadata: _,
                                segment: _,
                            } => self
                                .low_priority_bundles
                                .push_front(message_iter)
                                .expect("Can't push message iter"),
                            Message::TransferSegment {
                                metadata: _,
                                segment,
                            } => {
                                if segment.index > 0 {
                                    self.low_priority_bundles
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

#[cfg(test)]
mod tests {
    use crate::sender::{Priority, Sender};

    #[test]
    fn repeat() {
        let mut sender: Sender<'_, 16> = Sender::new().unwrap();

        sender.queue_bundle(&[1; 500], Priority::Normal, 2);

        let mut out = [0; 1500];
        sender.poll(&mut out).unwrap();

        assert_eq!(out, [1; 1500]);
    }
}
