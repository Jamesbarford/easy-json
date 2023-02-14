TARGET := test 
CC     := gcc
CFLAGS := -Wall -O2 -DERROR_REPORTING=1

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $^ 

clean:
	rm -rf $(TARGET)
