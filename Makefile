# Makefile wrapper for waf

all:
	./waf

# free free to change this part to suit your requirements
configure:
	./waf configure --enable-examples --enable-tests

build:
	./waf build

install:
	./waf install

clean:
	./waf clean

distclean:
	./waf distclean

data: parta partb

parta:
	./waf --run scratch/part13
	
partb:
	./waf --run scratch/part23

graph: grapha graphb

grapha:
	cd PartA
	python plotter.py
	cd ..
	
graphb:
	cd PartB
	python plotter.py
	cd ..
