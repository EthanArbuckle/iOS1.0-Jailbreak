CC = clang
CFLAGS = -I../deps/include/ -I../src/
LDFLAGS = -L../deps/lib/ -lcrypto -lz -lusb-1.0
TARGET = irecovery
SRC = main.c ../src/s5l8900_exploit.c ../src/idevice.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC) -framework CoreFoundation -framework IOKit -framework Security

clean:
	rm -f $(TARGET)