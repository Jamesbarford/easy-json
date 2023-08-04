TARGET := parser 
CC     := gcc
CFLAGS := -Wall -O2 
TESTS  := tests

all: $(TARGET) $(TESTS)

format:
	clang-format *.c -i
	clang-format *.h -i

$(TARGET): main.c json.c json-selector.c
	$(CC) $(CFLAGS) -o $@ $^ 

$(TESTS): test.c json.c
	$(CC) $(CFLAGS) -o $@ $^ 


clean:
	rm -rf $(TARGET)
