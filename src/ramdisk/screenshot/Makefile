CC = clang
SYSROOT = /Users/ethanarbuckle/Downloads/iPhoneOS1.1.5.sdk
CRT = /Users/ethanarbuckle/Downloads/iphone-dev/trunk/csu/crt1.o
LD_DIR = /Users/ethanarbuckle/Downloads/ios1-ld
TARGET = screenshot
SRC = screenshot.m /Users/ethanarbuckle/Downloads/iPhoneOS1.1.5.sdk/usr/lib/gcc/arm-apple-darwin9/4.0.1/libgcc.a

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -arch armv6 -mthumb -isysroot $(SYSROOT) -B$(LD_DIR) -miphoneos-version-min=1.0 -fno-objc-arc -fobjc-runtime=macosx-fragile -nostdlib -L$(SYSROOT)/usr/lib -framework CoreFoundation -framework Foundation -framework UIKit -framework CoreGraphics -lobjc -lSystem $(CRT) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
