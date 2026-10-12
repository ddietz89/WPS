!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! Module: bitarray_module
!
! Purpose: This module provides a two-dimensional bit array and a set of 
!   routines to manipulate and examine the bits of the array.
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
module bitarray_module

   use module_debug

   type bitarray
      integer, pointer, dimension(:,:) :: iarray ! Storage array
      integer :: nx, ny                 ! Number of bits in the x and y directions
      integer :: x_int_dim, y_int_dim   ! Number of integers in the x and y directions
      integer :: integer_size           ! Number of bits in an integer
   end type bitarray
 
   contains
 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   ! Name: bitarray_create
   !
   ! Purpose: Allocate and initialize a bit array so that all bits are FALSE 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   subroutine bitarray_create(b, i, j)
   
      implicit none
  
      ! Arguments
      integer, intent(in) :: i, j   
      type (bitarray), intent(out) :: b
  
      b%integer_size = bit_size(b%integer_size) 
  
      b%nx = i
      b%ny = j
  
      b%x_int_dim = ceiling(real(b%nx)/real(b%integer_size))
      b%y_int_dim = b%ny 

      nullify(b%iarray)
      allocate(b%iarray(b%x_int_dim, b%y_int_dim))
      b%iarray = 0
 
   end subroutine bitarray_create


   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   ! Name: bitarray_copy
   !
   ! Purpose: Duplicate a bitarray.
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   subroutine bitarray_copy(src, dst)
   
      implicit none
  
      ! Arguments
      type (bitarray), intent(in) :: src
      type (bitarray), intent(out) :: dst
  
      dst%integer_size = src%integer_size
  
      dst%nx = src%nx
      dst%ny = src%ny
  
      dst%x_int_dim = src%x_int_dim
      dst%y_int_dim = src%y_int_dim

      allocate(dst%iarray(dst%x_int_dim, dst%y_int_dim))
      dst%iarray = src%iarray
 
   end subroutine bitarray_copy
 
 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   ! Name: bitarray_set
   !
   ! Purpose: Set the bit located at (i,j) to TRUE
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   subroutine bitarray_set(b, i, j)
 
      implicit none
  
      ! Arguments
      integer, intent(in) :: i, j
      type (bitarray), intent(inout) :: b
  
      ! Local variables
      integer :: n_integer, n_bit
  
      n_integer = ((i-1) / b%integer_size) + 1
      n_bit = mod((i-1), b%integer_size)
  
      b%iarray(n_integer, j) = ibset(b%iarray(n_integer, j), n_bit) 
 
   end subroutine bitarray_set
 
 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   ! Name: bitarray_clear
   !
   ! Purpose: Set the bit located at (i,j) to FALSE 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   subroutine bitarray_clear(b, i, j)
 
      implicit none
  
      ! Arguments
      integer, intent(in) :: i, j
      type (bitarray), intent(inout) :: b
  
      ! Local variables
      integer :: n_integer, n_bit
  
      n_integer = ((i-1) / b%integer_size) + 1
      n_bit = mod((i-1), b%integer_size)
  
      b%iarray(n_integer, j) = ibclr(b%iarray(n_integer, j), n_bit) 
 
   end subroutine bitarray_clear
 
 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   ! Name: bitarray_test
   !
   ! Purpose: To return the value of the bit located at (i,j)
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   function bitarray_test(b, i, j)
 
      implicit none
  
      ! Arguments
      integer, intent(in) :: i, j
      type (bitarray), intent(in) :: b
  
      ! Local variables
      logical :: bitarray_test
      integer :: n_integer, n_bit
  
      n_integer = ((i-1) / b%integer_size) + 1
      n_bit = mod((i-1), b%integer_size)
  
      bitarray_test = btest(b%iarray(n_integer,j), n_bit) 
 
   end function bitarray_test
 
 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   ! Name: bitarray_merge
   !
   ! Purpose: The first bitarray argument, b1, is set to the union of the .TRUE. 
   !   bits in b1 and b2. That is, after returning, a bit x in b1 is set if
   !   either x was set in b1 or x was set in b2. Thus, b1 AND b2 MUST BE BIT 
   !   ARRAYS OF THE SAME DIMENSIONS.
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   subroutine bitarray_merge(b1, b2)
 
      implicit none
  
      ! Arguments
      type (bitarray), intent(inout) :: b1, b2
  
      ! Local variables
      integer :: i, j
  
      if (b1%x_int_dim /= b2%x_int_dim .or. b1%y_int_dim /= b2%y_int_dim) then
         call mprintf(.true.,ERROR,'In bitarray_merge(), b1 and b2 have different dimensions.')
      end if
  
      do i=1,b1%x_int_dim
         do j=1,b1%y_int_dim
            b1%iarray(i,j) = ior(b1%iarray(i,j), b2%iarray(i,j))  
         end do
      end do
 
   end subroutine bitarray_merge
   
 
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   ! Name: bitarray_destroy
   !
   ! Purpose: To deallocate all allocated memory associated with the bit array
   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
   subroutine bitarray_destroy(b)
 
      implicit none
  
      ! Arguments
      type (bitarray), intent(inout) :: b
  
      if (associated(b%iarray)) then
         deallocate(b%iarray)
      else
         call mprintf(.true.,WARN,'In bitarray_destroy(), b is not allocated.')
      end if
 
   end subroutine bitarray_destroy
 
#ifdef _METGRID
   logical function bitarray_all_valid_rect(b, nx, ny)

      implicit none

      ! Arguments
      type (bitarray), intent(in) :: b
      integer, intent(in) :: nx, ny   ! Requested logical rectangle dims (bits)

      ! Locals
      integer :: ii, jj      ! Loop counters over integers (words)
      integer :: full, tail  ! Full words and tail bits in x
      integer :: needed      ! Integer words needed per row in x
      integer :: words       ! Integer words to scan in x this row
      integer :: w           ! Scratch word
      integer :: allbits     ! All-ones mask
      logical :: row_ok      ! This row status

      ! Conservative default: report FALSE for any unsupported/uncertain case so
      ! that callers fall back to the unchanged original loops.
      bitarray_all_valid_rect = .false.

      ! Configured default: INTEGER32. Require 32-bit integers both in the
      ! compiled default kind of this module and in this descriptor.
      if (bit_size(0) /= 32) return
      if (b%integer_size /= 32) return

      ! Descriptor association MUST be checked before any bounds query.
      if (.not. associated(b%iarray)) return

      ! Require positive header dimensions.
      if (b%nx <= 0) return
      if (b%ny <= 0) return

      ! Require positive request, within header bounds.
      if (nx <= 0) return
      if (nx >= huge(0)) return
      if (ny <= 0) return
      if (ny >= huge(0)) return
      if (nx > b%nx) return
      if (ny > b%ny) return

      ! Require unit lower bounds on the actual storage array.
      if (lbound(b%iarray, 1) /= 1) return
      if (lbound(b%iarray, 2) /= 1) return

      ! Compute complete words plus possible tail without nx+31 overflow.
      full = nx / 32
      tail = mod(nx, 32)
      needed = full
      if (tail > 0) needed = full + 1
      if (needed <= 0) return

      ! The actual storage integer dimensions must suffice for the requested
      ! complete words plus tail, conservatively, per the actual allocation
      ! convention (CEILING(real(nx)/real(integer_size))).
      if (b%x_int_dim < needed) return
      if (b%y_int_dim < ny) return
      if (size(b%iarray, 1) < needed) return
      if (size(b%iarray, 2) < ny) return

      allbits = not(0)

      do jj = 1, ny
         row_ok = .true.
         do ii = 1, full
            w = b%iarray(ii, jj)
            if (w /= allbits) then
               row_ok = .false.
               exit
            end if
         end do
         if (row_ok .and. tail > 0) then
            w = b%iarray(full + 1, jj)
            if (ibits(w, 0, tail) /= ibits(allbits, 0, tail)) then
               row_ok = .false.
            end if
         end if
         if (.not. row_ok) return
      end do

      bitarray_all_valid_rect = .true.

   end function bitarray_all_valid_rect
#endif

end module bitarray_module
