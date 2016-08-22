
*Testcase ieee-div-nans.tst: IEEE-NaNs Test DIVIDE with zero, QNaNs and SNaNs
*Message Testcase ieee-div-nans.tst: IEEE Division with zero and NaNs
*Message ..Test case values include zero, QNaNs, and SNaNs

sysclear
archmode esame
loadcore "$(testpath)/ieee-div-nans.core"
runtest .1

r 400.40       # Display short BFP results in test log
r 440.80       # Display long BFP results in test log
r 500.80       # Display extended BFP results in test log

*Compare
r 400.10  # BFP Short quotients part 1 Expecting +inf, +inf, QNaN(1), QNaN(1)
*Want "DEB/DEBR NaN 1/4" 7F800000 7F800000 7FC10000 7FC10000
*Compare
r 410.10  # BFP Short quotients part 2 Expecting QNaN(1), QNaN(1), QNaN(3), QNaN(3)
*Want "DEB/DEBR NaN 2/4" 7FC10000 7FC10000 7FC30000 7FC30000
*Compare
r 420.10  # BFP Short quotients part 3 Expecting QNaN(3), QNaN(3), 0, 0
*Want "DEB/DEBR NaN 3/4" 7FC30000 7FC30000 00000000 00000000
*Compare
r 430.10  # BFP Short quotients part 4 Expecting QNaN(3), QNaN(3), 0, 0
*Want "DEB/DEBR NaN 4/4" 00000000 00000000 00000000 00000000

*Compare
r 440.10  # BFP Long quotients part 1 Expecting +inf, +inf
*Want "DDB/DDBR NaN 1/8" 7FF00000 00000000 7FF00000 00000000
*Compare
r 450.10  # BFP Long quotients part 2 Expecting QNaN(1), QNaN(1)
*Want "DDB/DDBR NaN 2/8" 7FF81000 00000000 7FF81000 00000000
*Compare
r 460.10  # BFP Long quotients part 3 Expecting QNaN(1), QNaN(1)
*Want "DDB/DDBR NaN 3/8" 7FF81000 00000000 7FF81000 00000000
*Compare
r 470.10  # BFP Long quotients part 4 Expecting QNaN(3), QNaN(3)
*Want "DDB/DDBR NaN 4/8" 7FF83000 00000000 7FF83000 00000000
*Compare
r 480.10  # BFP Long quotients part 5 Expecting QNaN(3), QNaN(3)
*Want "DDB/DDBR NaN 5/8" 7FF83000 00000000 7FF83000 00000000
*Compare
r 490.10  # BFP Long quotients part 6 Expecting 0, 0
*Want "DDB/DDBR NaN 6/8" 00000000 00000000 00000000 00000000
*Compare
r 4A0.10  # BFP Long quotients part 7 Expecting 0, 0
*Want "DDB/DDBR NaN 7/8" 00000000 00000000 00000000 00000000
*Compare
r 4B0.10  # BFP Long quotients part 8 Expecting 0, 0
*Want "DDB/DDBR NaN 8/8" 00000000 00000000 00000000 00000000

*Compare
r 500.10  # BFP Extended quotients part 1 Expecting +inf
*Want "DXBR NaN 1/8" 7FFF0000 00000000 00000000 00000000
*Compare
r 510.10  # BFP Extended quotients part 2 Expecting QNaN(1)
*Want "DXBR NaN 2/8" 7FFF8100 00000000 00000000 00000000
*Compare
r 520.10  # BFP Extended quotients part 3 Expecting QNaN(1)
*Want "DXBR NaN 3/8" 7FFF8100 00000000 00000000 00000000
*Compare
r 530.10  # BFP Extended quotients part 4 Expecting QNaN(3)
*Want "DXBR NaN 4/8" 7FFF8300 00000000 00000000 00000000
*Compare
r 540.10  # BFP Extended quotients part 5 Expecting QNaN(3)
*Want "DXBR NaN 5/8" 7FFF8300 00000000 00000000 00000000
*Compare
r 550.10  # BFP Extended quotients part 6 Expecting 0
*Want "DXBR NaN 6/8" 00000000 00000000 00000000 00000000
*Compare
r 560.10  # BFP Extended quotients part 7 Expecting 0
*Want "DXBR NaN 7/8" 00000000 00000000 00000000 00000000
*Compare
r 570.10  # BFP Extended quotients part 8 Expecting 0
*Want "DXBR NaN 8/8" 00000000 00000000 00000000 00000000


*Done

