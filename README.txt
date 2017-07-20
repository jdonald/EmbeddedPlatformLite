
The EmbeddedPlatformLite is designed to facilitate C++ embedded systems
development in an environment too constrained to provide traditional operating
system services.  This is a narrowly defined platform which can be developed 
against in a traditional desktop development environment before cross compiling
to an embedded system.


* Test Driver.  A lightweight reimplementation of GoogleTest.

* Profiling.  Captures a hierarchical timeline view with a minimum of overhead.

* DMA.  Cross platform DMA API with validation.

* Memory Management.  Hides a range of allocation strategies behind a simple
  RAII interface.

* Container Support.  Provides a minimal non-reallocating version std::vector
  and std::allocator.

* Deterministic Replay.  A tool for playing back the inputs, outputs and
  intermediate calculations of an non-portable body of code for validation.
