
// look at solution here to add more memory
// https://e2e.ti.com/support/microcontrollers/c2000/f/171/t/540367?program-will-not-fit-into-available-memory-TMS320F2837xS

MEMORY
{
PAGE 0 :
   /* BEGIN is used for the "boot to SARAM" bootloader mode   */

   BEGIN           	: origin = 0x000000, length = 0x000002
   RAMM0           	: origin = 0x0000F5, length = 0x00030B
// comment out this
/*   RAMLS0          	: origin = 0x008000, length = 0x000800
   RAMLS1          	: origin = 0x008800, length = 0x000800
   RAMLS2      		: origin = 0x009000, length = 0x000800
   RAMLS3      		: origin = 0x009800, length = 0x000800
   RAMLS4      		: origin = 0x00A000, length = 0x000800 */
   // add this
   RAMLS04 : origin = 0x008000, length = 0x002800
   // you can try adding RAMLS06 and add 2800 to 10800 (origin of RAMLS05+2800) and set length to 2800
   // then set other sections that are running out of memory to RAMLS06... can probably repeat with other
   // RAMLS0X
   RAMLS05 : origin = 0x010800, length = 0x002800
   //
   RESET           	: origin = 0x3FFFC0, length = 0x000002

PAGE 1 :

   BOOT_RSVD       : origin = 0x000002, length = 0x0000F3     /* Part of M0, BOOT rom will use this for stack */
   RAMM1           : origin = 0x000400, length = 0x000400     /* on-chip RAM block M1 */

   RAMLS5      : origin = 0x00A800, length = 0x000800
   RAMLS6      : origin = 0x00B000, length = 0x000800
   RAMLS7      : origin = 0x00B800, length = 0x000800
   
   RAMGS0      : origin = 0x00C000, length = 0x002000
   RAMGS1      : origin = 0x00E000, length = 0x002000
   RAMGS2      : origin = 0x010000, length = 0x002000
   RAMGS3      : origin = 0x012000, length = 0x002000
}


SECTIONS
{
   codestart        : > BEGIN,     PAGE = 0
   .TI.ramfunc      : > RAMM0      PAGE = 0
   //.text            : >>RAMM0 | RAMLS0 | RAMLS1 | RAMLS2 | RAMLS3 | RAMLS4,   PAGE = 0
   //*add this
   .text 			: > RAMLS04,   PAGE = 0
   //
   .cinit           : > RAMM0,     PAGE = 0
   .pinit           : > RAMM0,     PAGE = 0
   .switch          : > RAMM0,     PAGE = 0
   .reset           : > RESET,     PAGE = 0, TYPE = DSECT /* not used, */

   .stack           : > RAMM1,     PAGE = 1
   // if run out of memory, just keep adding RAMLS04 instead of whatever it is linked to and change page to 0
   .ebss            : > RAMLS05,   PAGE = 0
//   .ebss            : > RAMLS5,    PAGE = 1
   .econst          : > RAMLS04,    PAGE = 0
//   .econst          : > RAMLS5,    PAGE = 1
   .esysmem         : > RAMLS04,    PAGE = 0
//   .esysmem         : > RAMLS5,    PAGE = 1

   ramgs0           : > RAMGS0,    PAGE = 1
   ramgs1           : > RAMGS1,    PAGE = 1
}

/*
//===========================================================================
// End of file.
//===========================================================================
*/
