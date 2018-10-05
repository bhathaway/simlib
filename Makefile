CXX=gcc
CXXFLAGS=-std=c89 -Wall -Wextra -pedantic-errors -g

All: simulation_code simulation_code_transient

minheap.o: minheap.h minheap.c
	$(CXX) $(CXXFLAGS) -c minheap.c

simlib.o: simlib.c simlib.h simlibdefs.h
	$(CXX) $(CXXFLAGS) -c simlib.c

simulation_code: simlib.o minheap.o simulation_code.c
	$(CXX) $(CXXFLAGS) -o simulation_code simulation_code.c simlib.o minheap.o -lm

simulation_code_transient: simlib.o minheap.o simulation_code_transient.c
	$(CXX) $(CXXFLAGS) -o simulation_code_transient simulation_code_transient.c simlib.o minheap.o -lm

clean:
	rm *.o

