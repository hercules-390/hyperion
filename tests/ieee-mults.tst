*Testcase ieee-mults.tst: Load Lengthened and Multiply tests
*Message Testcase ieee-divs.tst: Load Lengthened and Multiply tests
*Message ..includes LOAD LENGTHENED (6), MULTIPLY (9), MULTIPLY AND ADD (4)
*Message ..MULTPLY AND SUBTRACT (4), 23 instr total
*Message ..Test case values generate only integer results.  

sysclear
archmode esame
loadcore "$(testpath)/ieee-mults.core"
runtest .1

*Compare
r 420.10  # BFP Short Products part 1 Expecting 2 2 8 8
*Want "MEEB/MEEBR tests 1, 2" 40000000 40000000 41000000 41000000
*Compare
r 430.10  # BFP Short Products part 2 -8 -8 4 4
*Want "MEEB/MEEBR tests 3, 4" C1000000 C1000000 40800000 40800000

*Compare
r 4C0.10  # BFP Short M-Add part 1 Expecting 3 3 10 10
*Want "MAEB/MAEBR tests 1, 2" 40400000 40400000 41200000 41200000
*Compare
r 4D0.10  # BFP Short M-Add part 2 Expecting -4, -4, 2, 2
*Want "MAEB/MAEBR tests 3, 4" C0800000 C0800000 40000000 40000000 

*Compare
r 4E0.10  # BFP Short M-Sub part 1 Expecting 1, 1, 6, 6
*Want "MSEB/MSEBR tests 1, 2" 3F800000 3F800000 40C00000 40C00000
*Compare
r 4F0.10  # BFP Short M-Sub part 2 Expecting -12, -12, 6, 6
*Want "MSEB/MSEBR tests 3, 4" C1400000 C1400000 40C00000 40C00000 

*Compare
r 440.10  # BFP Long products from short x short part 1 Expecting 2.0 2.0
*Want "MDEB/MDEBR test 1" 40000000 00000000 40000000 00000000
*Compare
r 450.10  # BFP Long products from short x short part 2 Expecting 8.0 8.0
*Want "MDEB/MDEBR test 2" 40200000 00000000 40200000 00000000
*Compare
r 460.10  # BFP Long products from short x short part 3 Expecting -8.0 -8.0
*Want "MDEB/MDEBR test 3" C0200000 00000000 C0200000 00000000
*Compare
r 470.10  # BFP Long products from short x short part 4 Expecting 4.0 4.0
*Want "MDEB/MDEBR test 4" 40100000 00000000 40100000 00000000

*Compare
r 480.10  # BFP Long products from long x long part 1
*Want "MDB/MDBR test 1" 40000000 00000000 40000000 00000000
*Compare
r 490.10  # BFP Long products from long x long part 2
*Want "MDB/MDBR test 2" 40200000 00000000 40200000 00000000
*Compare
r 4A0.10  # BFP Long products from long x long part 3
*Want "MDB/MDBR test 3" C0200000 00000000 C0200000 00000000
*Compare
r 4B0.10  # BFP Long products from long x long part 4
*Want "MDB/MDBR test 4" 40100000 00000000 40100000 00000000

*Compare
r 300.10  # BFP Long M-Add part 1 Expecting 3 3 
*Want "MADB/MADBR test 1" 40080000 00000000 40080000 00000000
*Compare
r 310.10  # BFP Long M-Add part 2 Expecting 10 10
*Want "MADB/MADBR test 2" 40240000 00000000 40240000 00000000
*Compare
r 320.10  # BFP Long M-Add part 3 Expecting -4, -4
*Want "MADB/MADBR test 3" C0100000 00000000 C0100000 00000000
*Compare
r 330.10  # BFP Long M-Add part 4 Expecting 2, 2
*Want "MADB/MADBR test 4" 40000000 00000000 40000000 00000000

*Compare
r 340.10  # BFP Long M-Sub part 1 Expecting 1, 1
*Want "MSDB/MSDBR test 1" 3FF00000 00000000 3FF00000 00000000
*Compare
r 350.10  # BFP Long M-Sub part 2 Expecting 6, 6
*Want "MSDB/MSDBR test 2" 40180000 00000000 40180000 00000000
*Compare
r 360.10  # BFP Long M-Sub part 3 Expecting -12, -12
*Want "MSDB/MSDBR test 3" C0280000 00000000 C0280000 00000000
*Compare
r 370.10  # BFP Long M-Sub part 4 Expecting 6, 6
*Want "MSDB/MSDBR test 4" 40180000 00000000 40180000 00000000

*Compare
r 500.10  # BFP Extended products from long x long part 1a
*Want "MXDBR test 1a" 40000000 00000000 00000000 00000000
*Compare
r 510.10  # BFP Extended products from long x long part 1b
*Want "MXDBR test 1b" 40000000 00000000 00000000 00000000
*Compare
r 520.10  # BFP Extended products from long x long part 2a
*Want "MXDBR test 2a" 40020000 00000000 00000000 00000000
*Compare
r 530.10  # BFP Extended products from long x long part 2b
*Want "MXDBR test 2b" 40020000 00000000 00000000 00000000
*Compare
r 540.10  # BFP Extended products from long x long part 3a
*Want "MXDBR test 3a" C0020000 00000000 00000000 00000000
*Compare
r 550.10  # BFP Extended products from long x long part 3b
*Want "MXDBR test 3b" C0020000 00000000 00000000 00000000
*Compare
r 560.10  # BFP Extended products from long x long part 4a
*Want "MXDBR test 4a" 40010000 00000000 00000000 00000000
*Compare
r 570.10  # BFP Extended products from long x long part 4b
*Want "MXDBR test 4b" 40010000 00000000 00000000 00000000

*Compare
r 580.10  # BFP Extended products from extended x extended part 1a
*Want "MXBR test 1a" 40000000 00000000 00000000 00000000
*Compare
r 590.10  # BFP Extended products from extended x extended part 1b
*Want "MXBR test 1b" 40000000 00000000 00000000 00000000
*Compare
r 5A0.10  # BFP Extended products from extended x extended part 2a
*Want "MXBR test 2a" 40020000 00000000 00000000 00000000
*Compare
r 5B0.10  # BFP Extended products from extended x extended part 2b
*Want "MXBR test 2b" 40020000 00000000 00000000 00000000
*Compare
r 5C0.10  # BFP Extended products from extended x extended part 3a
*Want "MXBR test 3a" C0020000 00000000 00000000 00000000
*Compare
r 5D0.10  # BFP Extended products from extended x extended part 3b
*Want "MXBR test 3b" C0020000 00000000 00000000 00000000
*Compare
r 5E0.10  # BFP Extended products from extended x extended part 4a
*Want "MXBR test 4a" 40010000 00000000 00000000 00000000
*Compare
r 5F0.10  # BFP Extended products from extended x extended part 4b
*Want "MXBR test 4b" 40010000 00000000 00000000 00000000

*Done

