#!/usr/bin/python3
#!/usr/bin/python2
import sys

list_avg = []
for i in range(1, 5):
    input_file = open(sys.argv[i],'r')
    line = input_file.readline()
    num = 0.0
    avg = 0.0
    while line:
        avg += float(line.split()[1])
        num += 1.0
        line = input_file.readline()
    
    avg /= num
    list_avg.append(avg)

print(list_avg)
sum_avg = list_avg[0]+list_avg[1]+list_avg[2]+list_avg[3]
cut_perc = list_avg[3]*100/sum_avg
print(cut_perc)

