#!/usr/bin/env python

import sys
import os
import string
import math
import numpy
import matplotlib.pyplot as plt
import glob

os.chdir(os.path.dirname(sys.argv[0]))

version = sys.argv[1] 
folder_name = "Results"+version 
print(folder_name)


file_path = os.path.join(folder_name, "cnvg0.dat")
# errors0 = numpy.loadtxt(f"Results2D{version}/cnvg0.dat", delimiter=" ", skiprows=0);
# errors1 = numpy.loadtxt(f"Results2D{version}/cnvg1.dat", delimiter=" ", skiprows=0);
errors2 = numpy.loadtxt(f"Results2D{version}/cnvg2.dat", delimiter=" ", skiprows=0);

n = errors2.shape[0]
nv = errors2.shape[1]
resolutions = numpy.arange(0,n)
resolutions = 2**resolutions


# L2 errors
for i in range(0,nv,2):
	# plt.loglog(resolutions,errors0[0::,i],'r-x')
	# plt.loglog(2*resolutions,errors1[0::,i],'b-x')
	plt.loglog(4*resolutions,errors2[0::,i],'g-x')
# L2 errors
for i in range(1,nv,2):
	# plt.loglog(resolutions,errors0[0::,i],'r--x')
	# plt.loglog(2*resolutions,errors1[0::,i],'b--x')
	plt.loglog(4*resolutions,errors2[0::,i],'g--x')
plt.xlabel('Resolution')
plt.ylabel('Error')
plt.savefig(f"Results2D{version}/Error.pdf")
plt.close()	

# print('L2 convergence')
# for i in range(0,nv,2):
# 	print(math.log2(errors0[n-2,i]/errors0[n-1,i]))
# 	print(math.log2(errors1[n-2,i]/errors1[n-1,i]))
# 	print(math.log2(errors2[n-2,i]/errors2[n-1,i]))

# print('Linf convergence')
# for i in range(1,nv,2):
# 	print(math.log2(errors0[n-2,i]/errors0[n-1,i]))
# 	print(math.log2(errors1[n-2,i]/errors1[n-1,i]))
# 	print(math.log2(errors2[n-2,i]/errors2[n-1,i]))