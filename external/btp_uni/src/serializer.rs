//! Serializer for Bundle Transport Protocol unidirectional messages

use crate::{
    TransferIdentifier,
    message::{METADATA_FLAG, Message, MessageContent, MessageHeader, MessageType, Segment},
};

/// Size of the header in bytes
const HEADER_SIZE: usize = 4;

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
        } else if size < (2 ^ 20) + HEADER_SIZE {
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

/// Serializer structure
pub struct Serializer;

impl Serializer {
    /// Create a new parser
    pub fn new() -> Self {
        Self
    }

    /// Parse a full message with content slice
    pub fn serialize_bundle<'a>(
        &mut self,
        bundle_buffer: &'a [u8],
        pdu: PduSize,
        transfer_identifier: TransferIdentifier,
    ) -> MessageIter<'a> {
        MessageIter {
            transfer_identifier,
            mtu: pdu.0 - HEADER_SIZE,
            bundle_buffer,
            bytes_read: 0,
            segment_index: 0,
        }
    }
}

impl Default for Serializer {
    fn default() -> Self {
        Self::new()
    }
}

/// An iterator over [Message]s generated over a `Bundle` buffer
pub struct MessageIter<'a> {
    transfer_identifier: TransferIdentifier,
    mtu: usize,
    bundle_buffer: &'a [u8],
    bytes_read: usize,
    segment_index: u32,
}

impl<'a> Iterator for MessageIter<'a> {
    type Item = Message<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.bytes_read == 0 {
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

                assert_eq!(metadata.size(), 4);

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
}

#[cfg(test)]
mod tests {
    use crate::{
        message::{
            METADATA_FLAG, Message, MessageContent, MessageHeader, MessageType, Metadata, Segment,
        },
        serializer::{HEADER_SIZE, Serializer},
    };

    #[test]
    fn single_message() {
        let mut serializer = Serializer::new();
        let mut iter = serializer.serialize_bundle(
            &[0; 500],
            512.try_into().unwrap(),
            crate::TransferIdentifier(1),
        );

        assert_eq!(
            iter.next(),
            Some(Message {
                header: MessageHeader {
                    kind: MessageType::Bundle,
                    flags: 0,
                    length: 500
                },
                content: MessageContent::BundleMessage(&[0; 500]),
                metadata: None
            })
        );

        assert_eq!(iter.next(), None)
    }

    #[test]
    fn multiple_messages() {
        let mut serializer = Serializer::new();
        let mut bundle_buffer: [u8; 1536] = [0; 1536];
        let pdu = 512;

        const METADATA_SIZE: usize = 4;

        for (index, value) in bundle_buffer.iter_mut().enumerate() {
            *value = (index % 255) as u8;
        }

        let mut iter = serializer.serialize_bundle(
            &bundle_buffer,
            pdu.try_into().unwrap(),
            crate::TransferIdentifier(1),
        );

        let end = pdu - HEADER_SIZE - METADATA_SIZE;
        assert_eq!(
            iter.next(),
            Some(Message {
                header: MessageHeader {
                    kind: MessageType::TransferStart,
                    flags: METADATA_FLAG,
                    length: end as u32
                },
                content: MessageContent::TransferStart(Segment {
                    index: 3,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[0..end]
                }),
                metadata: Some(Metadata::BundleLength(1536))
            })
        );

        let start = end;
        let end = end + pdu - HEADER_SIZE - METADATA_SIZE;

        assert_eq!(
            iter.next(),
            Some(Message {
                header: MessageHeader {
                    kind: MessageType::TransferSegment,
                    flags: METADATA_FLAG,
                    length: (end - start) as u32
                },
                content: MessageContent::TransferSegment(Segment {
                    index: 2,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[start..end]
                }),
                metadata: Some(Metadata::BundleLength(1536))
            })
        );

        let start = end;
        let end = end + pdu - HEADER_SIZE - METADATA_SIZE;

        assert_eq!(
            iter.next(),
            Some(Message {
                header: MessageHeader {
                    kind: MessageType::TransferSegment,
                    flags: METADATA_FLAG,
                    length: (end - start) as u32
                },
                content: MessageContent::TransferSegment(Segment {
                    index: 1,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[start..end]
                }),
                metadata: Some(Metadata::BundleLength(1536))
            })
        );

        assert_eq!(
            iter.next(),
            Some(Message {
                header: MessageHeader {
                    kind: MessageType::TransferSegment,
                    flags: METADATA_FLAG,
                    length: (1536 - end) as u32
                },
                content: MessageContent::TransferSegment(Segment {
                    index: 0,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[end..]
                }),
                metadata: Some(Metadata::BundleLength(1536))
            })
        );
    }
}
