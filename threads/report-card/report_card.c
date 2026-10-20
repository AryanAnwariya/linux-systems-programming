
#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_SUBJECTS 32
#define MAX_STUDENTS 128
#define INPUT_FILE   "input.txt"

typedef struct {
    int student_id;
    double average;
    int minimum;
    int maximum;
    int subject_marks[MAX_SUBJECTS];
} student_stats;

struct thread_params {
    int thread_id;    
    int start_index;   
    int segment_size;   
};


static int num_subjects;
static int num_students;
static int marks[MAX_SUBJECTS][MAX_STUDENTS];       
static student_stats student_arr[MAX_STUDENTS];    
static student_stats sorted_arr[MAX_STUDENTS];      
static int sort_key;                                


static int read_input_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("Error opening " INPUT_FILE);
        return -1;
    }

    int values[2 + MAX_SUBJECTS * MAX_STUDENTS];
    int count = 0, needed = 2;
    char line[8192];

    while (count < needed && fgets(line, sizeof line, fp)) {
        char *comment = strstr(line, "//");
        if (comment) *comment = '\0';

        char *p = line;
        while (*p && count < needed) {
            if (isdigit((unsigned char)*p) ||
                ((*p == '-' || *p == '+') && isdigit((unsigned char)p[1]))) {
                char *end;
                long v = strtol(p, &end, 10);
                values[count++] = (int)v;
                p = end;

                if (count == 2) {           /* dimensions known: validate */
                    if (values[0] < 1 || values[0] > MAX_SUBJECTS) {
                        fprintf(stderr, "Number of subjects must be 1..%d\n", MAX_SUBJECTS);
                        fclose(fp);
                        return -1;
                    }
                    if (values[1] < 1 || values[1] > MAX_STUDENTS) {
                        fprintf(stderr, "Number of students must be 1..%d\n", MAX_STUDENTS);
                        fclose(fp);
                        return -1;
                    }
                    needed = 2 + values[0] * values[1];
                }
            } else {
                p++;
            }
        }
    }
    fclose(fp);

    if (count < needed) {
        fprintf(stderr, "Error: " INPUT_FILE " has %d values, expected %d\n", count, needed);
        return -1;
    }

    num_subjects = values[0];
    num_students = values[1];
    for (int i = 0; i < num_subjects; i++)
        for (int j = 0; j < num_students; j++)
            marks[i][j] = values[2 + i * num_students + j];
    return 0;
}



static void *aggregate_worker(void *arg)
{
    int id = *(int *)arg;
    student_stats *s = &student_arr[id];  

    int sum = 0;
    int mn = marks[0][id], mx = marks[0][id];

    s->student_id = id;
    for (int i = 0; i < num_subjects; i++) {
        int m = marks[i][id];
        s->subject_marks[i] = m;
        sum += m;
        if (m < mn) mn = m;
        if (m > mx) mx = m;
    }
    s->average = (double)sum / num_subjects;
    s->minimum = mn;
    s->maximum = mx;

    pthread_exit(NULL);
}



static double key_value(const student_stats *s)
{
    switch (sort_key) {
    case -3: return s->maximum;
    case -2: return s->minimum;
    case -1: return s->average;
    default: return s->subject_marks[sort_key];
    }
}

static int comes_before(const student_stats *a, const student_stats *b)
{
    double va = key_value(a), vb = key_value(b);
    if (va > vb) return 1;
    if (va < vb) return 0;
    return a->student_id < b->student_id;
}



static void *sort_worker(void *arg)
{
    struct thread_params *p = (struct thread_params *)arg;
    int lo = p->start_index;
    int hi = p->start_index + p->segment_size;   

 
    for (int i = lo + 1; i < hi; i++) {
        student_stats tmp = student_arr[i];
        int j = i - 1;
        while (j >= lo && comes_before(&tmp, &student_arr[j])) {
            student_arr[j + 1] = student_arr[j];
            j--;
        }
        student_arr[j + 1] = tmp;
    }

    pthread_exit(NULL);
}


static void kway_merge(struct thread_params *params, int k)
{
    int pos[MAX_STUDENTS];   
    int end[MAX_STUDENTS];  

    for (int t = 0; t < k; t++) {
        pos[t] = params[t].start_index;
        end[t] = params[t].start_index + params[t].segment_size;
    }

    for (int out = 0; out < num_students; out++) {
        int best = -1;
        for (int t = 0; t < k; t++) {
            if (pos[t] >= end[t]) continue;
            if (best == -1 || comes_before(&student_arr[pos[t]], &student_arr[pos[best]]))
                best = t;
        }
        sorted_arr[out] = student_arr[pos[best]++];
    }
}


static int read_int_in_range(const char *prompt, int lo, int hi)
{
    char line[256];
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) {
            printf("\n");
            exit(EXIT_FAILURE);
        }
        int v;
        char extra;
        if (sscanf(line, " %d %c", &v, &extra) == 1 && v >= lo && v <= hi)
            return v;
        printf("Invalid input. Please enter an integer between %d and %d.\n", lo, hi);
    }
}



static void print_student(int id)
{
    const student_stats *s = NULL;
    for (int i = 0; i < num_students; i++) {
        if (sorted_arr[i].student_id == id) {
            s = &sorted_arr[i];
            break;
        }
    }
    if (!s) {
        printf("Invalid student ID. Valid range: 0..%d\n", num_students - 1);
        return;
    }

    printf("Student ID : %d\n", s->student_id);
    printf("Average : %.2f\n", s->average);
    printf("Minimum : %d\n", s->minimum);
    printf("Maximum : %d\n", s->maximum);
    printf("Subject Marks: [");
    for (int i = 0; i < num_subjects; i++)
        printf("%s%d", i ? ", " : "", s->subject_marks[i]);
    printf("]\n");
}

static void print_sorted(void)
{
    for (int i = 0; i < num_students; i++) {
        const student_stats *s = &sorted_arr[i];
        if (sort_key == -1)
            printf("s%d %.2f\n", s->student_id, s->average);
        else
            printf("s%d %d\n", s->student_id, (int)key_value(s));
    }
}



int main(void)
{
    if (read_input_file(INPUT_FILE) != 0)
        return EXIT_FAILURE;

    pthread_t agg_tids[MAX_STUDENTS];
    int agg_ids[MAX_STUDENTS];

    for (int i = 0; i < num_students; i++) {
        agg_ids[i] = i;
        int rc = pthread_create(&agg_tids[i], NULL, aggregate_worker, &agg_ids[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create (aggregation %d): %s\n", i, strerror(rc));
            return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < num_students; i++) {
        int rc = pthread_join(agg_tids[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join (aggregation %d): %s\n", i, strerror(rc));
            return EXIT_FAILURE;
        }
    }

    
    char prompt[128];
    snprintf(prompt, sizeof prompt,
             "Enter sort key (-3=max, -2=min, -1=avg, 0..%d=subject index): ",
             num_subjects - 1);
    sort_key = read_int_in_range(prompt, -3, num_subjects - 1);

    int k = read_int_in_range("Enter number of sorting threads (k): ", 1, num_students);

    
    struct thread_params params[MAX_STUDENTS];
    pthread_t sort_tids[MAX_STUDENTS];
    int base = num_students / k, rem = num_students % k, start = 0;

    for (int t = 0; t < k; t++) {
        params[t].thread_id = t;
        params[t].start_index = start;
        params[t].segment_size = base + (t < rem ? 1 : 0);
        start += params[t].segment_size;
    }
    for (int t = 0; t < k; t++) {
        int rc = pthread_create(&sort_tids[t], NULL, sort_worker, &params[t]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create (sorter %d): %s\n", t, strerror(rc));
            return EXIT_FAILURE;
        }
    }
    for (int t = 0; t < k; t++) {
        int rc = pthread_join(sort_tids[t], NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join (sorter %d): %s\n", t, strerror(rc));
            return EXIT_FAILURE;
        }
    }

    kway_merge(params, k);

  
    printf("--- Report Card Menu ---\n");
    char line[256];
    for (;;) {
        printf("Enter command (1 <id> = view student, 2 = view all, 0 = exit): ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) {
            printf("\n");
            break;
        }

        int cmd, id;
        char extra;
        int n = sscanf(line, " %d %d %c", &cmd, &id, &extra);

        if (n == 1 && cmd == 0) {
            break;
        } else if (n == 1 && cmd == 2) {
            print_sorted();
        } else if (n == 2 && cmd == 1) {
            print_student(id);
        } else if (n >= 1 && cmd == 1) {
            printf("Usage: 1 <student_id>\n");
        } else {
            printf("Invalid command.\n");
        }
    }

    return EXIT_SUCCESS;
}
