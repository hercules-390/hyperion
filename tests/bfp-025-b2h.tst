*Testcase bfp-025-b2h.tst:        THDR, THDER

#Testcase bfp-025-h2b.tst: Convert BFP to HFP (2)
#..Test cases evaluate subnormal and non-finite inputs and overflow and
#..underflow detection and results for long BFP inputs.  Convert BFP to
#..HFP does not generate NaN results, does not use or change the FPCR,
#..does not generate IEEE exceptions, and does not report nor trap
#..HFP exceptions (


sysclear
archmode esame

loadcore "$(testpath)/bfp-025-b2h.core"

runtest 10.0


# Short BFP to long HFP test results
*Compare
r 1000.10
*Want "THDER  0, -0"                             00000000 00000000 80000000 00000000
r 1010.10
*Want "THDER  1.5, -1.5"                         41180000 00000000 C1180000 00000000
r 1020.10
*Want "THDER  2.5, -2.5"                         41280000 00000000 C1280000 00000000
r 1030.10
*Want "THDER  4.5, -4.5"                         41480000 00000000 C1480000 00000000
r 1040.10
*Want "THDER  8.5, -8.5"                         41880000 00000000 C1880000 00000000
r 1050.10
*Want "THDER  +Dmin, -Dmin"                      1B800000 00000000 9B800000 00000000
r 1060.10
*Want "THDER  +Dmax, -Dmax"                      21200000 00000000 A1200000 00000000
r 1070.10
*Want "THDER  +Nmax, -Nmax"                      60FFFFFF 00000000 E0FFFFFF 00000000
r 1080.10
*Want "THDER  < +Nmax, -Nmax"                    60FFFFFE 00000000 E0FFFFFE 00000000
r 1090.10
*Want "THDER  +Nmin, +Nmax"                      21400000 00000000 A1400000 00000000
r 10A0.10
*Want "THDER  +infinity, -infinity"              7FFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
r 10B0.10
*Want "THDER  +QNaN, -QNaN"                      7FFFFFFF FFFFFFFF 7FFFFFFF FFFFFFFF
r 10C0.10
*Want "THDER  +SNaN, -SNaN"                      7FFFFFFF FFFFFFFF 7FFFFFFF FFFFFFFF


# Short BFP to long HFP FPCR results
*Compare
r 1200.10
*Want "THDER FPCR 0, -0, 1.5, -1.5"              F8000000 F8000000 F8000002 F8000001
r 1210.10
*Want "THDER FPCR 2.5, -2.5, 4.5,-4.5"           F8000002 F8000001 F8000002 F8000001
r 1220.10
*Want "THDER FPCR 8.5, -8.5, +Dmin, -Dmin"       F8000002 F8000001 F8000002 F8000001
r 1230.10
*Want "THDER FPCR +Dmax, -Dmax, +Nmax, -Nmax"    F8000002 F8000001 F8000002 F8000001
r 1240.10
*Want "THDER FPCR < +Nmax, +Nmax, +Nminn, -Nmin" F8000002 F8000001 F8000002 F8000001
r 1250.10
*Want "THDER FPCR +inf, -inf, +QNaN, -QNaN"      F8000003 F8000003 F8000003 F8000003
r 1260.8
*Want "THDER FPCR +SNaN, -SNaN"                  F8000003 F8000003


# Long BFP to long HFP test results
*Compare
r 1300.10
*Want "THDR 0, -0"                               00000000 00000000 80000000 00000000
r 1310.10
*Want "THDR 1.5, -1.5"                           41180000 00000000 C1180000 00000000
r 1320.10
*Want "THDR 2.5, -2.5"                           41280000 00000000 C1280000 00000000
r 1330.10
*Want "THDR 4.5, -4.5"                           41480000 00000000 C1480000 00000000
r 1340.10
*Want "THDR 8.5, -8.5"                           41880000 00000000 C1880000 00000000
r 1350.10
*Want "THDR +Hmax, -Hmax"                        7FFFFFFF FFFFFFF8 FFFFFFFF FFFFFFF8
r 1360.10
*Want "THDR < +Hmax, -Hmax"                      7FFFFFFF FFFFFFF0 FFFFFFFF FFFFFFF0
r 1370.10
*Want "THDR > +Hmax, -Hmax"                      7FFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
r 1380.10
*Want "THDR +Hmin, -Hmin"                        00100000 00000000 80100000 00000000
r 1390.10
*Want "THDR > +Hmin, -Hmin"                      00100000 00000001 80100000 00000001
r 13A0.10
*Want "THDR < +Hmin, -Hmin"                      00000000 00000000 80000000 00000000
r 13B0.10
*Want "THDR +Dmin, -Dmin"                        00000000 00000000 80000000 00000000
r 13C0.10
*Want "THDR +Dmax, -Dmax"                        00000000 00000000 80000000 00000000
r 13D0.10
*Want "THDR +Nmax, -Nmax"                        7FFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
r 13E0.10
*Want "THDR +infinity, -infinity"                7FFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
r 13F0.10
*Want "THDR +QNaN, -QNaN"                        7FFFFFFF FFFFFFFF 7FFFFFFF FFFFFFFF
r 1400.10
*Want "THDR +SNaN, -SNaN"                        7FFFFFFF FFFFFFFF 7FFFFFFF FFFFFFFF


# Long BFP to long HFP FPCR results
*Compare
r 1600.10
*Want "THDR FPCR 0, -0, 1.5, -1.5"               F8000000 F8000000 F8000002 F8000001
r 1610.10
*Want "THDR FPCR 2.5, -2.5, 4.5,-4.5"            F8000002 F8000001 F8000002 F8000001
r 1620.10
*Want "THDR FPCR 8.5, -8.5, +Hmax, -Hmax"        F8000002 F8000001 F8000002 F8000001
r 1630.10
*Want "THDR FPCR < +Hmax, -Hmax, > +Hmax, -Hmax" F8000002 F8000001 F8000003 F8000003
r 1640.10
*Want "THDR FPCR +Hmin, -Hmin, > +Hmin, -Hmin"   F8000002 F8000001 F8000002 F8000001
r 1650.10
*Want "THDR FPCR < +Hmin, -Hmin, +Dmin, -Dmin"   F8000002 F8000001 F8000002 F8000001
r 1660.10
*Want "THDR FPCR +Dmax, -Dmax, +Nmax, -Nmax"     F8000002 F8000001 F8000003 F8000003
r 1670.10
*Want "THDR FPCR +inf, -inf, +QNaN, -QNaN"       F8000003 F8000003 F8000003 F8000003
r 1680.8
*Want "THDR FPCR +SNaN, -SNaN"                   F8000003 F8000003


*Done

