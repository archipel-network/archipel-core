//! A module containing the bundle transfer protocol receiver logic

use crate::{TransferWindow, message::Segment};

/// A bundle transfer protocol receiver
#[derive(Default)]
pub struct Receiver {
    window: TransferWindow,
}

impl Receiver {
    /// Creates a new receiver with a sliding transfer window
    pub const fn new(window: TransferWindow) -> Self {
        Self { window }
    }

    fn cancel_outdated_transfers(&mut self) {
        todo!()
    }

    fn continue_processing(&mut self) {
        todo!()
    }

    /// Processes a [Segment]
    pub fn process_segment(&mut self, segment: Segment) {
        if self.window.is_new(segment.transfer_identifier) {
            self.window.slide_to(segment.transfer_identifier);
            self.cancel_outdated_transfers();
        } else if self.window.is_in(segment.transfer_identifier) {
            self.continue_processing();
        }
    }
}
