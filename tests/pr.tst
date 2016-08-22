#----------------------------------------------------------------------
* Prefix tests.  Hand written.
#----------------------------------------------------------------------

defsym  arch          z/Arch
defsym  fffff_pfx     00000000000fe000
script "$(testpath)/pr.subtst"

defsym  arch          ESA/390
defsym  fffff_pfx     00000000000ff000
script "$(testpath)/pr.subtst"

defsym  arch          S/370
defsym  fffff_pfx     00000000000ff000
script "$(testpath)/pr.subtst"
