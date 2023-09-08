CC=g++
CFLAGS=-Wall -pthread $(shell pkg-config --cflags --libs gstreamer-1.0)

MAIN_SRC=play.cpp
OUT=play

all: $(OUT)

$(OUT): $(MAIN_SRC)
	$(CC) $(MAIN_SRC) -o $(OUT) $(CFLAGS) -fpermissive

clean:
	rm -f $(OUT)