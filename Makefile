TARGETS		= rescue

CXX		= g++

PCLIBS		=

CXXFLAGS	= -ggdb3 -std=c++11
LDFLAGS		= -ggdb3
LDLIBS		= -lusb -lm

CXXFLAGS	+= $(shell pkg-config --cflags $(PCLIBS))
LDLIBS		+= $(shell pkg-config --libs $(PCLIBS))

.PHONY:		all clean

all:		$(TARGETS)

clean:
		rm -rf *.o $(TARGETS)

%.o:		%.cc
		$(CXX) -c $(CXXFLAGS) $^

rescue:		rescue.o
		$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS)

