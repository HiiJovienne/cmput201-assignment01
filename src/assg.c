#include <stdio.h>
#include <stdlib.h>

#define MAX_PATH_LENGTH 256
#define MAX_ROUTE_LENGTH 16
#define MAX_STOP_NAME_LENGTH 256
#define MAX_POSSIBLE_ROUTES 14

/* Columns in the tap array. */
#define YEAR 0
#define MONTH 1
#define DAY 2
#define HOUR 3
#define MINUTE 4
#define TYPE 5
#define STOP 6
#define TAP_FIELDS 7

/* Columns in the history array. */
#define ENTRY_STOP 0
#define ENTRY_HOUR 1
#define EXIT_STOP 2
#define FREQUENCY 3

void fail(char message[]) {
    printf("%s\n", message);
    exit(EXIT_FAILURE);
}

FILE *open_file(char path[], char mode[]) {
    FILE *file = fopen(path, mode);
    if (file == NULL)
        fail("Cannot open file");
    return file;
}

int days_in_month(int month, int year) {
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && (year % 400 == 0 ||
                      (year % 4 == 0 && year % 100 != 0)))
        return 29;
    return days[month - 1];
}

/* Read digits from a string without using sscanf or atoi. */
int read_number(char line[], int start, int end) {
    int value = 0;
    for (int i = start; i < end; i++) {
        if (line[i] < '0' || line[i] > '9')
            fail("Invalid data format");
        /* Stop IDs use the lab machine's 32-bit signed int range. */
        if (value > 214748364 ||
            (value == 214748364 && line[i] > '7'))
            fail("Invalid stop ID");
        value = value * 10 + line[i] - '0';
    }
    return value;
}

/* Each input line has the form MM-DD-YYYY,HH:MM AM,E,1234. */
void read_tap(FILE *file, int tap[]) {
    char line[MAX_PATH_LENGTH];
    int length = 0;
    if (fgets(line, MAX_PATH_LENGTH, file) == NULL)
        fail("Invalid data format");
    while (line[length] != '\0' && line[length] != '\n' &&
           line[length] != '\r')
        length++;
    if (length < 22 || line[2] != '-' || line[5] != '-' ||
        line[10] != ',' || line[13] != ':' || line[16] != ' ' ||
        line[18] != 'M' || line[19] != ',' || line[21] != ',' ||
        (line[17] != 'A' && line[17] != 'P'))
        fail("Invalid data format");

    tap[YEAR] = read_number(line, 6, 10);
    tap[MONTH] = read_number(line, 0, 2);
    tap[DAY] = read_number(line, 3, 5);
    tap[HOUR] = read_number(line, 11, 13);
    tap[MINUTE] = read_number(line, 14, 16);
    tap[TYPE] = line[20];
    if (tap[YEAR] < 1 || tap[MONTH] < 1 || tap[MONTH] > 12 ||
        tap[DAY] < 1 || tap[HOUR] < 1 || tap[HOUR] > 12 ||
        tap[MINUTE] > 59)
        fail("Invalid data format");
    if (tap[DAY] > days_in_month(tap[MONTH], tap[YEAR]) ||
        (tap[TYPE] != 'E' && tap[TYPE] != 'X' && tap[TYPE] != 'M'))
        fail("Invalid data format");
    if (length == 22 && tap[TYPE] != 'M')
        fail("Invalid data format");

    tap[STOP] = -1;
    if (length > 22)
        tap[STOP] = read_number(line, 22, length);
    tap[HOUR] %= 12;
    if (line[17] == 'P')
        tap[HOUR] += 12;
}

/* Convert a date and time to minutes, including month/year boundaries. */
long long total_minutes(int tap[]) {
    long long years = tap[YEAR] - 1;
    long long days = years * 365 + years / 4 - years / 100 + years / 400;
    for (int month = 1; month < tap[MONTH]; month++)
        days += days_in_month(month, tap[YEAR]);
    days += tap[DAY] - 1;
    return days * 1440 + tap[HOUR] * 60 + tap[MINUTE];
}

void write_time(FILE *file, int tap[]) {
    fprintf(file, "%04d-%02d-%02dT%02d:%02d:00", tap[YEAR], tap[MONTH],
            tap[DAY], tap[HOUR], tap[MINUTE]);
}

void route_details(char path[], char route[], char *direction) {
    int start = 0, i, j = 0;
    for (i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/')
            start = i + 1;
    }
    i = start + 6;                  /* Skip "route_". */
    while (path[i] != '_')
        route[j++] = path[i++];
    route[j] = '\0';
    *direction = path[i + 1];
}

int read_header(FILE *file) {
    int stops;
    if (fscanf(file, "%*[^,], %d Stops", &stops) != 1 || stops < 1)
        fail("Invalid data format");
    return stops;
}

/* Search only the section of stop_ids belonging to one route. */
int find_stop(int stop_ids[], int start, int end, int wanted) {
    for (int i = start; i < end; i++) {
        if (stop_ids[i] == wanted)
            return i;
    }
    return -1;
}

/* history row: entry stop, entry hour, exit stop, frequency. */
int guess_exit(int history[][4], int count, int entry, int hour) {
    int best_stop = -1, best_count = 0;
    for (int i = 0; i < count; i++) {
        if (history[i][ENTRY_STOP] == entry && history[i][ENTRY_HOUR] == hour) {
            if (history[i][FREQUENCY] > best_count ||
                (history[i][FREQUENCY] == best_count &&
                 history[i][EXIT_STOP] < best_stop)) {
                best_stop = history[i][EXIT_STOP];
                best_count = history[i][FREQUENCY];
            }
        }
    }
    return best_stop;
}

void record_trip(int history[][4], int *count, int entry, int hour, int exit) {
    int i;
    for (i = 0; i < *count; i++) {
        if (history[i][ENTRY_STOP] == entry && history[i][ENTRY_HOUR] == hour &&
            history[i][EXIT_STOP] == exit) {
            history[i][FREQUENCY]++;
            return;
        }
    }
    history[i][ENTRY_STOP] = entry;
    history[i][ENTRY_HOUR] = hour;
    history[i][EXIT_STOP] = exit;
    history[i][FREQUENCY] = 1;
    (*count)++;
}

int main(int argc, char *argv[]) {
    FILE *list_file, *schedule_file, *tap_file, *output_file;
    char path[MAX_PATH_LENGTH];
    int schedule_count = 0, tap_count, i, j;

    if (argc != 2 && argc != 3)
        return 1;
    list_file = open_file(argv[1], "r");
    while (fscanf(list_file, "%255s", path) == 1)
        schedule_count++;
    fclose(list_file);
    if (schedule_count == 0)
        fail("Invalid data format");

    char paths[schedule_count][MAX_PATH_LENGTH];
    char routes[schedule_count][MAX_ROUTE_LENGTH], directions[schedule_count];
    int first_stop[schedule_count + 1];
    first_stop[0] = 0;

    list_file = open_file(argv[1], "r");
    if (argc == 2)
        printf("Processing %d schedule files...\n", schedule_count);
    for (i = 0; i < schedule_count; i++) {
        fscanf(list_file, "%255s", paths[i]);
        schedule_file = open_file(paths[i], "r");
        int stops = read_header(schedule_file);
        fclose(schedule_file);
        first_stop[i + 1] = first_stop[i] + stops;
        route_details(paths[i], routes[i], &directions[i]);
        if (argc == 2)
            printf("schedule #%d is route %s it has %d stops and is in the %c direction\n",
                   i + 1, routes[i], stops, directions[i]);
    }
    fclose(list_file);
    if (argc == 2)
        return 0;

    /* Each route occupies [first_stop[i], first_stop[i + 1]). */
    int total_stops = first_stop[schedule_count];
    int stop_ids[total_stops];
    char stop_names[total_stops][MAX_STOP_NAME_LENGTH];
    for (i = 0; i < schedule_count; i++) {
        schedule_file = open_file(paths[i], "r");
        read_header(schedule_file);
        for (j = first_stop[i]; j < first_stop[i + 1]; j++) {
            if (fscanf(schedule_file, "%d,%255s", &stop_ids[j], stop_names[j]) != 2)
                fail("Invalid data format");
        }
        fclose(schedule_file);
    }

    tap_file = open_file(argv[2], "r");
    if (fscanf(tap_file, "%d", &tap_count) != 1 ||
        tap_count < 0 || tap_count % 2 != 0)
        fail("Invalid data format");
    fgets(path, MAX_PATH_LENGTH, tap_file); /* Consume the count line's newline. */
    if (tap_count == 0) {
        fclose(tap_file);
        output_file = open_file("output.txt", "w");
        fclose(output_file);
        return 0;
    }

    int taps[tap_count][TAP_FIELDS];
    long long times[tap_count];
    int trip_count = tap_count / 2;
    int order[trip_count], selected[trip_count], exits[trip_count];
    int history[trip_count][4], history_count = 0;
    for (i = 0; i < tap_count; i++) {
        read_tap(tap_file, taps[i]);
        times[i] = total_minutes(taps[i]);
    }
    fclose(tap_file);
    for (i = 0; i < tap_count; i++) {
        if (taps[i][STOP] != -1 &&
            find_stop(stop_ids, 0, total_stops, taps[i][STOP]) == -1)
            fail("Invalid stop ID");
    }

    /* Assumption from the examples: each X/M is followed by its E. */
    for (i = 0; i < trip_count; i++) {
        int x = 2 * i, e = x + 1;
        if ((taps[x][TYPE] != 'X' && taps[x][TYPE] != 'M') ||
            taps[e][TYPE] != 'E' || times[x] < times[e])
            fail("Invalid data format");
        order[i] = i;
    }
    /* Insertion sort: rearrange trip indices, keeping each pair together. */
    for (i = 1; i < trip_count; i++) {
        int trip = order[i];
        j = i;
        while (j > 0 && times[2 * order[j - 1] + 1] > times[2 * trip + 1]) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = trip;
    }

    for (i = 0; i < trip_count; i++) {
        int trip = order[i], x = 2 * trip, e = x + 1;
        int possible[MAX_POSSIBLE_ROUTES], count = 0, choice = 1;
        exits[trip] = taps[x][STOP];
        if (taps[x][TYPE] == 'M') {
            exits[trip] = guess_exit(history, history_count, taps[e][STOP], taps[e][HOUR]);
            if (exits[trip] == -1)
                fail("Cannot guess exit stop");
        }
        for (j = 0; j < schedule_count; j++) {
            if (find_stop(stop_ids, first_stop[j], first_stop[j + 1], taps[e][STOP]) != -1 &&
                find_stop(stop_ids, first_stop[j], first_stop[j + 1], exits[trip]) != -1)
                possible[count++] = j;
        }
        if (count == 0)
            fail("No matching route");
        if (count > 1) {
            int route = possible[0];
            int entry_index = find_stop(stop_ids, first_stop[route], first_stop[route + 1], taps[e][STOP]);
            int exit_index = find_stop(stop_ids, first_stop[route], first_stop[route + 1], exits[trip]);
            printf("Ambiguous trip detected:\n");
            printf("Entry: %d (%s)\n", taps[e][STOP], stop_names[entry_index]);
            printf("Exit: %d (%s)\n", exits[trip], stop_names[exit_index]);
            printf("Possible routes:\n");
            for (j = 0; j < count; j++)
                printf("%d. %s %c\n", j + 1, routes[possible[j]], directions[possible[j]]);
            printf("Select route (1-%d): \n", count);
            if (scanf("%d", &choice) != 1 || choice < 1 || choice > count)
                return 1;
        }
        selected[trip] = possible[choice - 1];
        /* Only observed X taps are added to the history, not predictions. */
        if (taps[x][TYPE] == 'X')
            record_trip(history, &history_count, taps[e][STOP], taps[e][HOUR], exits[trip]);
    }

    output_file = open_file("output.txt", "w");
    for (i = 0; i < trip_count; i++) {
        int trip = order[i], x = 2 * trip, e = x + 1;
        int route = selected[trip];
        int entry_index = find_stop(stop_ids, first_stop[route], first_stop[route + 1], taps[e][STOP]);
        int exit_index = find_stop(stop_ids, first_stop[route], first_stop[route + 1], exits[trip]);
        long long duration = times[x] - times[e];
        fprintf(output_file, "%s %c,%s,%s,", routes[route], directions[route],
                stop_names[entry_index], stop_names[exit_index]);
        write_time(output_file, taps[e]);
        fprintf(output_file, ",");
        write_time(output_file, taps[x]);
        fprintf(output_file, ",%lld:%02lld:00", duration / 60, duration % 60);
        if (taps[x][TYPE] == 'M')
            fprintf(output_file, ",Guessed");
        fprintf(output_file, "\n");
    }
    fclose(output_file);
    return 0;
}
