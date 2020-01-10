#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/common.h"
#include "../src/server/commands.h"

int main(int argc, char *argv[])
{
    char str[] = "Sa1b2c3d4e5!test";

    char **matches;
    int nmatches = regex_match(
        S_MSGREGEX,
        str,
        &matches
    );

    if (nmatches == 0)
    {
        printf("nessun match\n");
    }
    for (int i = 0; i < nmatches; i++)
    {
        printf("match %d: %s\n", i, matches[i]);
    }

    regex_match_free(&matches, nmatches);
}