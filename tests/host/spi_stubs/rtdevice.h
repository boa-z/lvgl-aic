#pragma once
#include <stdint.h>
#define RT_EOK 0
typedef uint32_t rt_uint32_t;
struct rt_spi_ops { void (*wait_completion)(void); void (*gstatus)(void); };
struct rt_spi_bus { const struct rt_spi_ops *ops; };
struct rt_spi_device { struct rt_spi_bus *bus; };
int rt_spi_wait_completion(struct rt_spi_device *device);
rt_uint32_t rt_spi_get_transfer_status(struct rt_spi_device *device);
