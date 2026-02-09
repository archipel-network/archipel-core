//! Parser for Bundle Transport Protocol unidirectional messages

use crate::{
    TransferIdentifier,
    message::{METADATA_FLAG, Message, MessageHeader, MessageType, Metadata, Segment},
};

/// Parser structure
pub struct Parser;

/// Error occuring during parsing
#[derive(Debug, PartialEq, Eq)]
pub enum ParseError {
    /// Bytes are missing to complete parsing
    IncompleteInput,
    /// Maybe bytes are missing to complete a indefinite length padding
    MaybeIncompleteInput,
    /// Message type is unknown
    UnknownType(u8),
    /// Metadata type was unknown
    UnknownMetadataType(u8),
    /// Parsing of Bundle length metadata failed
    InvalidBundleLengthMetadata,
    /// Invalid message content
    InvalidContent,
}

impl TryFrom<u8> for MessageType {
    type Error = u8;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(Self::IndefinitePadding),
            1 => Ok(Self::DefinitePadding),
            2 => Ok(Self::Bundle),
            3 => Ok(Self::TransferEnd),
            4 => Ok(Self::TransferSegment),
            5 => Ok(Self::TransferCancel),
            other => Err(other),
        }
    }
}

macro_rules! parser_get {
    ($source:expr, $range:expr) => {
        $source
            .get($range)
            .ok_or(ParseError::IncompleteInput)
            .and_then(|it| it.try_into().map_err(|_| ParseError::IncompleteInput))
    };
}

impl Parser {
    /// Create a new parser
    pub fn new() -> Self {
        Parser
    }

    /// Parse a full message with content slice
    pub fn parse_message<'a>(
        &mut self,
        buffer: &'a [u8],
    ) -> Result<(Message<'a>, usize), ParseError> {
        let (header, mut bytes_read) = self.parse_message_header(buffer)?;
        let (metadata, metadata_length) = if (header.flags & METADATA_FLAG) == METADATA_FLAG {
            let (meta, length) = self.parse_metadata(&buffer[4..])?;
            bytes_read += length;
            (Some(meta), length)
        } else {
            (None, 0)
        };

        let content_length = header.length as usize - metadata_length;

        // Special case for indefinite padding message
        if matches!(header.kind, MessageType::IndefinitePadding) {
            let mut padding_length = 0;
            for char in &buffer[bytes_read..] {
                if char == &b'\0' {
                    padding_length += 1;
                } else {
                    return Ok((
                        Message::IndefinitePadding(padding_length),
                        bytes_read + padding_length,
                    ));
                }
            }
            return Err(ParseError::MaybeIncompleteInput);
        }

        let content_buffer = &buffer
            .get(bytes_read..(bytes_read + content_length))
            .ok_or(ParseError::IncompleteInput)?;

        match header.kind {
            MessageType::IndefinitePadding => {
                panic!("Indefinite padding message should be handled beforehand")
            }
            MessageType::DefinitePadding => Ok((
                Message::DefinitePadding(content_buffer.len()),
                bytes_read + content_length,
            )),
            MessageType::Bundle => Ok((
                Message::Bundle {
                    content: content_buffer,
                },
                bytes_read + content_length,
            )),
            MessageType::TransferEnd => {
                let transfer_number = u32::from_be_bytes(parser_get!(content_buffer, 0..4)?);
                let segment_index = u32::from_be_bytes(parser_get!(content_buffer, 4..8)?);

                Ok((
                    Message::TransferEnd {
                        metadata,
                        segment: Segment {
                            index: segment_index,
                            transfer_identifier: TransferIdentifier(transfer_number),
                            data: &content_buffer[8..],
                        },
                    },
                    bytes_read + content_length,
                ))
            }
            MessageType::TransferSegment => {
                let transfer_number = u32::from_be_bytes(parser_get!(content_buffer, 0..4)?);
                let segment_index = u32::from_be_bytes(parser_get!(content_buffer, 4..8)?);

                Ok((
                    Message::TransferSegment {
                        metadata,
                        segment: Segment {
                            index: segment_index,
                            transfer_identifier: TransferIdentifier(transfer_number),
                            data: &content_buffer[8..],
                        },
                    },
                    bytes_read + content_length,
                ))
            }
            MessageType::TransferCancel => {
                if content_buffer.len() != 4 {
                    return Err(ParseError::InvalidContent);
                }

                let transfer_number = u32::from_be_bytes(parser_get!(content_buffer, 0..4)?);

                Ok((
                    Message::TransferCancel(TransferIdentifier(transfer_number)),
                    bytes_read + 4,
                ))
            }
        }
    }

    /// Parse a message header from a buffer
    ///
    /// Returns a tuple of [MessageHeader] and the number of bytes read on success
    pub fn parse_message_header(
        &mut self,
        buffer: &[u8],
    ) -> Result<(MessageHeader, usize), ParseError> {
        let kind_bytes: [u8; 1] = parser_get!(buffer, 0..1)?;
        let kind = MessageType::try_from(u8::from_be_bytes(kind_bytes))
            .map_err(ParseError::UnknownType)?;

        if matches!(kind, MessageType::IndefinitePadding) {
            return Ok((
                MessageHeader {
                    kind,
                    flags: 0,
                    length: 0,
                },
                1,
            ));
        }

        let flags_bytes: [u8; 1] = parser_get!(buffer, 1..2)?;

        let flags: u8 = (u8::from_be_bytes(flags_bytes) & 0b11110000) >> 4;

        let length_byte: [u8; 3] = parser_get!(buffer, 1..4)?;
        let mut length_bytes_buffer = [0_u8; 4];
        length_bytes_buffer.as_mut_slice()[1..4].copy_from_slice(&length_byte);
        length_bytes_buffer[1] &= 0b00001111;

        let length = u32::from_be_bytes(length_bytes_buffer);

        Ok((
            MessageHeader {
                kind,
                flags,
                length,
            },
            4,
        ))
    }

    /// Parse a single metadata item in a buffer
    pub fn parse_metadata(&mut self, buffer: &[u8]) -> Result<(Metadata, usize), ParseError> {
        let kind_bytes = parser_get!(buffer, 0..1)?;
        let kind = u8::from_be_bytes(kind_bytes);

        let length_bytes = parser_get!(buffer, 1..2)?;
        let length = u8::from_be_bytes(length_bytes);

        match kind {
            1 => match length {
                1 => {
                    let bytes: [u8; 1] = parser_get!(buffer, 2..3)?;
                    let bundle_length = u8::from_be_bytes(bytes);
                    Ok((Metadata::BundleLength(bundle_length as u64), 3))
                }
                2 => {
                    let bytes: [u8; 2] = parser_get!(buffer, 2..4)?;
                    let bundle_length = u16::from_be_bytes(bytes);
                    Ok((Metadata::BundleLength(bundle_length as u64), 4))
                }
                4 => {
                    let bytes: [u8; 4] = parser_get!(buffer, 2..6)?;
                    let bundle_length = u32::from_be_bytes(bytes);
                    Ok((Metadata::BundleLength(bundle_length as u64), 6))
                }
                8 => {
                    let bytes: [u8; 8] = parser_get!(buffer, 2..10)?;
                    let bundle_length = u64::from_be_bytes(bytes);
                    Ok((Metadata::BundleLength(bundle_length), 10))
                }
                _ => Err(ParseError::InvalidBundleLengthMetadata),
            },
            other => Err(ParseError::UnknownMetadataType(other)),
        }
    }
}

impl Default for Parser {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod test {
    use crate::{
        TransferIdentifier,
        message::{METADATA_FLAG, Message, MessageHeader, MessageType, Metadata},
        parser::{ParseError, Parser},
    };

    #[test]
    fn indefinite_padding_message() {
        let bytes = [
            0x0, // Type
            0x0, // Flags & length
            0x0, 0x0, // length
            0x0, 0x0, 0x0, 0x0, 0x1,
        ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let Message::IndefinitePadding(padding_length) = message else {
            panic!("Expected Indefinite length padding message");
        };

        assert_eq!(bytes_red, 8);
        assert_eq!(padding_length, 7);
    }

    #[test]
    fn bundle_message() {
        let bytes = [
            0x2, // Type
            0x0, // Flags & length
            0x0, 121, // length
            // Bundle content
            0x9f, 0x89, 0x07, 0x00, 0x01, 0x82, 0x01, 0x71, 0x2f, 0x2f, 0x65, 0x78, 0x61, 0x6d,
            0x70, 0x6c, 0x65, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x6c, 0x6f, 0x67, 0x82, 0x01, 0x78,
            0x33, 0x2f, 0x2f, 0x65, 0x70, 0x69, 0x63, 0x6b, 0x69, 0x77, 0x69, 0x2e, 0x64, 0x74,
            0x6e, 0x2f, 0x36, 0x65, 0x31, 0x34, 0x32, 0x39, 0x37, 0x32, 0x2d, 0x35, 0x35, 0x65,
            0x32, 0x2d, 0x34, 0x33, 0x63, 0x37, 0x2d, 0x38, 0x64, 0x62, 0x32, 0x2d, 0x37, 0x31,
            0x65, 0x31, 0x63, 0x65, 0x38, 0x31, 0x66, 0x33, 0x31, 0x62, 0x82, 0x01, 0x00, 0x82,
            0x1b, 0x00, 0x00, 0x00, 0xbd, 0xc4, 0x7b, 0xae, 0x02, 0x01, 0x1a, 0x05, 0x26, 0x5c,
            0x00, 0x42, 0x58, 0xbc, 0x85, 0x01, 0x01, 0x00, 0x00, 0x4c, 0x48, 0x65, 0x6c, 0x6c,
            0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x0a, 0xff,
        ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let Message::Bundle { content } = message else {
            panic!("Expected bundle message");
        };

        assert_eq!(bytes_red, 125);
        assert_eq!(content.len(), 121);
    }

    #[test]
    fn transfer_start_message() {
        let bytes = [
            0x3,                // Type
            METADATA_FLAG << 4, // Flags (with metadata) & length
            0x0,
            26, // length
            // Bundle length metadata
            1,
            1,
            121,
            0x0,
            0x0,
            0x0,
            64, // Transfer number
            0x0,
            0x0,
            0x0,
            8, // Segment index
            // Bundle content
            0x9f,
            0x89,
            0x07,
            0x00,
            0x01,
            0x82,
            0x01,
            0x71,
            0x2f,
            0x2f,
            0x65,
            0x78,
            0x61,
            0x6d,
            0x70,
        ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let Message::TransferEnd { metadata, segment } = message else {
            panic!("Expected a transfer start message");
        };

        assert_eq!(bytes_red, 30);
        assert_eq!(metadata, Some(Metadata::BundleLength(121)));
        assert_eq!(segment.transfer_identifier, TransferIdentifier(64));
        assert_eq!(segment.index, 8);
        assert_eq!(segment.data.len(), 15);
    }

    #[test]
    fn transfer_segment_message() {
        let bytes = [
            0x4, // Type
            0,   // Flags (with metadata) & length
            0x0, 23, // length
            0x0, 0x0, 0x0, 64, // Transfer number
            0x0, 0x0, 0x0, 7, // Segment index
            // Bundle content
            0x6c, 0x65, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x6c, 0x6f, 0x67, 0x82, 0x01, 0x78, 0x33,
            0x2f,
        ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let Message::TransferSegment { metadata, segment } = message else {
            panic!("Expected a transfer segment message");
        };

        assert_eq!(bytes_red, 27);
        assert_eq!(metadata, None);
        assert_eq!(segment.transfer_identifier, TransferIdentifier(64));
        assert_eq!(segment.index, 7);
        assert_eq!(segment.data.len(), 15);
    }

    #[test]
    fn transfer_cancel_message() {
        let bytes = [
            0x5, // Type
            0,   // Flags (with metadata) & length
            0x0, 4, // length
            0x0, 0x0, 0x0, 64, // Transfer number
        ];

        let (message, bytes_red) = Parser::new()
            .parse_message(&bytes)
            .expect("Parse should not fail");

        let Message::TransferCancel(transfer_number) = message else {
            panic!("Expected a transfer cancel message");
        };

        assert_eq!(bytes_red, 8);
        assert_eq!(transfer_number, TransferIdentifier(64));
    }
    #[test]
    fn incomplete_input() {
        let bytes = [
            0x3,                // Type
            METADATA_FLAG << 4, // Flags (with metadata) & length
            0x0,
            26, // length
            // Bundle length metadata
            1,
            1,
            121,
            0x0, /* ...incomplete input... */
        ];

        let Err(error) = Parser::new().parse_message(&bytes) else {
            panic!("Parsing should fail");
        };
        assert_eq!(error, ParseError::IncompleteInput)
    }

    #[test]
    fn big_message() {
        assert_eq!(
            Parser::new().parse_message_header(&[
                3,   // Type
                0x0, // Flags & length
                0x0, 0x0F // length
            ]),
            Ok((
                MessageHeader {
                    kind: MessageType::TransferEnd,
                    flags: 0,
                    length: 15,
                },
                4
            ))
        );
    }

    #[test]
    fn big_message_with_meta() {
        assert_eq!(
            Parser::new().parse_message_header(&[
                3,                 // Type
                0b10000000 | 0x03, // Flags (with metadata) & length
                0xff,
                0x75, // length
                1,
                1,
                10
            ]),
            Ok((
                MessageHeader {
                    kind: MessageType::TransferEnd,
                    flags: METADATA_FLAG,
                    length: 262005
                },
                4
            ))
        );

        assert_eq!(
            Parser::new().parse_message_header(&[
                3,                 // Type
                0b10000000 | 0x03, // Flags (with metadata) & length
                0xff,
                0x75, // length
                1,
                2,
                0,
                10
            ]),
            Ok((
                MessageHeader {
                    kind: MessageType::TransferEnd,
                    flags: METADATA_FLAG,
                    length: 262005
                },
                4
            ))
        );

        assert_eq!(
            Parser::new().parse_message_header(&[
                3,                 // Type
                0b10000000 | 0x03, // Flags (with metadata) & length
                0xff,
                0x75, // length
                1,
                4,
                0,
                0,
                0,
                10
            ]),
            Ok((
                MessageHeader {
                    kind: MessageType::TransferEnd,
                    flags: METADATA_FLAG,
                    length: 262005,
                },
                4
            ))
        );

        assert_eq!(
            Parser::new().parse_message_header(&[
                3,                 // Type
                0b10000000 | 0x03, // Flags (with metadata) & length
                0xff,
                0x75, // length
                1,
                8,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                10
            ]),
            Ok((
                MessageHeader {
                    kind: MessageType::TransferEnd,
                    flags: METADATA_FLAG,
                    length: 262005,
                },
                4
            ))
        );
    }
}
