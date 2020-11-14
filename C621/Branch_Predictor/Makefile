SOURCE	:= Main.c Trace.c Branch_Predictor.c
CC	:= gcc
TARGET	:= Main
LINK	:= -lm

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) -o $(TARGET) $(SOURCE) $(LINK)

test_traces.txt: $(TARGET)
	./$(TARGET) CPU_traces/531.deepsjeng_r_branches.cpu_trace >> test_traces.txt
	./$(TARGET) CPU_traces/541.leela_r_branches.cpu_trace >> test_traces.txt
	./$(TARGET) CPU_traces/548.exchange2_r_branches.cpu_trace >> test_traces.txt

clean:
	rm -f $(TARGET)
	rm -f test_traces.txt
