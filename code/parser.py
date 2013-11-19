# PARSE DATA LOG

import subprocess
import shlex

FILE = "data.log"

# LOOP THROUGH PARAMETERS

t = 1 # number of threads
o = 1 # number of ops per txn
r = 1 # ratio of gets:puts (not used currently)

itr = 0

while t<=4:
    
    o = 1
    while o<=2:

        for cc in [ 'hle', 'rtm', 'tbl', 'spin' ]:
            # PARSE        
            #LIKE :: egrep "tester|Total" data.log | grep -A 1 "t1'" | grep -A 1 "hle" | grep "Total" | awk '{print $3;}'
            #cmd = [ "egrep", "tester|Total", FILE, " | grep -A 1 \"t"+str(t)+"'\" | grep -A 1 \""+str(cc)+"\" | grep \"Total\" | awk '{print $3;}'" ] 
            #cmd =  "egrep " + "tester|Total " + FILE + " | grep -A 1 \"t"+str(t)+"'\" | grep -A 1 \""+str(cc)+"\" | grep \"Total\" | awk '{print $3;}'" 
            cmd =  "egrep " + " \"tester|Total\" " + FILE + " | grep t"+str(t)+" "  
            args = shlex.split(cmd);
            print args

            task =  subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE )   
            (out, err) = task.communicate()
            print out 



        o *= 2        
    t *= 2 
