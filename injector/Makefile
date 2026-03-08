CC = /Users/ethanarbuckle/Desktop/Developer/usr/bin/clang
SYSROOT = /Users/ethanarbuckle/Downloads/iPhoneOS1.1.5.sdk
CRT = /Users/ethanarbuckle/Downloads/iphone-dev/trunk/csu/crt1.o
TARGET = injector
SRC = injector.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -arch armv6 -mthumb -isysroot $(SYSROOT) -B../ -miphoneos-version-min=1.0 -nostdlib -L$(SYSROOT)/usr/lib -lSystem $(CRT) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
