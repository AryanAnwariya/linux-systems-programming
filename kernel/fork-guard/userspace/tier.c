

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>


#define CTL_PATH        "/proc/fork_guard_ctl"
#define STATS_PATH      "/proc/fork_guard"
#define TIER_PATH       "/proc/sched_tier"


#define TIER_SILVER  0
#define TIER_GOLD    1
#define TIER_BRONZE  2



static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s set <username> <gold|silver|bronze>\n"
        "  %s reset\n"
        "  %s show\n",
        prog, prog, prog);
    exit(EXIT_FAILURE);
}


static int parse_tier(const char *s)
{
    if (strcasecmp(s, "gold")   == 0) return TIER_GOLD;
    if (strcasecmp(s, "silver") == 0) return TIER_SILVER;
    if (strcasecmp(s, "bronze") == 0) return TIER_BRONZE;
    return -1;
}

static const char *tier_name(int t)
{
    switch (t) {
    case TIER_GOLD:   return "GOLD";
    case TIER_BRONZE: return "BRONZE";
    default:          return "SILVER";
    }
}

static int write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}


static int send_ctl(const char *cmd)
{
    int fd;
    size_t len = strlen(cmd);

    fd = open(CTL_PATH, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "tier: cannot open %s: %s\n",
                CTL_PATH, strerror(errno));
        fprintf(stderr, "      (is the fork_guard kernel patch loaded?)\n");
        return -1;
    }

    if (write_all(fd, cmd, len) != 0) {
        fprintf(stderr, "tier: write to %s failed: %s\n",
                CTL_PATH, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}


static int cmd_set(const char *username, const char *tier_str)
{
    struct passwd *pw;
    int tier;
    char cmd[64];

 
    errno = 0;
    pw = getpwnam(username);
    if (!pw) {
        if (errno)
            fprintf(stderr, "tier: getpwnam(%s): %s\n",
                    username, strerror(errno));
        else
            fprintf(stderr, "tier: unknown user '%s'\n", username);
        return EXIT_FAILURE;
    }

   
    tier = parse_tier(tier_str);
    if (tier < 0) {
        fprintf(stderr, "tier: unknown tier '%s' "
                        "(use gold, silver, or bronze)\n", tier_str);
        return EXIT_FAILURE;
    }

   
    snprintf(cmd, sizeof(cmd), "set %u %d\n", (unsigned)pw->pw_uid, tier);

    if (send_ctl(cmd) != 0)
        return EXIT_FAILURE;

    printf("user %s (uid %u) -> %s\n",
           username, (unsigned)pw->pw_uid, tier_name(tier));
    return EXIT_SUCCESS;
}


static int cmd_reset(void)
{
    if (send_ctl("reset\n") != 0)
        return EXIT_FAILURE;

    printf("all users reset to SILVER\n");
    return EXIT_SUCCESS;
}

static int cmd_show(void)
{
    FILE *fp;
    char line[256];
    int  lines_printed = 0;

  
    fp = fopen(STATS_PATH, "r");
    if (!fp) {
        fprintf(stderr, "tier: cannot open %s: %s\n",
                STATS_PATH, strerror(errno));
        return EXIT_FAILURE;
    }

   
    if (fgets(line, sizeof(line), fp)) {
        
        line[strcspn(line, "\n")] = '\0';
        printf("%s\n\n", line);
    }
    fclose(fp);

    
    fp = fopen(TIER_PATH, "r");
    if (!fp) {
        fprintf(stderr, "tier: cannot open %s: %s\n",
                TIER_PATH, strerror(errno));
        return EXIT_FAILURE;
    }

    while (fgets(line, sizeof(line), fp)) {
       
        if (lines_printed == 0) {
          
            printf("%s", line);
            lines_printed++;
            continue;
        }
        printf("%s", line);
        lines_printed++;
    }
    fclose(fp);

    if (lines_printed <= 1) {
        
        printf("(no user is tiered; everyone is SILVER)\n");
    }

    return EXIT_SUCCESS;
}



int main(int argc, char *argv[])
{
    if (argc < 2)
        usage(argv[0]);


    if (geteuid() != 0) {
        fprintf(stderr, "tier: must run as root (use sudo)\n");
        return EXIT_FAILURE;
    }



    if (strcmp(argv[1], "set") == 0) {
        if (argc != 4) {
            fprintf(stderr, "tier: 'set' requires a username and a tier\n");
            usage(argv[0]);
        }
        return cmd_set(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "reset") == 0) {
        if (argc != 2) {
            fprintf(stderr, "tier: 'reset' takes no arguments\n");
            usage(argv[0]);
        }
        return cmd_reset();
    }

    if (strcmp(argv[1], "show") == 0) {
        if (argc != 2) {
            fprintf(stderr, "tier: 'show' takes no arguments\n");
            usage(argv[0]);
        }
        return cmd_show();
    }

    fprintf(stderr, "tier: unknown command '%s'\n", argv[1]);
    usage(argv[0]);
    return EXIT_FAILURE;
}
