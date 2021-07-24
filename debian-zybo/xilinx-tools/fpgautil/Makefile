FPGA_UTIL = fpgautil

# Add any other object files to this list below
FPGA_UTIL_OBJS = fpgautil.o

all: $(FPGA_UTIL) 

$(FPGA_UTIL): $(FPGA_UTIL_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(FPGA_UTIL_OBJS) $(LDLIBS)

install: $(FPGA_UTIL)
	cp $(FPGA_UTIL) /usr/bin/

clean:
	rm -f $(FPGA_UTIL) *.elf *.gdb *.o


