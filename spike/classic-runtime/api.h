// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
#ifndef CLASSIC_RUNTIME_SPIKE_API_H
#define CLASSIC_RUNTIME_SPIKE_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* Deliberately tiny research ABI, not a released SDK contract. */
#define CLASSIC_NET_SPIKE_MAJOR 1UL
#define CLASSIC_NET_SPIKE_OK 0
#define CLASSIC_NET_SPIKE_INCOMPATIBLE 1

typedef struct ClassicNetSpikeAPI {
    unsigned long api_size;
    unsigned long major;
    unsigned long minor;
    long (*add_for_probe)(long a, long b);
} ClassicNetSpikeAPI;

/* No caller allocation or transfer of ownership in this experiment. */
int classic_net_spike_get_api(unsigned long requested_major,
                              const ClassicNetSpikeAPI **out_api);

#ifdef __cplusplus
}
#endif
#endif
