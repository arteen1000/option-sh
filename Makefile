
osh: osh.c my-lib/stoi-ge0.c

clean:
	mv osh.c Makefile ..
	-rm *
	mv ../Makefile ../osh.c .
