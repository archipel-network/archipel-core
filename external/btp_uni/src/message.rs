//! A module containing the bundle transfer protocol message logic
//!
//! Message format :
//!
//!  0                   1                   2                   3
//!  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//! +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//! |     Type      |Flags|      Length (20-bit unsigned int)       |
//! +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//! |              ... Optional Hint Items ...                  :
//! +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//! |                       ... Content ...                         :
//! +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

use crate::TransferIdentifier;

/// Flag indicating that further hint is included in message
pub const HINT_FLAG: u8 = 0b00001000;

/// Size of a message header in bytes
pub const MESSAGE_HEADER_SIZE: usize = 4;

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// Message type defined in message header
pub enum MessageType {
    /// Message is an indefinite length Padding message (section 8.6)
    IndefinitePadding = 0,
    /// Message is a definite padding length message (section 8.5)
    DefinitePadding = 1,
    /// Message is a single bundle message (section 8.1)
    Bundle = 2,
    /// Message is a segment transfer message (section 8.3)
    TransferSegment = 3,
    /// Message is a transfer start message (section 8.2)
    TransferEnd = 4,
    /// Message is a transfer cancel message (Section 8.4)
    TransferCancel = 5,
    // 6 is reserved to avoid clash with BPv6
}

/// Different types of hint included in some messages (section 7.2)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Hint {
    /// Ful length of bundle to be transfered (section 9.1)
    BundleLength(u64),
}

/// The size of the hint type field in bytes
const TYPE_SIZE: usize = 1;

/// The size of the hint length field in bytes
const LENGTH_SIZE: usize = 1;

#[derive(Debug)]
/// Error occuring when trying to write data to a buffer
pub enum WriteToError {
    /// The provided buffer is too small
    #[allow(missing_docs)]
    BufferTooSmall { needs: usize, provided: usize },
}

impl Hint {
    /// Returns the size in bytes of the hint
    pub const fn size(&self) -> usize {
        match self {
            Hint::BundleLength(length) => {
                TYPE_SIZE
                    + LENGTH_SIZE
                    // Compute the needed size of the `bundle length` field in bytes
                    + if *length < 0x10000 {
                        if *length < 0x100 { 1 } else { 2 }
                    } else if *length < 0x100000000 { 4 } else { 8 }
            }
        }
    }

    /// Tries to write this [Hint] as bytes into a buffer.
    /// Returns the number of bytes written on success.
    pub fn write_to_buf(&self, buf: &mut [u8]) -> Result<usize, WriteToError> {
        let size = self.size();
        if buf.len() < size {
            return Err(WriteToError::BufferTooSmall {
                needs: size,
                provided: buf.len(),
            });
        }
        let bytes_written = match self {
            Hint::BundleLength(length) => {
                buf[0] = 0;
                let length_size = if *length < 0x10000 {
                    if *length < 0x100 {
                        buf[2] = *length as u8;
                        1
                    } else {
                        buf[2..4].copy_from_slice(&(*length as u16).to_be_bytes());
                        2
                    }
                } else if *length < 0x100000000 {
                    buf[2..6].copy_from_slice(&(*length as u32).to_be_bytes());
                    4
                } else {
                    buf[2..10].copy_from_slice(&(*length).to_be_bytes());
                    8
                };

                buf[1] = length_size;
                length_size as usize
            }
        };

        Ok(TYPE_SIZE + LENGTH_SIZE + bytes_written)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
/// Header of any message
pub struct MessageHeader {
    /// Type of following message
    pub kind: MessageType,
    /// Flags added to this message (section 7.1)
    pub flags: u8,
    /// Length of message including hint
    pub length: u32,
}

impl From<MessageHeader> for [u8; 4] {
    fn from(value: MessageHeader) -> Self {
        let length_array = value.length.to_be_bytes();
        let flags_length_mix: u8 =
            ((value.flags << 4) & 0b11110000) | (length_array[0] & 0b00001111);
        [
            value.kind as u8,
            flags_length_mix,
            length_array[2],
            length_array[3],
        ]
    }
}

impl MessageHeader {
    /// Returns the memory representation of this identifier as a byte array in big-endian (network) byte order.
    fn to_be_bytes(&self) -> [u8; MESSAGE_HEADER_SIZE] {
        self.clone().into()
    }

    /// Tries to write this [MessageHeader] as bytes into a buffer.
    /// Returns the number of bytes written on success.
    fn write_to_buf(&self, buf: &mut [u8]) -> Result<usize, WriteToError> {
        if buf.len() < MESSAGE_HEADER_SIZE {
            return Err(WriteToError::BufferTooSmall {
                needs: MESSAGE_HEADER_SIZE,
                provided: buf.len(),
            });
        }
        buf[..MESSAGE_HEADER_SIZE].copy_from_slice(&self.to_be_bytes());
        Ok(MESSAGE_HEADER_SIZE)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
/// A fragment of a divided [Bundle] that didn't fit entirely in a PDU
pub struct Segment<'a> {
    /// A monotonically decreasing integral index that indicates the relative position
    /// of the [Segment] within the total sequence of [Segment]s
    ///
    /// `0` indicates the final segment
    pub index: u32,
    /// The ifentifier of the associated transfer of segments' sequence
    pub transfer_identifier: TransferIdentifier,
    /// Data of the segment in transfer
    pub data: &'a [u8],
}

impl Segment<'_> {
    /// Tries to write this [Segment] as bytes into a buffer.
    /// Returns the number of bytes written on success.
    pub fn write_to_buf(&self, buf: &mut [u8]) -> Result<usize, WriteToError> {
        // Size of the segment in bytes
        let segment_size = 8 + self.data.len();
        if buf.len() < segment_size {
            return Err(WriteToError::BufferTooSmall {
                needs: segment_size,
                provided: buf.len(),
            });
        }

        buf[..4].copy_from_slice(&self.index.to_be_bytes());
        buf[4..8].copy_from_slice(&self.transfer_identifier.0.to_be_bytes());
        buf[8..8 + self.data.len()].copy_from_slice(self.data);

        Ok(segment_size)
    }

    /// Returns the size in bytes of this segment
    pub fn size(&self) -> usize {
        size_of::<u32>() + size_of::<TransferIdentifier>() + self.data.len()
    }
}

/// An enum of the different message types
#[derive(PartialEq, Debug)]
pub enum Message<'a> {
    /// A padding without a defined length
    IndefinitePadding(usize),
    /// A padding with a defined length
    DefinitePadding(usize),
    /// A message containing an entire bundle
    Bundle {
        /// The content of the bundle
        content: &'a [u8],
    },
    /// The last segment of a bundle
    TransferEnd {
        /// Optional [Hint] accompagning this transfer
        hint: Option<Hint>,
        /// The [Segment] of this transfer
        segment: Segment<'a>,
    },
    /// A segment of a bundle
    TransferSegment {
        /// Optional [Hint] accompagning this transfer
        hint: Option<Hint>,
        /// The [Segment] of this transfer
        segment: Segment<'a>,
    },
    /// A message to cancel a transfer
    TransferCancel(TransferIdentifier),
}

impl Message<'_> {
    /// Tries to write this [Message] as bytes into a buffer.
    /// Returns the number of bytes written on success.
    pub fn write_to_buf(&self, buf: &mut [u8]) -> Result<usize, WriteToError> {
        Ok(match self {
            Message::IndefinitePadding(size) => {
                buf[..size + 1].fill(0);
                size + 1
            }
            Message::DefinitePadding(size) => {
                MessageHeader {
                    kind: MessageType::DefinitePadding,
                    flags: 0,
                    length: *size as u32,
                }
                .write_to_buf(buf)?;

                if buf.len() < (*size + MESSAGE_HEADER_SIZE) {
                    return Err(WriteToError::BufferTooSmall {
                        needs: *size + MESSAGE_HEADER_SIZE,
                        provided: buf.len(),
                    });
                }

                buf[MESSAGE_HEADER_SIZE..*size + MESSAGE_HEADER_SIZE].fill(0);
                *size + MESSAGE_HEADER_SIZE
            }
            Message::Bundle { content } => {
                MessageHeader {
                    kind: MessageType::Bundle,
                    flags: 0,
                    length: content.len() as u32,
                }
                .write_to_buf(buf)?;

                if buf.len() < content.len() {
                    return Err(WriteToError::BufferTooSmall {
                        needs: content.len() + MESSAGE_HEADER_SIZE,
                        provided: buf.len(),
                    });
                }

                buf[MESSAGE_HEADER_SIZE..content.len() + MESSAGE_HEADER_SIZE]
                    .copy_from_slice(content);
                content.len() + MESSAGE_HEADER_SIZE
            }
            Message::TransferEnd { hint, segment } => {
                MessageHeader {
                    kind: MessageType::TransferEnd,
                    flags: if hint.is_some() { HINT_FLAG } else { 0 },
                    length: (segment.size() + hint.map(|hint| hint.size()).unwrap_or(0)) as u32,
                }
                .write_to_buf(buf)?;

                let mut written = MESSAGE_HEADER_SIZE;

                if let Some(hint) = hint {
                    written += hint.write_to_buf(&mut buf[MESSAGE_HEADER_SIZE..])?;
                }

                written + segment.write_to_buf(&mut buf[written..])?
            }
            Message::TransferSegment { hint, segment } => {
                MessageHeader {
                    kind: MessageType::TransferSegment,
                    flags: if hint.is_some() { HINT_FLAG } else { 0 },
                    length: (segment.size() + hint.map(|hint| hint.size()).unwrap_or(0)) as u32,
                }
                .write_to_buf(buf)?;

                let mut written = MESSAGE_HEADER_SIZE;

                if let Some(hint) = hint {
                    written += hint.write_to_buf(&mut buf[MESSAGE_HEADER_SIZE..])?;
                }

                written + segment.write_to_buf(&mut buf[written..])?
            }
            Message::TransferCancel(transfer_identifier) => {
                MessageHeader {
                    kind: MessageType::TransferCancel,
                    flags: 0,
                    length: 4,
                }
                .write_to_buf(buf)?;

                if buf.len() < 4 + MESSAGE_HEADER_SIZE {
                    return Err(WriteToError::BufferTooSmall {
                        needs: 4 + MESSAGE_HEADER_SIZE,
                        provided: buf.len(),
                    });
                }
                buf[MESSAGE_HEADER_SIZE..4 + MESSAGE_HEADER_SIZE]
                    .copy_from_slice(&transfer_identifier.0.to_be_bytes());
                4 + MESSAGE_HEADER_SIZE
            }
        })
    }
}

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
        } else if size > 2usize.pow(20) {
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
            let _ = self.repeat.saturating_sub(1);
            self.bytes_read = self.bundle_buffer.len();
            Some(Message::Bundle {
                content: self.bundle_buffer,
            })
        } else if self.bytes_read < self.bundle_buffer.len() - 1 {
            let segment_index = self.segment_index;
            self.segment_index += 1;
            let start_byte = self.bytes_read;

            let remaining_bytes = self.bundle_buffer.len() - self.bytes_read;

            let hint = crate::message::Hint::BundleLength(
                self.bundle_buffer
                    .len()
                    .try_into()
                    .expect("Bundle buffer size is larger than u64 max value"),
            );

            if remaining_bytes
                <= mtu.saturating_sub(
                    hint.size() + size_of::<TransferIdentifier>() + size_of::<u32>(),
                )
            {
                self.bytes_read = self.bundle_buffer.len();
                Some(Message::TransferEnd {
                    hint: Some(hint),
                    segment: Segment {
                        transfer_identifier: self.transfer_identifier,
                        index: segment_index,
                        data: &self.bundle_buffer[start_byte..self.bundle_buffer.len()],
                    },
                })
            } else {
                mtu = mtu.saturating_sub(
                    hint.size() + size_of::<TransferIdentifier>() + size_of::<u32>(),
                );

                self.bytes_read += mtu;
                let data = &self.bundle_buffer[start_byte..self.bytes_read];

                Some(Message::TransferSegment {
                    hint: Some(hint),
                    segment: Segment {
                        transfer_identifier: self.transfer_identifier,
                        index: segment_index,
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
        TransferIdentifier,
        message::{Hint, MESSAGE_HEADER_SIZE, Message, MessageIter, PduSize, Segment},
    };

    #[test]
    fn indefinite_padding_message() {
        let mut out = [0; 9];
        Message::IndefinitePadding(8)
            .write_to_buf(&mut out)
            .unwrap();
        assert_eq!(out, [0, 0, 0, 0, 0, 0, 0, 0, 0]);
    }

    #[test]
    fn definite_padding_message() {
        let mut out = [0; 8 + MESSAGE_HEADER_SIZE];
        Message::DefinitePadding(8).write_to_buf(&mut out).unwrap();
        assert_eq!(out, [1, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0]);
    }

    #[test]
    fn bundle_message() {
        let mut out = [0; 10 + MESSAGE_HEADER_SIZE];
        Message::Bundle {
            content: &[128; 10],
        }
        .write_to_buf(&mut out)
        .unwrap();
        assert_eq!(
            out,
            [
                2, 0, 0, 10, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
            ]
        );
    }

    #[test]
    fn transfer_segment_message() {
        let mut out = [0; 19];
        Message::TransferSegment {
            hint: Some(Hint::BundleLength(4)),
            segment: Segment {
                index: 5,
                transfer_identifier: TransferIdentifier(7),
                data: &[128; 4],
            },
        }
        .write_to_buf(&mut out)
        .unwrap();
        assert_eq!(
            out,
            [
                3, 0b10000000, 0, 15, 0, 1, 4, 0, 0, 0, 5, 0, 0, 0, 7, 128, 128, 128, 128
            ]
        );
    }

    #[test]
    fn transfer_end_message() {
        let mut out = [0; 19];
        Message::TransferEnd {
            hint: Some(Hint::BundleLength(4)),
            segment: Segment {
                index: 5,
                transfer_identifier: TransferIdentifier(7),
                data: &[128; 4],
            },
        }
        .write_to_buf(&mut out)
        .unwrap();
        assert_eq!(
            out,
            [
                4, 0b10000000, 0, 15, 0, 1, 4, 0, 0, 0, 5, 0, 0, 0, 7, 128, 128, 128, 128
            ]
        );
    }

    #[test]
    fn transfer_cancel_message() {
        let mut out = [0; 8];
        Message::TransferCancel(TransferIdentifier(7))
            .write_to_buf(&mut out)
            .unwrap();
        assert_eq!(out, [5, 0, 0, 4, 0, 0, 0, 7]);
    }

    #[test]
    fn single_message() {
        let mut iter = MessageIter::new(&[0; 500], crate::TransferIdentifier(1), 0);

        assert_eq!(
            iter.next(PduSize::new(504).unwrap()),
            Some(Message::Bundle { content: &[0; 500] })
        );

        assert_eq!(iter.next(PduSize::new(12).unwrap()), None)
    }

    #[test]
    fn multiple_messages() {
        let mut bundle_buffer: [u8; 1536] = [0; 1536];
        let pdu = 512;

        const HINT_SIZE: usize = 4;
        const TRANSFER_IDENTIFIER_SIZE: usize = size_of::<TransferIdentifier>();
        const SEGMENT_INDEX_SIZE: usize = size_of::<u32>();

        for (index, value) in bundle_buffer.iter_mut().enumerate() {
            *value = (index % 256) as u8;
        }

        let mut iter = MessageIter::new(&bundle_buffer, crate::TransferIdentifier(1), 0);

        let end =
            pdu - MESSAGE_HEADER_SIZE - HINT_SIZE - TRANSFER_IDENTIFIER_SIZE - SEGMENT_INDEX_SIZE;
        let mut buf = [0u8; 512];
        let mesg = iter.next(pdu.try_into().unwrap()).unwrap();
        mesg.write_to_buf(&mut buf).unwrap();

        assert_eq!(
            Some(mesg),
            Some(Message::TransferSegment {
                hint: Some(Hint::BundleLength(1536)),
                segment: Segment {
                    index: 0,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[0..end]
                }
            })
        );

        let start = end;
        let end = end + pdu
            - MESSAGE_HEADER_SIZE
            - HINT_SIZE
            - TRANSFER_IDENTIFIER_SIZE
            - SEGMENT_INDEX_SIZE;

        assert_eq!(
            iter.next(pdu.try_into().unwrap()),
            Some(Message::TransferSegment {
                hint: Some(Hint::BundleLength(1536)),
                segment: Segment {
                    index: 1,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[start..end]
                }
            })
        );

        let start = end;
        let end = end + pdu
            - MESSAGE_HEADER_SIZE
            - HINT_SIZE
            - TRANSFER_IDENTIFIER_SIZE
            - SEGMENT_INDEX_SIZE;

        assert_eq!(
            iter.next(pdu.try_into().unwrap()),
            Some(Message::TransferSegment {
                hint: Some(Hint::BundleLength(1536)),
                segment: Segment {
                    index: 2,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[start..end]
                }
            })
        );

        assert_eq!(
            iter.next(pdu.try_into().unwrap()),
            Some(Message::TransferEnd {
                hint: Some(Hint::BundleLength(1536)),
                segment: Segment {
                    index: 3,
                    transfer_identifier: crate::TransferIdentifier(1),
                    data: &bundle_buffer[end..]
                }
            })
        );

        assert_eq!(iter.next(pdu.try_into().unwrap()), None);
    }
}
