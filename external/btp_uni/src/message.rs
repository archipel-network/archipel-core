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

const METADATA_FLAG: u8 = 0b1000;

#[repr(u8)]
pub enum MessageType {
    IndefinitePadding = 0,
    DefinitePadding = 1,
    Bundle = 2,
    TransferStart = 3,
    TransferSegment = 4,
    TransferCancel = 5,
    // 6 is reserved to avoid clash with BPv6
}

pub struct Metadata;

#[repr(packed)]
pub struct Message {
    kind: MessageType,
    flags: u8,
    length: u32,
    metadata_items: Option<Metadata>,
    content: [u8; 8],
}

#[cfg(test)]
mod tests {
    use crate::message::Message;

    #[test]
    fn message_size() {
        assert_eq!(size_of::<Message>() * 8, 32);
    }
}
