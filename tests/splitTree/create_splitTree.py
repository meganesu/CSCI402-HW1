splitTree = open('splitTree', 'w')
a = open('write1', 'w')
b = open('write2', 'w')
c = open('write3', 'w')
d = open('write4', 'w')
e = open('read1', 'w')
f = open('read2', 'w')
g = open('read3', 'w')
h = open('read4', 'w')

for i in range(4096,0,-1):
  splitTree.write('a ' + str(i) + ' 7\n')

  if (i % 4 == 0):
    e.write('q ' + str(i) + '\n')
  elif (i % 4 == 1):
    f.write('q ' + str(i) + '\n')
  elif (i % 4 == 2):
    g.write('q ' + str(i) + '\n')
  elif (i % 4 == 3):
    h.write('q ' + str(i) + '\n')

for i in range(4096, 8192, 1):
  splitTree.write('a ' + str(i) + ' 7\n')

for i in range(8192, 4096, -1):

  if (i % 4 == 0):
    a.write('d ' + str(i) + ' \n')
  elif (i % 4 == 1):
    b.write('d ' + str(i) + ' \n')
  elif (i % 4 == 2):
    c.write('d ' + str(i) + ' \n')
  elif (i % 4 == 3):
    d.write('d ' + str(i) + ' \n')

