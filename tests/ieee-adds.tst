*
*Testcase ieee-adds.tst: IEEE Add, Convert From/To Fixed
*Message Testcase ieee-adds.tst: IEEE Add, Convert From/To Fixed
*Message ..Includes CONVERT FROM/TO FIXED 32 (6), ADD (5), 11 instr total
*
* ADD tests - Binary Floating Point
*
#
# Tests five addition instructions:
#   ADD (extended BFP, RRE) 
#   ADD (long BFP, RRE) 
#   ADD (long BFP, RXE) 
#   ADD (short BFP, RRE) 
#   ADD (short BFP, RXE) 
#
# Also tests the following three conversion instructions
#   CONVERT FROM FIXED (32 to short BFP, RRE)
#   CONVERT FROM FIXED (32 to long BFP, RRE) 
#   CONVERT FROM FIXED (32 to extended BFP, RRE)  
#   CONVERT TO FIXED (32 to short BFP, RRE)
#   CONVERT TO FIXED (32 to long BFP, RRE) 
#   CONVERT TO FIXED (32 to extended BFP, RRE)  
#
# Also tests the following floating point support instructions
#   LOAD  (Short)
#   LOAD  (Long)
#   LOAD ZERO (Long)
#   STORE (Short)
#   STORE (Long)
#
#  Convert integers 1, 2, 4, -2 to each BFP floating point format
#  Add first and second pairs together (1 + 2, 4 + -2) in each format
#
sysclear
archmode esame
loadcore $(testpath)/ieee-adds.core
runtest .1

*Compare
r 410.10  # Inputs converted to BFP short 
*Want "CEFBR results 1-4" 3F800000 40000000 40800000 C0000000

*Compare
r 420.10  # Inputs converted to BFP long one of two
*Want "CDFBR results 1, 2" 3FF00000 00000000 40000000 00000000
*Compare
r 430.10  # Inputs converted to BFP long two of two
*Want "CDFBR results 3, 4" 40100000 00000000 C0000000 00000000

*Compare
r 440.10  # Inputs converted to BFP ext one of four 
*Want "CXFBR result 1" 3FFF0000 00000000 00000000 00000000
*Compare
r 450.10  # Inputs converted to BFP ext two of four 
*Want "CXFBR result 2" 40000000 00000000 00000000 00000000
*Compare
r 460.10  # Inputs converted to BFP ext three of four 
*Want "CXFBR result 3" 40010000 00000000 00000000 00000000
*Compare
r 470.10  # Inputs converted to BFP ext four of four 
*Want "CXFBR result 4" C0000000 00000000 00000000 00000000

*Compare
r 500.10  # BFP short sums RXE & RRE
*Want "AEB/AEBR results 1-2" 40400000 40400000 40000000 40000000

*Compare
r 510.10  # BFP long sums RXE and RRE result 1
*Want "ADB/ADBR result 1" 40080000 00000000 40080000 00000000
*Compare
r 520.10  # BFP long sums RXE and RRE result 2
*Want "ADB/ADBR result 2" 40000000 00000000 40000000 00000000

*Compare
r 530.10  # BFP extended sum RRE result 1
*Want "AXBR result 1" 40008000 00000000 00000000 00000000
*Compare
r 540.10  # BFP extended sum RRE result 2
*Want "AXBR result 2" 40000000 00000000 00000000 00000000

*Compare
r 550.10   # Short BFP to Integer results
*Want "CFEBR results 1-4"  00000003 00000003 00000002 00000002

*Compare
r 560.10   # Convert long BFP to Integer results
*Want "CFDBR results 1-4"  00000003 00000003 00000002 00000002

*Compare
r 570.8    # Convert extended BFP to Integer results
*Want "CFXBR results 1, 2"  00000003 00000002


*Done

