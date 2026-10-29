PKG_CONFIG ?= pkg-config

ifeq ($(shell $(PKG_CONFIG) --exists libusb-1.0 2>/dev/null && echo yes),yes)
USB_CFLAGS := $(shell $(PKG_CONFIG) --cflags libusb-1.0)
USB_LIBS := $(shell $(PKG_CONFIG) --libs libusb-1.0)
else ifeq ($(shell uname -s),Darwin)
USB_CFLAGS := -I../deps/include
USB_LIBS := -L../deps/lib -lusb-1.0 -framework IOKit -framework CoreFoundation
else
USB_LIBS := -lusb-1.0
endif

TARGET = tunnel
SRC = tunnel.c ../src/common/mux.c
HEADERS = ../src/common/mux.h

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(SRC) $(HEADERS)
	$(CC) $(CPPFLAGS) $(USB_CFLAGS) $(CFLAGS) -pthread $(LDFLAGS) -o $@ $(SRC) $(USB_LIBS) $(LDLIBS)

clean:
	rm -f $(TARGET)
