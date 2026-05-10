#include "user.h"

// Reads the USER button sysfs file and returns 0 if pressed, 1 if released
int userButtonValue(void)
{
    FILE *p_file = fopen(USERBUTTON, "r");
    if (p_file == NULL) {
        fprintf(stderr, "WARN: cannot open %s; treating button as released\n", USERBUTTON);
        return 1;
    }

    int value = 1;
    int items = fscanf(p_file, "%d", &value);
    fclose(p_file);

    if (items <= 0) {
        fprintf(stderr, "WARN: cannot read %s; treating button as released\n", USERBUTTON);
        return 1;
    }

    return value;
}

// Polls the USER button and signals all threads to shut down when pressed
void *exitProgram(void *arg)
{
    struct thread_data *data = arg;

    while (1) {
        bool triggered = false;

        pthread_mutex_lock(&g_mutex_control);
        {
            triggered = data->end_all_threads;
        }
        pthread_mutex_unlock(&g_mutex_control);

        if (!triggered) {
            int button_val = userButtonValue();

            // On the board, 0 is the value for button pressed
            if (button_val == 0) {
                printf("USER button pressed! Closing all threads...\n");

                pthread_mutex_lock(&g_mutex_control);
                {
                    data->end_all_threads = true;
                }
                pthread_mutex_unlock(&g_mutex_control);
                triggered = true;
            }
        }

        if (triggered) {
            pthread_join(g_tid[THREAD_IDX_OUTPUT], NULL);
            pthread_join(g_tid[THREAD_IDX_TEMP],   NULL);
            pthread_join(g_tid[THREAD_IDX_IR],     NULL);
            pthread_join(g_tid[THREAD_IDX_CO],     NULL);
            pthread_join(g_tid[THREAD_IDX_CO2],    NULL);
            pthread_join(g_tid[THREAD_IDX_SMOKE],  NULL);
            pthread_join(g_tid[THREAD_IDX_STATUS], NULL);
            pthread_join(g_tid[THREAD_IDX_ALARM],  NULL);
            break;
        }

        // Check every 100 ms
        sleepForMs(100);
    }

    return NULL;
}
