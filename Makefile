EXCUTALBE_FILE := logging.exe

SOURCES := $(wildcard *.cc)
HEADS := $(wildcard *.h)
objects := $(patsubst %.cc, %.o, $(SOURCES))

include /localdisk/changqwa/lib/makefile.rules

GTESTDIR := /localdisk/changqwa/lib/google_utest/
PRODIR := /localdisk/changqwa/code2012/william_log
VPATH := $(PRODIR) $(GTESTDIR)
CCFLAGS += -g -D __DEBUG__ -D __GTEST_UNITTEST__

all: $(EXCUTALBE_FILE)
$(EXCUTALBE_FILE): LOCFLAGS = -lpthread
$(EXCUTALBE_FILE): $(objects) -lgtest
	$(CC) $(CCFLAGS) $(LOCFLAGS) $^ -o $@
	
$(objects) :LOCFLAGS = -I$(PRODIR) -I$(GTESTDIR) 
$(objects) : $(SOURCES) $(HEADS)