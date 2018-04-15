// RDMA Server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "dccs_parameters.h"
#include "dccs_rdma.h"

char *port = "1234";

int main(int argc, char *argv[]) {
    struct rdma_cm_id *listen_id, *id;
    struct rdma_addrinfo *res;
    struct dccs_conn_param local_conn, remote_conn;
    struct ibv_mr *read_mr, *send_mr;
    struct ibv_wc wc;
    int rv = 0;

    memset(&local_conn, 0, sizeof local_conn);
    memset(&remote_conn, 0, sizeof remote_conn);

    if ((rv = dccs_listen(&listen_id, &id, &res, port)) != 0)
        goto end;

    size_t length = 16;
    void *buf = malloc(length);
    memset(buf, 0, length);
    strcpy(buf, "Hello world.");
    printf("Buffer: %s.\n", (char *)buf);

    if ((send_mr = dccs_reg_msgs(id, &local_conn, sizeof local_conn)) == NULL)
        goto out_free_buf;
    if ((read_mr = dccs_reg_read(id, buf, length)) == NULL)
        goto out_dereg_send_mr;
    local_conn.addr = htonll(read_mr->addr);
    local_conn.rkey = htonl(read_mr->rkey);

    if ((rv = dccs_rdma_send(id, &local_conn, sizeof local_conn, send_mr)) != 0) {
        goto out_dereg_read_mr;
    }
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0)
        goto out_dereg_read_mr;

    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0)
        goto out_dereg_read_mr;

    // TODO: add wait for completion
    printf("End of operations\n");

out_dereg_read_mr:
    dccs_dereg_mr(read_mr);
out_dereg_send_mr:
    dccs_dereg_mr(send_mr);
out_free_buf:
    free(buf);
// out_disconnect:
    dccs_server_disconnect(id, listen_id, res);
end:
    return rv;
}