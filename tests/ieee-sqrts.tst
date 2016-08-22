*
*Testcase ieee-sqrts.tst: IEEE Square Root, Convert From/To Fixed
*Message Testcase ieee-sqrts.tst: IEEE Square Root, Convert From/To Fixed
*Message ..Includes SQUARE ROOT (5), CONVERT FROM/TO FIXED 32 (6), 11 instr total
*
* SQUARE ROOT tests - Binary Floating Point
*
#
# Tests five square root instructions:
#   SQUARE ROOT (extended BFP, RRE) 
#   SQUARE ROOT (long BFP, RRE) 
#   SQUARE ROOT (long BFP, RXE) 
#   SQUARE ROOT (short BFP, RRE) 
#   SQUARE ROOT (short BFP, RXE) 
#
# Also tests the following twelve conversion instructions
#   CONVERT FROM FIXED (32 to short BFP, RRE)
#   CONVERT FROM FIXED (32 to long BFP, RRE) 
#   CONVERT FROM FIXED (32 to extended BFP, RRE)  
#   CONVERT TO FIXED (32 to short BFP, RRE)
#   CONVERT TO FIXED (32 to long BFP, RRE) 
#   CONVERT TO FIXED (32 to extended BFP, RRE)  
#   CONVERT FROM FIXED (64 to short BFP, RRE)
#   CONVERT FROM FIXED (64 to long BFP, RRE) 
#   CONVERT FROM FIXED (64 to extended BFP, RRE)  
#   CONVERT TO FIXED (64 to short BFP, RRE)
#   CONVERT TO FIXED (64 to long BFP, RRE) 
#   CONVERT TO FIXED (64 to extended BFP, RRE)  
#
# Also tests the following floating point support instructions
#   LOAD  (Short)
#   LOAD  (Long)
#   LOAD ZERO (Long)
#   STORE (Short)
#   STORE (Long)
#
#  Convert integers 1, 2, 4, 16 to each BFP floating point format
#  Take the square root of each in each BFP format
#  Convert back to integers (Note: SQRT(2) will/should round)
#  Because all inputs must be positive, we'll use this rig to test 
#     logical (unsigned) conversions when that support is added to 
#     Hercules.   And we will test 32 and 64 bit logical conversions.  
#     The comments will still say "integers" though. 
#
sysclear
archmode esame
loadcore "$(testpath)/ieee-sqrts.core"
runtest .1

*Compare
r 410.10  # Inputs converted to BFP short 
*Want "CEFBR/CEGBR test pairs 1-2" 3F800000 40000000 40800000 41800000

*Compare
r 420.10  # Inputs converted to BFP long one of two
*Want "CDFBR/CDGBR test pair 1" 3FF00000 00000000 40000000 00000000
*Compare
r 430.10  # Inputs converted to BFP long two of two
*Want "CDFBR/CDGBR test pair 2" 40100000 00000000 40300000 00000000

*Compare
r 440.10  # Inputs converted to BFP ext one of four 
*Want "CXFBR test 1" 3FFF0000 00000000 00000000 00000000
*Compare
r 450.10  # Inputs converted to BFP ext two of four 
*Want "CXGBR test 1" 40000000 00000000 00000000 00000000
*Compare
r 460.10  # Inputs converted to BFP ext three of four 
*Want "CXFBR test 2" 40010000 00000000 00000000 00000000
*Compare
r 470.10  # Inputs converted to BFP ext four of four 
*Want "CXGBR test 2" 40030000 00000000 00000000 00000000

*Compare
r 500.10  # BFP short square roots RXE and RRE part 1
*Want "SQEB/SQEBR Tests 1-2" 3F800000 3F800000 3FB504F3 3FB504F3
*Compare
r 510.10  # BFP short square roots RXE and RRE part 2
*Want "SQEB/SQEBR Tests 3-4" 40000000 40000000 40800000 40800000

*Compare
r 520.10  # BFP long square roots RXE and RRE part 1
*Want "SQDB/SQDBR Test 1" 3FF00000 00000000 3FF00000 00000000
*Compare
r 530.10  # BFP long square roots RXE and RRE part 2
*Want "SQDB/SQDBR Test 2" 3FF6A09E 667F3BCD 3FF6A09E 667F3BCD
*Compare
r 540.10  # BFP long square roots RXE and RRE part 3
*Want "SQDB/SQDBR Test 3" 40000000 00000000 40000000 00000000
*Compare
r 550.10  # BFP long square roots RXE and RRE part 4
*Want "SQDB/SQDBR Test 4" 40100000 00000000 40100000 00000000

*Compare
r 560.10  # BFP extended square roots RRE part 1
*Want "SQXBR test 1" 3FFF0000 00000000 00000000 00000000
*Compare
r 570.10  # BFP extended square roots RRE part 2
*Want "SQXBR test 2" 3FFF6A09 E667F3BC C908B2FB 1366EA95
*Compare
r 580.10  # BFP extended square roots RRE part 3
*Want "SQXBR test 3" 40000000 00000000 00000000 00000000
*Compare
r 590.10  # BFP extended square roots RRE part 4
*Want "SQXBR test 4" 40010000 00000000 00000000 00000000


*Compare
r A00.10   # Short BFP to Integer results part 1
*Want "CFEBR Test pair 1" 00000000 00000001 00000000 00000001
*Compare
r A10.10   # Short BFP to Integer results part 2
*Want "CGEBR Test pair 1" 00000000 00000001 00000000 00000001
*Compare
r A20.10   # Short BFP to Integer results part 3
*Want "CFEBR Test pair 2" 00000000 00000002 00000000 00000002
*Compare
r A30.10   # Short BFP to Integer results part 4
*Want "CGEBR Test pair 2" 00000000 00000004 00000000 00000004

*Compare
r A40.10   # Convert long BFP to Integer results part 1
*Want "CFDBR Test pair 1" 00000000 00000001 00000000 00000001
*Compare
r A50.10   # Convert long BFP to Integer results part 2
*Want "CGDBR Test pair 1" 00000000 00000001 00000000 00000001
*Compare
r A60.10   # Convert long BFP to Integer results part 3
*Want "CFDBR Test pair 2" 00000000 00000002 00000000 00000002
*Compare
r A70.10   # Convert long BFP to Integer results part 4
*Want "CGDBR Test pair 2" 00000000 00000004 00000000 00000004

*Compare
r A80.10    # Convert extended BFP to Integer results part 1
*Want "CFXBR/CGXBR Test Pair 1" 00000000 00000001 00000000 00000001
*Compare
r A90.10    # Convert extended BFP to Integer results part 2
*Want "CFXBR/CGXBR Test Pair 2"  00000000 00000002 00000000 00000004

*Done

