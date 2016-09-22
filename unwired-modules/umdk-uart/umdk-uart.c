/*
 * Copyright (C) 2016 Unwired Devices [info@unwds.com]
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup
 * @ingroup
 * @brief
 * @{
 * @file		umdk-uart.c
 * @brief       umdk-uart module implementation
 * @author      EP
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "periph/gpio.h"
#include "periph/uart.h"

#include "board.h"

#include "unwds-common.h"
#include "include/umdk-uart.h"

#include "thread.h"
#include "xtimer.h"

static uwnds_cb_t *callback;
static uint8_t rxbuf[UMDK_UART_RXBUF_SIZE] = {};

static volatile uint8_t num_bytes_received;

static kernel_pid_t writer_pid;
static char writer_stack[THREAD_STACKSIZE_MAIN];

static msg_t send_msg;
static msg_t send_msg_ovf;

static xtimer_t send_timer;

void *writer(void *arg) {
    msg_t msg;
    msg_t msg_queue[8];
    msg_init_queue(msg_queue, 8);

    while (1) {
        msg_receive(&msg);

        module_data_t data;
        data.data[0] = UNWDS_UART_MODULE_ID;
        data.length = 2;

        /* Received payload, send it */
        if (msg.content.value == send_msg.content.value) {
			data.length += num_bytes_received;
			data.data[1] = UMDK_UART_REPLY_RECEIVED;

			memcpy(data.data + 2, rxbuf, num_bytes_received);

			printf("[umdk-uart] rx: %s | len: %d\n", rxbuf, num_bytes_received);

			num_bytes_received = 0;
        } else if (msg.content.value == send_msg_ovf.content.value) { /* RX buffer overflowed, send error message */
        	data.length = 2;
        	data.data[1] = UMDK_UART_REPLY_ERR_OVF;

        	puts("[umdk-uart] Input buffer overflowed");
        	num_bytes_received = 0;
        }

        callback(&data);
    }

	return NULL;
}

void rx_cb(void *arg, uint8_t data)
{
	/* Buffer overflow */
	if (num_bytes_received == UMDK_UART_RXBUF_SIZE) {
		num_bytes_received = 0;

		msg_send(&send_msg_ovf, writer_pid);

		return;
	}

	rxbuf[num_bytes_received++] = data;

	/* Schedule sending after timeout */
	xtimer_set_msg(&send_timer, 1e3 * UMDK_UART_SYMBOL_TIMEOUT_MS, &send_msg, writer_pid);
}

void umdk_uart_init(uint32_t *non_gpio_pin_map, uwnds_cb_t *event_callback)
{
    (void) non_gpio_pin_map;

    callback = event_callback;

    /* Initialize the UART */
    if (uart_init(UMDK_UART_DEV, UMDK_UART_BAUDRATE, rx_cb, NULL)) {
        return;
    }

    send_msg.content.value = 0;
    send_msg_ovf.content.value = 1;

	/* Create handler thread */
	writer_pid = thread_create(writer_stack, sizeof(writer_stack), THREAD_PRIORITY_MAIN - 1, 0, writer, NULL, "umdk-uart writer thread");
}

static void do_reply(module_data_t *reply, umdk_uart_reply_t r)
{
    reply->length = 2;
    reply->data[0] = UNWDS_UART_MODULE_ID;
    reply->data[0] = r;
}

bool umdk_uart_cmd(module_data_t *data, module_data_t *reply)
{
    if (data->length < 1) {
        do_reply(reply, UMDK_UART_REPLY_ERR_FMT);
        return false;
    }

    umdk_uart_prefix_t prefix = data->data[0];
    switch (prefix) {
        case UMDK_UART_SEND_ALL:
            /* Cannot send nothing */
            if (data->length == 1) {
                do_reply(reply, UMDK_UART_REPLY_ERR_FMT);
                return false;
            }

            uart_write(UMDK_UART_DEV, (uint8_t *) data->data + 1, data->length - 1);
            do_reply(reply, UMDK_UART_REPLY_SENT);

            break;

        default:
        	do_reply(reply, UMDK_UART_REPLY_ERR_FMT);
        	return false;
    }

    return true;
}

#ifdef __cplusplus
}
#endif
