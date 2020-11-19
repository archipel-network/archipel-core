# Performance measurements in Linux with perf_events

Performance measurement techniques using time-based metrics are quite
inaccurate when it comes to very fast operations. A more accurate metric are
hardware counters. These counters can be used via the `perf_events` interface of
the Linux Kernel.

This document describes the usage of this interface. It takes a simple parsing
tasks in µD3TN as an example. For example, this method was used for the RFC 5050
vs. BPbis parsing performance comparison

## Header definitions

```c
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>

static int perf_fd;
long long count;
long long total_count;

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
               int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, hw_event, pid, cpu,
            group_fd, flags);
}


#define PERF(expr) do { \
    ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0); \
    ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0); \
    expr; \
    ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0); \
    count = 0; \
    read(perf_fd, &count, sizeof(long long)); \
    total_count += count; \
} while (0)


```

You have to initialize `perf_events` in you task initializer function:

```c
void my_task(void * const param)
{
    // ...

    // -------------------
    // Parsing Performance
    // -------------------
    struct perf_event_attr pe;
    long long count;

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    perf_fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (perf_fd == -1) {
        fprintf(stderr, "Error opening leader %llx\n", pe.config);
        exit(EXIT_FAILURE);
    }
    // -----------------------
    // End Parsing Performance
    // -----------------------

    // Do your heavy lifiting ...
}
```

Now you can count hardware cycles for specific parts of your code with the
`PERF` helper:

```c

static void forward_to_specific_parser(uint8_t byte)
{
    if (input_parser.stage != INPUT_EXPECT_DATA) {
        /* Current parser is input_parser */
        input_parser_read_byte(&input_parser, byte);
    } else {
        switch (input_parser.type) {
        case INPUT_TYPE_ROUTER_COMMAND_DATA:
            cur_parser = router_parser.basedata;
            router_parser_read_byte(&router_parser, byte);
            break;
        case INPUT_TYPE_BUNDLE_VERSION:
            PERF(select_bundle_parser_version(byte));
            break;
        case INPUT_TYPE_BUNDLE_V6:
            cur_parser = bundle6_parser.basedata;
            PERF(bundle6_parser_read_byte(&bundle6_parser, byte));
            break;
        case INPUT_TYPE_BUNDLE_V7:
            cur_parser = bundle7_parser.basedata;
            PERF(bundle7_parser_read_byte(&bundle7_parser, byte));
            break;
        default:
            reset_parser();
            return;
        }
    }
}


static void reset_parser(void)
{
    // ...

    // Read and reset performance counter
    printf("COUNT_HW_CPU_CYCLES: %lld", count);
    ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
    total_count = 0;
}

```
