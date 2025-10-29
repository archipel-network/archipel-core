use crate::{TransferIdentifier, message::{METADATA_FLAG, Message, MessageHeader, MessageType, Metadata, TransfertCancelMessage, TransfertSegmentMessage, TransfertStartMessage}};

///! Parser for Bundle Transport Protocol unidirectional messages

/// Parser struct
pub struct Parser;

/// Error occuring during parsing
#[derive(Debug, PartialEq, Eq)]
pub enum ParseError {
    /// Bytes are missing to complete parsing
    IncompleteInput,
    /// Maybe bytes are missing to complete a indefinite length padding
    MaybeIncompleteInput,
    /// Message type is ujnknown
    UnknownType(u8),
    /// Metadata type was unknown
    UnknownMetadataType(u8),
    /// Parsing of Bundle length metadata failed
    InvalidBundleLengthMetadata,
    /// Invalid message content
    InvalidContent
}

impl TryFrom<u8> for MessageType {
    type Error = u8;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(Self::IndefinitePadding),
            1 => Ok(Self::DefinitePadding),
            2 => Ok(Self::Bundle),
            3 => Ok(Self::TransferStart),
            4 => Ok(Self::TransferSegment),
            5 => Ok(Self::TransferCancel),
            other => Err(other)
        }
    }
}

impl Parser {

    /// Create a new parser
    pub fn new() -> Self {
        Parser
    }
    
    /// Parse a full message with content slice
    pub fn parse_message<'a>(&mut self, buffer: &'a [u8]) -> Result<(Message<'a>, usize), ParseError> {
        let (header, header_length) = self.parse_message_header(buffer)?;
        let content_length = header.content_length;
        
        // Special case for indefinite padding message
        if matches!(header.kind, MessageType::IndefinitePadding) {
            let mut padding_length = 0;
            for char in &buffer[header_length..] {
                if char == &('\0' as u8) {
                    padding_length += 1;
                } else {
                    return Ok((
                        Message::IndefinitePadding(&buffer[header_length..(header_length+padding_length)]),
                        header_length+padding_length
                    ));
                }
            }
            return Err(ParseError::MaybeIncompleteInput)
        }

        let content_buffer = &buffer[header_length..(header_length+content_length as usize)];
        if content_buffer.len() < content_length as usize {
            return Err(ParseError::IncompleteInput)
        }

        match header.kind {
            MessageType::IndefinitePadding => 
                panic!("Idefinite padding message should be handled before"),
            MessageType::DefinitePadding => {
                Ok((
                    Message::DefinitePadding(content_buffer),
                    header_length+content_length as usize
                ))
            },
            MessageType::Bundle => {
                Ok((
                    Message::BundleMessage(content_buffer),
                    header_length+content_length as usize
                ))
            },
            MessageType::TransferStart => {
                let transfert_number_bytes: [u8; 4] = content_buffer[0..4]
                    .try_into().map_err(|_| ParseError::IncompleteInput)?;
                let transfert_number = u32::from_be_bytes(transfert_number_bytes);

                let segment_index_bytes: [u8; 4] = content_buffer[4..8]
                    .try_into().map_err(|_| ParseError::IncompleteInput)?;
                let segment_index = u32::from_be_bytes(segment_index_bytes);

                Ok((
                    Message::TransferStart(TransfertStartMessage {
                        transfert_number: TransferIdentifier(transfert_number),
                        segment_index: segment_index,
                        data: &content_buffer[8..]
                    }),
                    header_length+content_length as usize
                ))
            },
            MessageType::TransferSegment => {
                let transfert_number_bytes: [u8; 4] = content_buffer[0..4]
                    .try_into().map_err(|_| ParseError::IncompleteInput)?;
                let transfert_number = u32::from_be_bytes(transfert_number_bytes);

                let segment_index_bytes: [u8; 4] = content_buffer[4..8]
                    .try_into().map_err(|_| ParseError::IncompleteInput)?;
                let segment_index = u32::from_be_bytes(segment_index_bytes);

                Ok((
                    Message::TransferSegment(TransfertSegmentMessage {
                        transfert_number: TransferIdentifier(transfert_number),
                        segment_index: segment_index,
                        data: &content_buffer[8..]
                    }),
                    header_length+content_length as usize
                ))
            },
            MessageType::TransferCancel => {
                if content_buffer.len() != 4 {
                    return Err(ParseError::InvalidContent);
                }

                let transfert_number_bytes: [u8; 4] = content_buffer[0..4]
                    .try_into().map_err(|_| ParseError::IncompleteInput)?;
                let transfert_number = u32::from_be_bytes(transfert_number_bytes);

                Ok((
                    Message::TransferCancel(TransfertCancelMessage {
                        transfert_number: TransferIdentifier(transfert_number)
                    }),
                    header_length+4
                ))
            },
        }
    }

    /// Parse a message header from a buffer
    pub fn parse_message_header(&mut self, buffer: &[u8]) -> Result<(MessageHeader, usize), ParseError> {
        let kind_bytes: [u8;1] = buffer[0..1]
            .try_into().map_err(|_| ParseError::IncompleteInput)?;

        let kind = MessageType::try_from(u8::from_be_bytes(kind_bytes))
            .map_err(|other| ParseError::UnknownType(other))?;
    
        if matches!(kind, MessageType::IndefinitePadding){
            return Ok((MessageHeader {
                kind: kind,
                flags: 0,
                length: 0,
                content_length: 0,
                metadata: None
            }, 1));
        }

        let flags_bytes: [u8;1] = buffer[1..2]
            .try_into().map_err(|_| ParseError::IncompleteInput)?;

        let flags: u8 = ( u8::from_be_bytes(flags_bytes) & 0b11110000 ) >> 4;

        let length_byte: [u8; 3] = buffer[1..4].try_into().map_err(|_| ParseError::IncompleteInput)?;
        let mut length_bytes_buffer = [0_u8;4];
        length_bytes_buffer.as_mut_slice()[1..4].copy_from_slice(&length_byte);
        length_bytes_buffer[1] &= 0b00001111;

        let length = u32::from_be_bytes(length_bytes_buffer);
        let mut content_length = length;

        let mut byte_red = 4;

        return Ok((
            MessageHeader {
                kind,
                flags, 
                length,
                metadata: if (flags & METADATA_FLAG) == METADATA_FLAG {
                    let (meta, length) = self.parse_metadata(&buffer[4..])?;
                    byte_red += length;
                    content_length -= length as u32;
                    Some(meta)
                } else { None },
                content_length
            },
            byte_red
        ))
    }

    /// Parse a single metadata item in a buffer
    pub fn parse_metadata(&mut self, buffer: &[u8]) -> Result<(Metadata, usize), ParseError> {
        let kind_bytes = buffer[0..1]
            .try_into().map_err(|_| ParseError::IncompleteInput)?;
        let kind = u8::from_be_bytes(kind_bytes);

        let length_bytes = buffer[1..2]
            .try_into().map_err(|_| ParseError::IncompleteInput)?;
        let length = u8::from_be_bytes(length_bytes);

        return match kind {
            1 => match length {
                1 => {
                    let bytes: [u8; 1] = buffer[2..3]
                        .try_into().map_err(|_| ParseError::IncompleteInput)?;
                    let bundle_length = u8::from_be_bytes(bytes);
                    Ok((Metadata::BundleLength(bundle_length as u64), 3))
                },
                2 => {
                    let bytes: [u8; 2] = buffer[2..4]
                        .try_into().map_err(|_| ParseError::IncompleteInput)?;
                    let bundle_length = u16::from_be_bytes(bytes);
                    Ok((Metadata::BundleLength(bundle_length as u64), 4))
                },
                4 => {
                    let bytes: [u8; 4] = buffer[2..6]
                        .try_into().map_err(|_| ParseError::IncompleteInput)?;
                    let bundle_length = u32::from_be_bytes(bytes);
                    Ok((Metadata::BundleLength(bundle_length as u64), 6))
                },
                8 => {
                    let bytes: [u8; 8] = buffer[2..10]
                        .try_into().map_err(|_| ParseError::IncompleteInput)?;
                    let bundle_length = u64::from_be_bytes(bytes);
                    Ok((Metadata::BundleLength(bundle_length as u64), 10))
                },
                _ => Err(ParseError::InvalidBundleLengthMetadata)
            }
            other => Err(ParseError::UnknownMetadataType(other))
        }
    }

}

#[cfg(test)]
mod test {
    use crate::{message::{METADATA_FLAG, MessageHeader, MessageType, Metadata}, parser::Parser};


    #[test]
    fn indefinite_padding_message_header(){
        assert_eq!(
            Parser::new()
                .parse_message_header(&[
                    0x0, // Type
                    0x0, // Flags & length
                    0x0, 0x0 // length
                ]),
            Ok((MessageHeader {
                kind: MessageType::IndefinitePadding,
                flags: 0,
                length: 0,
                content_length: 0,
                metadata: None
            }, 1))
        );
    }

    #[test]
    fn big_message(){
        assert_eq!(
            Parser::new()
                .parse_message_header(&[
                    3, // Type
                    0x0, // Flags & length
                    0x0, 0x0F // length
                ]),
            Ok((MessageHeader {
                kind: MessageType::TransferStart,
                flags: 0,
                length: 15,
                content_length: 15,
                metadata: None
            }, 4))
        );
    }

    #[test]
    fn big_message_with_meta(){
        assert_eq!(
            Parser::new()
                .parse_message_header(&[
                    3, // Type
                    0b10000000 | 0x03, // Flags (with metadata) & length
                    0xff, 0x75, // length
                    1, 1, 10
                ]),
            Ok((MessageHeader {
                kind: MessageType::TransferStart,
                flags: METADATA_FLAG,
                length: 262005,
                content_length: 262005 - 3,
                metadata: Some(Metadata::BundleLength(10))
            }, 7))
        );

        assert_eq!(
            Parser::new()
                .parse_message_header(&[
                    3, // Type
                    0b10000000 | 0x03, // Flags (with metadata) & length
                    0xff, 0x75, // length
                    1, 2, 0, 10
                ]),
            Ok((MessageHeader {
                kind: MessageType::TransferStart,
                flags: METADATA_FLAG,
                length: 262005,
                content_length: 262005 - 4,
                metadata: Some(Metadata::BundleLength(10))
            }, 8))
        );

        assert_eq!(
            Parser::new()
                .parse_message_header(&[
                    3, // Type
                    0b10000000 | 0x03, // Flags (with metadata) & length
                    0xff, 0x75, // length
                    1, 4, 0, 0, 0, 10
                ]),
            Ok((MessageHeader {
                kind: MessageType::TransferStart,
                flags: METADATA_FLAG,
                length: 262005,
                content_length: 262005 - 6,
                metadata: Some(Metadata::BundleLength(10))
            }, 10))
        );

        assert_eq!(
            Parser::new()
                .parse_message_header(&[
                    3, // Type
                    0b10000000 | 0x03, // Flags (with metadata) & length
                    0xff, 0x75, // length
                    1, 8, 0, 0, 0, 0, 0, 0, 0, 10
                ]),
            Ok((MessageHeader {
                kind: MessageType::TransferStart,
                flags: METADATA_FLAG,
                length: 262005,
                content_length: 262005 - 10,
                metadata: Some(Metadata::BundleLength(10))
            }, 14))
        );
    }

}