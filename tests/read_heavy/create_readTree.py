readTree = open('readTree', 'w')
a = open('read1', 'w')
b = open('read2', 'w')
c = open('read3', 'w')
d = open('read4', 'w')
e = open('read5', 'w')
f = open('read6', 'w')
g = open('read7', 'w')
h = open('read8', 'w')

for i in range(8192):
  readTree.write('a ' + str(i) + ' 5\n')

for i in range(8192):
  if (i % 8 == 0):
    a.write('q ' + str(i) + '\n')
  elif (i % 8 == 1):
    b.write('q ' + str(i) + '\n')
  elif (i % 8 == 2):
    c.write('q ' + str(i) + '\n')
  elif (i % 8 == 3):
    d.write('q ' + str(i) + '\n')
  elif (i % 8 == 4):
    e.write('q ' + str(i) + '\n')
  elif (i % 8 == 5):
    f.write('q ' + str(i) + '\n')
  elif (i % 8 == 6):
    g.write('q ' + str(i) + '\n')
  elif (i % 8 == 7):
    h.write('q ' + str(i) + '\n')

