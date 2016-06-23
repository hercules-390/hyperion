*Testcase ieee-divs.tst: Load and Divide tests
*Message Testcase ieee-divs.tst: LOAD ROUNDED (3), LOAD FP INTEGER (3), 
*Message ..DIVIDE (5), DIVIDE TO INTEGER (2), 13 instructions total
*Message ..Test case values generate integer and fractional results.

sysclear
archmode esame
loadcore ieee-divs.core
runtest 

r 420.20       # Display short BFP results in test log
r 4C0.20       # Display short BFP Divide to Integer results in test log
r 4E0.10       # Display short BFP Load FP Integer results in test log
r 440.40       # Display long BFP results in test log
r 500.40       # Display long BFP Divide to Integer results in test log
r 540.20       # Display long BFP Load FP Integer results in test log
r 480.40       # Display extended BFP results in test log
r 560.40       # Display extended BFP Load FP Integer results in test log

*Compare
r 420.10  # BFP Short quotients part 1 Expecting 0.5 0.5 0.5 0.5
*Want "DEB/DEBR test pairs 1-2" 3F000000 3F000000 3F000000 3F000000
*Compare
r 430.10  # BFP Short quotients part 2 Expecting -2 -2 1 1
*Want "DEB/DEBR test pairs 3-4" C0000000 C0000000 3F800000 3F800000

*Compare
r 4C0.10  # BFP Short Divide to Integer part 1 Expecting 0.0 1.0 0.0 2.0
*Want "DIEBR tests 1-2" 00000000 3F800000 00000000 40000000
*Compare
r 4D0.10  # BFP Short Divide to Integer part 2 Expecting -2.0 0.0 1.0 -0.0
*Want "DIEBR tests 3-4" C0000000 00000000 3F800000 80000000
# Yup, -0.  Remainder sign in DIEBR is exclusive or of divisor and dividend signs.  

*Compare
r 4E0.10  # BFP Short Load FP Integer Expecting 0.0 0.0 -2.0 1.0
*Want "FIEBR tests 1-4" 00000000 00000000 C0000000 3F800000

*Compare
r 440.10  # BFP Long quotients part 1 Expecting 0.5 0.5
*Want "DDB/DDBR test pair 1" 3FE00000 00000000 3FE00000 00000000
*Compare
r 450.10  # BFP Long quotients part 2 Expecting 0.5 0.5
*Want "DDB/DDBR test pair 2" 3FE00000 00000000 3FE00000 00000000
*Compare
r 460.10  # BFP Long quotients part 3 Expecting -2.0 -2.0
*Want "DDB/DDBR test pair 3" C0000000 00000000 C0000000 00000000
*Compare
r 470.10  # BFP Long quotients part 4 Expecting 1.0 1.0
*Want "DDB/DDBR test pair 4" 3FF00000 00000000 3FF00000 00000000

*Compare
r 500.10  # BFP Long divide to integer part 1 Expecting 0.0 1.0
*Want "DIDBR test 1" 00000000 00000000 3FF00000 00000000
*Compare
r 510.10  # BFP Long divide to integer part 2 Expecting 0.0 2.0
*Want "DIDBR test 2" 00000000 00000000 40000000 00000000
*Compare
r 520.10  # BFP Long divide to integer part 3 Expecting -2.0 0.0
*Want "DIDBR test 3" C0000000 00000000 00000000 00000000
*Compare
r 530.10  # BFP Long divide to integer part 4 Expecting 1.0 -0
*Want "DIDBR test 4" 3FF00000 00000000 80000000 00000000

*Compare
r 540.10  # BFP Long Load FP Integer Expecting 0.0 0.0
*Want "FIDBR test 1" 00000000 00000000 00000000 00000000
*Compare
r 550.10  # Short Load FP Integer Expecting -2.0 1.0
*Want "FIDBR test 2" C0000000 00000000 3FF00000 00000000 

*Compare
r 480.10  # BFP Extended quotients part 1
*Want "DXBR test 1" 3FFE0000 00000000 00000000 00000000
*Compare
r 490.10  # BFP Extended quotients part 2
*Want "DXBR test 2" 3FFE0000 00000000 00000000 00000000
*Compare
r 4A0.10  # BFP Extended quotients part 3
*Want "DXBR test 3" C0000000 00000000 00000000 00000000
*Compare
r 4B0.10  # BFP Extended quotients part 4
*Want "DXBR test 4" 3FFF0000 00000000 00000000 00000000

*Compare 
r 560.10  # BFP Extended load FP integer part 1 Expecting 0.0
*Want "FIXBR test 1" 00000000 00000000 00000000 00000000
*Compare
r 570.10  # BFP Extended load FP integer part 2 Expecting 0.0
*Want "FIXBR test 2" 00000000 00000000 00000000 00000000
*Compare
r 580.10  # BFP Extended load FP integer part 3 Expecting -2.0
*Want "FIXBR test 3" C0000000 00000000 00000000 00000000
*Compare
r 590.10  # BFP Extended load FP integer part 4 Expecting 1.0
*Want "FIXBR test 4" 3FFF0000 00000000 00000000 00000000

*Done

