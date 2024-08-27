#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define BUFFER_SIZE 256
#define STATUS_WIDTH 20
#define PING_WIDTH (COLS - STATUS_WIDTH)

typedef struct {
  uv_timer_t timer;
  char ip[INET_ADDRSTRLEN];
  WINDOW *status_win;
  WINDOW *ping_win;
} ip_status_t;

int real_ping(const char *ip, char *ping_output, size_t max_size) {
  char cmd[100];
  snprintf(cmd, sizeof(cmd), "ping -c 3 -W 1 %s", ip); // Changed to -c 3

  FILE *fp = popen(cmd, "r");
  if (fp == NULL) {
    perror("popen");
    return 0; // Error executing ping
  }

  char buffer[BUFFER_SIZE];
  ping_output[0] = '\0'; // Initialize buffer
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    strncat(ping_output, buffer,
            max_size - strlen(ping_output) - 1); // Append output to buffer
  }

  pclose(fp);

  // Determine the status based on the output
  if (strstr(ping_output, "0 packets received") != NULL) {
    return 0; // DOWN
  } else {
    return 1; // UP
  }
}

void ping_ip(uv_timer_t *timer) {
  ip_status_t *ip_data = (ip_status_t *)timer->data;
  char ping_output[BUFFER_SIZE * 3]; // Buffer to hold the output
  int status = real_ping(ip_data->ip, ping_output, sizeof(ping_output));

  // Clear windows and print new content
  wclear(ip_data->status_win);
  wclear(ip_data->ping_win);

  // Print status on the left side
  box(ip_data->status_win, 0, 0);
  mvwprintw(ip_data->status_win, 1, 1, "IP %s is %s", ip_data->ip,
            status ? "UP" : "DOWN");

  // Print ping output on the right side
  box(ip_data->ping_win, 0, 0);
  mvwprintw(ip_data->ping_win, 1, 1, "%s", ping_output);

  // Refresh windows to update the screen
  wrefresh(ip_data->status_win);
  wrefresh(ip_data->ping_win);
}

int main() {
  initscr();  // Initialize ncurses
  noecho();   // Disable automatic echoing of typed characters
  cbreak();   // Disable line buffering
  timeout(0); // Non-blocking input

  int rows, cols;
  getmaxyx(stdscr, rows, cols); // Get terminal size

  if (cols < STATUS_WIDTH + 20) { // Ensure terminal is wide enough
    endwin();
    fprintf(stderr, "Terminal width too small\n");
    return 1;
  }

  WINDOW *status_win = newwin(rows - 1, STATUS_WIDTH, 0, 0);
  WINDOW *ping_win = newwin(rows - 1, cols - STATUS_WIDTH, 0, STATUS_WIDTH);

  uv_loop_t *loop = uv_default_loop();

  printf("Enter IP addresses (type 'done' to finish):\n");

  ip_status_t *ip_data = NULL;
  size_t ip_count = 0;
  size_t ip_capacity = 10; // Initial capacity for IP addresses

  ip_data = malloc(ip_capacity * sizeof(ip_status_t));
  if (ip_data == NULL) {
    endwin();
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
        endwin();
        perror("realloc");
        return 1;
      }
    }

    ip_data[ip_count].status_win = status_win;
    ip_data[ip_count].ping_win = ping_win;
    strcpy(ip_data[ip_count].ip, ip);

    uv_timer_init(loop, &ip_data[ip_count].timer);
    ip_data[ip_count].timer.data = &ip_data[ip_count];
    uv_timer_start(
        &ip_data[ip_count].timer, ping_ip, 0,
        10000); // Start with immediate ping and repeat every 10 seconds
    ip_count++;
  }

  uv_run(loop, UV_RUN_DEFAULT);

  // Cleanup ncurses
  endwin();
  free(ip_data);

  return 0;
}
