#pragma once
#include <stdint.h>
#include <stddef.h>
#define RT_EOK 0
typedef uint32_t rt_uint32_t;
struct rt_spi_ops { void (*wait_completion)(void); void (*gstatus)(void); void (*configure)(void); void (*xfer)(void); void (*nonblock)(void); };
struct rt_spi_bus { const struct rt_spi_ops *ops; };
struct rt_spi_device { struct rt_spi_bus *bus; };
int rt_spi_wait_completion(struct rt_spi_device *device);
rt_uint32_t rt_spi_get_transfer_status(struct rt_spi_device *device);

struct rt_qspi_device { struct rt_spi_device parent; };
struct rt_qspi_message {
    struct { const void *send_buf; void *recv_buf; size_t length; void *next; unsigned cs_take,cs_release; } parent;
    struct { uint8_t content,qspi_lines; } instruction;
    struct { uint32_t content; uint8_t size,qspi_lines; } address,alternate_bytes;
    uint32_t dummy_cycles;
    uint8_t qspi_data_lines;
};
int rt_spi_nonblock_set(struct rt_spi_device *device,unsigned mode);
size_t rt_qspi_transfer_message(struct rt_qspi_device *device,struct rt_qspi_message *message);
