import os
import subprocess
import re

files = [f for f in os.listdir('.') if re.match(r'^data', f)]

print files
for f1 in files:
	if not re.search(r'congestion', f1) and not re.search(r'png', f1):
		proc = subprocess.Popen(['gnuplot','-p'], 
				        shell=True,
				        stdin=subprocess.PIPE,
				        )
		proc.stdin.write('set terminal png size 1200,1000\n')
		proc.stdin.write('set output "'+f1+'.png"\n')
		if re.search(r'tp', f1):
			temp=" throughput in bits VS time "
		elif re.search(r'gp', f1):
			temp=" goodput in bits VS time "
		elif re.search(r'cwnd', f1):
			temp=" congestion window size VS time "
		
		proc.stdin.write("plot '"+f1+"' using 1:2 title '"+f1+temp+"' with linespoints\n")
		proc.stdin.write("exit")