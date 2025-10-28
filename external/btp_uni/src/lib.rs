#![no_std]
#![warn(missing_docs)]
#![warn(clippy::missing_docs_in_private_items)]

//! If a [Bundle] is larger than a single PDU, the [Bundle] needs to be divided into multiple segments

pub mod receiver;
pub mod sender;

//TODO Use Definite Padding Message

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
    greatest_transfer_identifier: Option<u32>,
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
        self.greatest_transfer_identifier
            .is_none_or(|g_id| valid_id_computation(g_id, id.0) < self.size)
    }

    /// Checks if the provided `id` is greater than the upper limit of the window
    pub fn is_new(&self, id: TransferIdentifier) -> bool {
        self.greatest_transfer_identifier.is_none_or(|g_id| {
            let greatest = g_id as i32;
            greatest.wrapping_sub(id.0 as i32).is_negative()
        })
    }

    /// Slide the window to the provided `id` as an upper limit
    pub fn slide_to(&mut self, id: TransferIdentifier) {
        self.greatest_transfer_identifier = Some(id.0)
    }
}

/// An identifier to a transfer
///
/// A transfer is mapped to a segmentation of a single [Bundle]
#[derive(Clone, Copy)]
pub struct TransferIdentifier(u32);

impl TransferIdentifier {
    /// Returns the next identifier
    pub fn next(self) -> Self {
        Self(self.0.wrapping_add(1))
    }
}

/// A fragment of a divided [Bundle] that didn't fit entirely in a PDU
pub struct Segment {
    /// A monotonically decreasing integral index that indicates the relative position
    /// of the [Segment] within the total sequence of [Segment]s
    ///
    /// `0` indicates the final segment
    index: usize,
    /// The ifentifier of the associated transfer of segments' sequence
    transfer_identifier: TransferIdentifier,
}

#[inline]
const fn valid_id_computation(greatest_id: u32, id: u32) -> u32 {
    greatest_id
        .wrapping_sub(id)
        .wrapping_add(u32::MAX)
        .wrapping_add(1)
}

#[cfg(test)]
mod tests {
    use super::*;
    // extern crate std;

    #[test]
    fn valid_calculation_parity() {
        let greatest = u32::MAX;
        for id in 0..greatest {
            let my_computation = valid_id_computation(greatest, id);
            let rfc_computation = (((greatest as u64)
                .saturating_sub(id as u64)
                .wrapping_add(2u64.pow(32)))
                % 2u64.pow(32)) as u32;
            assert_eq!(my_computation, rfc_computation);
        }
    }

    #[test]
    fn transfer_valid() {
        let mut window = TransferWindow::default();
        assert!(window.is_in(TransferIdentifier(1))); // Checks if valid in new window

        window.slide_to(TransferIdentifier(20));

        for i in 5..20 {
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
    fn invalid_transfert() {
        let mut window = TransferWindow {
            greatest_transfer_identifier: Some(20),
            size: 16,
        };

        assert!(!window.is_in(TransferIdentifier(4)));

        window.greatest_transfer_identifier = Some(2);

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
