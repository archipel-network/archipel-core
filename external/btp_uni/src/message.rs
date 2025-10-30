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
    /// Message is a transfert start message (section 8.2)
    TransferStart = 3,
    /// Message is a segment transfert message (section 8.3)
    TransferSegment = 4,
    /// Message is a transfert cancel message (Section 8.4)
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

#[derive(Debug, Clone, PartialEq, Eq)]
/// Message starting a new transfert
pub struct TransfertStartMessage<'a> {
    /// Identifier of this transfert
    pub transfert_number: TransferIdentifier,
    /// index of first segment in this transfert
    pub segment_index: u32,
    /// Data of first segment in transfert
    pub data: &'a [u8],
}

#[derive(Debug, Clone, PartialEq, Eq)]
/// A segment transfert message
pub struct TransfertSegmentMessage<'a> {
    /// Identifier of trnasfert this segmnt belongs to
    pub transfert_number: TransferIdentifier,
    /// Index of this segment
    pub segment_index: u32,
    /// Data of this segment
    pub data: &'a [u8],
}

#[derive(Debug, Clone, PartialEq, Eq)]
/// Message cancelling a transfert
pub struct TransfertCancelMessage {
    /// Identifier of cancelled transfert
    pub transfert_number: TransferIdentifier,
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
    /// A transfert start message (section 8.2)
    TransferStart(TransfertStartMessage<'a>),
    /// A segment transfert message (section 8.3)
    TransferSegment(TransfertSegmentMessage<'a>),
    /// A transfert cancellation message (section 8.4)
    TransferCancel(TransfertCancelMessage),
}

/// A message sent or received
pub struct Message<'a> {
    /// Content of this message
    pub content: MessageContent<'a>,
    /// Optional metadata for this message
    pub metadata: Option<Metadata>,
}

#[cfg(test)]
mod tests {
    use crate::message::MessageHeader;

    #[test]
    fn message_size() {
        assert_eq!(size_of::<MessageHeader>() * 8, 32);
    }
}
