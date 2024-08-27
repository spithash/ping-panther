#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_IPS 100
#define MAX_IP_LENGTH 16
#define REFRESH_INTERVAL 5 // Time in seconds between updates
#define PING_COUNT 3       // Number of ping packets

// Function prototypes
void display_results(WINDOW *status_win, WINDOW *output_win, char *ips[],
                     int ip_count, int test_counts[]);
int ping_ip(const char *ip, char *output, size_t output_size);
void get_current_time(char *buffer, size_t buffer_size);

int main() {
  char *ips[MAX_IPS];
  int ip_count = 0;
  char input[MAX_IP_LENGTH];
  int i;

  // Initialize IP list
  printf("Enter IP addresses (type 'done' to finish):\n");
  while (1) {
    printf("IP: ");
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0'; // Remove newline character

    if (strcmp(input, "done") == 0) {
      break;
    }

    if (ip_count >= MAX_IPS) {
      printf("Maximum number of IPs reached.\n");
      break;
    }

    ips[ip_count] = strdup(input);
    ip_count++;
  }

  // Initialize test counts
  int test_counts[MAX_IPS] = {0};

  // Initialize ncurses
  initscr();
  cbreak();
  noecho();
  timeout(100); // Non-blocking input

  int height, width;
  getmaxyx(stdscr, height, width); // Get terminal size

  // Adjust window sizes according to terminal size
  WINDOW *status_win = newwin(height, width / 2, 0, 0);
  WINDOW *output_win = newwin(height, width / 2, 0, width / 2);

  // Main loop to display results and update every REFRESH_INTERVAL seconds
  while (1) {
    display_results(status_win, output_win, ips, ip_count, test_counts);
    sleep(REFRESH_INTERVAL); // Wait for a while before updating
  }

  // Cleanup ncurses
  delwin(status_win);
  delwin(output_win);
  endwin();

  // Free allocated memory
  for (i = 0; i < ip_count; i++) {
    free(ips[i]);
  }

  return 0;
}

void display_results(WINDOW *status_win, WINDOW *output_win, char *ips[],
                     int ip_count, int test_counts[]) {
  int i;
  char output[1024];
  char timestamp[64];

  wclear(status_win);
  wclear(output_win);
  wborder(status_win, '|', '|', '-', '-', '+', '+', '+', '+');
  wborder(output_win, '|', '|', '-', '-', '+', '+', '+', '+');

  // Print IP status
  for (i = 0; i < ip_count; i++) {
    int status = ping_ip(ips[i], output, sizeof(output));
    get_current_time(timestamp, sizeof(timestamp));
    mvwprintw(status_win, i + 1, 1, "%s (%d) IP %s is %s", timestamp,
              test_counts[i], ips[i], status ? "UP" : "DOWN");
    test_counts[i]++;
  }

  // Display ping output with timestamp
  mvwprintw(output_win, 0, 1, "Ping Output:");
  int line = 1;
  for (i = 0; i < ip_count; i++) {
    char ip_output[1024];
    char ip_timestamp[64];
    get_current_time(ip_timestamp, sizeof(ip_timestamp));
    ping_ip(ips[i], ip_output, sizeof(ip_output));
    mvwprintw(output_win, line, 1, "%s - IP %s:\n%s", ip_timestamp, ips[i],
              ip_output);
    line += PING_COUNT +
            2; // Move down by number of lines in ping output + some extra space
  }

  wrefresh(status_win);
  wrefresh(output_win);
}

int ping_ip(const char *ip, char *output, size_t output_size) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "ping -c %d -W 1 %s", PING_COUNT, ip);

  FILE *fp = popen(cmd, "r");
  if (fp == NULL) {
    snprintf(output, output_size, "Failed to execute ping");
    return 0; // Failure to execute ping
  }

  // Initialize output buffer
  memset(output, 0, output_size);

  char buffer[256];
  size_t len = 0;
  int received = 0;
  int total_packets = PING_COUNT;

  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    if (len + strlen(buffer) < output_size) {
      strcat(output, buffer);
      len += strlen(buffer);
    }

    // Check if packet loss is reported
    if (strstr(buffer, "packet loss") != NULL) {
      sscanf(buffer, "%*d packets transmitted, %d received", &received);
    }
  }

  pclose(fp);

  // Determine if the IP is up based on packet loss
  int is_up =
      (received >=
       (total_packets /
        2)); // Consider it UP if at least half of the packets are received
  return is_up;
}

void get_current_time(char *buffer, size_t buffer_size) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}
