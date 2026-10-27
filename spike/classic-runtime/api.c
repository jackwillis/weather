#include "api.h"

static long add_for_probe(long a, long b)
{
    return a + b;
}

static const ClassicNetSpikeAPI api = {
    sizeof(ClassicNetSpikeAPI),
    CLASSIC_NET_SPIKE_MAJOR,
    0,
    add_for_probe
};

int classic_net_spike_get_api(unsigned long requested_major,
                              const ClassicNetSpikeAPI **out_api)
{
    if (!out_api)
        return CLASSIC_NET_SPIKE_INCOMPATIBLE;
    *out_api = 0;
    if (requested_major != api.major)
        return CLASSIC_NET_SPIKE_INCOMPATIBLE;
    *out_api = &api;
    return CLASSIC_NET_SPIKE_OK;
}
