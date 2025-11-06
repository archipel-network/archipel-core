//! A module containing the bundle transfer protocol message logic
//!
//! Message format :
//!
//!  0                   1                   2                   3
//!  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//! +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//! |     Type      |Flags|      Length (20-bit unsigned int)       |
//! +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//! |              ... Optional Metadata Items ...                  :
//! +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//! |                       ... Content ...                         :
//! +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

use crate::TransferIdentifier;

/// Flag indicating that further metdata is included in message
pub const METADATA_FLAG: u8 = 0b00001000;

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
    /// Message is a transfer start message (section 8.2)
    TransferStart = 3,
    /// Message is a segment transfer message (section 8.3)
    TransferSegment = 4,
    /// Message is a transfer cancel message (Section 8.4)
    TransferCancel = 5,
    // 6 is reserved to avoid clash with BPv6
}

/// Different types of metadata included in some messages (section 7.2)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Metadata {
    /// Ful length of bundle to be transfered (section 9.1)
    BundleLength(u64), // pub struct Metadata {
                       //     kind: MessageType,
                       //     length: u8,
                       //     content: [u8; 8],
}

/// The size of the metadata type field in bytes
const TYPE_SIZE: usize = 1;

/// The size of the metadata length field in bytes
const LENGTH_SIZE: usize = 1;

/// Error occuring when trying to write data to a buffer
pub enum WriteToError {
    /// The provided buffer is too small
    #[allow(missing_docs)]
    BufferTooSmall { needs: usize, provided: usize },
}

impl Metadata {
    /// Returns the size in bytes of the metadata
    pub const fn size(&self) -> usize {
        match self {
            Metadata::BundleLength(length) => {
                TYPE_SIZE
                    + LENGTH_SIZE
                    // Compute the needed size of the `bundle length` field in bytes
                    + if *length < 0x10000 {
                        if *length < 0x100 { 1 } else { 2 }
                    } else if *length < 0x100000000 { 4 } else { 8 }
            }
        }
    }

    /// Tries to write this [Metadata] as bytes into a buffer.
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
            Metadata::BundleLength(length) => {
                buf[0] = 1;
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
    /// Length of message including metadata
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
    fn to_be_bytes(&self) -> [u8; 4] {
        self.clone().into()
    }

    /// Tries to write this [MessageHeader] as bytes into a buffer.
    /// Returns the number of bytes written on success.
    fn write_to_buf(&self, buf: &mut [u8]) -> Result<usize, WriteToError> {
        if buf.len() < 4 {
            return Err(WriteToError::BufferTooSmall {
                needs: 4,
                provided: buf.len(),
            });
        }
        buf[..4].copy_from_slice(&self.to_be_bytes());
        Ok(4)
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
        buf[8..self.data.len()].copy_from_slice(self.data);

        Ok(segment_size)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
/// Content of message
pub enum MessageContent<'a> {
    /// An indefinite length padding message (section 8.6)
    IndefinitePadding(&'a [u8]),
    /// A padding message (section 8.5)
    DefinitePadding(&'a [u8]),
    /// A single bundle message (section 8.1)
    BundleMessage(&'a [u8]),
    /// A transfer start message (section 8.2)
    TransferStart(Segment<'a>),
    /// A segment transfer message (section 8.3)
    TransferSegment(Segment<'a>),
    /// A transfer cancellation message (section 8.4)
    TransferCancel(TransferIdentifier),
}

impl MessageContent<'_> {
    /// Tries to write this [MessageContent] as bytes into a buffer.
    /// Returns the number of bytes written on success.
    pub fn write_to_buf(&self, buf: &mut [u8]) -> Result<usize, WriteToError> {
        Ok(match self {
            MessageContent::IndefinitePadding(content)
            | MessageContent::DefinitePadding(content)
            | MessageContent::BundleMessage(content) => {
                if buf.len() < content.len() {
                    return Err(WriteToError::BufferTooSmall {
                        needs: content.len(),
                        provided: buf.len(),
                    });
                }

                buf[..content.len()].copy_from_slice(content);
                content.len()
            }
            MessageContent::TransferStart(segment) | MessageContent::TransferSegment(segment) => {
                segment.write_to_buf(buf)?
            }
            MessageContent::TransferCancel(transfer_identifier) => {
                if buf.len() < 4 {
                    return Err(WriteToError::BufferTooSmall {
                        needs: 4,
                        provided: buf.len(),
                    });
                }
                buf[..4].copy_from_slice(&transfer_identifier.0.to_be_bytes());
                4
            }
        })
    }
}

#[derive(Debug, PartialEq, Eq)]
/// A message sent or received
pub struct Message<'a> {
    /// The header of this message
    pub header: MessageHeader,
    /// Optional metadata for this message
    pub metadata: Option<Metadata>,
    /// Content of this message
    pub content: MessageContent<'a>,
}

impl<'a> Message<'a> {
    /// Tries to write this [Message] as bytes into a buffer.
    /// Returns the number of bytes written on success.
    pub fn write_to_buf(&self, buf: &mut [u8]) -> Result<usize, WriteToError> {
        let mut bytes_written = self.header.write_to_buf(buf)?;

        if let Some(metadata) = self.metadata {
            bytes_written += metadata.write_to_buf(&mut buf[bytes_written..])?;
        }

        bytes_written += self.content.write_to_buf(&mut buf[bytes_written..])?;

        Ok(bytes_written)
    }
}

// #[cfg(test)]
// mod tests {
//     use crate::message::MessageHeader;

//     #[test]
//     fn message_size() {
//         assert_eq!(size_of::<MessageHeader>() * 8, 32);
//     }
// }
