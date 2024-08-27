#include <ncurses.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_IPS 100
#define MAX_IP_LENGTH 16
#define REFRESH_INTERVAL 5 // Time in seconds between updates
#define PING_COUNT 3       // Number of ping packets
#define MAX_OUTPUT_SIZE 1024
#define TEST_INTERVAL 3 // Interval in seconds for each IP test

typedef struct {
  char ip[MAX_IP_LENGTH];
  char output[MAX_OUTPUT_SIZE];
  int is_up;
  int test_count;
  pthread_t thread; // Add thread ID to manage the thread properly
} IPMonitor;

// Function prototypes
void *monitor_ip(void *arg);
void display_results(WINDOW *status_win, WINDOW *output_win,
                     IPMonitor monitors[], int ip_count);
int ping_ip(const char *ip, char *output, size_t output_size);
void get_current_time(char *buffer, size_t buffer_size);

int main() {
  IPMonitor monitors[MAX_IPS];
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

    strncpy(monitors[ip_count].ip, input, MAX_IP_LENGTH);
    monitors[ip_count].ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination
    monitors[ip_count].test_count = 0;
    monitors[ip_count].is_up = 0; // Default to DOWN
    ip_count++;
  }

  // Initialize threads for asynchronous monitoring
  for (i = 0; i < ip_count; i++) {
    pthread_create(&monitors[i].thread, NULL, monitor_ip, &monitors[i]);
  }

  // Initialize ncurses
  initscr();
  cbreak();
  noecho();
  timeout(100); // Non-blocking input

  start_color();
  init_pair(1, COLOR_GREEN, COLOR_BLACK); // UP status color
  init_pair(2, COLOR_RED, COLOR_BLACK);   // DOWN status color

  int height, width;
  getmaxyx(stdscr, height, width); // Get terminal size

  // Adjust window sizes according to terminal size
  WINDOW *status_win = newwin(height, width / 2, 0, 0);
  WINDOW *output_win = newwin(height, width / 2, 0, width / 2);

  // Print titles for the frames
  mvwprintw(status_win, 0, 1, "IP Monitor Output");
  mvwprintw(output_win, 0, 1, "Ping Output");

  // Main loop to display results and update every REFRESH_INTERVAL seconds
  int ch;
  while ((ch = wgetch(stdscr)) != 'q') {
    display_results(status_win, output_win, monitors, ip_count);
    sleep(REFRESH_INTERVAL); // Wait for a while before updating
  }

  // Join threads and cleanup ncurses
  for (i = 0; i < ip_count; i++) {
    pthread_cancel(monitors[i].thread);     // Cancel the thread
    pthread_join(monitors[i].thread, NULL); // Ensure thread termination
  }

  delwin(status_win);
  delwin(output_win);
  endwin();

  return 0;
}

void *monitor_ip(void *arg) {
  IPMonitor *monitor = (IPMonitor *)arg;

  while (1) {
    // Update the IP status
    monitor->is_up = ping_ip(monitor->ip, monitor->output, MAX_OUTPUT_SIZE);
    monitor->test_count++;
    sleep(TEST_INTERVAL); // Wait before the next test
  }

  return NULL;
}

void display_results(WINDOW *status_win, WINDOW *output_win,
                     IPMonitor monitors[], int ip_count) {
  int i;
  char timestamp[64];

  wclear(status_win);
  wclear(output_win);
  wborder(status_win, '|', '|', '-', '-', '+', '+', '+', '+');
  wborder(output_win, '|', '|', '-', '-', '+', '+', '+', '+');

  // Print IP Monitor Output title
  mvwprintw(status_win, 0, 1, "IP Monitor Output");

  // Print IP status
  for (i = 0; i < ip_count; i++) {
    get_current_time(timestamp, sizeof(timestamp));

    if (monitors[i].is_up) {
      wattron(status_win, COLOR_PAIR(1)); // Green color
    } else {
      wattron(status_win, COLOR_PAIR(2) | A_BOLD); // Red color, bold
    }

    mvwprintw(status_win, i + 1, 1, "%s (%d) IP %s is %s", timestamp,
              monitors[i].test_count, monitors[i].ip,
              monitors[i].is_up ? "UP" : "DOWN");
    wattroff(status_win, COLOR_PAIR(1) | COLOR_PAIR(2) | A_BOLD);
  }

  // Print Ping Output title
  mvwprintw(output_win, 0, 1, "Ping Output");

  // Display ping output with timestamp
  int line = 1;
  for (i = 0; i < ip_count; i++) {
    char ip_timestamp[64];
    get_current_time(ip_timestamp, sizeof(ip_timestamp));
    mvwprintw(output_win, line, 1, "%s - IP %s:\n%s", ip_timestamp,
              monitors[i].ip, monitors[i].output);
    line += PING_COUNT +
            2; // Move down by number of lines in ping output + some extra space
  }

  wrefresh(status_win);
  wrefresh(output_win);
}

int ping_ip(const char *ip, char *output, size_t output_size) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "ping -c %d -W 1 %s 2>/dev/null", PING_COUNT, ip);

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
