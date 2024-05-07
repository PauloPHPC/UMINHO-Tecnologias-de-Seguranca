CC=gcc
CFLAGS=$(shell pkg-config --cflags libbson-1.0)
LIBS=$(shell pkg-config --libs libbson-1.0) -lssl -lcrypto

TARGET=Cliente
SRC=Cliente.c
PYTHON_SERVER_SCRIPT=server.py

.PHONY: all clean runserver run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

runserver:
	@echo "Checking for running server..."
	@if ps aux | grep "$(PYTHON_SERVER_SCRIPT)" | grep -v grep > /dev/null; then \
		echo "Server is already running."; \
	else \
		echo "Starting server..."; \
		python3 $(PYTHON_SERVER_SCRIPT) & \
	fi

run: $(TARGET)
	./$(TARGET)
