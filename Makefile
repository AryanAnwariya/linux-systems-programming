CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2
BUILD   := build

PROGRAMS := gcd_pipes resource_monitor logtop belt_shell \
            report_card sudoku_analyzer program_a program_b \
            test_setnice tier

all: $(addprefix $(BUILD)/,$(PROGRAMS))

$(BUILD):
	mkdir -p $@

$(BUILD)/gcd_pipes: processes-ipc/gcd-pipes/gcd_pipes.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/resource_monitor: processes-ipc/resource-monitor/resource_monitor.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/logtop: processes-ipc/logtop/logtop.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/belt_shell: shell/belt-shell/belt_shell.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/report_card: threads/report-card/report_card.c | $(BUILD)
	$(CC) $(CFLAGS) -pthread -o $@ $<

$(BUILD)/sudoku_analyzer: threads/sudoku-analyzer/sudoku_analyzer.c | $(BUILD)
	$(CC) $(CFLAGS) -pthread -o $@ $<

$(BUILD)/program_a: scheduling/rr-vs-fifo/program_a.c scheduling/rr-vs-fifo/*.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/program_b: scheduling/rr-vs-fifo/program_b.c scheduling/rr-vs-fifo/*.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/test_setnice: kernel/setnice-logged/test/test_setnice.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/tier: kernel/fork-guard/userspace/tier.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
