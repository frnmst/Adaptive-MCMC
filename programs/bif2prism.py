##                                bif2prism.py                                ##
# Converts bayesian networks specified in 'bif' format to Prism source code.

import re
import sys
import copy

if len(sys.argv) != 3:
    bif = raw_input('enter input(bif) filename: ')
    prism = raw_input('enter output(prism) filename: ')
else:
    bif = sys.argv[1]
    prism = sys.argv[2]

variables = []
domains = {}
parents = {}
cpts = {}

bif_file = open(bif, 'r')
while True:
    line = bif_file.readline()
    if len(line) == 0:
        break
    match = re.search(r'^\s*variable\s+(\w+)\s*{', line)
    if match:
        variable = match.group(1).lower()
        variables.append(variable)
        nextline = bif_file.readline()
        match1 = re.search(r'^\s*type\s+discrete.+{(.+)}.+', nextline)
        if match1:
            crumb = match1.group(1)
            crumb = crumb.replace(' ', '')
            domains[variable]=crumb.lower().split(',')
        else:
            print 'domain not found for '+variable

bif_file.seek(0)

while True:
    line = bif_file.readline()
    if len(line) == 0:
        break
    match = re.search(r'^\s*probability\s+\(\s*(\w+)\s*(\|(.+))?\)\s*{', line)
    if match:
        variable = match.group(1).lower()
        crumb = match.group(3)
        if not variable:
            print 'failed to recognize variable'
            break
        if not crumb:
	    parents[variable]=[]
        else:
            crumb = crumb.lower()
            crumb = crumb.replace(' ', '')
            parents[variable] = crumb.split(',')
	tableline = bif_file.readline()
	cpt = {}
	while not tableline.startswith('}'):
	    if len(tableline) == 0:
		break
	    if parents[variable] == []:
		tablematch = re.search(r'^\s*table\s+(.+);', tableline)
		if tablematch:
		    crumb1 = tablematch.group(1)
		    crumb1 = crumb1.replace(' ', '')
		    cpt[()] = crumb1.split(',')
	    else:
	        tablematch = re.search(r'^\s*\((.+)\)\s+(.+);', tableline)
	        if tablematch:
		    crumb1 = tablematch.group(1).lower()
		    crumb2 = tablematch.group(2)
		    crumb1 = crumb1.replace(' ', '')
		    crumb2 = crumb2.replace(' ', '')
		    cpt[tuple(crumb1.split(','))] = crumb2.split(',')
	    tableline = bif_file.readline()
	cpts[variable] = cpt
	## print variable
	## print str(cpts[variable])

bif_file.close()
# we have complete data-structures for writing Prism source code
prism_file = open(prism, 'w')
for variable in variables:
    if len(parents[variable]) == 0:
        prism_file.write('values('+variable+', [')
        prism_file.write(','.join(domains[variable]))
        prism_file.write(']).\n')
    else:
        prism_file.write('values('+variable+'(')
        prism_file.write(','.join(['_'] * len(parents[variable])))
        prism_file.write('), ['+','.join(domains[variable]))
        prism_file.write(']).\n')
prism_file.write('\n')
for variable in variables:
    cpt = cpts[variable]
    for row in cpt:
        prism_file.write('set_sw('+variable)
        if row == ():
            prism_file.write(', ['+','.join(cpt[row])+']).')
            prism_file.write('\n')
        else:
            prism_file.write('('+','.join(map(str, row))+'), ['+','.join(cpt[row])+']).')
            prism_file.write('\n')

dummy = copy.deepcopy(parents)

# do topological sort and write to Prism file
prism_file.write('world(')
tempv = copy.deepcopy(variables)
for i in xrange(0, len(tempv)):
    tempv[i] = tempv[i][0].upper()+tempv[i][1:]
prism_file.write(','.join(tempv)+') :-\n')
    
while len(dummy) > 0:
    keystodelete=[]
    for key in dummy:
        if len(dummy[key]) == 0:
            # we should write this variable now
            if len(parents[key]) == 0:
                prism_file.write('\tmsw('+key+', '+key[0].upper()+key[1:]+'),\n')
            else:
                print '%%%%%%%%%%%%%%%%i am here%%%%%%%%%%%%%%%%'
                prism_file.write('\tmsw('+key+'(')
                pacopy = copy.deepcopy(parents[key])
                for i in xrange(0,len(pacopy)):
                    pacopy[i] = pacopy[i][0].upper()+pacopy[i][1:]
                prism_file.write(','.join(pacopy)+'), '+key[0].upper()+key[1:]+'),\n')

            keystodelete.append(key)
    print '********************'
    for key in keystodelete:
        del dummy[key]
        print 'deleted key '+key
    for key in keystodelete:
        print 'processing =='+key
        for key1 in dummy:
            print 'key1+++'+key1
            if key in dummy[key1]:
                dummy[key1].remove(key)

prism_file.close()
