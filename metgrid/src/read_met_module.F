module read_met_module

   use constants_module
   use module_debug
   use misc_definitions_module
   use met_data_module

   ! State variables?
   integer :: input_unit
   character (len=MAX_FILENAME_LEN) :: filename
 
   logical :: file_payload_sizes_checked = .false.
   logical :: file_payload_sizes_validated = .false.

   contains
 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   ! Name: read_met_init
   !
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   subroutine read_met_init(fg_source, source_is_constant, datestr, istatus, audit_for_selection)
 
      implicit none
  
      ! Arguments
      logical, optional, intent(in) :: audit_for_selection
      integer, intent(out) :: istatus
      logical, intent(in) :: source_is_constant
      character (len=*), intent(in) :: fg_source
      character (len=*), intent(in) :: datestr
  
      ! Local variables
      integer :: io_status
      logical :: is_used

      istatus = 0
      file_payload_sizes_checked = .false.
      file_payload_sizes_validated = .false.
    
      !  1) BUILD FILENAME BASED ON TIME 
      filename = ' '
      if (.not. source_is_constant) then 
         write(filename, '(a)') trim(fg_source)//':'//trim(datestr)
      else
         write(filename, '(a)') trim(fg_source)
      end if
  
      ! The stream audit must precede sequential OPEN for runtimes that
      ! prohibit two units connected to the same file (including AOCC).
      if (present(audit_for_selection)) then
         if (audit_for_selection) then
            file_payload_sizes_validated = audit_intermediate_payload_sizes(filename)
            file_payload_sizes_checked = .true.
         end if
      end if

      !  2) OPEN FILE
      do input_unit=10,100
         inquire(unit=input_unit, opened=is_used)
         if (.not. is_used) exit
      end do 
      call mprintf((input_unit > 100),ERROR,'In read_met_init(), couldn''t find an available Fortran unit.')
      open(unit=input_unit, file=trim(filename), status='old', form='unformatted', iostat=io_status)

      if (io_status > 0) istatus = 1

      return
  
 
   end subroutine read_met_init
 
 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   ! Name: read_next_met_field
   !
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   subroutine read_next_met_field(fg_data, istatus, selected_fields)
 
      implicit none
  
      ! Arguments
      character(len=*), dimension(:), optional, intent(in) :: selected_fields
      logical :: do_skip
      type (met_data), intent(inout) :: fg_data
      integer, intent(out) :: istatus
  
      ! Local variables
      character (len=8) :: startloc
  
      istatus = 1
  
      if (present(selected_fields)) then
         if (.not. file_payload_sizes_checked) then
            file_payload_sizes_validated = audit_intermediate_payload_sizes(filename)
            file_payload_sizes_checked = .true.
         end if
      end if

      !  1) READ FORMAT VERSION
      read(unit=input_unit,err=1001,end=1001) fg_data % version
  
      ! PREGRID
      if (fg_data % version == 3) then

         read(unit=input_unit) fg_data % hdate, &
                               fg_data % xfcst, &
                               fg_data % field, &
                               fg_data % units, &
                               fg_data % desc,  &
                               fg_data % xlvl,  &
                               fg_data % nx,    &
                               fg_data % ny,    &
                               fg_data % iproj

         fg_data % map_source = ' '

         if (fg_data % field == 'HGT      ') fg_data % field = 'GHT      '

         fg_data % starti = 1.0
         fg_data % startj = 1.0
     
         ! Cylindrical equidistant
         if (fg_data % iproj == 0) then
            fg_data % iproj = PROJ_LATLON
            read(unit=input_unit,err=1001,end=1001) fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % deltalat, &
                                                    fg_data % deltalon
     
         ! Mercator
         else if (fg_data % iproj == 1) then
            fg_data % iproj = PROJ_MERC
            read(unit=input_unit,err=1001,end=1001) fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % truelat1
     
         ! Lambert conformal
         else if (fg_data % iproj == 3) then
            fg_data % iproj = PROJ_LC
            read(unit=input_unit,err=1001,end=1001) fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % xlonc,    &
                                                    fg_data % truelat1, &
                                                    fg_data % truelat2
     
         ! Polar stereographic
         else if (fg_data % iproj == 5) then
            fg_data % iproj = PROJ_PS
            read(unit=input_unit,err=1001,end=1001) fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % xlonc,    &
                                                    fg_data % truelat1

         ! ?????????
         else
            call mprintf(.true.,ERROR,'Unrecognized projection code %i when reading from %s', &
                         i1=fg_data % iproj, s1=filename)
     
         end if
     
         fg_data % earth_radius = EARTH_RADIUS_M / 1000.

#if (defined _GEOGRID) || (defined _METGRID)
         fg_data % dx = fg_data % dx * 1000.
         fg_data % dy = fg_data % dy * 1000.

         if (fg_data % xlonc    > 180.) fg_data % xlonc    = fg_data%xlonc    - 360.

         if (fg_data % startlon > 180.) fg_data % startlon = fg_data%startlon - 360.
  
         if (fg_data % startlat < -90.) fg_data % startlat = -90.
         if (fg_data % startlat >  90.) fg_data % startlat = 90.
#endif
     
         fg_data % is_wind_grid_rel = .true.
     
         do_skip = .false.
         if (present(selected_fields)) then
            if (file_payload_sizes_validated) then
               do_skip = .not. any(selected_fields == fg_data % field)
            end if
         end if
         if (do_skip) then
            allocate(fg_data % slab(0, 0))
            read(unit=input_unit,err=1001,end=1001)
         else
            allocate(fg_data % slab(fg_data % nx, fg_data % ny))
            read(unit=input_unit,err=1001,end=1001) fg_data % slab
         end if
     
         istatus = 0 
    
      ! GRIB_PREP
      else if (fg_data % version == 4) then
  
         read(unit=input_unit) fg_data % hdate,      &
                               fg_data % xfcst,      &
                               fg_data % map_source, &
                               fg_data % field,      &
                               fg_data % units,      &
                               fg_data % desc,       &
                               fg_data % xlvl,       &
                               fg_data % nx,         &
                               fg_data % ny,         &
                               fg_data % iproj
  
         if (fg_data % field == 'HGT      ') fg_data % field = 'GHT      '
  
         ! Cylindrical equidistant
         if (fg_data % iproj == 0) then
            fg_data % iproj = PROJ_LATLON
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % deltalat, &
                                                    fg_data % deltalon

         ! Mercator
         else if (fg_data % iproj == 1) then
            fg_data % iproj = PROJ_MERC
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % truelat1

         ! Lambert conformal
         else if (fg_data % iproj == 3) then
            fg_data % iproj = PROJ_LC
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % xlonc,    &
                                                    fg_data % truelat1, &
                                                    fg_data % truelat2

         ! Polar stereographic
         else if (fg_data % iproj == 5) then
            fg_data % iproj = PROJ_PS
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % xlonc,    &
                                                    fg_data % truelat1
     
         ! ?????????
         else
            call mprintf(.true.,ERROR,'Unrecognized projection code %i when reading from %s', &
                         i1=fg_data % iproj, s1=filename)
     
         end if
  
         if (startloc == 'CENTER  ') then
            fg_data % starti = real(fg_data % nx)/2.
            fg_data % startj = real(fg_data % ny)/2.
         else if (startloc == 'SWCORNER') then
            fg_data % starti = 1.0
            fg_data % startj = 1.0
         end if

         fg_data % earth_radius = EARTH_RADIUS_M / 1000.

#if (defined _GEOGRID) || (defined _METGRID)
         fg_data % dx = fg_data % dx * 1000.
         fg_data % dy = fg_data % dy * 1000.

         if (fg_data % xlonc    > 180.) fg_data % xlonc    = fg_data % xlonc    - 360.

         if (fg_data % startlon > 180.) fg_data % startlon = fg_data % startlon - 360.
  
         if (fg_data % startlat < -90.) fg_data % startlat = -90.
         if (fg_data % startlat >  90.) fg_data % startlat = 90.
#endif
         
         fg_data % is_wind_grid_rel = .true.
      
         do_skip = .false.
         if (present(selected_fields)) then
            if (file_payload_sizes_validated) then
               do_skip = .not. any(selected_fields == fg_data % field)
            end if
         end if
         if (do_skip) then
            allocate(fg_data % slab(0, 0))
            read(unit=input_unit,err=1001,end=1001)
         else
            allocate(fg_data % slab(fg_data % nx, fg_data % ny))
            read(unit=input_unit,err=1001,end=1001) fg_data % slab
         end if
      
         istatus = 0

      ! WPS
      else if (fg_data % version == 5) then
  
         read(unit=input_unit) fg_data % hdate,      &
                               fg_data % xfcst,      &
                               fg_data % map_source, &
                               fg_data % field,      &
                               fg_data % units,      &
                               fg_data % desc,       &
                               fg_data % xlvl,       &
                               fg_data % nx,         &
                               fg_data % ny,         &
                               fg_data % iproj
  
         if (fg_data % field == 'HGT      ') fg_data % field = 'GHT      '
  
         ! Cylindrical equidistant
         if (fg_data % iproj == 0) then
            fg_data % iproj = PROJ_LATLON
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % deltalat, &
                                                    fg_data % deltalon, &
                                                    fg_data % earth_radius

         ! Mercator
         else if (fg_data % iproj == 1) then
            fg_data % iproj = PROJ_MERC
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % truelat1, &
                                                    fg_data % earth_radius

         ! Lambert conformal
         else if (fg_data % iproj == 3) then
            fg_data % iproj = PROJ_LC
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % xlonc,    &
                                                    fg_data % truelat1, &
                                                    fg_data % truelat2, &
                                                    fg_data % earth_radius

         ! Gaussian
         else if (fg_data % iproj == 4) then
            fg_data % iproj = PROJ_GAUSS
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % deltalat, &
                                                    fg_data % deltalon, &
                                                    fg_data % earth_radius

         ! Polar stereographic
         else if (fg_data % iproj == 5) then
            fg_data % iproj = PROJ_PS
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % xlonc,    &
                                                    fg_data % truelat1, &
                                                    fg_data % earth_radius
     
         ! CASSINI  
         else if (fg_data % iproj == 6) then
            fg_data % iproj = PROJ_CASSINI
            read(unit=input_unit,err=1001,end=1001) startloc, &
                                                    fg_data % startlat, &
                                                    fg_data % startlon, &
                                                    fg_data % dx,       &
                                                    fg_data % dy,       &
                                                    fg_data % centerlat,    &
                                                    fg_data % centerlon,    &
                                                    fg_data % earth_radius
     
            if ( fg_data % centerlat > 0. ) then
                fg_data % pole_lat = 90. - fg_data % centerlat
                fg_data % pole_lon = 180.
                fg_data % xlonc = -fg_data % centerlon
            else
                fg_data % pole_lat = 90. + fg_data % centerlat
                fg_data % pole_lon = 0.
                fg_data % xlonc = 180. - fg_data % centerlon
            end if
            fg_data % deltalon = fg_data % dx
            fg_data % deltalat = fg_data % dy

         ! ?????????
         else
            call mprintf(.true.,ERROR,'Unrecognized projection code %i when reading from %s', &
                         i1=fg_data % iproj, s1=filename)
     
         end if
  
         if (startloc == 'CENTER  ') then
            fg_data % starti = real(fg_data % nx)/2.
            fg_data % startj = real(fg_data % ny)/2.
         else if (startloc == 'SWCORNER') then
            fg_data % starti = 1.0
            fg_data % startj = 1.0
         end if

#if (defined _GEOGRID) || (defined _METGRID)
         fg_data % dx = fg_data % dx * 1000.
         fg_data % dy = fg_data % dy * 1000.

         if (fg_data % xlonc    > 180.) fg_data % xlonc    = fg_data % xlonc    - 360.

         if (fg_data % startlon > 180.) fg_data % startlon = fg_data % startlon - 360.
         
         if (fg_data % startlat < -90.) fg_data % startlat = -90.
         if (fg_data % startlat >  90.) fg_data % startlat =  90.
#endif
 
         read(unit=input_unit,err=1001,end=1001) fg_data % is_wind_grid_rel
      
         do_skip = .false.
         if (present(selected_fields)) then
            if (file_payload_sizes_validated) then
               do_skip = .not. any(selected_fields == fg_data % field)
            end if
         end if
         if (do_skip) then
            allocate(fg_data % slab(0, 0))
            read(unit=input_unit,err=1001,end=1001)
         else
            allocate(fg_data % slab(fg_data % nx, fg_data % ny))
            read(unit=input_unit,err=1001,end=1001) fg_data % slab
         end if
      
         istatus = 0

      else
         call mprintf(.true.,ERROR,'Didn''t recognize format version of data in %s.\n'// &
                                   'Found version %i but expected either 3, 4, or 5. This could be an endian problem.', &
                                   s1=filename, i1=fg_data % version)
      end if
  
      return
 
   1001 return
 
   end subroutine read_next_met_field
 
 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   ! Name: read_met_close
   !
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   subroutine read_met_close()
 
      implicit none
  
      close(unit=input_unit)
      filename = 'UNINITIALIZED_FILENAME'
      file_payload_sizes_checked = .false.
      file_payload_sizes_validated = .false.
  
   end subroutine read_met_close

! Codex local implementation: worker audit rejected; no defective helper reused.
   logical function audit_intermediate_payload_sizes(audit_filename)
      use iso_fortran_env, only: int32, int64
      implicit none
      character(len=*), intent(in) :: audit_filename
      integer :: u, ios, close_status
      integer(int32) :: version, nx, ny, iproj
      integer(int64) :: file_bytes, cursor, payload_start, record_bytes
      integer(int64) :: header_offset, projection_min, slab_bytes
      character(len=8) :: startloc
      logical :: ok, saw_field

      audit_intermediate_payload_sizes = .false.
      if (storage_size(0.) /= 32 .or. bit_size(0) /= 32) return
      if (storage_size(.false.) /= 32) return
      open(newunit=u, file=audit_filename, form='unformatted', access='stream', &
           status='old', action='read', iostat=ios)
      if (ios /= 0) return
      file_bytes = -1_int64
      inquire(unit=u, size=file_bytes, iostat=ios)
      if (ios /= 0 .or. file_bytes < 0_int64) then
         close(u, iostat=close_status)
         return
      end if
      cursor = 0_int64
      saw_field = .false.
      ok = .true.
      do while (cursor < file_bytes)
         call envelope(payload_start, record_bytes)
         if (.not. ok) exit
         if (record_bytes < 4_int64) then
            ok = .false.; exit
         end if
         read(u, pos=payload_start+1_int64, iostat=ios) version
         if (ios /= 0) then
            ok = .false.; exit
         end if
         select case(version)
         case(3); header_offset = 112_int64
         case(4,5); header_offset = 144_int64
         case default; ok = .false.; exit
         end select
         call envelope(payload_start, record_bytes)
         if (.not. ok) exit
         if (record_bytes < header_offset+12_int64) then
            ok = .false.; exit
         end if
         read(u, pos=payload_start+header_offset+1_int64, iostat=ios) nx, ny, iproj
         if (ios /= 0) then
            ok = .false.; exit
         end if
         if (nx <= 0 .or. ny <= 0) then
            ok = .false.; exit
         end if
         if (int(nx,int64) > huge(0_int64)/(4_int64*int(ny,int64))) then
            ok = .false.; exit
         end if
         slab_bytes = 4_int64*int(nx,int64)*int(ny,int64)
         projection_min = -1_int64
         select case(version)
         case(3)
            select case(iproj)
            case(0); projection_min = 16_int64
            case(1); projection_min = 20_int64
            case(3); projection_min = 28_int64
            case(5); projection_min = 24_int64
            end select
         case(4)
            select case(iproj)
            case(0); projection_min = 24_int64
            case(1); projection_min = 28_int64
            case(3); projection_min = 36_int64
            case(5); projection_min = 32_int64
            end select
         case(5)
            select case(iproj)
            case(0,4); projection_min = 28_int64
            case(1); projection_min = 32_int64
            case(3); projection_min = 40_int64
            case(5,6); projection_min = 36_int64
            end select
         end select
         if (projection_min < 0_int64) then
            ok = .false.; exit
         end if
         call envelope(payload_start, record_bytes)
         if (.not. ok) exit
         if (record_bytes < projection_min) then
            ok = .false.; exit
         end if
         if (version >= 4) then
            read(u, pos=payload_start+1_int64, iostat=ios) startloc
            if (ios /= 0) then
               ok = .false.; exit
            end if
            if (startloc /= 'SWCORNER' .and. startloc /= 'CENTER  ') then
               ok = .false.; exit
            end if
         end if
         if (version == 5) then
            call envelope(payload_start, record_bytes)
            if (.not. ok) exit
            if (record_bytes < 4_int64) then
               ok = .false.; exit
            end if
         end if
         call envelope(payload_start, record_bytes)
         if (.not. ok) exit
         if (record_bytes < slab_bytes) then
            ok = .false.; exit
         end if
         saw_field = .true.
      end do
      audit_intermediate_payload_sizes = ok .and. cursor == file_bytes .and. saw_field
      close(u, iostat=close_status)
      if (close_status /= 0) audit_intermediate_payload_sizes = .false.

   contains
      subroutine envelope(body_start, bytes)
         integer(int64), intent(out) :: body_start, bytes
         integer(int32) :: leading, trailing
         integer :: status
         body_start = 0_int64; bytes = 0_int64
         if (.not. ok) return
         if (cursor < 0_int64 .or. cursor > file_bytes) then
            ok = .false.; return
         end if
         if (file_bytes-cursor < 8_int64) then
            ok = .false.; return
         end if
         read(u, pos=cursor+1_int64, iostat=status) leading
         if (status /= 0) then
            ok = .false.; return
         end if
         if (leading < 0_int32) then
            ok = .false.; return
         end if
         bytes = int(leading,int64)
         if (bytes > file_bytes-cursor-8_int64) then
            ok = .false.; return
         end if
         read(u, pos=cursor+bytes+5_int64, iostat=status) trailing
         if (status /= 0) then
            ok = .false.; return
         end if
         if (trailing /= leading) then
            ok = .false.; return
         end if
         body_start = cursor+4_int64
         cursor = cursor+bytes+8_int64
      end subroutine envelope
   end function audit_intermediate_payload_sizes

end module read_met_module
