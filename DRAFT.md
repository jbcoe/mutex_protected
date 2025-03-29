<!-- markdownlint-disable MD029, MD041 -->

# `mutex_protected`: A Mutex that Owns the Resource it Protects

ISO/IEC JTC1 SC22 WG21 Programming Language C++

Working Group: Library Evolution, Library

_Jonathan Coe \<<jonathanbcoe@gmail.com>\>_

_Timo Ewalds \<<timo@ewalds.ca>\>_

# Abstract

We propose the addition of a new class template to the C++ Standard Library: `mutex_protected`. 

Like `mutex`, `mutex_protected` is used to protect a shared resource from concurrent access, 
however unlike `mutex`, `mutex_protected` owns the resources being protected, which makes it
explicit what the mutex protects, and prevents accidental misuse. The type system enforces that
the mutex is locked before any operations can be performed on the protected resource, and uses
a proxy object holding a lock guard to ensure that the mutex is unlocked when the resource is no
longer needed.

# History

Initial version.

# Motivation

The standard library provides `mutex` for protecting shared resources from concurrent access, 
however it does not provide a way to explicitly specify what the mutex protects. This makes it
easy to accidentally forget to lock a mutex before accessing a protected resource, especially as
code evolves. So far this has mainly been solved by code review and 
[static analysis](https://clang.llvm.org/docs/ThreadSafetyAnalysis.html) tools, but it would be
much simpler if the type system simply enforced that the mutex is locked before any operations can
be performed on the protected resource.

Having a free floating `mutex` gives a lot of power and freedom, but is rather error-prone.
Like many C++'s features, we think the default should be flipped so the usual safe and easy
case is the default, and the powerful but unsafe must be explicitly chosen.

We propose the addition of a new class: `mutex_protected`, which is a mutex that owns the variables
it protects. This class provides RAII semantics for locking and unlocking the mutex, and ensures
that the mutex is always locked before any operations can be performed on the protected resource.

# Design requirements

We review the fundamental design requirements of `mutex_protected`.

...

# Prior work

Similar implementations exist in 
[Boost](https://www.boost.org/doc/libs/1_81_0/doc/html/thread/sds.html) and 
[Folly](https://github.com/facebook/folly/blob/main/folly/docs/Synchronized.md).

# Impact on the standard

This proposal is a pure library extension. It requires additions to be made to
the standard library header `<mutex>`.

# Technical specifications

...

# Reference implementation

A reference implementation of this proposal is available on GitHub at
<https://www.github.com/jbcoe/mutex_protected>.

# Acknowledgements

The authors would like to thank
...
for suggestions and useful discussion.

# References

_Thread Safety Analysis_\
<https://clang.llvm.org/docs/ThreadSafetyAnalysis.html>

