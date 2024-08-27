#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define BUFFER_SIZE 256

typedef struct {
  uv_timer_t timer;
  char ip[INET_ADDRSTRLEN];
} ip_status_t;

int real_ping(const char *ip) {
  char cmd[100];
  snprintf(cmd, sizeof(cmd), "ping -c 3 -W 1 %s", ip);

  FILE *fp = popen(cmd, "r");
  if (fp == NULL) {
    perror("popen");
    return 0; // Error executing ping
  }

  char buffer[BUFFER_SIZE];
  int packets_received = 0;

  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    printf("Output: %s", buffer); // Debug output
    // Check if the line contains packet transmission summary
    if (strstr(buffer, "packets transmitted") != NULL) {
      if (strstr(buffer, "0 received") != NULL) {
        packets_received = 0;
      } else {
        packets_received = 1;
      }
    }
  }

  pclose(fp);
  printf("Packets received: %d\n", packets_received); // Debug output
  return packets_received > 0 ? 1 : 0;
}

void ping_ip(uv_timer_t *timer) {
  ip_status_t *ip_data = (ip_status_t *)timer->data;
  int success = real_ping(ip_data->ip);

  if (success) {
    printf("IP %s is UP\n", ip_data->ip);
  } else {
    printf("IP %s is DOWN\n", ip_data->ip);
  }
}

int main() {
  uv_loop_t *loop = uv_default_loop();

  printf("Enter IP addresses (type 'done' to finish):\n");

  ip_status_t *ip_data = NULL;
  size_t ip_count = 0;
  size_t ip_capacity = 10; // Initial capacity for IP addresses

  ip_data = malloc(ip_capacity * sizeof(ip_status_t));
  if (ip_data == NULL) {
    perror("malloc");
    return 1;
  }

  while (1) {
    char ip[INET_ADDRSTRLEN];
    printf("IP: ");
    if (scanf("%s", ip) != 1 || strcmp(ip, "done") == 0) {
      break;
    }

    if (ip_count >= ip_capacity) {
      ip_capacity *= 2;
      ip_data = realloc(ip_data, ip_capacity * sizeof(ip_status_t));
      if (ip_data == NULL) {
        perror("realloc");
        return 1;
      }
    }

    strcpy(ip_data[ip_count].ip, ip);
    uv_timer_init(loop, &ip_data[ip_count].timer);
    ip_data[ip_count].timer.data = &ip_data[ip_count];
    uv_timer_start(
        &ip_data[ip_count].timer, ping_ip, 0,
        10000); // Start with immediate ping and repeat every 10 seconds
    ip_count++;
  }

  uv_run(loop, UV_RUN_DEFAULT);

  free(ip_data);
  return 0;
}
