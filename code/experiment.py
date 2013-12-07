# EXPERIMENT RUNNER

import argparse
import copy
import os
import subprocess
import shlex

EXEC = "./tester/main"

# LOOP THROUGH PARAMETERS

t = 8 # number of threads
s = 1 # time for each run
o = 1 # number of ops per txn
r = 1 # ratio of gets:puts (not used currently)

itr = 0

k = "uniform" # choose distribution

def add_variable(title, labels, vals, variable, this_range):
    new_labels = []
    title.append(variable)
    new_vals = []
    for i in range(0, len(vals)):
        for j in this_range:
            new_arg = copy.copy(vals[i])
            new_arg[variable] = j
            new_vals.append(new_arg)

            new_label = copy.copy(labels[i])
            new_label.append(j)
            new_labels.append(new_label)

    return (new_labels, new_vals)

def experiment(title, labels, vals, iters):
    for i in range(0, len(labels), iters):
        print str(title),
        for j in range(0, iters):
            print("\t%s" % labels[i+j]),
        print("")
        for cc in [ 'hle', 'rtm', 'tbl', 'spin', 'sspin']:
            results = []
            for j in range(0, iters):
                val = vals[i+j]
                # RUN EXPERIMENT
                cmd = [ EXEC, '-s'+str(val['s']), '-t'+str(val['t']), '-o'+str(val['o']), '-k'+val['k'], '-e0', cc]
                #print str(cmd)
                task =  subprocess.Popen(cmd, 
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.PIPE
                                         )
                (out, err) = task.communicate()
                results.append(int(out) * val['o'])
                
            print("%s" % cc),
            for result in results:
                print("\t%s" % result),
            print("")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Run various workloads.")
    parser.add_argument('--ops', dest='ops', action='store_true',
                        default=False)
    parser.add_argument('--threads', dest='threads', action='store_true',
                        default=False)
    args = parser.parse_args()

    vals = []
    vals.append({'s':s, 't':t, 'o':o, 'k':k})

    labels = []
    labels.append([])

    title = []

    iters = 1

    print str(args)
    if args.ops:
        (labels, vals) = add_variable(title, labels, vals, 'o', range(1, 10, 2))
        iters = 5
    if args.threads:
        (labels, vals) = add_variable(title, labels, vals, 't', range(1, 10, 2))
        iters = 5

    experiment(title, labels, vals, iters)
