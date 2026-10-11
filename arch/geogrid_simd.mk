# The helper is separate so the legacy reader keeps strict O2 compilation.
geogrid_c_compiler := $(notdir $(firstword $(SCC)))
ifneq ($(filter gcc% clang% icx% icc%,$(geogrid_c_compiler)),)
geogrid_strict_flags := $(if $(filter icx% icc%,$(geogrid_c_compiler)),-fp-model strict,-fno-fast-math -ffp-contract=off)
# Default target for this performance fork; override for another CPU.
GEOGRID_SIMD_ARCH_FLAGS ?= -march=znver2
GEOGRID_READER_FLAGS ?= -O2 $(geogrid_strict_flags)
GEOGRID_SIMD_FLAGS ?= -O3 $(GEOGRID_SIMD_ARCH_FLAGS) $(geogrid_strict_flags) -fopenmp-simd -DWPS_GEOGRID_READER_SIMD
else
# Portable C helper; vendor-specific optimization flags can be supplied.
GEOGRID_READER_FLAGS ?=
GEOGRID_SIMD_FLAGS ?=
endif
