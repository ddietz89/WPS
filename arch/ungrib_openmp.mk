# OpenMP is confined to ungrib. Override with UNGRIB_OPENMP_FLAGS=
# for a serial build, or provide the flag for another Fortran compiler.
ungrib_fortran_compiler := $(notdir $(firstword $(SFC)))
UNGRIB_OPENMP_FLAGS ?= $(if $(filter ifort% ifx%,$(ungrib_fortran_compiler)),-qopenmp,$(if $(filter gfortran% flang%,$(ungrib_fortran_compiler)),-fopenmp))
