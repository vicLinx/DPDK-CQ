#include "main.h"

static uint64_t 
u64_diff(uint64_t x, uint64_t y) {
    return (x > y ? (x-y) : (y-x));
}

static void 
init_tx(int port_id) {
    uint32_t lcore = rte_lcore_id();
    uint64_t cpu_freq = rte_get_tsc_hz();
    uint64_t current_time = rte_get_tsc_cycles();
    uint64_t tx_rate_scale = 0;
    uint64_t tx_rate_bps, target_tx_rate_bps;

    tx_rate_scale = (((app.tx_rate_mbps >> 3) * (uint64_t)1e6) << RATE_SCALE) / cpu_freq;
    app.cpu_freq[lcore] = cpu_freq;
    app.prev_time[port_id] = current_time;
    app.token[port_id] = app.bucket_size;
    app.core_tx[port_id] = lcore;
    app.tx_rate_scale[port_id] = tx_rate_scale;
    tx_rate_bps = (app.tx_rate_scale[port_id] * 8 * rte_get_tsc_hz())>>RATE_SCALE;
    target_tx_rate_bps = app.tx_rate_mbps * (uint64_t)1e6;
    RTE_LOG(
        INFO, SWITCH,
        "%s: actual tx_rate of port %u: %lubps=%luMbps\n",
        __func__,
        app.ports[port_id],
        tx_rate_bps,
        tx_rate_bps/(uint64_t)1e6
    );
    if (u64_diff(tx_rate_bps, target_tx_rate_bps) > target_tx_rate_bps/20) {
        RTE_LOG(
            ERR, SWITCH,
            "%s: Calculated tx_rate(%lubps) is significantly different from origin tx rate(%lubps). Integer overflow?\n",
            __func__,
            tx_rate_bps, target_tx_rate_bps
        );
    }
}

void
app_main_loop_tx(void) {
    uint32_t i;

    RTE_LOG(INFO, SWITCH, "Core %u is doing TX\n", rte_lcore_id());

    for (i = 0; i < app.n_ports; i++) {
        init_tx(i);
    }
    for (i = 0; !force_quit; i = ((i + 1) & (app.n_ports - 1))) {
        app_main_tx_port(i);
    }
}

void
app_main_loop_tx_each_port(uint32_t port_id) {

    RTE_LOG(INFO, SWITCH, "Core %u is doing TX for port %u\n", rte_lcore_id(), app.ports[port_id]);
    init_tx(port_id);
    while(!force_quit) {
        app_main_tx_port(port_id);
    }
}

void 
app_main_tx_port(uint32_t port_id) {
    uint64_t tx_begin, tx_end, cut_begin = 0, cut_end = 0;
    struct rte_mbuf *pkt;
    uint64_t current_time, prev_time = app.prev_time[port_id];
    uint64_t tx_rate_scale = app.tx_rate_scale[port_id];
    uint64_t token = app.token[port_id];
    int deq_ret;

    current_time = rte_get_tsc_cycles();
    tx_begin = current_time;
    if (app.tx_rate_mbps > 0) {
        // tbf: generate tokens
        token += ((tx_rate_scale * (current_time - prev_time)) >> RATE_SCALE);
        token = MIN(token, (app.bucket_size<<1));
        app.prev_time[port_id] = current_time;
        app.token[port_id] = token;
        if (token < app.bucket_size) {
            return ;
        }
    }

    if (rte_ring_count(app.rings_tx[port_id]) < app.thd 
         && rte_ring_count(app.rings_long[port_id]) > 0) {
        struct rte_mbuf *mbufs[1000];
	    uint32_t num;
	    if (app.thd - rte_ring_count(app.rings_tx[port_id]) >= rte_ring_count(app.rings_long[port_id]))
	    	num = rte_ring_count(app.rings_long[port_id]);
	    else
		    num = app.thd - rte_ring_count(app.rings_tx[port_id]);
        
        cut_begin = rte_get_tsc_cycles();
	    deq_ret = rte_ring_mc_dequeue_bulk(
                app.rings_long[port_id],
                (void **) mbufs,
                num,
                NULL
        );
        if (deq_ret > 0) {
            rte_ring_mp_enqueue_bulk(
                app.rings_tx[port_id],
                (void **) mbufs,
                deq_ret, 
                NULL
            );
        }
        cut_end = rte_get_tsc_cycles();
    }

    deq_ret = rte_ring_mc_dequeue(
            app.rings_tx[port_id],
            (void **) &pkt
    );
    if (deq_ret == -ENOENT) /* no packets in tx ring */
        return ;
    
    app.qlen_pkts_out[port_id]+= 1;
    app.qlen_bytes_out[port_id] += pkt->pkt_len;
    if (app.tx_rate_mbps > 0) 
        app.token[port_id] -= pkt->pkt_len;
    RTE_LOG(
        DEBUG, SWITCH,
        "%s: port %u receive packet\n",
        __func__, app.ports[port_id]
    );

    uint16_t ret;
    do {
        ret = rte_eth_tx_burst(app.ports[port_id], 0, &pkt, 1);
    } while (ret == 0);

    tx_end = rte_get_tsc_cycles();
    if (cut_begin!=0 && app.ports[port_id]==2) {
        printf("txx: %llu\n", tx_end-tx_begin);
        printf("cut: %llu\n", cut_end-cut_begin);
    }
    return ; 
}
