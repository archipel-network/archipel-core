//! Parser for Bundle Transport Protocol unidirectional messages

use crate::{TransferIdentifier, message::{METADATA_FLAG, Message, MessageContent, MessageHeader, MessageType, Metadata, TransfertCancelMessage, TransfertSegmentMessage, TransfertStartMessage}};

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
                        Message {
                            content: MessageContent::IndefinitePadding(&buffer[header_length..(header_length+padding_length)]),
                            metadata: header.metadata
                        },
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
                    Message { content: MessageContent::DefinitePadding(content_buffer), metadata: header.metadata },
                    header_length+content_length as usize
                ))
            },
            MessageType::Bundle => {
                Ok((
                    Message { content: MessageContent::BundleMessage(content_buffer), metadata: header.metadata },
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
                    Message { content: MessageContent::TransferStart(TransfertStartMessage {
                        transfert_number: TransferIdentifier(transfert_number),
                        segment_index: segment_index,
                        data: &content_buffer[8..]
                    }), metadata: header.metadata },
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
                    Message { content: MessageContent::TransferSegment(TransfertSegmentMessage {
                        transfert_number: TransferIdentifier(transfert_number),
                        segment_index: segment_index,
                        data: &content_buffer[8..]
                    }), metadata: header.metadata },
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
                    Message { content: MessageContent::TransferCancel(TransfertCancelMessage {
                        transfert_number: TransferIdentifier(transfert_number)
                    }), metadata: header.metadata},
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
    use crate::{TransferIdentifier, message::{METADATA_FLAG, MessageContent, MessageHeader, MessageType, Metadata}, parser::{ParseError, Parser}};


    #[test]
    fn indefinite_padding_message(){
        let bytes = [
                    0x0, // Type
                    0x0, // Flags & length
                    0x0, 0x0, // length
                    0x0, 0x0, 0x0, 0x0, 0x1, 
                ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let MessageContent::IndefinitePadding(padding_content) = message.content else {
            panic!("Expected Indefinite length padding message");
        };
        
        assert_eq!(message.metadata, None);
        assert_eq!(bytes_red, 8);
        assert_eq!(padding_content.len(), 7);
    }

    #[test]
    fn bundle_message(){
        let bytes = [
                    0x2, // Type
                    0x0, // Flags & length
                    0x0, 121, // length
                    // Bundle content
                    0x9f,0x89,0x07,0x00,0x01,0x82,0x01,0x71,0x2f,0x2f,0x65,0x78,0x61,0x6d,0x70,0x6c,0x65,0x2e,0x6f,0x72,0x67,0x2f,0x6c,0x6f,0x67,0x82,0x01,0x78,0x33,0x2f,
                    0x2f,0x65,0x70,0x69,0x63,0x6b,0x69,0x77,0x69,0x2e,0x64,0x74,0x6e,0x2f,0x36,0x65,0x31,0x34,0x32,0x39,0x37,0x32,0x2d,0x35,0x35,0x65,0x32,0x2d,0x34,0x33,
                    0x63,0x37,0x2d,0x38,0x64,0x62,0x32,0x2d,0x37,0x31,0x65,0x31,0x63,0x65,0x38,0x31,0x66,0x33,0x31,0x62,0x82,0x01,0x00,0x82,0x1b,0x00,0x00,0x00,0xbd,0xc4,
                    0x7b,0xae,0x02,0x01,0x1a,0x05,0x26,0x5c,0x00,0x42,0x58,0xbc,0x85,0x01,0x01,0x00,0x00,0x4c,0x48,0x65,0x6c,0x6c,0x6f,0x20,0x77,0x6f,0x72,0x6c,0x64,0x0a,
                    0xff
                ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let MessageContent::BundleMessage(bundle_content) = message.content else {
            panic!("Expected bundle message");
        };
        
        assert_eq!(message.metadata, None);
        assert_eq!(bytes_red, 125);
        assert_eq!(bundle_content.len(), 121);
    }

    #[test]
    fn transfer_start_message(){
        let bytes = [
                    0x3, // Type
                    METADATA_FLAG << 4, // Flags (with metadata) & length
                    0x0, 26, // length
                    // Bundle length metadata
                    1, 1, 121,
                    0x0, 0x0, 0x0, 64, // Transfer number
                    0x0, 0x0, 0x0, 8, // Segment index
                    // Bundle content
                    0x9f,0x89,0x07,0x00,0x01,0x82,0x01,0x71,0x2f,0x2f,0x65,0x78,0x61,0x6d,0x70
                ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let MessageContent::TransferStart(start_message) = message.content else {
            panic!("Expected a transfer start message");
        };
        
        assert_eq!(bytes_red, 30);
        assert_eq!(message.metadata, Some(Metadata::BundleLength(121)));
        assert_eq!(start_message.transfert_number, TransferIdentifier(64));
        assert_eq!(start_message.segment_index, 8);
        assert_eq!(start_message.data.len(), 15);
    }

    #[test]
    fn transfer_segment_message(){
        let bytes = [
                    0x4, // Type
                    0, // Flags (with metadata) & length
                    0x0, 23, // length
                    0x0, 0x0, 0x0, 64, // Transfer number
                    0x0, 0x0, 0x0, 7, // Segment index
                    // Bundle content
                    0x6c,0x65,0x2e,0x6f,0x72,0x67,0x2f,0x6c,0x6f,0x67,0x82,0x01,0x78,0x33,0x2f
                ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let MessageContent::TransferSegment(start_message) = message.content else {
            panic!("Expected a transfer segment message");
        };
        
        assert_eq!(bytes_red, 27);
        assert_eq!(message.metadata, None);
        assert_eq!(start_message.transfert_number, TransferIdentifier(64));
        assert_eq!(start_message.segment_index, 7);
        assert_eq!(start_message.data.len(), 15);
    }

    #[test]
    fn transfer_cancel_message(){
        let bytes = [
                    0x5, // Type
                    0, // Flags (with metadata) & length
                    0x0, 4, // length
                    0x0, 0x0, 0x0, 64, // Transfer number
                ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let MessageContent::TransferCancel(cancel_message) = message.content else {
            panic!("Expected a transfer cancel message");
        };
        
        assert_eq!(bytes_red, 8);
        assert_eq!(message.metadata, None);
        assert_eq!(cancel_message.transfert_number, TransferIdentifier(64));
    }
    #[test]
    fn incomplete_input(){
        let bytes = [
                    0x3, // Type
                    METADATA_FLAG << 4, // Flags (with metadata) & length
                    0x0, 26, // length
                    // Bundle length metadata
                    1, 1, 121,
                    0x0, /* ...incomplete input... */
                ];

        let Err(error) = Parser::new()
            .parse_message(&bytes) else {
                panic!("Parsing should fail");
        };
        assert_eq!(error, ParseError::IncompleteInput)
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