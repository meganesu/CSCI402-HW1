makeTree = open('listTree', 'w')

a = open('delete1', 'w')
b = open('delete2', 'w')
c = open('delete3', 'w')
d = open('delete4', 'w')
e = open('delete5', 'w')
f = open('delete6', 'w')
g = open('delete7', 'w')
h = open('delete8', 'w')

for i in range(0,16384):
  makeTree.write('a ' + str(i).zfill(5) + ' 5\n') # Only care about key, so assign arbitrary value 5

for i in range(16384,0,-1):
  if (i % 8 == 0):
    a.write('d ' + str(i).zfill(5) + '\n')
  elif (i % 8 == 1):
    b.write('d ' + str(i).zfill(5) + '\n')
  elif (i % 8 == 2):
    c.write('d ' + str(i).zfill(5) + '\n')
  elif (i % 8 == 3):
    d.write('d ' + str(i).zfill(5) + '\n')
  elif (i % 8 == 4):
    e.write('d ' + str(i).zfill(5) + '\n')
  elif (i % 8 == 5):
    f.write('d ' + str(i).zfill(5) + '\n')
  elif (i % 8 == 6):
    g.write('d ' + str(i).zfill(5) + '\n')
  elif (i % 8 == 7):
    h.write('d ' + str(i).zfill(5) + '\n')

