SOURCE	:= Main.c Trace.c Cache.c
CC	:= gcc
TARGET	:= Main
LINK	:= -lm

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) -o $(TARGET) $(SOURCE) $(LINK)

test_traces.txt: $(TARGET)
	./$(TARGET) mem_trace/531.deepsjeng_r_llc.mem_trace >> test_traces.txt
	./$(TARGET) mem_trace/541.leela_r_llc.mem_trace >> test_traces.txt
	./$(TARGET) mem_trace/548.exchange2_r_llc.mem_trace >> test_traces.txt


clean:
	rm -f $(TARGET)