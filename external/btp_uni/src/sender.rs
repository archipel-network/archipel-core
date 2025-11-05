//! A module containing the bundle transfer protocol sender logic

use crate::{TransferWindow, message::Segment};

/// A bundle transfer protocol sender
pub struct Sender {
    window: TransferWindow,
}

impl Sender {
    /// Creates a new sender with a sliding transfer window
    pub const fn new(window: TransferWindow) -> Self {
        Self { window }
    }

    fn cancel_outdated_transfers(&mut self) {
        todo!()
    }

    fn continue_processing(&mut self) {
        todo!()
    }

    /// Sends a [Segment]
    pub fn send(&mut self, segment: Segment) {
        if self.window.is_new(segment.transfer_identifier) {
            self.window.slide_to(segment.transfer_identifier);
            self.cancel_outdated_transfers();
        } else if self.window.is_in(segment.transfer_identifier) {
            self.continue_processing();
        }
    }
}
