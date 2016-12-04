*Testcase KLMD fc0
sysclear
archmode esame
r 1A0=00000001800000000000000000000200 # z/Arch restart PSW
r 1D0=0002000180000000000000000000DEAD # z/Arch pgm new PSW
r 200=41000000     # LA R0,0           R0->function code 0
r 204=4110f500      #  LA R1,PB          R1->parameter block
r 208=41200000     # LA R2,SO          R2->second operand address
r 20C=41300000     # LA R3,SOL         R3->second operand length
r 210=B93F0002     # KLMD R0,R2        Compute last message digest
r 214=12ee077eB2B20300      #  LPSWE WAITPSW     Load enabled wait PSW
r 300=00020001800000000000000000000000 #  WAITPSW Enabled wait state PSW
*
r 500=000102030405060708090A0B0C0D0E0F # Parameter block
*
r 580=F0000000000000000000000000000000 # Expected result
*
ostailor null
runtest .1
*Compare
* Display parameter block
r 500.10
*Want  C0000000 00000000 00000000 00000000
*Done


*Testcase KLMD fc1
sysclear
archmode esame
r 1A0=00000001800000000000000000000200 # z/Arch restart PSW
r 1D0=0002000180000000000000000000DEAD # z/Arch pgm new PSW
r 200=41000001     # LA R0,1           R0->function code 1
r 204=4110f500      #  LA R1,PB          R1->parameter block
r 208=4120f600      #  LA R2,SO          R2->second operand address
r 20C=41300040     # LA R3,SOL         R3->second operand length
r 210=B93F0002     # KLMD R0,R2        Compute last message digest
r 214=12ee077eB2B20300      #  LPSWE WAITPSW     Load enabled wait PSW
r 300=00020001800000000000000000000000 #  WAITPSW Enabled wait state PSW
*
r 500=67452301EFCDAB8998BADCFE10325476 # Parameter block
r 510=C3D2E1F00000000000000040         # Parameter block
*
r 580=C5592921382388B41AE53FA27A726F8B # Expected result
r 590=64D9FFA2000000000000004000000000 # Expected result
*
r 600=000102030405060708090A0B0C0D0E0F # Second operand
r 610=101112131415161718191A1B1C1D1E1F # Second operand
r 620=202122232425262728292A2B2C2D2E2F # Second operand
r 630=303132333435363738393A3B3C3D3E3F # Second operand
*
ostailor null
runtest .1
*Compare
* Display parameter block
r 500.10
*Want  C5592921 382388B4 1AE53FA2 7A726F8B
r 510.4
*Want  64D9FFA2
*Done
