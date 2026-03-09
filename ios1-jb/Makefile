CC = clang
CFLAGS = -I../deps/include/
LDFLAGS = -L../deps/lib/ -lpng -lusb-1.0 -lcrypto -lz -framework CoreFoundation -framework IOKit -framework Security
TARGET = ios1-jb
SRC = file_payload.c ibootim.c idevice.c main.c s5l8900_exploit.c

all: $(TARGET)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
