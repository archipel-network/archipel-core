//! Serializer for Bundle Transport Protocol unidirectional messages

use crate::{
    TransferIdentifier,
    message::{MESSAGE_HEADER_SIZE, Message, Segment},
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

    /// Returns the tranfer identifier of the undelying message
    pub fn transfer_id(&self) -> TransferIdentifier {
        self.transfer_identifier
    }

    /// Returns the next [Message] of this iterator
    pub fn next(&'_ mut self, pdu: PduSize) -> Option<Message<'_>> {
        let mut mtu = pdu.0 - MESSAGE_HEADER_SIZE;
        if self.bytes_read == 0 && self.bundle_buffer.len() <= mtu {
            self.bytes_read = self.bundle_buffer.len();
            Some(Message::Bundle {
                content: self.bundle_buffer,
            })
        } else if self.bytes_read < self.bundle_buffer.len() - 1 {
            let segment_index = self.segment_index;
            self.segment_index += 1;
            let start_byte = self.bytes_read;

            let remaining_bytes = self.bundle_buffer.len() - self.bytes_read;

            let metadata = crate::message::Metadata::BundleLength(
                self.bundle_buffer
                    .len()
                    .try_into()
                    .expect("Bundle buffer size is larger than u64 max value"),
            );

            if remaining_bytes <= mtu - metadata.size() {
                self.bytes_read = self.bundle_buffer.len();
                Some(Message::TransferEnd {
                    metadata: Some(metadata),
                    segment: Segment {
                        transfer_identifier: self.transfer_identifier,
                        index: segment_index,
                        data: &self.bundle_buffer[start_byte..self.bundle_buffer.len()],
                    },
                })
            } else {
                mtu -= metadata.size();
                self.bytes_read += mtu;
                let data = &self.bundle_buffer[start_byte..self.bytes_read];

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
            }
        } else {
            if self.repeat > 0 {
                self.repeat -= 1;
                self.bytes_read = 0;
                self.next(pdu)
            } else {
                None
            }
        }
    }
}

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
            Some(Message::TransferEnd {
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
