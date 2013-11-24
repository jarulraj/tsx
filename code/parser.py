# PARSE DATA LOG

import subprocess
import shlex

FILE = "data.log"

# LOOP THROUGH PARAMETERS

t = 2 # number of threads
r = 1 # ratio of gets:puts (not used currently)

itr = 0

while t<=32:

    for cc in [ 'hle', 'rtm', 'tbl', 'spin' ]:

        print "Threads : "+str(t)+" CC : "+str(cc)

        #CMD :: egrep "tester|Total" data.log | grep -A 1 "t1'" | grep -A 1 "hle" | grep "Total" | awk '{print $3;}'
        cmd =  [ "egrep", "tester|Total", FILE ]; 
        task1 =  subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        cmd = [ "grep", "-A 1", "t"+str(t) ];
        task2 =  subprocess.Popen(cmd, stdin=task1.stdout, stdout=subprocess.PIPE, stderr=subprocess.PIPE )

        cmd = [ "grep", "-A 1", str(cc) ];
        task3 =  subprocess.Popen(cmd, stdin=task2.stdout, stdout=subprocess.PIPE, stderr=subprocess.PIPE )

        cmd = [ "grep", "Total" ];
        task4 =  subprocess.Popen(cmd, stdin=task3.stdout, stdout=subprocess.PIPE, stderr=subprocess.PIPE )

        cmd = [ "awk", "{ print $3; }" ];
        task5 =  subprocess.Popen(cmd, stdin=task4.stdout, stdout=subprocess.PIPE, stderr=subprocess.PIPE )
        
        (out, err) = task5.communicate()
        print out
        

    t *= 4 
