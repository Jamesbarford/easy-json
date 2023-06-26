TARGET := parser 
CC     := gcc
CFLAGS := -Wall -O2 -DERROR_REPORTING

all: $(TARGET)


$(TARGET): main.c json.c json-selector.c
	$(CC) $(CFLAGS) -o $@ $^ 

clean:
	rm -rf $(TARGET)
