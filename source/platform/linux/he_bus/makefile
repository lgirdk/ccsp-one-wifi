include ./makefile.inc

#
# program
#

TARGET = $(INSTALLDIR)/lib/libbus.a
#TARGET = $(INSTALLDIR)

INCLUDEDIRS = \
	-I$(BUS_HOME)/inc \
	-I$(BUS_LIB)/inc \
	-I$(BUS_UTILS) \

CXXFLAGS = $(INCLUDEDIRS) -g -fPIC 
LIBS = -lm -lpthread -ldl
LDFLAGS = $(LIBS) -shared

BUS_SOURCES = $(wildcard $(BUS_HOME)/src/*.c) \
		$(wildcard $(BUS_LIB)/src/*.c) \

ifdef ONEWIFI_RBUS_SUPPORT
BUS_SOURCES += $(BUS_HOME)/platform/rdkb/bus.c
else
#BUS_SOURCES += $(BUS_HOME)/platform/linux/bus.c
endif

BUS_OBJECTS = $(BUS_SOURCES:.c=.o) 

all: $(TARGET)

$(TARGET): $(BUS_OBJECTS)
	$(CXX) -o $@ $(BUS_OBJECTS) $(LDFLAGS)

$(BUS_OBJECTS): %.o: %.c
	$(CC) $(CXXFLAGS) -o $@ -c $<

# Clean target: "make -f Makefile.Linux clean" to remove unwanted objects and executables.
#

clean:
	$(RM) $(BUS_OBJECTS) $(TARGET)

#
# Run target: "make -f Makefile.Linux run" to execute the application
#             You will need to add $(VARIABLE_NAME) for any command line parameters 
#             that you defined earlier in this file.
# 

run:
	./$(TRAGET)
