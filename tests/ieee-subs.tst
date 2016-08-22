*
*Testcase ieee-subs.tst: IEEE Subtract, Convert From/To Fixed
*Message Testcase ieee-subs.tst: IEEE subtract, Convert From/To Fixed
*Message ..Includes SUBTRACT (5), CONVERT FROM/TO FIXED 32 (6), 11 instr total
*
*
* SUBTRACT tests - Binary Floating Point
*
sysclear
archmode esame
loadcore "$(testpath)/ieee-subs.core"
runtest .1

*Compare
r 410.10  # Inputs converted to BFP short 2 1 4 -2
*Want "CEGBR tests 1-4" 40000000 3F800000 40800000 C0000000

*Compare
r 420.10  # Inputs converted to BFP long part 1
*Want "CDGBR tests 1-2"  40000000 00000000 3FF00000 00000000
*Compare
r 430.10  # Inputs converted to BFP long part 2
*Want "CDGBR tests 3-4" 40100000 00000000 C0000000 00000000

*Compare
r 440.10  # Inputs converted to BFP ext part 1
*Want "CXGBR tests 1" 40000000 00000000 00000000 00000000
*Compare
r 450.10  # Inputs converted to BFP ext part 2
*Want "CXGBR tests 2" 3FFF0000 00000000 00000000 00000000
*Compare
r 460.10  # Inputs converted to BFP ext part 3
*Want "CXGBR tests 3" 40010000 00000000 00000000 00000000
*Compare
r 470.10  # Inputs converted to BFP ext part 4
*Want "CXGBR tests 4" C0000000 00000000 00000000 00000000

*Compare
r 500.10  # BFP short differences RXE and RRE 
*Want "SEB/SEBR test pairs 1-2" 3F800000 3F800000 40C00000 40C00000 

*Compare
r 510.10  # BFP long differences RXE and RRE part 1
*Want "SDB/SDBR test pair 1" 3FF00000 00000000 3FF00000 00000000
*Compare
r 520.10  # BFP long differences RXE and RRE part 2
*Want "SDB/SDBR test pair 2" 40180000 00000000 40180000 00000000

*Compare
r 530.10  # BFP extended differences RRE part 1
*Want "SXBR test 1" 3FFF0000 00000000 00000000 00000000
*Compare
r 540.10  # BFP extended differences RRE part 2
*Want "SXBR test 1" 40018000 00000000 00000000 00000000

*Compare
r 550.10   # Short BFP to Integer results part 1
*Want "CGEB/CGEBR test pair 1" 00000000 00000001 00000000 00000001
*Compare
r 560.10   # Short BFP to Integer results part 2
*Want "CGEB/CGEBR test pair 2" 00000000 00000006 00000000 00000006

*Compare
r 570.10   # Convert long BFP to Integer results part 1
*Want "CGDB/CGDBR test pair 1" 00000000 00000001 00000000 00000001 
*Compare
r 580.10   # Convert long BFP to Integer results part 2
*Want "CGDB/CGDBR test pair 2" 00000000 00000006 00000000 00000006

*Compare
r 590.10    # Convert extended BFP to Integer results
*Want "CGXBR tests 1-2" 00000000 00000001 00000000 00000006

*Done

