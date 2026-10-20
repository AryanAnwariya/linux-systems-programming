

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define N           9
#define NUM_CELLS   81
#define NUM_UNITS   27
#define NUM_BLOCKS  9
#define VALID       1
#define INVALID     0

struct unit_params {
    int unit_id;           
};

struct unit_result {
    int unit_id;
    int valid;             
    int missing_mask;       
    int dup_mask;          
    int blank_count;        
};

struct block_params {
    int block_id;           
};


static int grid[N][N];             
static int validity[NUM_UNITS];
static int dup_masks[NUM_UNITS];
static int heatmap[N][N];


static void unit_cell(int unit_id, int k, int *r, int *c)
{
    if (unit_id < 9) {                
        *r = unit_id;
        *c = k;
    } else if (unit_id < 18) {          
        *r = k;
        *c = unit_id - 9;
    } else {                          
        int b = unit_id - 18;
        *r = (b / 3) * 3 + k / 3;
        *c = (b % 3) * 3 + k % 3;
    }
}

static const char *unit_kind(int unit_id)
{
    if (unit_id < 9)  return "Row";
    if (unit_id < 18) return "Column";
    return "Sub-grid";
}

static int unit_index(int unit_id)
{
    return unit_id % 9;
}

static int popcount9(int mask)
{
    int n = 0;
    for (int d = 1; d <= 9; d++)
        if (mask & (1 << d)) n++;
    return n;
}


static int read_grid(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return -1;
    }

    int ch, cells = 0, line = 1, col = 0;
    while ((ch = fgetc(fp)) != EOF) {
        col++;
        if (ch == '\n') {
            line++;
            col = 0;
            continue;
        }
        if (isspace(ch))
            continue;

        int value;
        if (ch >= '1' && ch <= '9')
            value = ch - '0';
        else if (ch == '0' || ch == '.')
            value = 0;
        else {
            fprintf(stderr, "Error: invalid character '%c' at line %d, column %d of %s\n",
                    ch, line, col, path);
            fclose(fp);
            return -1;
        }

        if (cells < NUM_CELLS)
            grid[cells / N][cells % N] = value;
        cells++;
    }
    fclose(fp);

    if (cells != NUM_CELLS) {
        fprintf(stderr, "Error: %s contains %d cells; exactly %d are required (%s)\n",
                path, cells, NUM_CELLS, cells < NUM_CELLS ? "too few" : "too many");
        return -1;
    }
    return 0;
}



static void *validate_unit(void *arg)
{
    const struct unit_params *p = (const struct unit_params *)arg;
    int counts[10] = {0};

    for (int k = 0; k < N; k++) {
        int r, c;
        unit_cell(p->unit_id, k, &r, &c);
        counts[grid[r][c]]++;
    }

    struct unit_result *res = malloc(sizeof *res);
    if (!res)
        pthread_exit(NULL);           

    res->unit_id = p->unit_id;
    res->blank_count = counts[0];
    res->missing_mask = 0;
    res->dup_mask = 0;
    for (int d = 1; d <= 9; d++) {
        if (counts[d] == 0)
            res->missing_mask |= 1 << d;
        else if (counts[d] >= 2)
            res->dup_mask |= 1 << d;
    }
    
    res->valid = (res->missing_mask == 0 && res->blank_count == 0) ? VALID : INVALID;

    validity[p->unit_id] = res->valid;  
    pthread_exit(res);
}


static void *analyze_block(void *arg)
{
    const struct block_params *p = (const struct block_params *)arg;
    int b = p->block_id;

    for (int k = 0; k < N; k++) {
        int r = (b / 3) * 3 + k / 3;
        int c = (b % 3) * 3 + k % 3;
        int v = grid[r][c];

        if (v == 0) {
            heatmap[r][c] = 3;         
        } else {
            int bit = 1 << v;
            heatmap[r][c] = ((dup_masks[r] & bit) != 0)        
                          + ((dup_masks[9 + c] & bit) != 0)      
                          + ((dup_masks[18 + b] & bit) != 0);    
        }
    }
    pthread_exit(NULL);
}


static void print_grid(void)
{
    printf("Input Grid:\n");
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            if (grid[r][c])
                printf("%d", grid[r][c]);
            else
                printf(".");
            printf(c == N - 1 ? "\n" : " ");
        }
    }
    printf("\n");
}

static void print_missing_report(struct unit_result *results[])
{
    printf("Missing Digit Report:\n");
    for (int u = 0; u < NUM_UNITS; u++) {
        const struct unit_result *res = results[u];
        if (res->valid)
            continue;

        printf("  %-8s %d : missing {", unit_kind(u), unit_index(u));
        int first = 1;
        for (int d = 1; d <= 9; d++) {
            if (res->missing_mask & (1 << d)) {
                printf(first ? "%d" : ",%d", d);
                first = 0;
            }
        }
        printf("}  blanks=%d\n", res->blank_count);

        
        if (res->blank_count == 1 && popcount9(res->missing_mask) == 1) {
            int digit = 0;
            for (int d = 1; d <= 9; d++)
                if (res->missing_mask & (1 << d)) digit = d;
            for (int k = 0; k < N; k++) {
                int r, c;
                unit_cell(u, k, &r, &c);
                if (grid[r][c] == 0) {
                    printf("      -> repair: place %d at (%d,%d)\n", digit, r, c);
                    break;
                }
            }
        }
    }
}



int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <grid_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (read_grid(argv[1]) != 0)
        return EXIT_FAILURE;

    print_grid();

   
    pthread_t utids[NUM_UNITS];
    struct unit_params uparams[NUM_UNITS];
    struct unit_result *results[NUM_UNITS] = {0};
    int created = 0, status = EXIT_SUCCESS;

    for (int u = 0; u < NUM_UNITS; u++) {
        uparams[u].unit_id = u;
        int rc = pthread_create(&utids[u], NULL, validate_unit, &uparams[u]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create (unit %d): %s\n", u, strerror(rc));
            status = EXIT_FAILURE;
            break;
        }
        created++;
    }
    for (int u = 0; u < created; u++) {
        void *ret = NULL;
        int rc = pthread_join(utids[u], &ret);
        if (rc != 0) {
            fprintf(stderr, "pthread_join (unit %d): %s\n", u, strerror(rc));
            status = EXIT_FAILURE;
            continue;
        }
        results[u] = ret;
        if (!ret) {
            fprintf(stderr, "Validation thread %d failed to allocate its result\n", u);
            status = EXIT_FAILURE;
        }
    }
    if (status != EXIT_SUCCESS)
        goto cleanup;

    int valid_units = 0;
    for (int u = 0; u < NUM_UNITS; u++)
        valid_units += validity[u];

    if (valid_units == NUM_UNITS) {
        printf("VALID\n");
        goto cleanup;
    }
    printf("INVALID\n");

   
    printf("Validity Score: %.2f%%\n\n", 100.0 * valid_units / NUM_UNITS);

    
    for (int u = 0; u < NUM_UNITS; u++)
        dup_masks[u] = results[u]->dup_mask;   

    pthread_t btids[NUM_BLOCKS];
    struct block_params bparams[NUM_BLOCKS];
    created = 0;
    for (int b = 0; b < NUM_BLOCKS; b++) {
        bparams[b].block_id = b;
        int rc = pthread_create(&btids[b], NULL, analyze_block, &bparams[b]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create (block %d): %s\n", b, strerror(rc));
            status = EXIT_FAILURE;
            break;
        }
        created++;
    }
    for (int b = 0; b < created; b++) {
        int rc = pthread_join(btids[b], NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join (block %d): %s\n", b, strerror(rc));
            status = EXIT_FAILURE;
        }
    }
    if (status != EXIT_SUCCESS)
        goto cleanup;

    printf("Conflict Density Heatmap:\n");
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            printf(c == N - 1 ? "%d\n" : "%d ", heatmap[r][c]);
    printf("\n");

    
    int hotspots = 0;
    printf("Hotspots (score 3):\n");
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            if (heatmap[r][c] != 3)
                continue;
            if (grid[r][c])
                printf("  (%d,%d) value=%d\n", r, c, grid[r][c]);
            else
                printf("  (%d,%d) value=. (empty)\n", r, c);
            hotspots++;
        }
    }
    printf("Total Hotspots: %d\n\n", hotspots);

   
    print_missing_report(results);

cleanup:
    for (int u = 0; u < NUM_UNITS; u++)
        free(results[u]);
    return status;
}
