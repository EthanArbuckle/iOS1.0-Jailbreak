CC = clang
SYSROOT = /Users/ethanarbuckle/Downloads/iPhoneOS1.1.5.sdk
CRT = /Users/ethanarbuckle/Downloads/iphone-dev/trunk/csu/crt1.o
TARGET = show_image
SRC = show_image.m

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -arch armv6 -mthumb -isysroot $(SYSROOT) -B../ -miphoneos-version-min=1.0 -fno-objc-arc -fobjc-runtime=macosx-fragile -nostdlib -L$(SYSROOT)/usr/lib -framework CoreFoundation -framework Foundation -framework UIKit -lobjc -lSystem $(CRT) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
