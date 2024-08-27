#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // For sleep and usleep functions

#define MAX_IPS 100
#define PING_COUNT 3

WINDOW *monitor_win;
WINDOW *ping_win;
char *ip_addresses[MAX_IPS];
int ip_test_count[MAX_IPS];
size_t num_ips = 0;

// Function to ping an IP address and return if it is up
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

// Initialize ncurses and create windows
void init_curses() {
  initscr();            // Initialize the window
  cbreak();             // Disable line buffering
  noecho();             // Don't display typed characters
  keypad(stdscr, TRUE); // Enable special keys
  timeout(100);         // Set a timeout for getch

  // Create windows for monitoring and ping output
  int height, width;
  getmaxyx(stdscr, height, width);

  monitor_win = newwin(height, width / 2, 0, 0);
  ping_win = newwin(height, width / 2, 0, width / 2);

  // Add titles to windows
  box(monitor_win, 0, 0);
  mvwprintw(monitor_win, 0, 1, " IP Monitor Output ");
  wrefresh(monitor_win);

  box(ping_win, 0, 0);
  mvwprintw(ping_win, 0, 1, " Ping Output ");
  wrefresh(ping_win);
}

// Update the monitor window
void update_monitor(const char *ip, int is_up, int test_count) {
  werase(monitor_win);
  box(monitor_win, 0, 0);

  // Display the IP monitor output
  mvwprintw(monitor_win, 1, 1, "IP Monitor Output");
  for (size_t i = 0; i < num_ips; ++i) {
    int y = i + 2;
    if (y >= LINES - 1)
      break;

    mvwprintw(monitor_win, y, 1, "[%d] %s", ip_test_count[i], ip_addresses[i]);
    if (is_up) {
      mvwprintw(monitor_win, y, 30, "UP");
      wattron(monitor_win, COLOR_PAIR(1)); // Green
      mvwprintw(monitor_win, y, 30, "UP");
      wattroff(monitor_win, COLOR_PAIR(1));
    } else {
      mvwprintw(monitor_win, y, 30, "DOWN");
      wattron(monitor_win, COLOR_PAIR(2)); // Red
      mvwprintw(monitor_win, y, 30, "DOWN");
      wattroff(monitor_win, COLOR_PAIR(2));
    }
    ip_test_count[i]++;
  }
  wrefresh(monitor_win);
}

// Update the ping output window
void update_ping_output(const char *ping_output) {
  werase(ping_win);
  box(ping_win, 0, 0);

  // Display the ping output
  mvwprintw(ping_win, 1, 1, "Ping Output:");
  mvwprintw(ping_win, 2, 1, "%s", ping_output);
  wrefresh(ping_win);
}

int main() {
  // Initialize ncurses and create windows
  init_curses();

  // Set up colors
  start_color();
  init_pair(1, COLOR_GREEN, COLOR_BLACK);
  init_pair(2, COLOR_RED, COLOR_BLACK);

  char ping_output[1024];
  int ch;

  // Main loop
  while ((ch = getch()) != 'q') {
    for (size_t i = 0; i < num_ips; ++i) {
      int is_up = ping_ip(ip_addresses[i], ping_output, sizeof(ping_output));
      update_monitor(ip_addresses[i], is_up, ip_test_count[i]);
      update_ping_output(ping_output);

      // Sleep between tests
      usleep(1000000); // Sleep for 1 second
    }
  }

  endwin(); // Clean up ncurses
  return 0;
}
