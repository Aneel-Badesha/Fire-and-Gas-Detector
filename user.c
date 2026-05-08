#include "user.h"


int UserButtonValue() {
    FILE *pFile = fopen(USERBUTTON, "r");
    if (pFile == NULL) {
        fprintf(stderr, "WARN: cannot open %s; treating button as released\n", USERBUTTON);
        return 1;
    }

    int value = 1;
    int items = fscanf(pFile, "%d", &value);
    fclose(pFile);

    if (items <= 0) {
        fprintf(stderr, "WARN: cannot read %s; treating button as released\n", USERBUTTON);
        return 1;
    }

    return value;
}

void *exitProgram(void *arg)
{
    struct thread_data *data = arg;

    while(1) {
        bool triggered = false;

        pthread_mutex_lock(&data->mutexControl);
        {
            triggered = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        if (!triggered) {
            int button_value = UserButtonValue();

            // On the board, 0 is the value for button pressed
            if (button_value == 0) {
                printf("USER button pressed! Closing all threads...\n");

                pthread_mutex_lock(&data->mutexControl);
                {
                    data->end_all_threads = true;
                }
                pthread_mutex_unlock(&data->mutexControl);
                triggered = true;
            }
        }

        if (triggered) {
            pthread_join(data->id_output, NULL);
            pthread_join(data->id_temp, NULL);
            pthread_join(data->id_IR, NULL);
            pthread_join(data->id_air, NULL);
            pthread_join(data->id_status, NULL);
            pthread_join(data->id_alarm, NULL);
            break;
        }

        // Check every 100 ms
        sleepForMs(100);
    }

    return NULL;
}
