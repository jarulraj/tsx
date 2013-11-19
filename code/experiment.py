# EXPERIMENT RUNNER

import os
import subprocess
import shlex

EXEC = "./tester/main"

# LOOP THROUGH PARAMETERS

t = 1 # number of threads
s = 1 # time for each run
o = 1 # number of ops per txn
r = 1 # ratio of gets:puts (not used currently)

itr = 0

while t<=16:
    
    o = 1
    while o<=16:
    
        for cc in [ 'hle', 'rtm', 'tbl', 'spin' ]:
            # RUN EXPERIMENT        
            cmd = [ EXEC, '-s'+str(s), '-t'+str(t), '-o'+str(o), cc]

            itr += 1
            print "Case " + str(itr) + " " + str(cmd)

            task =  subprocess.Popen(cmd, 
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE
                    )

            (out, err) = task.communicate()
            print out

        o *= 2        
    t *= 2
