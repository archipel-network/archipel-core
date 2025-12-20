//! Serializer for Bundle Transport Protocol unidirectional messages

use crate::{
    TransferIdentifier,
    message::{MESSAGE_HEADER_SIZE, METADATA_FLAG, Message, MessageHeader, MessageType, Segment},
};

#[derive(Clone, Copy)]
/// The size of the link-layer protocol data unit in bytes
pub struct PduSize(usize);

/// An error occuring at [PduSize] creation
#[derive(Debug)]
pub enum PduError {
    /// The provided size is zero
    Zero,
    /// The provided size is larger than `1048580` which is : `2^20` (`length` field's length) + 4 bytes of header
    TooLarge,
}

impl PduSize {
    /// Returns a new [PduSize] if the provided size is greater than zero and less than 1048580
    pub const fn new(size: usize) -> Result<Self, PduError> {
        if size == 0 {
            Err(PduError::Zero)
        } else if size < (2 ^ 20) + MESSAGE_HEADER_SIZE {
            Err(PduError::TooLarge)
        } else {
            Ok(Self(size))
        }
    }
}

impl TryFrom<usize> for PduSize {
    type Error = PduError;

    fn try_from(value: usize) -> Result<Self, Self::Error> {
        PduSize::new(value)
    }
}

#[derive(Debug)]
/// An iterator over [Message]s generated over a `Bundle` buffer
pub struct MessageIter<'a> {
    transfer_identifier: TransferIdentifier,
    bundle_buffer: &'a [u8],
    bytes_read: usize,
    segment_index: u32,
    repeat: usize,
}

impl<'a> MessageIter<'a> {
    /// Creates an [Iterator] over [Message]s from a bundle buffer and a PDU size
    pub fn new(
        bundle_buffer: &'a [u8],
        transfer_identifier: TransferIdentifier,
        repeat: usize,
    ) -> Self {
        Self {
            transfer_identifier,
            bundle_buffer,
            bytes_read: 0,
            segment_index: 0,
            repeat,
        }
    }

    pub fn next(&mut self, pdu: PduSize) -> Option<Message> {
        let mut mtu = pdu.0 - MESSAGE_HEADER_SIZE;
        if self.bytes_read == 0 {
            if self.bundle_buffer.len() <= mtu {
                self.bytes_read = self.bundle_buffer.len();
                Some(Message::Bundle {
                    content: self.bundle_buffer,
                })
            } else {
                let metadata = crate::message::Metadata::BundleLength(
                    self.bundle_buffer
                        .len()
                        .try_into()
                        .expect("Bundle buffer size is larger than u64 max value"),
                );

                mtu -= metadata.size();
                self.segment_index = self.bundle_buffer.len().div_ceil(self.mtu) as u32 - 1;
                self.bytes_read = mtu;
                Some(Message::TransferStart {
                    metadata: Some(metadata),
                    segment: Segment {
                        transfer_identifier: self.transfer_identifier,
                        index: self.segment_index,
                        data: &self.bundle_buffer[0..self.mtu],
                    },
                })
            }
        } else if self.bytes_read < self.bundle_buffer.len() - 1 {
            self.segment_index -= 1;
            let start_byte = self.bytes_read;
            self.bytes_read += self.mtu;

            let data =
                &self.bundle_buffer[start_byte..self.bytes_read.min(self.bundle_buffer.len())];

            Some(Message::TransferSegment {
                metadata: Some(crate::message::Metadata::BundleLength(
                    self.bundle_buffer
                        .len()
                        .try_into()
                        .expect("Bundle buffer size is larger than u64 max value"),
                )),
                segment: Segment {
                    transfer_identifier: self.transfer_identifier,
                    index: self.segment_index,
                    data,
                },
            })
        } else {
            None
        }
    }
}

/*
impl<'a> Iterator for MessageIter<'a> {
    type Item = Message<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.bytes_read == 0 && self.repeat > 0 {
            if self.bundle_buffer.len() <= self.mtu {
                self.bytes_read = self.bundle_buffer.len();
                Some(Message {
                    header: MessageHeader {
                        kind: MessageType::Bundle,
                        flags: 0,
                        length: self.bundle_buffer.len() as u32,
                    },
                    content: MessageContent::BundleMessage(self.bundle_buffer),
                    metadata: None,
                })
            } else {
                let metadata = crate::message::Metadata::BundleLength(
                    self.bundle_buffer
                        .len()
                        .try_into()
                        .expect("Bundle buffer size is larger than u64 max value"),
                );

                self.mtu -= metadata.size();
                self.segment_index = self.bundle_buffer.len().div_ceil(self.mtu) as u32 - 1;
                self.bytes_read = self.mtu;
                Some(Message {
                    header: MessageHeader {
                        kind: MessageType::TransferStart,
                        flags: METADATA_FLAG,
                        length: self.mtu as u32,
                    },
                    content: MessageContent::TransferStart(Segment {
                        transfer_identifier: self.transfer_identifier,
                        index: self.segment_index,
                        data: &self.bundle_buffer[0..self.mtu],
                    }),
                    metadata: Some(metadata),
                })
            }
        } else if self.bytes_read < self.bundle_buffer.len() - 1 {
            self.segment_index -= 1;
            let start_byte = self.bytes_read;
            self.bytes_read += self.mtu;

            let data =
                &self.bundle_buffer[start_byte..self.bytes_read.min(self.bundle_buffer.len())];

            Some(Message {
                header: MessageHeader {
                    kind: MessageType::TransferSegment,
                    flags: METADATA_FLAG,
                    length: data.len() as u32,
                },
                content: MessageContent::TransferSegment(Segment {
                    transfer_identifier: self.transfer_identifier,
                    index: self.segment_index,
                    data,
                }),
                metadata: Some(crate::message::Metadata::BundleLength(
                    self.bundle_buffer
                        .len()
                        .try_into()
                        .expect("Bundle buffer size is larger than u64 max value"),
                )),
            })
        } else {
            None
        }
    }
}*/

#[cfg(test)]
mod tests {
    use crate::{
        message::{Message, Metadata, Segment},
        serializer::{MESSAGE_HEADER_SIZE, MessageIter, PduSize},
    };

    #[test]
    fn single_message() {
        let mut iter = MessageIter::new(&[0; 500], crate::TransferIdentifier(1), 1);

        assert_eq!(
            iter.next(PduSize::new(500).unwrap()),
            Some(Message::Bundle { content: &[0; 500] })
        );

        assert_eq!(iter.next(PduSize::new(12).unwrap()), None)
    }

    #[test]
    fn multiple_messages() {
        let mut bundle_buffer: [u8; 1536] = [0; 1536];
        let pdu = 512;

        const METADATA_SIZE: usize = 4;

        for (index, value) in bundle_buffer.iter_mut().enumerate() {
            *value = (index % 255) as u8;
        }

        let mut iter = MessageIter::new(&bundle_buffer, crate::TransferIdentifier(1), 1);

        let end = pdu - MESSAGE_HEADER_SIZE - METADATA_SIZE;
        assert_eq!(
            iter.next(pdu.try_into().unwrap()),
            Some(Message::TransferStart {
                metadata: Some(Metadata::BundleLength(1536)),
                segment: Segment {
                    index: 3,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[0..end]
                }
            })
        );

        let start = end;
        let end = end + pdu - MESSAGE_HEADER_SIZE - METADATA_SIZE;

        assert_eq!(
            iter.next(pdu.try_into().unwrap()),
            Some(Message::TransferSegment {
                metadata: Some(Metadata::BundleLength(1536)),
                segment: Segment {
                    index: 2,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[start..end]
                }
            })
        );

        let start = end;
        let end = end + pdu - MESSAGE_HEADER_SIZE - METADATA_SIZE;

        assert_eq!(
            iter.next(pdu.try_into().unwrap()),
            Some(Message::TransferSegment {
                metadata: Some(Metadata::BundleLength(1536)),
                segment: Segment {
                    index: 1,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[start..end]
                }
            })
        );

        assert_eq!(
            iter.next(pdu.try_into().unwrap()),
            Some(Message::TransferSegment {
                metadata: Some(Metadata::BundleLength(1536)),
                segment: Segment {
                    index: 0,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[end..]
                }
            })
        );
    }
}
