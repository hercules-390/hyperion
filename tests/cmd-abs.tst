*
#--------------
defsym cmd abs
#--------------
*
defsym arch S/370
script $(testpath)/cmd-abs-2K.txt
*
defsym arch S/390
script $(testpath)/cmd-abs-4K.txt
*
defsym arch z/Arch
script $(testpath)/cmd-abs-4K.txt
