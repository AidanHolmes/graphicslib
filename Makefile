CXX = g++
CXXFLAGS=-Wall $(shell freetype-config --cflags)
LIBS = -ljpeg
LDFLAGS = 

SRCS_LIB = displayimage.cpp
H_LIB = $(SRCS_LIB:.cpp=.hpp)
OBJS_LIB = $(SRCS_LIB:.cpp=.o)

SRCS_XBMUTIL = xbm2bin.cpp
OBJS_XBMUTIL = $(SRCS_XBMUTIL:.cpp=.o)

SRCS_PSFUTIL = psf2bin.cpp
OBJS_PSFUTIL = $(SRCS_PSFUTIL:.cpp=.o)

XBMUTIL = xbm2bin
PSFUTIL = pcf2bin
ARCHIVE = libdisp.a

.PHONY: all
all: $(EXECUTABLE) $(ARCHIVE) $(XBMUTIL) $(PSFUTIL) $(NOKTST)

$(XBMUTIL): $(OBJS_XBMUTIL)
	$(CXX) $(OBJS_XBMUTIL) -o $@

$(PSFUTIL): $(OBJS_PSFUTIL)
	$(CXX) $(OBJS_PSFUTIL) $(shell freetype-config --libs) -o $@

$(ARCHIVE): $(OBJS_LIB)
	ar r $@ $?

$(OBJS_LIB): $(H_LIB)

.PHONY: clean
clean:
	rm -f *.o $(ARCHIVE) $(XBMUTIL) $(PSFUTIL)
