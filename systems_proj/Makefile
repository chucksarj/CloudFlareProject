ADT_NAME  = CLI
SOURCES   = $(ADT_NAME).c 
OBJECTS   = $(ADT_NAME).o
EXEBIN    = $(ADT_NAME)
FLAGS     = -std=c99 -Wall -Wextra -Wpedantic -Wshadow -O2 -g
VGFLAGS = --leak-check=full

all: $(EXEBIN)

$(EXEBIN) : $(OBJECTS)
	gcc -g -o $(EXEBIN) $(OBJECTS)

$(OBJECTS) : $(SOURCES)
	gcc -c $(FLAGS) $(SOURCES)

leak :
	valgrind $(VGFLAGS) $(EXEBIN)

clean :
	rm -f $(EXEBIN) $(OBJECTS)
