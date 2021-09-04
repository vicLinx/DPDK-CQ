#include "main.h"

uint32_t
qlen_threshold_equal_division(uint32_t port_id) {
    port_id = port_id << 1; /* prevent warning */
    uint32_t result = app.buff_size_bytes / app.n_ports;
    return result;
}

uint32_t
qlen_threshold_dt(uint32_t port_id) {
    port_id = port_id << 1; /* prevent warning */
    return ((app.buff_size_bytes - get_buff_occu_bytes()) << app.dt_shift_alpha);
}

uint32_t get_qlen_bytes(uint32_t port_id) {
    return app.qlen_bytes_in[port_id] - app.qlen_bytes_out[port_id];
}

uint32_t get_buff_occu_bytes(void) {
    uint32_t i, result = 0;
    for (i = 0; i < app.n_ports; i++) {
        result += (app.qlen_bytes_in[i] - app.qlen_bytes_out[i]);
    }
    return result;
}

static int mark_packet_with_ecn(struct rte_mbuf *pkt) {
    struct ipv4_hdr *iphdr;
    uint16_t cksum;
    if (RTE_ETH_IS_IPV4_HDR(pkt->packet_type)) {
        iphdr = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, sizeof(struct ether_hdr));
        if ((iphdr->type_of_service & 0x03) != 0) {
            iphdr->type_of_service |= 0x3;
            iphdr->hdr_checksum = 0;
            cksum = rte_ipv4_cksum(iphdr);
            iphdr->hdr_checksum = cksum;
        } else {
            return -2;
        }
        return 0;
    } else {
        return -1;
    }
}

int packet_enqueue(uint32_t dst_port, struct rte_mbuf *pkt, uint8_t tos) {
    int ret = 0;
    int mark_pkt = 0, mark_ret;
    uint32_t qlen_bytes = get_qlen_bytes(dst_port);
    uint32_t threshold = 0;
    uint32_t qlen_enque = qlen_bytes + pkt->pkt_len;
    uint32_t buff_occu_bytes = 0;
    mark_pkt = (app.ecn_enable && qlen_bytes >= (app.ecn_thresh_kb<<10));
    /*Check whether buffer overflows after enqueue*/
    if (app.shared_memory) {
        buff_occu_bytes = get_buff_occu_bytes();
        threshold = app.get_threshold(dst_port);
        if (qlen_enque > threshold) {
            ret = -1;
        } else if (buff_occu_bytes + pkt->pkt_len > app.buff_size_bytes) {
            ret = -2;
        }
    } else if (qlen_enque > app.buff_size_per_port_bytes/2) {
        ret = -2;
    }

    if (ret == 0 && mark_pkt) {
        /* do ecn marking */
        mark_ret = mark_packet_with_ecn(pkt);
        if (mark_ret < 0) {
            ret = -3;
        }
        /* end */
    }

    if (ret == 0) 
    {
        int enq_ret;
        if (tos > 0x4) {
            enq_ret = rte_ring_sp_enqueue(
                app.rings_long[dst_port],
                pkt
            );
        } else {
            enq_ret = rte_ring_sp_enqueue(
                app.rings_tx[dst_port],
                pkt
            );
        }

        if (enq_ret != 0) {
            RTE_LOG(
                ERR, SWITCH,
                "%s: packet cannot enqueue in port %u",
                __func__, app.ports[dst_port]
            );
        }

        app.qlen_pkts_in[dst_port] += 1;
        app.qlen_bytes_in[dst_port] += pkt->pkt_len;

        if (app.log_qlen && pkt->pkt_len >= MEAN_PKT_SIZE &&
            (app.log_qlen_port >= app.n_ports ||
             app.log_qlen_port == app.ports[dst_port])
        ) {
            if (app.qlen_start_cycle == 0) {
                app.qlen_start_cycle = rte_get_tsc_cycles();
            }
            fprintf(
                app.qlen_file,
                "%-12.6f %-8u %-8u %-8u\n",
                (float) (rte_get_tsc_cycles() - app.qlen_start_cycle) / app.cpu_freq[rte_lcore_id()],
                app.ports[dst_port],
                qlen_bytes,
                buff_occu_bytes
            );
        }
    } else {
        rte_pktmbuf_free(pkt);
        if (tos <= 0x4)
            ++app.num_drop[dst_port];
    }

    switch (ret) {
    case 0:
        RTE_LOG(
            DEBUG, SWITCH,
            "%s: packet enqueue to port %u\n",
            __func__, app.ports[dst_port]
        );
        break;
    case -1:
        RTE_LOG(
            DEBUG, SWITCH,
            "%s: Packet dropped due to queue length > threshold\n",
            __func__
        );
        break;
    case -2:
        RTE_LOG(
            DEBUG, SWITCH,
            "%s: Packet dropped due to buffer overflow\n",
            __func__
        );
    case -3:
        RTE_LOG(
            DEBUG, SWITCH,
            "%s: Cannot mark packet with ECN, drop packet\n",
            __func__
        );
    }
    return ret;
}
