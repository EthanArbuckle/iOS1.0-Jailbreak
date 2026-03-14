CC = clang
CFLAGS = -I../deps/include/
LDFLAGS = -L../deps/lib/ -lusb-1.0 -lcrypto -framework CoreFoundation -framework IOKit -framework Security
TARGET = tunnel
SRC = tunnel.c


all: $(TARGET)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
