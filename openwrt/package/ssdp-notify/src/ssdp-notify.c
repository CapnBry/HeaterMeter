#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>     // select
#include <sys/time.h>   // gettimeofday
#include "lssdp.h"

/* ssdp-notify
 *
 * 1. create SSDP socket with port 1900
 * 2. select SSDP socket with timeout 0.5 seconds
 *    - when select return value > 0, invoke lssdp_socket_read
 * 3. per X seconds do:
 *    - update network interface
 *    - send NOTIFY
 *    - check neighbor timeout
 * 4. when network interface is changed
 *    - show interface list
 *    - re-bind the socket
 */

void log_callback(const char * file, const char * tag, int level, int line, const char * func, const char * message) {
    char * level_name = "DEBUG";
    if (level == LSSDP_LOG_INFO)   level_name = "INFO";
    if (level == LSSDP_LOG_WARN)   level_name = "WARN";
    if (level == LSSDP_LOG_ERROR)  level_name = "ERROR";

    printf("[%-5s][%s] %s", level_name, tag, message);
}

long long get_current_time() {
    struct timeval time = {};
    if (gettimeofday(&time, NULL) == -1) {
        printf("gettimeofday failed, errno = %s (%d)\n", strerror(errno), errno);
        return -1;
    }
    return (long long) time.tv_sec * 1000 + (long long) time.tv_usec / 1000;
}

static int show_neighbor_list(lssdp_ctx * lssdp) {
    int i = 0;
    lssdp_nbr * nbr;
    puts("\nSSDP List:");
    for (nbr = lssdp->neighbor_list; nbr != NULL; nbr = nbr->next) {
        printf("%d. id = %-9s, ip = %-20s, name = %-12s, device_type = %-8s (%lld)\n",
            ++i,
            nbr->sm_id,
            nbr->location,
            nbr->usn,
            nbr->device_type,
            nbr->update_time
        );
    }
    printf("%s\n", i == 0 ? "Empty" : "");
    return 0;
}

int show_interface_list_and_rebind_socket(lssdp_ctx * lssdp) {
    // 1. show interface list
    printf("\nNetwork Interface List (%zu):\n", lssdp->interface_num);
    size_t i;
    for (i = 0; i < lssdp->interface_num; i++) {
        printf("%zu. %-6s: %s\n",
            i + 1,
            lssdp->interface[i].name,
            lssdp->interface[i].ip
        );
    }
    printf("%s\n", i == 0 ? "Empty" : "");

    // 2. re-bind SSDP socket
    if (lssdp_socket_create(lssdp) != 0) {
        puts("SSDP create socket failed");
        return -1;
    }

    return 0;
}


int main(int argc, char *argv[]) {
    if (argc < 2)
    {
        fprintf(stderr, "Usage: ssdp-notify unique_service_name\n");
        return EXIT_FAILURE;
    }
        
    lssdp_set_log_callback(log_callback);

    lssdp_ctx lssdp = {
        // .debug = true,           // debug
        .port = 1900,
        .neighbor_timeout = 30000,  // 30 seconds
        .header = {
            .search_target       = "upnp:rootdevice",
            .location.prefix     = "http://",
            .location.suffix     = "/luci/ssdp"
        },

        // callback
        //.neighbor_list_changed_callback     = show_neighbor_list,
        .network_interface_changed_callback = show_interface_list_and_rebind_socket,
    };
    snprintf(lssdp.header.unique_service_name, LSSDP_FIELD_LEN, "uuid:%s::upnp:rootdevice", argv[1]);

    /* get network interface at first time, network_interface_changed_callback will be invoke
     * SSDP socket will be created in callback function
     */
    lssdp_network_interface_update(&lssdp);

    long long last_time = 0;
    // Main Loop
    for (;;) {
        fd_set fs;
        FD_ZERO(&fs);
        if (lssdp.sock > 0)
          FD_SET(lssdp.sock, &fs);
        struct timeval tv = {
            .tv_sec = 1
        };

        int ret = select(lssdp.sock + 1, &fs, NULL, NULL, &tv);
        if (ret < 0) {
            printf("select error, ret = %d\n", ret);
            break;
        }

        if (ret > 0) {
            // Read and send direct NOTIFY if M-SEARCH seen
            lssdp_socket_read(&lssdp);
        }

        // get current time
        long long current_time = get_current_time();
        if (current_time < 0) {
            printf("got invalid timestamp %lld\n", current_time);
            break;
        }

        // doing task per X seconds
        long long elapsed = (current_time - last_time) / 1000;
        if (lssdp.sock > 0 && elapsed >= 30)
        {
            lssdp_network_interface_update(&lssdp);
            lssdp_neighbor_check_timeout(&lssdp);

            // network_interface_update+callback can close the socket if list changes
            // and the socket can not be recreated/configured on the new list
            if (lssdp.sock > 0)
              lssdp_send_notify(&lssdp);

            last_time = current_time;
        } /* if have socket */
        else if (lssdp.sock < 0 && elapsed >= 10)
        {
          /* This works around a problem where the interface list changes but
           * IP_ADD_MEMBERSHIP fails with "No such device"
           */
          if (lssdp_socket_create(&lssdp) != 0)
          {
            puts("SSDP create socket failed");
            last_time = current_time;
          }
        } /* if no socket */
    } /* for(;;) */

    return EXIT_SUCCESS;
}
