#include "api.h"

int main(void)
{
    const ClassicNetSpikeAPI *api = 0;
    if (classic_net_spike_get_api(99, &api) != CLASSIC_NET_SPIKE_INCOMPATIBLE)
        return 1;
    if (api != 0)
        return 2;
    if (classic_net_spike_get_api(CLASSIC_NET_SPIKE_MAJOR, &api))
        return 3;
    if (!api || api->api_size < sizeof(*api) || !api->add_for_probe)
        return 4;
    if (api->add_for_probe(20, 22) != 42)
        return 5;
    return 0;
}
