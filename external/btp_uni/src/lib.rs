#![no_std]
#![warn(missing_docs)]
#![warn(clippy::missing_docs_in_private_items)]

//! If a [Bundle] is larger than a single PDU, the [Bundle] needs to be divided into multiple segments

pub mod message;
pub mod parser;
pub mod receiver;
pub mod sender;

#[derive(Debug)]
/// Error to return when transfer window creation fails
pub enum TransferWindowError {
    /// The provided id is too small, it needs to be equal or greater than `4`
    TooSmall,
    /// The provided id is too large, it needs to less than `2^12`
    TooLarge,
}

/// A sliding window to know if a transfer is valid or needs to be rejected
pub struct TransferWindow {
    size: u32,
    greatest_transfer_identifier: Option<TransferIdentifier>,
}

impl Default for TransferWindow {
    fn default() -> Self {
        Self {
            size: 16,
            greatest_transfer_identifier: None,
        }
    }
}

impl TransferWindow {
    /// Creates a new transfer window
    ///
    /// Returns an error if size is smaller than `4` or greater than `2^12-1`
    pub const fn new(size: u32) -> Result<Self, TransferWindowError> {
        if size < 4 {
            Err(TransferWindowError::TooSmall)
        } else if size >= 2u32.pow(12) {
            Err(TransferWindowError::TooLarge)
        } else {
            Ok(Self {
                size,
                greatest_transfer_identifier: None,
            })
        }
    }

    /// Checks if the provided `id` is in window
    pub fn is_in(&self, id: TransferIdentifier) -> bool {
        self.greatest_transfer_identifier.is_none_or(|g_id| {
            let greater = g_id.0 as i32;
            let distance = greater.wrapping_sub(id.0 as i32);
            (distance as u32) < self.size
        })
    }

    /// Checks if the provided `id` is greater than the upper limit of the window
    pub fn is_new(&self, id: TransferIdentifier) -> bool {
        self.greatest_transfer_identifier.is_none_or(|g_id| {
            let greatest = g_id.0 as i32;
            greatest.wrapping_sub(id.0 as i32).is_negative()
        })
    }

    /// Slide the window to the provided `id` as an upper limit
    pub fn slide_to(&mut self, id: TransferIdentifier) {
        self.greatest_transfer_identifier = Some(id)
    }
}

/// An identifier to a transfer
///
/// A transfer is mapped to a segmentation of a single [Bundle]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TransferIdentifier(u32);

impl TransferIdentifier {
    /// Returns the next identifier
    pub fn next(self) -> Self {
        Self(self.0.wrapping_add(1))
    }

    /// Returns the memory representation of this identifier as a byte array in big-endian (network) byte order.
    pub fn to_be_bytes(self) -> [u8; 4] {
        self.0.to_be_bytes()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    // extern crate std;

    #[test]
    fn transfer_valid() {
        let mut window = TransferWindow::default();
        assert!(window.is_in(TransferIdentifier(1))); // Checks if valid in new window

        window.slide_to(TransferIdentifier(20));

        for i in 5..=20 {
            assert!(window.is_in(TransferIdentifier(i)));
        }

        window.slide_to(TransferIdentifier(1));

        let mut id = 1u32.wrapping_sub(15u32);

        while id != 2 {
            assert!(window.is_in(TransferIdentifier(id)));
            id = id.wrapping_add(1);
        }
    }

    #[test]
    fn invalid_transfer() {
        let mut window = TransferWindow {
            greatest_transfer_identifier: Some(TransferIdentifier(20)),
            size: 16,
        };

        assert!(!window.is_in(TransferIdentifier(4)));

        window.greatest_transfer_identifier = Some(TransferIdentifier(2));

        assert!(!window.is_in(TransferIdentifier(2u32.wrapping_sub(17))))
    }

    #[test]
    fn new_transfer() {
        let mut window = TransferWindow::default();

        for id in 0..u32::MAX {
            assert!(window.is_new(TransferIdentifier(id))); // Checks if valid in new window
            window.slide_to(TransferIdentifier(id));
        }
    }
}
