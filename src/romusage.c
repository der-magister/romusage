// This is free and unencumbered software released into the public domain.
// For more information, please refer to <https://unlicense.org>
// bbbbbr 2020

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "common.h"
#include "banks.h"
#include "banks_color.h"
#include "map_file.h"
#include "noi_file.h"
#include "ihx_file.h"
#include "cdb_file.h"
#include "rom_file.h"

#define VERSION "version 1.2.4"

void static display_cdb_warning(void);
void static display_help(void);
int handle_args(int argc, char * argv[]);
static bool matches_extension(char *, char *);
static void init(void);
void cleanup(void);


char filename_in[MAX_STR_LEN] = {'\0'};
int  show_help_and_exit = false;


static void display_cdb_warning() {
    printf("\n"
           "   ************************ NOTICE ************************ \n"
           "    .cdb output ONLY counts (most) data from C sources.     \n"
           "   It cannot count functions and data from ASM and LIBs.    \n"
           "   Bank totals may be incorrect/missing. (-nB to hide this) \n"
           "   ************************ NOTICE ************************ \n");
}

static void display_help(void) {
    fprintf(stdout,
           "romusage input_file.[map|noi|ihx|cdb|.gb[c]|.pocket|.duck] [options]\n"
           VERSION"\n"
           "\n"
           "Options\n"
           "-h  : Show this help\n"
           "-a  : Show Areas in each Bank. Optional sort by, address:\"-aA\" or size:\"-aS\" \n"
           "-g  : Show a small usage graph per bank (-gA for ascii style)\n"
           "-G  : Show a large usage graph per bank (-GA for ascii style)\n"
           "\n"
           "-m  : Manually specify an Area -m:NAME:HEXADDR:HEXLENGTH\n"
           "-e  : Manually specify an Area that should not overlap -e:NAME:HEXADDR:HEXLENGTH\n"
           "-E  : All areas are exclusive (except HEADERs), warn for any overlaps\n"
           "-q  : Quiet, no output except warnings and errors\n"
           "-R  : Return error code for Area warnings and errors\n"
           "\n"
           "-sR : [Rainbow] Color output (-sRe for Row Ends, -sRd for Center Dimmed, -sRp %% based)\n"
           "-sP : Custom Color Palette. Colon separated entries are decimal VT100 color codes\n"
           "      -sP:DEFAULT:ROM:VRAM:SRAM:WRAM:HRAM (section based color only)\n"
           "-sC : Show Compact Output, hide non-essential columns\n"
           "-sH : Show HEADER Areas (normally hidden)\n"
           "-nB : Hide warning banner (for .cdb output)\n"
           "-nA : Hide areas (shown by default in .cdb output)\n"
           "-z  : Hide areas smaller than SIZE -z:DECSIZE\n"
           "\n"
           "Use: Read a .map, .noi, .cdb or .ihx file to display area sizes\n"
           "Example 1: \"romusage build/MyProject.map\"\n"
           "Example 2: \"romusage build/MyProject.noi -a -e:STACK:DEFF:100 -e:SHADOW_OAM:C000:A0\"\n"
           "Example 3: \"romusage build/MyProject.ihx -g\"\n"
           "Example 4: \"romusage build/MyProject.map -q -R\"\n"
           "Example 5: \"romusage build/MyProject.noi -sR -sP:90:32:90:35:33:36\"\n"
           "\n"
           "Notes:\n"
           "  * GBDK / RGBDS map file format detection is automatic.\n"
           "  * Estimates are as close as possible, but may not be complete.\n"
           "    Unless specified with -m/-e they *do not* factor regions lacking\n"
           "    complete ranges in the Map/Noi/Ihx file, for example Shadow OAM and Stack.\n"
           "  * IHX files can only detect overlaps, not detect memory region overflows.\n"
           "  * CDB file output ONLY counts (most) data from C sources.\n"
           "    It cannot count functions and data from ASM and LIBs,\n"
           "    so bank totals may be incorrect/missing.\n"
           "  * GB/GBC/ROM files are just guessing at everything, no promises.\n"
           );
}


int handle_args(int argc, char * argv[]) {

    int i;

    if( argc < 2 ) {
        display_help();
        return false;
    }

    // Copy input filename (if not preceded with option dash)
    if (argv[1][0] != '-')
        snprintf(filename_in, sizeof(filename_in), "%s", argv[1]);

    // Start at first optional argument, argc is zero based
    for (i = 1; i <= (argc -1); i++ ) {

        if (strstr(argv[i], "-h") == argv[i]) {
            display_help();
            show_help_and_exit = true;
            return true;  // Don't parse further input when -h is used
        } else if (strstr(argv[i], "-a") == argv[i]) {
            banks_output_show_areas(true);
            if      (argv[i][2] == 'S') set_option_area_sort(OPT_AREA_SORT_SIZE_DESC);
            else if (argv[i][2] == 'A') set_option_area_sort(OPT_AREA_SORT_ADDR_ASC);

        } else if (strstr(argv[i], "-sR") == argv[i]) {
            switch (argv[i][ + strlen("-sR")]) {
                case 'p': set_option_percentage_based_color(true); break; // Turns on default color mode if not set
                case 'e': set_option_color_mode(OPT_PRINT_COLOR_ROW_ENDS); break;
                case 'd': set_option_color_mode(OPT_PRINT_COLOR_WHOLE_ROW_DIMMED); break;
                case 'w': set_option_color_mode(OPT_PRINT_COLOR_WHOLE_ROW); break;
                default:  set_option_color_mode(OPT_PRINT_COLOR_DEFAULT); break;
            }
        } else if (strstr(argv[i], "-sP") == argv[i]) {
            if (!set_option_custom_bank_colors(argv[i])) {
                fprintf(stdout,"malformed custom color palette: %s\n\n", argv[i]);
                display_help();
                return false;
            }
        } else if (strstr(argv[i], "-sH") == argv[i]) {
            banks_output_show_headers(true);

        } else if (strstr(argv[i], "-sC") == argv[i]) {
            set_option_show_compact(true);

        } else if (strstr(argv[i], "-nB") == argv[i]) {
            set_option_hide_banners(true);

        } else if (strstr(argv[i], "-nA")) {
            set_option_area_sort(OPT_AREA_SORT_HIDE);

        } else if (strstr(argv[i], "-g") == argv[i]) {
            banks_output_show_minigraph(true);
            if (argv[i][2] == 'A') set_option_display_asciistyle(true);
        } else if (strstr(argv[i], "-G") == argv[i]) {
            banks_output_show_largegraph(true);
            if (argv[i][2] == 'A') set_option_display_asciistyle(true);
        } else if (strstr(argv[i], "-E") == argv[i]) {
            set_option_all_areas_exclusive(true);

        } else if (strstr(argv[i], "-q") == argv[i]) {
            set_option_quiet_mode(true);
        } else if (strstr(argv[i], "-R") == argv[i]) {
            set_option_error_on_warning(true);

        } else if (strstr(argv[i], "-z:") == argv[i]) {
            set_option_area_hide_size( strtol(argv[i] + 3, NULL, 10));

        } else if ((strstr(argv[i], "-m") == argv[i]) ||
                   (strstr(argv[i], "-e") == argv[i])) {
            if (!area_manual_add(argv[i])) {
                fprintf(stdout,"malformed manual area argument: %s\n\n", argv[i]);
                display_help();
                return false;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stdout,"Unknown argument: %s\n\n", argv[i]);
            display_help();
            return false;
        }

    }

    return true;
}


// Case insensitive
static bool matches_extension(char * filename, char * extension) {

    if (strlen(filename) >= strlen(extension)) {
        char * str_ext = filename + (strlen(filename) - strlen(extension));

        return (strncasecmp(str_ext, extension, strlen(extension)) == 0);
    }
    else
        return false;
}


static void init(void) {
    cdb_init();
    noi_init();
    banks_init();
}


// Register as an exit handler
void cleanup(void) {
    cdb_cleanup();
    noi_cleanup();
    banks_cleanup();
}


int main( int argc, char *argv[] )  {

    int ret = EXIT_FAILURE; // Default to failure on exit

    // Register cleanup with exit handler
    atexit(cleanup);

    init();

    if (handle_args(argc, argv)) {

        if (show_help_and_exit) {
            ret = EXIT_SUCCESS;
        }
        else {
            // detect file extension
            if (matches_extension(filename_in, (char *)".noi")) {
                if (noi_file_process_areas(filename_in)) {
                    banklist_finalize_and_show();
                    ret = EXIT_SUCCESS; // Exit with success
                }
            } else if (matches_extension(filename_in, (char *)".map")) {
                if (map_file_process_areas(filename_in)) {
                    banklist_finalize_and_show();
                    ret = EXIT_SUCCESS; // Exit with success
                }
            } else if (matches_extension(filename_in, (char *)".ihx")) {
                if (ihx_file_process_areas(filename_in)) {
                    banklist_finalize_and_show();
                    ret = EXIT_SUCCESS; // Exit with success
                }
            } else if (matches_extension(filename_in, (char *)".gb"    ) ||
                       matches_extension(filename_in, (char *)".gbc"   ) ||
                       matches_extension(filename_in, (char *)".pocket") ||
                       matches_extension(filename_in, (char *)".duck") ) {
                printf("ROM FILE\n");
                if (rom_file_process(filename_in)) {
                    banklist_finalize_and_show();
                    ret = EXIT_SUCCESS; // Exit with success
                }
            } else if (matches_extension(filename_in, (char *)".cdb")) {
                if (cdb_file_process_symbols(filename_in)) {
                    if (!get_option_hide_banners()) display_cdb_warning();

                    banklist_finalize_and_show();

                    if (!get_option_hide_banners()) display_cdb_warning();
                    ret = EXIT_SUCCESS; // Exit with success
                }
            }

        }
    }

    if (ret == EXIT_FAILURE)
        printf("Problem with filename or unable to open file! %s\n", filename_in);

    // Override exit code if was set during processing
    if (get_exit_error())
        ret = EXIT_FAILURE;

    return ret;
}
