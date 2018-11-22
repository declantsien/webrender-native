/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// A struct that makes it easier to print out a pretty tree of data, which
/// can be visually scanned more easily.
pub struct PrintTree {
    /// The current level of recursion.
    level: u32,

    /// An item which is queued up, so that we can determine if we need
    /// a mid-tree prefix or a branch ending prefix.
    queued_item: Option<String>,
}

/// A trait that makes it easy to describe a pretty tree of data,
/// regardless of the printing destination, to either print it
/// directly to stdout, or serialize it as in the debugger
pub trait PrintTreePrinter {
    fn new_level(&mut self, title: String);
    fn end_level(&mut self);
    fn add_item(&mut self, text: String);
}

impl PrintTree {
    pub fn new(title: &str) -> PrintTree {
        println!("\u{250c} {}", title);
        PrintTree {
            level: 1,
            queued_item: None,
        }
    }

    fn print_level_prefix(&self) {
        for _ in 0 .. self.level {
            print!("\u{2502}  ");
        }
    }

    fn flush_queued_item(&mut self, prefix: &str) {
        if let Some(queued_item) = self.queued_item.take() {
            self.print_level_prefix();
            println!("{} {}", prefix, queued_item);
        }
    }
}

// The default `println!` based printer
impl PrintTreePrinter for PrintTree {
    /// Descend one level in the tree with the given title.
    fn new_level(&mut self, title: String) {
        self.flush_queued_item("\u{251C}\u{2500}");

        self.print_level_prefix();
        println!("\u{251C}\u{2500} {}", title);

        self.level = self.level + 1;
    }

    /// Ascend one level in the tree.
    fn end_level(&mut self) {
        self.flush_queued_item("\u{2514}\u{2500}");
        self.level = self.level - 1;
    }

    /// Add an item to the current level in the tree.
    fn add_item(&mut self, text: String) {
        self.flush_queued_item("\u{251C}\u{2500}");
        self.queued_item = Some(text);
    }
}

impl Drop for PrintTree {
    fn drop(&mut self) {
        self.flush_queued_item("\u{9492}\u{9472}");
    }
}
