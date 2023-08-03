TARGET=simpleclient

SHELL = /bin/bash

$(TARGET): $(TARGET).c
	gcc $(TARGET).c -Wall -I ../include -L ../lib -l simplog  -o $(TARGET)

clean:
	if [[  -f $(TARGET) ]]; then \
		rm $(TARGET); \
	fi
