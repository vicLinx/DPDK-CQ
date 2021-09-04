#include "main.h"

/* 采样 */
void
app_main_loop_sampling(void)
{
    FILE *fp = NULL;
    fp = fopen("/home/user/output", "w");
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

        double cur_qd = 0.0, num_sht = 0.0;
        uint32_t k, cur_len = 0;
        for (k = 0; k < cp_ret; ++k) {
            cur_len += mbufs[k]->pkt_len;
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
        
        if (num_sht > 0.0)
            fprintf(fp, "Port: %u    queueing-delay: %lf    num_short: %.0lf\n",app.ports[i], app.qd[i], num_sht); 

        if (app.num_drop[i] > 0 && app.ports[i] == 3){
            double ratio = (double)(app.num_drop[i]*100.0 / app.num_sht[i]); 
            printf("Drop_rate: %lf\n",ratio);
        }
    }   
    fclose(fp);  
    return ;      
}
