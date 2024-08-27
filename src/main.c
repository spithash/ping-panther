#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_IPS 100
#define MAX_IP_LENGTH 16
#define REFRESH_INTERVAL 5 // Time in seconds between updates

// Function prototypes
void display_results(WINDOW *status_win, WINDOW *output_win, char *ips[],
                     int ip_count);
int ping_ip(const char *ip, char *output, size_t output_size);

int main() {
  char *ips[MAX_IPS];
  int ip_count = 0;
  char input[MAX_IP_LENGTH];
  int i;

  // Initialize standard input/output
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
    display_results(status_win, output_win, ips, ip_count);
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
                     int ip_count) {
  int i;
  char output[256];
  wclear(status_win);
  wclear(output_win);
  wborder(status_win, '|', '|', '-', '-', '+', '+', '+', '+');
  wborder(output_win, '|', '|', '-', '-', '+', '+', '+', '+');

  // Print IP status
  for (i = 0; i < ip_count; i++) {
    int status = ping_ip(ips[i], output, sizeof(output));
    mvwprintw(status_win, i + 1, 1, "IP %s is %s", ips[i],
              status ? "UP" : "DOWN");
  }

  // Display ping output for each IP
  mvwprintw(output_win, 0, 1, "Ping Output:");
  for (i = 0; i < ip_count; i++) {
    char ip_output[256];
    int status = ping_ip(ips[i], ip_output, sizeof(ip_output));
    mvwprintw(output_win, i + 1, 1, "IP %s:\n%s", ips[i], ip_output);
  }

  wrefresh(status_win);
  wrefresh(output_win);
}

int ping_ip(const char *ip, char *output, size_t output_size) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "ping -c 3 -W 1 %s", ip);

  FILE *fp = popen(cmd, "r");
  if (fp == NULL) {
    snprintf(output, output_size, "Failed to execute ping");
    return 0; // Failure to execute ping
  }

  // Initialize output buffer
  memset(output, 0, output_size);

  char buffer[256];
  size_t len = 0;
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    if (len + strlen(buffer) < output_size) {
      strcat(output, buffer);
      len += strlen(buffer);
    }
  }

  int is_up = strstr(output, " 0% packet loss") != NULL;
  pclose(fp);
  return is_up;
}
