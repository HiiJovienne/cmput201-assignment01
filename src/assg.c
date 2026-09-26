#include <stdio.h>
#include <string.h>

#define MAX_PATH_LENGTH 256
#define MAX_ROUTE_LENGTH 16

int main(int argc, char *argv[]) {
    FILE *list_file;
    FILE *schedule_file;

    char schedule_path[MAX_PATH_LENGTH];
    char route_id[MAX_ROUTE_LENGTH];
    char *file_name;

    char direction;

    int schedule_count = 0;
    int schedule_number = 0;
    int stops;
    int i;

    if (argc != 2) {
        return 1;
    }

    list_file = fopen(argv[1], "r");

    if (list_file == NULL) {
        return 1;
    }

    while (fscanf(list_file, "%255s", schedule_path) == 1) {
        schedule_count++;
    }

    printf("Processing %d schedule files...\n", schedule_count);

    fclose(list_file);

    list_file = fopen(argv[1], "r");

    if (list_file == NULL) {
        return 1;
    }

    while (fscanf(list_file, "%255s", schedule_path) == 1) {
        schedule_number++;

        schedule_file = fopen(schedule_path, "r");

        if (schedule_file == NULL) {
            fclose(list_file);
            return 1;
        }

        if (fscanf(schedule_file, "%*[^,], %d Stops", &stops) != 1) {
            fclose(schedule_file);
            fclose(list_file);
            return 1;
        }

        file_name = strrchr(schedule_path, '/');

        if (file_name == NULL) {
            file_name = schedule_path;
        } else {
            file_name++;
        }

        i = 6;

        while (file_name[i] != '_') {
            route_id[i - 6] = file_name[i];
            i++;
        }

        route_id[i - 6] = '\0';

        i++;

        direction = file_name[i];

        printf("schedule #%d is route %s it has %d stops and is in the %c direction\n",
               schedule_number,
               route_id,
               stops,
               direction);

        fclose(schedule_file);
    }

    fclose(list_file);

    return 0;
}