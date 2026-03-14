CC = /Users/ethanarbuckle/Desktop/Developer/usr/bin/clang
SYSROOT = /Users/ethanarbuckle/Downloads/iPhoneOS1.1.5.sdk
CRT = /Users/ethanarbuckle/Downloads/iphone-dev/trunk/csu/crt1.o
LD_DIR = /Users/ethanarbuckle/Downloads/ios1-ld
TARGET = inject
SRC = inject.c button.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -arch armv6 -mthumb -isysroot $(SYSROOT) -B$(LD_DIR) -miphoneos-version-min=1.0 -nostdlib -L$(SYSROOT)/usr/lib -framework CoreFoundation -lSystem $(CRT) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
