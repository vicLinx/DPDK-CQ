#include "main.h"

/* 采样 */
void
app_main_loop_sampling(void)
{
    uint32_t i;
    for (i = 0; !force_quit; i = ((i + 1) & (app.n_ports - 1)))
    {
        sleep(0.5);
        if (rte_ring_count(app.rings_tx[i]) < 1) 
            continue;

        struct rte_mbuf *mbufs[1000];
        uint32_t cp_ret = rte_ring_copy_queue(
                            app.rings_tx[i], 
                            (void **)mbufs,
                            rte_ring_count(app.rings_tx[i])
        );

        uint8_t tos;
        struct ipv4_hdr *pkt_ip;
        double cur_qd = 0.0, num_sht = 0.0;
        uint32_t k, cur_len = 0;
        for (k = 0; k < cp_ret; ++k) {
            cur_len += mbufs[k]->pkt_len;
            pkt_ip = rte_pktmbuf_mtod_offset(
                mbufs[k], 
                struct ipv4_hdr *, 
                sizeof(struct ether_hdr)
            );
            tos = pkt_ip->type_of_service;
            if (tos > 0x4) 
                continue ;

            double len = cur_len * 8.0;
            cur_qd += len / (2047.0 * 1000000.0);
            num_sht += 1.0;
        }

        if (num_sht == 0.0)
            continue ;
        cur_qd /= num_sht;
        
        if (app.qd[i] == 0.0)
            app.qd[i] = cur_qd;
        else
            app.qd[i] = 0.8 * app.qd[i] + 0.2 * cur_qd;
        
        if(num_sht > 0.0)
            fprintf(app.qd_file, "Port: %u    queueing-delay: %lf    num_short: %.0lf    num_total: %u \n",app.ports[i], app.qd[i], num_sht, cp_ret);  
        
        if (app.num_drop[i] > 0){
            double ratio = (double)((uint64_t)app.num_drop[i]*100.0 / app.num_sht[i]); 
            printf("Drop_rate: %lf\n",ratio);
        }
    }   
    return ;      
}
