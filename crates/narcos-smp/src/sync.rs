use core::cell::UnsafeCell;
use core::sync::atomic::{AtomicBool, Ordering};
use core::ops::{Deref, DerefMut};

/// A thread-safe, bare-metal Spinlock for multi-core synchronization.
pub struct Spinlock<T> {
    lock: AtomicBool,
    data: UnsafeCell<T>,
}

unsafe impl<T: Send> Sync for Spinlock<T> {}
unsafe impl<T: Send> Send for Spinlock<T> {}

impl<T> Spinlock<T> {
    /// Creates a new spinlock wrapping the given data.
    pub const fn new(data: T) -> Self {
        Self {
            lock: AtomicBool::new(false),
            data: UnsafeCell::new(data),
        }
    }

    /// Acquires the spinlock, spinning until it succeeds.
    /// Returns a guard that releases the lock when dropped.
    pub fn lock(&self) -> SpinlockGuard<'_, T> {
        // Spin until we successfully swap false to true.
        while self.lock.compare_exchange_weak(false, true, Ordering::Acquire, Ordering::Relaxed).is_err() {
            // Hint to the CPU that we are in a spin-wait loop (helps with energy consumption and hyperthreading).
            core::hint::spin_loop();
        }
        SpinlockGuard { spinlock: self }
    }
}

/// An RAII guard returned by `Spinlock::lock`.
/// Automatically releases the lock when dropped.
pub struct SpinlockGuard<'a, T> {
    spinlock: &'a Spinlock<T>,
}

impl<'a, T> Deref for SpinlockGuard<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        unsafe { &*self.spinlock.data.get() }
    }
}

impl<'a, T> DerefMut for SpinlockGuard<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { &mut *self.spinlock.data.get() }
    }
}

impl<'a, T> Drop for SpinlockGuard<'a, T> {
    fn drop(&mut self) {
        // Set lock state back to false.
        self.spinlock.lock.store(false, Ordering::Release);
    }
}
