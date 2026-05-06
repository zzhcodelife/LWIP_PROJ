/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Sergio R. Caprile - "commonalization" from prior samples and/or documentation extension
 *******************************************************************************/
#include <stdint.h>

/* 必须在创建 mqtt 线程前调用一次，初始化 socket 互斥锁 */
void   transport_init(void);
/* 高层操作（一次完整的 publish/ping/subscribe）需要把 send+select+recv
 * 整段包在 transport_lock()/transport_unlock() 之间，避免和另一个线程
 * 的 close/select 交错，触发 sockets.c 的 select_waiting==0 断言。
 * 互斥锁是递归的，可以和 transport_open/close/send/get 内部的锁安全嵌套。 */
void   transport_lock(void);
void   transport_unlock(void);

int32_t transport_sendPacketBuffer(uint8_t *buf, int32_t buflen);
int32_t transport_getdata(uint8_t *buf, int32_t count);
int32_t transport_open(int8_t *servip, int32_t port);
int32_t transport_close(void);
