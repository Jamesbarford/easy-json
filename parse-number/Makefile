TARGET := num 
CC     := gcc
CFLAGS := -Wall -O2 -DERROR_REPORTING=1

all: $(TARGET)


$(TARGET): parser.c
	$(CC) $(CFLAGS) -o $@ $^ 

clean:
	rm -rf $(TARGET)
