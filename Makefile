WAF = ./waf

all:
	$(WAF)

configure:
	$(WAF) configure --enable-examples --enable-tests

build:
	$(WAF) build

install:
	$(WAF) install

clean:
	$(WAF) clean

distclean:
	$(WAF) distclean

data: singleFlow multiFlow

singleFlow:
	$(WAF) --run scratch/singleFlow
	
multiFlow:
	$(WAF) --run scratch/multiFlow

graph: grapha graphb

grapha:
	cd PartA
	python plotter.py
	cd ..
	
graphb:
	cd PartB
	python plotter.py
	cd ..
