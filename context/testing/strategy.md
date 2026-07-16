# Test Strategy

Testing is layered so failures appear near their source:

1. Compile with strict warnings and validate schemas at load boundaries.
2. Unit-test invariants, boundary values, malformed input, and repeated operations.
3. Integration-test serialization, commands, assets, project loading, and subsystem recovery.
4. Run real-GPU smoke tests and deterministic content simulations.
5. Add stress, fuzz, cancellation, low-resource, migration, and failure-injection tests as their systems land.

Every confirmed defect must be evaluated for a regression test. Machine-readable test results, benchmark conditions, and material findings belong in the context library.
