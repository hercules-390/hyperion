*
#------------
defsym cmd r
#------------
*
defsym arch S/370
script $(testpath)/cmd-rv-2K.txt
*
defsym arch S/390
script $(testpath)/cmd-rv-4K-32.txt
*
defsym arch z/Arch
script $(testpath)/cmd-rv-4K-64.txt
*
#------------
defsym cmd v
#------------
*
defsym arch S/370
script $(testpath)/cmd-rv-2K.txt
*
defsym arch S/390
script $(testpath)/cmd-rv-4K-32.txt
*
defsym arch z/Arch
script $(testpath)/cmd-rv-4K-64.txt
