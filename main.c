// Compilation Command:     gcc main.c -o <executable_name> -pthread -w
// Run Command:             ./<executable_name> <input_file_name> <output_file_name>

#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/shm.h>

void paint_box(int box_number, char box_colors[], char *current_color, int n_boxes, FILE *file_pointer);
int count_distinct_colors(char box_colors[], int n_boxes);

int main(int argc, char **argv)
{
    // Create named semaphores.
    sem_t *painting_sem = sem_open("/painting_sem", O_CREAT, 0644, 2);
    sem_t *color_changing_sem = sem_open("/color_changing_sem", O_CREAT, 0644, 1);
    // Create necessary variables.
    FILE *file_pointer;
    int n_boxes, f;

    // Create shared memory 1.
    // ftok() needs an existing file. Create an arbitrary file.
    file_pointer = fopen("shmfile.txt", "w");
    fclose(file_pointer);

    key_t key1 = ftok("shmfile.txt", 65); // Create unique key for shared memory using "shmfile.txt".
    int shmid1 = shmget(key1, 1024, 0666 | IPC_CREAT);// Create shared memory with the key.
    char *current_color = (char *)shmat(shmid1, (void *)0, 0);// Create variable in shared memory.
    

    // Open input file and get number of boxes from it.
    file_pointer = fopen(argv[1], "r");
    fscanf(file_pointer, "%d", &n_boxes);

    // Create shared memory 2.
    key_t key2 = ftok("shmfile.txt", 64);// Create unique key for shared memory using "shmfile.txt".
    int shmid2 = shmget(key2, n_boxes * sizeof(char), 0666 | IPC_CREAT);// Create shared memory with the key.
    char *box_colors = (char *)shmat(shmid2, (void *)0, 0);// Create variable in shared memory.

    // Read all colors from the input file.
    for (size_t i = 0; i < n_boxes; i++)
    {
        fscanf(file_pointer, "%c", &box_colors[i]);
        while (isspace(box_colors[i])) // Ignore whitespaces.
            fscanf(file_pointer, "%c", &box_colors[i]);
    }

    // Close input file, open output file.
    fclose(file_pointer);
    file_pointer = fopen(argv[2], "w");

    // There must be a color change for each unique color.
    // Find color changes and write it to the file.
    int color_changes = count_distinct_colors(box_colors, n_boxes);
    fprintf(file_pointer, "%d\n", color_changes);
    fflush(file_pointer);
    printf("%d\n", color_changes);

    // Make current_color the first box's color. Create children processes.
    *current_color = box_colors[0];
    for (size_t i = 0; i < n_boxes; i++)
    {
        f = fork();
        if (f == 0)
        {
            paint_box(i, box_colors, current_color, n_boxes, file_pointer);
            break;
        }
    }

    // Wait for all children to end.
    for (size_t i = 0; i < n_boxes; i++)
    {
        wait();
    }

    // Close semaphores.
    sem_close(painting_sem);
    sem_close(color_changing_sem);

    // Unlink semaphors, close the file, detach and destroy shared memories (if parent).
    if (f > 0)
    {
        sem_unlink("/painting_sem");
        sem_unlink("/color_changing_sem");
        shmdt(current_color);
        shmdt(box_colors);
        shmctl(shmid1, IPC_RMID, NULL);
        shmctl(shmid2, IPC_RMID, NULL);
        fclose(file_pointer);
    }

    return 0;
}

void paint_box(int box_number, char box_colors[], char *current_color, int n_boxes, FILE *file_pointer)
{
    // Get named semaphores
    sem_t *painting_sem = sem_open("/painting_sem", 0);
    sem_t *color_changing_sem = sem_open("/color_changing_sem", 0);

    // If current color is not the process' color, wait.
    while (*current_color != box_colors[box_number])
        ;

    // Critical section 1 (2 process may enter)
    sem_wait(painting_sem);

    // Write pid and process's color to the file.
    fprintf(file_pointer, "%d %c\n", getpid(), box_colors[box_number]);
    printf("%d %c\n", getpid(), box_colors[box_number]);

    // Wait functions to simulate painting.
    if (box_colors[box_number] == 'G' || box_colors[box_number] == 'O' || box_colors[box_number] == 'B')
        sleep(2);
    else
        sleep(1);

    // Critical section 2 (1 process may enter)
    sem_wait(color_changing_sem);

    // Check colored box.
    box_colors[box_number] = '+';

    // Check if all boxes with the current color are painted.
    int current_color_finished = 1;
    for (size_t i = 0; i < n_boxes; i++)
    {
        if (box_colors[i] == *current_color)
        {
            current_color_finished = 0;
            break;
        }
    }

    // If all boxes with the current color are painted, decide next color.
    if (current_color_finished)
    {
        char next_color;
        for (size_t i = 0; i < n_boxes; i++)
        {
            if (box_colors[i] != *current_color && box_colors[i] != '+')
            {
                next_color = box_colors[i];
                break;
            }
        }
        // Change current color to the next color.
        *current_color = next_color;
    }
    // Critical Section 2 ends.
    sem_post(color_changing_sem);
    // Critical Section 1 ends.
    sem_post(painting_sem);
}

int count_distinct_colors(char box_colors[], int n_boxes)
{
    int res = 1;

    for (int i = 1; i < n_boxes; i++)
    {
        int j = 0;
        for (j = 0; j < i; j++)
            if (box_colors[i] == box_colors[j])
                break;

        if (i == j)
            res++;
    }
    return res;
}
