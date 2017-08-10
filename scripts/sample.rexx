/* REXX */
 parse arg parms

 parms = space(parms)
 argc  = words(parms)

 parse version ver
 parse source src

 env = address()

 parse var src . . cmd
 who = filespec("n",cmd)
 parse var who who "." . 

 say who " started"
 say who " version . . . .  :" ver
 say who " source  . . . .  :" src
 say who " hostenv . . . .  :" env


 say who " date  . . . . .  :" date()
 say who " time  . . . . .  :" time() 
 
 
 if parms = "" 
 then do
      say who " arguments . . .  : no arguments given"
      ret = 0
      end
 else do
      say who " arguments . . .  :" parms
      ret = parms
      end

 Say who " Hercules version :" value('version',,'SYSTEM')
 Say who " RC environment . :" value('HERCULES_RC',,'ENVIRONMENT')

 address hercules 'uptime'

 say who " ended"
 
 exit ret
 
