ifneq ($(strip $(METGRID_STRICT_CONFIG)),)
include $(METGRID_STRICT_CONFIG)
else
include ../../configure.wps
endif
.DEFAULT_GOAL := all
override CPPFLAGS += -D_METGRID

OBJS = cio.o wrf_debug.o bitarray_module.o constants_module.o datatype_module.o module_stringutil.o gridinfo_module.o metgrid.o input_module.o interp_module.o interp_option_module.o list_module.o llxy_module.o met_data_module.o minheap_module.o misc_definitions_module.o module_date_pack.o module_debug.o module_map_utils.o module_mergesort.o output_module.o parallel_module.o process_domain_module.o queue_module.o read_met_module.o rotate_winds_module.o storage_module.o write_met_module.o scan_input.o mpas_mesh.o target_mesh.o remapper.o


# Optional NetCDF-owned parallel compression; an ordinary build retains WRF I/O.
# Set an absolute prefix containing the patched NetCDF-C 4.9.2 installation.
ifneq ($(strip $(METGRID_PRECOMPRESSED_NETCDF_PREFIX)),)
ifneq ($(filter /%,$(strip $(METGRID_PRECOMPRESSED_NETCDF_PREFIX))),$(strip $(METGRID_PRECOMPRESSED_NETCDF_PREFIX)))
$(error METGRID_PRECOMPRESSED_NETCDF_PREFIX must be absolute)
endif
ifeq ($(filter -D_MPI,$(CPPFLAGS)),)
$(error METGRID_PRECOMPRESSED_NETCDF_PREFIX requires an MPI configuration)
endif
ifeq ($(filter -DIO_NETCDF,$(CPPFLAGS)),)
$(error METGRID_PRECOMPRESSED_NETCDF_PREFIX requires IO_NETCDF)
endif
ifeq ($(wildcard $(METGRID_PRECOMPRESSED_NETCDF_PREFIX)/include/netcdf_precompressed.h),)
$(error Patched NetCDF provider header missing in METGRID_PRECOMPRESSED_NETCDF_PREFIX)
endif
override CPPFLAGS += -DWPS_CHUNK_MPI
override CFLAGS += -I$(METGRID_PRECOMPRESSED_NETCDF_PREFIX)/include
override OBJS += wps_bridge.o wps_registration.o
override WRF_LIB := -L$(METGRID_PRECOMPRESSED_NETCDF_PREFIX)/lib $(WRF_LIB) -lz
endif

# Recompile this small module when switching between normal and provider builds.
# Recursive WPS make can override flags; the prefix records the selected mode.
.PHONY: metgrid_backend_mode_force
.metgrid_backend_mode: metgrid_backend_mode_force
	@printf '%s\n' '$(METGRID_PRECOMPRESSED_NETCDF_PREFIX)' > $@.tmp
	@cmp -s $@.tmp $@ || cp $@.tmp $@
	@rm -f $@.tmp
output_module.o: .metgrid_backend_mode
wps_bridge.o wps_registration.o: .metgrid_backend_mode netcdf_precompressed.h

all: 
	clear ;
	@echo " "
	@echo "go up two directories and type compile to build WPS"
	@echo " "
	@echo " "

metgrid.exe: $(OBJS)
	$(FC) $(LDFLAGS) -o $@ $(OBJS) \
                $(WRF_DIR)/frame/module_driver_constants.o \
		$(WRF_DIR)/frame/pack_utils.o $(WRF_DIR)/frame/module_machine.o \
		$(WRF_DIR)/frame/module_internal_header_util.o \
                $(WRF_INCLUDE) \
                $(WRF_LIB) \
                $(MPI_LIB)

bitarray_module.o: module_debug.o

cio.o:

constants_module.o:

datatype_module.o: bitarray_module.o module_debug.o

module_stringutil.o:

gridinfo_module.o: misc_definitions_module.o module_debug.o

metgrid.o: gridinfo_module.o interp_option_module.o module_debug.o parallel_module.o process_domain_module.o

input_module.o: gridinfo_module.o misc_definitions_module.o parallel_module.o queue_module.o 

interp_module.o: bitarray_module.o misc_definitions_module.o module_debug.o queue_module.o

interp_option_module.o: list_module.o misc_definitions_module.o module_debug.o module_stringutil.o

list_module.o: module_debug.o

llxy_module.o: gridinfo_module.o module_map_utils.o module_debug.o misc_definitions_module.o

met_data_module.o:

minheap_module.o: datatype_module.o

misc_definitions_module.o:

module_date_pack.o: module_debug.o

module_debug.o: parallel_module.o

module_map_utils.o: constants_module.o misc_definitions_module.o module_debug.o

module_mergesort.o:

output_module.o: gridinfo_module.o misc_definitions_module.o module_debug.o parallel_module.o storage_module.o

parallel_module.o:

process_domain_module.o: module_date_pack.o bitarray_module.o gridinfo_module.o input_module.o interp_module.o interp_option_module.o list_module.o llxy_module.o misc_definitions_module.o module_debug.o module_mergesort.o output_module.o parallel_module.o read_met_module.o rotate_winds_module.o storage_module.o scan_input.o mpas_mesh.o target_mesh.o remapper.o

queue_module.o: module_debug.o

read_met_module.o: constants_module.o misc_definitions_module.o module_debug.o met_data_module.o

rotate_winds_module.o: bitarray_module.o constants_module.o llxy_module.o misc_definitions_module.o module_debug.o module_map_utils.o

storage_module.o: datatype_module.o minheap_module.o misc_definitions_module.o module_debug.o parallel_module.o module_stringutil.o

wrf_debug.o: gridinfo_module.o cio.o

write_met_module.o: misc_definitions_module.o module_debug.o met_data_module.o

scan_input.o:

mpas_mesh.o: scan_input.o

target_mesh.o:

remapper.o: scan_input.o mpas_mesh.o target_mesh.o

clean:
	$(RM) $(OBJS) wps_bridge.o wps_registration.o .metgrid_backend_mode .metgrid_backend_mode.tmp *.f90 *.mod
	$(RM) metgrid.exe

superclean:	clean
