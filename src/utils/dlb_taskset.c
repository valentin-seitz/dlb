/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include "LB_core/DLB_interface.h"

static int sys_size;

static const char* mu_to_str( const cpu_set_t *cpu_set ) {
    int i;
    static char str[CPU_SETSIZE*4];
    char str_i[8];
    strcpy( str, "[ " );
    for ( i=0; i<sys_size; i++ ) {
        if ( CPU_ISSET(i, cpu_set) ) {
            snprintf(str_i, sizeof(str_i), "%d ", i);
            strcat( str, str_i );
        } else { strcat( str,"- " ); }
    }
    strcat( str, "]\0" );
    return str;
}

static void mu_parse_mask( const char *str, cpu_set_t *mask ) {
    regex_t regex_bitmask;
    regex_t regex_range;
    CPU_ZERO( mask );

    /* Compile regular expression */
    if ( regcomp(&regex_bitmask, "^[0-1]+$", REG_EXTENDED|REG_NOSUB) ) {
        fprintf(stderr, "Could not compile regex");
    }

    if ( regcomp(&regex_range, "^[0-9,-]+$", REG_EXTENDED|REG_NOSUB) ) {
        fprintf(stderr, "Could not compile regex");
    }

    /* Regular expression matches bitmask, e.g.: 11110011 */
    if ( !regexec(&regex_bitmask, str, 0, NULL, 0) ) {
        // Parse
        int i;
        for (i=0; i<strlen(str); i++) {
            if ( str[i] == '1' && i < sys_size ) {
                CPU_SET( i, mask );
            }
        }
    }
    /* Regular expression matches range, e.g.: 0-3,6-7 */
    else if ( !regexec(&regex_range, str, 0, NULL, 0) ) {
        // Parse
        const char *ptr = str;
        char *endptr;
        while ( ptr < str+strlen(str) ) {
            // Discard junk at the left
            if ( !isdigit(*ptr) ) { ptr++; continue; }

            unsigned int start = strtoul( ptr, &endptr, 10 );
            ptr = endptr;

            // Single element
            if ( (*ptr == ',' || *ptr == '\0') && start < sys_size ) {
                CPU_SET( start, mask );
                ptr++;
                continue;
            }
            // Range
            else if ( *ptr == '-' ) {
                ptr++;
                if ( !isdigit(*ptr) ) { ptr++; continue; }
                unsigned int end = strtoul( ptr, &endptr, 10 );
                if ( end > start ) {
                    int i;
                    for ( i=start; i<=end && i<sys_size; i++ ) {
                        CPU_SET( i, mask );
                    }
                }
                ptr++;
                continue;
            }
            // Unexpected token
            else { }
        }
    }
    /* Regular expression does not match */
    else { }

    regfree(&regex_bitmask);
    regfree(&regex_range);

    if ( CPU_COUNT(mask) == 0 ) {
        fprintf( stderr, "Parsed mask does not seem to be a valid mask\n" );
        exit( EXIT_FAILURE );
    }
}

static void dlb_check( int error, pid_t pid ) {
    if ( error ) {
        if ( pid ) {
            fprintf( stderr, "Could not find PID %d in DLB shared memory\n", pid );
        } else {
            fprintf( stderr, "Unknown error calling DLB API\n" );
        }
        DLB_Drom_Finalize();
        exit( EXIT_FAILURE );
    }
}

static void print_usage( const char * program ) {
    fprintf( stdout, "usage:\n" );
    fprintf( stdout, "\t%s [-l|--list] [-p <pid>]\n", program );
    fprintf( stdout, "\t%s [-s|--set <cpu_list> -p <pid>]\n", program );
    fprintf( stdout, "\t%s [-r|--remove <cpu_list>] [-p <pid>]\n", program );
}

static void print_help( const char * program ) {
    fprintf( stdout, "DLB - Dynamic Load Balancing, version %s.\n", VERSION );
    print_usage(program);
    fprintf( stdout, "\n" );
    fprintf( stdout, "Retrieve or modify the CPU affinity of DLB processes.\n" );
    fprintf( stdout, "\n" );
    fprintf( stdout, "Options:\n" );
    fprintf( stdout, "  -l, --list                  list all DLB processes and their masks\n" );
    fprintf( stdout, "  -s, --set <cpu_list>        set affinity according to cpu_list (e.g., -c 0,5-7)\n" );
    fprintf( stdout, "  -r, --remove <cpu_list>     remove CPU ownership of any DLB process according to cpu_list\n" );
    fprintf( stdout, "  -p, --pid                   operate only on existing given pid\n" );
    fprintf( stdout, "  -h, --help                  print this help\n" );
}

static void set_affinity( pid_t pid, char *cpu_list ) {
    cpu_set_t new_mask;
    mu_parse_mask( cpu_list, &new_mask );

    DLB_Drom_Init();
    int error = DLB_Drom_SetProcessMask( pid, &new_mask );
    dlb_check( error, pid );
    DLB_Drom_Finalize();

    fprintf( stdout, "PID %d's current affinity set to: %s\n", pid, mu_to_str(&new_mask) );
}

static void remove_affinity_of_one( pid_t pid, const cpu_set_t *cpus_to_remove ) {
    int i, error;

    // Get current PID's mask
    cpu_set_t pid_mask;
    error = DLB_Drom_GetProcessMask(pid, &pid_mask);
    dlb_check( error, pid );

    // Remove cpus from the PID's mask
    for ( i=0; i<sys_size; ++i ) {
        if ( CPU_ISSET(i, cpus_to_remove) ) {
            CPU_CLR( i, &pid_mask );
        }
    }

    // Apply final mask
    error = DLB_Drom_SetProcessMask(pid, &pid_mask);
    dlb_check( error, pid );
}

static void remove_affinity( pid_t pid, const char *cpu_list ) {
    cpu_set_t cpus_to_remove;
    mu_parse_mask( cpu_list, &cpus_to_remove );

    DLB_Drom_Init();

    if ( pid ) {
        remove_affinity_of_one( pid, &cpus_to_remove );
    }
    else {
        // Get PID list from DLB
        int nelems;
        int *pidlist = (int*) malloc(sys_size*sizeof(int));
        DLB_Drom_GetPidList( pidlist, &nelems, sys_size);

        // Iterate pidlist
        int i;
        for ( i=0; i<nelems; ++i) {
            remove_affinity_of_one( pidlist[i], &cpus_to_remove );
        }
        free(pidlist);
    }
    DLB_Drom_Finalize();
}

static void show_affinity( pid_t pid ) {
    int error;
    cpu_set_t mask;

    DLB_Drom_Init();
    if ( pid ) {
        // Show CPU affinity of given PID
        error = DLB_Drom_GetProcessMask( pid, &mask );
        dlb_check( error, pid );
        fprintf( stdout, "PID %d's current affinity CPU list: %s\n", pid, mu_to_str(&mask) );
    } else {
        // Show CPU affinity of all processes attached to DLB
        DLB_Drom_PrintShmem();
    }
    DLB_Drom_Finalize();
}


int main ( int argc, char *argv[] ) {
    bool do_list = false;
    bool do_help = false;
    bool do_set = false;
    bool do_remove = false;

    pid_t pid = 0;
    char *cpu_list = NULL;

    int opt;
    extern char *optarg;
    struct option long_options[] = {
        {"list",     no_argument,       0, 'l'},
        {"set",      required_argument, 0, 's'},
        {"remove",   required_argument, 0, 'r'},
        {"pid",      required_argument, 0, 'p'},
        {"help",     no_argument,       0, 'h'},
        {0,          0,                 0, 0 }
    };

    while ( (opt = getopt_long(argc, argv, "ls:r:p:h", long_options, NULL)) != -1 ) {
        switch (opt) {
        case 'l':
            do_list = true;
            break;
        case 's':
            do_set = true;
            cpu_list = (char*) malloc( strlen(optarg) );
            strcpy( cpu_list, optarg );
            break;
        case 'r':
            do_remove = true;
            cpu_list = (char*) malloc( strlen(optarg) );
            strcpy( cpu_list, optarg );
            break;
        case 'p':
            pid = strtoul( optarg, NULL, 10 );
            break;
        case 'h':
            do_help = true;
            break;
        default:
            print_usage( argv[0] );
            exit( EXIT_SUCCESS );
        }
    }

    // Argument checks
    if ( do_set && do_remove ) {
        fprintf(stderr, "Only one option --set/--remove allowed\n");
        print_usage( argv[0] );
        exit( EXIT_FAILURE );
    }

    if ( do_set && !pid ) {
        fprintf(stderr, "Option --set requires a pid\n");
        print_usage( argv[0] );
        exit( EXIT_FAILURE );
    }

    sys_size = DLB_Drom_GetNumCpus();

    // Actions
    if ( do_help ) {
        print_help(argv[0]);
    }
    else if ( do_set ) {
        set_affinity( pid, cpu_list );
    }
    else if ( do_remove ) {
        remove_affinity( pid, cpu_list );
    }
    else if ( do_list || pid ) {
        show_affinity( pid );
    }
    else {
        print_usage( argv[0] );
        fprintf( stdout, "Try '%s --help' for more information.\n", argv[0] );
    }

    free( cpu_list );
    return EXIT_SUCCESS;
}