TARGET := parser 
CC     := gcc
CFLAGS := -Wall -O2 -DERROR_REPORTING

all: $(TARGET)

format:
	clang-format *.c -i
	clang-format *.h -i

$(TARGET): main.c json.c json-selector.c
	$(CC) $(CFLAGS) -o $@ $^ 

clean:
	rm -rf $(TARGET)
