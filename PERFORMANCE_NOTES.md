# WPS preprocessing optimizations

This branch adds three changes to the WPS 4.7.0 source baseline:

- Reuse packed bytes and unused bits in a per-call GBYTES reservoir for zero-skip, 1–31-bit extraction. Keep the original byte conversion and generic path for unsupported cases, with overflow guards.
- Write contiguous ungrib slabs as whole array sections, avoiding scalar runtime transfers while preserving unformatted records. Keep the original path for strided slabs. This uses Fortran 2008 CONTIGUOUS and IS_CONTIGUOUS, checked with the configured GNU, AOCC and Intel compilers.
- During metgrid mask discovery, read field headers but materialize only mask fields and their conservative table aliases. Audit file record envelopes before selection; uncertain or malformed files retain full reads. Normal processing reads are unchanged.

## Validation

Committed source bytes match the tested integrated GNU build. Standalone actual-source correctness checks covered GNU 13.3, AOCC 5.2 and Intel 2025.3. These compare each compiler with its original implementation; they do not establish cross-compiler equality.

The integrated GNU three-time, 1500 x 1140 Lambert case passed all seven exact scientific product comparisons against a fresh control; the control also passed against the established reference (14 comparisons). A subsequent concurrent pipeline using these same sources passed all seven product comparisons against that reference. Checks include field values and scientific metadata; numerical tolerances were not relaxed.

One paired sequential short run measured 277.23 seconds before and 224.20 seconds after these changes (19.1% less wall time), with 16 MPI ranks and serial ungrib. Cache and shared-load variation limit this estimate. Individual kernel timings are not additive stage-speedup claims.

A separate cluster-specific orchestration experiment completed in 89.75 seconds using up to 48 model cores. That workflow is not part of this source patch and does not establish equal-core speedup.

Full 25-time integrated WPS acceptance and downstream WRF initialization/forecast validation remain outstanding. These are experimental improvements; the production tree and original scientific reference were not modified.
